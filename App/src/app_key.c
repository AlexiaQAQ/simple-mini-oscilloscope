/**
  ******************************************************************************
  * @file    app_key.c
  * @brief   按键与编码器扫描、时基切换、LED 指示
  * @author  Alexia
  * @note    Pocket Scope —— STM32F103C8T6 + ST7735S 简易示波器
  *          代码许可：MIT（见仓库根目录 LICENSE）
  ******************************************************************************
  */
#include "main.h"
#include "adc.h"
#include "tim.h"
#include "app_key.h"
#include "app_scope.h"

#define LED_ACTIVE_LOW   1   /* 1 = 低电平点亮（LED 接 3.3V）；0 = 高电平点亮 */

/* 三个功能键：中断只记录"按下时刻"，主循环延时确认（非阻塞消抖） */
/* KEY1/2/3 各并联 100nF 做硬件消抖（τ ≈ 40kΩ × 100nF = 4ms），  */
/* 所以软件只需很短的时间窗；编码器按键没有硬件消抖，保留较长窗口。 */
/* 正交编码器状态表：索引 = (上一状态 << 2) | 当前状态，状态 = (A<<1)|B
   +1 = 顺时针一步，-1 = 逆时针一步，0 = 无效跳变（抖动/同相跳变） */
static const int8_t kQuadTable[16] =
{
	 0, -1, +1,  0,
	+1,  0,  0, -1,
	-1,  0,  0, +1,
	 0, +1, -1,  0
};

/* 每个有效边沿走一步：单步更轻快，也不怕偶尔丢沿                    */
/* 若变成"转一格跳两下"改成 2；若"两格才走一步"改成 3 或 4          */
#define ENC_EDGES_PER_STEP   1

#define KEY_DEBOUNCE_MS   5U
#define ENC_DEBOUNCE_MS   20U

static volatile uint32_t key1_tick = 0, key2_tick = 0, key3_tick = 0;
static volatile uint8_t  key1_pend = 0, key2_pend = 0, key3_pend = 0;

void LED_On(void)
{
#if LED_ACTIVE_LOW
	HAL_GPIO_WritePin(GPIOC,GPIO_PIN_13,GPIO_PIN_RESET);   /* 低电平点亮 */
#else
	HAL_GPIO_WritePin(GPIOC,GPIO_PIN_13,GPIO_PIN_SET);
#endif
}

void LED_Off(void)
{
#if LED_ACTIVE_LOW
	HAL_GPIO_WritePin(GPIOC,GPIO_PIN_13,GPIO_PIN_SET);
#else
	HAL_GPIO_WritePin(GPIOC,GPIO_PIN_13,GPIO_PIN_RESET);
#endif
}

/* 时基档位表：采样间隔由 TIM2 的触发周期决定，ADC 采样时间只需小于周期 */
/* T/div = 10 × 触发周期（波形区每格 10 个采样点）                        */
typedef struct
{
	uint32_t smp;          /* ADC 采样时间          */
	uint32_t periodUs;     /* TIM2 触发周期，单位 us */
} TimeBase_t;

static const TimeBase_t kTimeBase[6] =
{
	{ ADC_SAMPLETIME_1CYCLE_5,     2U     },  /* T/div  20us */
	{ ADC_SAMPLETIME_28CYCLES_5,   5U     },  /* T/div  50us */
	{ ADC_SAMPLETIME_55CYCLES_5,   20U    },  /* T/div 200us */
	{ ADC_SAMPLETIME_71CYCLES_5,   200U   },  /* T/div   2ms */
	{ ADC_SAMPLETIME_239CYCLES_5,  2000U  },  /* T/div  20ms */
	{ ADC_SAMPLETIME_239CYCLES_5,  10000U }   /* T/div 100ms */
};

#define TIMEBASE_NUM          6U
#define TIMEBASE_DEFAULT_IDX  2U          /* 上电默认 200us/div */

/**
  * @brief  应用第 idx 档时基：同时设置 ADC 采样时间和 TIM2 触发周期
  * @param  idx 档位下标（0~5，超范围会被钳位）
  * @retval None
  */
static void TimeBase_Apply(uint8_t idx)
{
	ADC_ChannelConfTypeDef sConfig = {0};

	if(idx >= TIMEBASE_NUM)
	{
		idx = TIMEBASE_NUM - 1U;
	}

	oscilloscope.sampletime     = kTimeBase[idx].smp;
	oscilloscope.samplePeriodUs = kTimeBase[idx].periodUs;

	/* TIM2 在 1MHz 计数，ARR = 周期(us) - 1 */
	__HAL_TIM_SET_AUTORELOAD(&htim2, kTimeBase[idx].periodUs - 1U);

	sConfig.Channel      = ADC_CHANNEL_9;
	sConfig.Rank         = ADC_REGULAR_RANK_1;
	sConfig.SamplingTime = kTimeBase[idx].smp;
	if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
	{
		Error_Handler();
	}
}

/**
  * @brief  上电时应用默认时基
  * @retval None
  */
void TIME_Init(void)
{
	TimeBase_Apply(TIMEBASE_DEFAULT_IDX);
}

/* 旋钮把时基往前/往后拨一档 */
static void TimeBase_Step(volatile struct Oscilloscope *value, int8_t dir)
{
	int8_t idx = 0;
	int8_t i;

	for(i = 0; i < (int8_t)TIMEBASE_NUM; i++)
	{
		if(kTimeBase[i].periodUs == (*value).samplePeriodUs)
		{
			idx = i;
			break;
		}
	}

	idx += dir;
	if(idx < 0)                     idx = 0;
	if(idx >= (int8_t)TIMEBASE_NUM)  idx = (int8_t)TIMEBASE_NUM - 1;

	TimeBase_Apply((uint8_t)idx);
}


/* 旋钮转动：调整当前选中项的数值/档位 */
static void Adjust_Selected(volatile struct Oscilloscope *value, int8_t dir)
{
	switch((*value).uiSel)
	{
		case SEL_RANGE:
			(*value).range10x = ((*value).range10x == 0) ? 1 : 0;
			break;

		case SEL_TIMEBASE:
			/* dir>0 = 变快（T/div 变小，放大波形） */
			TimeBase_Step(value, dir);
			break;

		case SEL_TRIG:
		{
			/* 步进取当前峰峰值的 5%；取值范围限制在波形的最高/最低点之间，
			   避免电平跑到波形之外（那样永远找不到触发点）          */
			float step = (*value).vpp * 0.05f;
			float lo   = (*value).range10x ? -30.0f : -3.0f;
			float hi   = (*value).range10x ?  50.0f :  5.0f;
			float sigLo, sigHi;

			if(Scope_GetSignalRange(&sigLo, &sigHi) != 0)
			{
				if(sigLo > lo) lo = sigLo;
				if(sigHi < hi) hi = sigHi;
			}

			if(step < 0.01f) step = 0.01f;
			if(step > 0.50f) step = 0.50f;

			(*value).trigLevel += ((dir > 0) ? step : -step);
			if((*value).trigLevel > hi) (*value).trigLevel = hi;
			if((*value).trigLevel < lo) (*value).trigLevel = lo;
		}
			break;

		default:
			break;
	}
}

void KEY_Handle(volatile struct Oscilloscope *value)
{
	switch((*value).keyValue)
	{
		/* KEY1 选量程，KEY2 选时基，KEY3 选触发电平 */
		case KEY1PRESS:  (*value).uiSel = SEL_RANGE;    break;
		case KEY2PRESS:  (*value).uiSel = SEL_TIMEBASE; break;
		case KEY3PRESS:  (*value).uiSel = SEL_TRIG;     break;

		case KEYAPRESS:  Adjust_Selected(value, -1);    break;
		case KEYBPRESS:  Adjust_Selected(value, +1);    break;

		default:
			break;
	}
	(*value).keyValue = NoPRESS;
}
/**
  * @brief  确认一个待处理的按键：距边沿超过消抖时间且引脚仍为低，才算有效
  * @param  pend 待确认标志（中断里置 1）
  * @param  tick 边沿时刻
  * @retval 有效返回对应键值，否则返回 0
  */
static uint8_t KEY_Confirm(volatile uint8_t *pend, volatile uint32_t *tick,
                           GPIO_TypeDef *port, uint16_t pin, uint8_t code,
                           uint16_t debounceMs)
{
	if(*pend == 0)
	{
		return 0;
	}
	if((HAL_GetTick() - *tick) < debounceMs)
	{
		return 0;                       /* 消抖时间还没到，下一轮再看 */
	}

	*pend = 0;                          /* 无论成功与否都清标志 */

	return (HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_RESET) ? code : 0;
}

void KEY_Scan(volatile struct Oscilloscope *value)
{
	uint8_t code;

	code = KEY_Confirm(&key1_pend, &key1_tick, GPIOB, GPIO_PIN_13,
	                   KEY1PRESS, KEY_DEBOUNCE_MS);
	if(code){ (*value).keyValue = code; return; }

	code = KEY_Confirm(&key2_pend, &key2_tick, GPIOB, GPIO_PIN_14,
	                   KEY2PRESS, KEY_DEBOUNCE_MS);
	if(code){ (*value).keyValue = code; return; }

	code = KEY_Confirm(&key3_pend, &key3_tick, GPIOB, GPIO_PIN_15,
	                   KEY3PRESS, KEY_DEBOUNCE_MS);
	if(code){ (*value).keyValue = code; }
}
void KEY_EncScan(volatile struct Oscilloscope *value)
{
	/* 编码器按键接 PA15，没有 EXTI：软件边沿检测                      */
	/* 策略：按下立即出键值（短按也灵敏），松手并稳定 20ms 后才重新武装，
	         这样触点抖动不会连发。                                    */
	static uint8_t  lastLevel = 1;      /* 上拉，未按下为高 */
	static uint8_t  armed     = 1;      /* 1 = 允许产生事件 */
	static uint32_t hiTick    = 0;      /* 引脚变高的时刻   */
	uint8_t  level = (HAL_GPIO_ReadPin(ENC_BTN_PORT, ENC_BTN_PIN) == GPIO_PIN_RESET) ? 0U : 1U;
	uint32_t now   = HAL_GetTick();

	if(level == 0U)                     /* 按住 */
	{
		if((armed != 0U) && (lastLevel != 0U))
		{
			(*value).keyValue = KEYDPRESS;   /* 下降沿：立即响应 */
			armed = 0;
		}
		hiTick = 0;
	}
	else                                /* 松开 */
	{
		if(hiTick == 0U)
		{
			hiTick = now;
		}
		if((armed == 0U) && ((now - hiTick) >= ENC_DEBOUNCE_MS))
		{
			armed = 1;                  /* 稳定松开后才允许下一次触发 */
		}
	}

	lastLevel = level;
}
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	if((GPIO_Pin == GPIO_PIN_3) || (GPIO_Pin == GPIO_PIN_4))
	{
		/* 双相四边沿解码：A、B 每次变化都查表，抖动会形成"进-退"自动抵消 */
		static uint8_t lastState = 0xFF;      /* 0xFF = 还没初始化 */
		static int8_t  accum     = 0;
		uint8_t a = (HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_4) == GPIO_PIN_SET) ? 1U : 0U;
		uint8_t b = (HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_3) == GPIO_PIN_SET) ? 1U : 0U;
		uint8_t now = (uint8_t)((a << 1) | b);
		int8_t  d;

		if(lastState == 0xFF)                 /* 第一次进中断只记状态 */
		{
			lastState = now;
			return;
		}

		d = kQuadTable[(((uint8_t)lastState & 0x03U) << 2) | now];
		lastState = now;

		if(d == 0)
		{
			return;                           /* 无效跳变（抖动）忽略 */
		}

		accum += d;
		if(accum >= ENC_EDGES_PER_STEP)
		{
			accum = 0;
			oscilloscope.keyValue = KEYBPRESS;   /* 顺时针 */
		}
		else if(accum <= -ENC_EDGES_PER_STEP)
		{
			accum = 0;
			oscilloscope.keyValue = KEYAPRESS;   /* 逆时针 */
		}
	}	if(GPIO_Pin == GPIO_PIN_13)
	{
		key1_tick = HAL_GetTick();
		key1_pend = 1;
	}
	if(GPIO_Pin == GPIO_PIN_14)
	{
		key2_tick = HAL_GetTick();
		key2_pend = 1;
	}
	if(GPIO_Pin == GPIO_PIN_15)
	{
		key3_tick = HAL_GetTick();
		key3_pend = 1;
	}
}
