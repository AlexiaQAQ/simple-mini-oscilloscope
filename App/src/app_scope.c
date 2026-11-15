/**
  ******************************************************************************
  * @file    app_scope.c
  * @brief   示波器帧处理：电压换算、测量、触发、波形绘制、零点校准
  * @author  Alexia
  * @note    Pocket Scope —— STM32F103C8T6 + ST7735S 简易示波器
  *          代码许可：MIT（见仓库根目录 LICENSE）
  ******************************************************************************
  */
#include "main.h"
#include "adc.h"
#include "tft.h"
#include "app_scope.h"
#include "app_ui.h"
#include "app_key.h"

/* ---- 采样与换算参数 ---------------------------------------------------- */
#define ADC_VREF             3.334f  /* 实测 VDDA，用于码值 -> 电压            */
#define ADC_FRONTEND_GAIN    1.97f   /* 前端衰减比的倒数（零点 2.483V，斜率 0.5065）*/
#define WAVE_POINTS          154U    /* 一帧绘制的点数，与波形区列数一致        */
#define WAVE_HALF_PX         24.0f   /* 半幅映射的像素数，占满显示高度          */
#define VPP_MIN_SCALE        VPP_NOISE_FLOOR  /* 低于噪声底不缩放，避免把噪声放大满屏 */
#define VPP_MIN_DUTY         0.10f   /* 低于此峰峰值占空比没有意义              */
#define ADC_CLIP_HI          4090U   /* 码值贴边判据，用于标记 ADC 饱和          */
#define ADC_CLIP_LO          5U

/* ---- 零点校准参数 ------------------------------------------------------ */
#define ZERO_CAL_AVG_NUM     64U     /* 平均点数                                */
#define ZERO_CAL_SETTLE_MS   300U    /* 等 -5V 电源稳定的时间                   */
#define ZERO_CAL_DISCARD     8U      /* 丢弃前几次未稳定的转换                  */
#define ZERO_CAL_DEFAULT     2.37f   /* 校准失效时的兜底零点（引脚电压）        */
#define ZERO_CAL_LIMIT_LO    1.50f   /* 有效零点范围                            */
#define ZERO_CAL_LIMIT_HI    3.00f

/* ---- 模块内部状态 ------------------------------------------------------ */
uint16_t adc_value[ADC_VALUE_NUM];
static float    zero_pin      = ZERO_CAL_DEFAULT;  /* 输入 0V 对应的引脚电压 */
static uint16_t triggerIndex  = 0;                 /* 本帧触发点下标         */
static float    s_median      = 0.0f;              /* 上一帧波形中点，供标记用 */
static float    s_gain        = 0.0f;              /* 上一帧纵向增益，供标记用 */
static float    s_vmin        = 0.0f;              /* 上一帧波形最低点（V）   */
static float    s_vmax        = 0.0f;              /* 上一帧波形最高点（V）   */
static uint8_t  s_rangeValid  = 0;                 /* 至少测过一帧            */

/**
  * @brief  ADC 自校准：消除内部比较器失调
  * @note   必须在任何转换启动前调用
  * @retval None
  */
void Scope_AdcCalibrate(void)
{
	if (HAL_ADCEx_Calibration_Start(&hadc1) != HAL_OK)
	{
		Error_Handler();
	}
}

/**
  * @brief  测量"输入 0V"对应的 ADC 引脚电压，作为换算的零点
  * @note   调用前输入必须接地；内部先等 -5V 电源稳定，再取平均
  * @retval None
  */
void Scope_ZeroCalibrate(void)
{
	uint32_t sum = 0;
	uint32_t guard;
	uint16_t n;
	float    zero;

	HAL_Delay(ZERO_CAL_SETTLE_MS);

	if (HAL_ADC_Start(&hadc1) != HAL_OK)
	{
		zero_pin = ZERO_CAL_DEFAULT;
		return;
	}

	for (n = 0; n < (ZERO_CAL_AVG_NUM + ZERO_CAL_DISCARD); n++)
	{
		guard = 0;
		while (__HAL_ADC_GET_FLAG(&hadc1, ADC_FLAG_EOC) == RESET)
		{
			if (++guard > 200000U)              /* 转换超时 */
			{
				HAL_ADC_Stop(&hadc1);
				zero_pin = ZERO_CAL_DEFAULT;
				return;
			}
		}

		if (n >= ZERO_CAL_DISCARD)              /* 丢掉开头几个未稳定的点 */
		{
			sum += HAL_ADC_GetValue(&hadc1);    /* 读 DR 同时清 EOC        */
		}
		else
		{
			(void)HAL_ADC_GetValue(&hadc1);
		}
	}

	HAL_ADC_Stop(&hadc1);

	zero = ((float)sum / (float)ZERO_CAL_AVG_NUM) * ADC_VREF / 4095.0f;

	/* 超出合理范围说明上电时输入不是 0V，退回固定值 */
	zero_pin = ((zero > ZERO_CAL_LIMIT_LO) && (zero < ZERO_CAL_LIMIT_HI))
	           ? zero : ZERO_CAL_DEFAULT;
}

/**
  * @brief  画触发电平三角，位置随本帧缩放变化
  * @param  median 本帧波形中点
  * @param  gain   本帧纵向缩放系数
  * @retval None
  */
static void Scope_DrawTrigMark(float median, float gain)
{
	static int16_t lastY = -1;
	float   trigRaw;
	int16_t y;

	if(oscilloscope.trigLevel >= median)
	{
		trigRaw = (float)WAVE_MID + (oscilloscope.trigLevel - median) * gain;
	}
	else
	{
		trigRaw = (float)WAVE_MID - (median - oscilloscope.trigLevel) * gain;
	}

	if(trigRaw < 0.0f)          trigRaw = 0.0f;
	if(trigRaw > (float)WAVE_H) trigRaw = (float)WAVE_H;
	y = (int16_t)((float)WAVE_YMAX - trigRaw + 0.5f);

	if(y != lastY)
	{
		if(lastY >= 0)
		{
			UI_TrigMark(lastY, 1);                /* 擦掉旧的三角 */
		}
		UI_TrigMark(y, 0);                        /* 画新的三角   */
		lastY = y;
	}
}

/**
  * @brief  处理一帧采样数据：换算电压、统计峰峰值/占空比、找触发点并绘制
  * @retval None
  */
void Scope_ProcessFrame(void)
{
	uint16_t i;
	uint16_t rawMin = 4095U, rawMax = 0U;
	uint16_t hiCount = 0;
	float    temp, vmax = -9999.0f, vmin = 9999.0f;
	float    median, gain;
	float    scale = oscilloscope.range10x ? (ADC_FRONTEND_GAIN * 10.0f) : ADC_FRONTEND_GAIN;

	/* 1) 码值换算成电压，同时统计极值与饱和情况 */
	for(i = 0; i < ADC_VALUE_NUM; i++)
	{
		if(adc_value[i] < rawMin) rawMin = adc_value[i];
		if(adc_value[i] > rawMax) rawMax = adc_value[i];

		temp = (adc_value[i] * ADC_VREF) / 4095.0f;
		oscilloscope.voltageValue[i] = (zero_pin - temp) * scale;

		if(oscilloscope.voltageValue[i] > vmax) vmax = oscilloscope.voltageValue[i];
		if(oscilloscope.voltageValue[i] < vmin) vmin = oscilloscope.voltageValue[i];
	}

	/* 2) 峰峰值、饱和标志、占空比、纵向增益 */
	oscilloscope.vpp      = vmax - vmin;
	oscilloscope.clipFlag = ((rawMax >= ADC_CLIP_HI) || (rawMin <= ADC_CLIP_LO)) ? 1U : 0U;
	median                = (vmax + vmin) * 0.5f;

	/* 纵向缩放：只要不是纯噪声就自动撑满，小信号也能看见 */
	if(oscilloscope.vpp > VPP_MIN_SCALE)
	{
		gain = WAVE_HALF_PX / (oscilloscope.vpp * 0.5f);
	}
	else
	{
		gain = 0.0f;
	}

	/* 占空比 = 高于中点的采样点占比，四舍五入到 1% */
	if(oscilloscope.vpp > VPP_MIN_DUTY)
	{
		for(i = 0; i < ADC_VALUE_NUM; i++)
		{
			if(oscilloscope.voltageValue[i] > median) hiCount++;
		}
		oscilloscope.duty = (uint8_t)((hiCount * 100U + ADC_VALUE_NUM/2U) / ADC_VALUE_NUM);
	}
	else
	{
		oscilloscope.duty = 0U;
	}

	/* 3) 立刻重启采集：绘制期间数据继续累积，不占用下一帧的时间 */
	HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_value, ADC_VALUE_NUM);

	/* 4) 边沿触发：先低于触发电平，再回升穿过它 */
	triggerIndex = 0;                             /* 找不到触发点时从头绘制 */
	for(i = 0; i < (ADC_VALUE_NUM - WAVE_POINTS); i++)
	{
		if(oscilloscope.voltageValue[i] <= oscilloscope.trigLevel)
		{
			for(; i < (ADC_VALUE_NUM - WAVE_POINTS); i++)
			{
				if(oscilloscope.voltageValue[i] > oscilloscope.trigLevel)
				{
					triggerIndex = i;
					break;
				}
			}
			break;
		}
	}

	/* 5) 逐点缩放并绘制 */
	for(i = triggerIndex; (i < triggerIndex + WAVE_POINTS) && (i < ADC_VALUE_NUM); i++)
	{
		float volt = oscilloscope.voltageValue[i];

		if(volt >= median)
		{
			volt = WAVE_MID + (volt - median) * gain;
		}
		else
		{
			volt = WAVE_MID - (median - volt) * gain;
		}
		drawCurve(WAVE_YMAX, volt);
	}

	/* 记下本帧的缩放参数与幅值范围：调电平时用它立即重画并限幅 */
	s_median     = median;
	s_gain       = gain;
	s_vmin       = vmin;
	s_vmax       = vmax;
	s_rangeValid = 1;
}

/**
  * @brief  取上一帧波形的幅值范围，用于限制触发电平的取值范围
  * @param  lo 返回最低点
  * @param  hi 返回最高点
  * @retval 1 = 有效（已测过至少一帧），0 = 还没有数据
  */
uint8_t Scope_GetSignalRange(float *lo, float *hi)
{
	if(s_rangeValid == 0)
	{
		return 0;
	}

	*lo = s_vmin;
	*hi = s_vmax;
	return 1;
}

/**
  * @brief  用上一帧的缩放参数刷新触发电平三角
  * @note   每轮主循环都可调用；内部只在位置变化时才重画，
  *         这样慢时基下（一帧要几秒）转旋钮也能立刻看到标记移动
  * @retval None
  */
void Scope_UpdateTrigMark(void)
{
	Scope_DrawTrigMark(s_median, s_gain);
}

/**
  * @brief  暂停 / 继续：冻结画面并点亮 LED，再按一次恢复
  * @retval None
  */
void Scope_Hold(void)
{
	LED_On();
	UI_ShowHold(1);

	do
	{
		KEY_EncScan(&oscilloscope);
	} while(oscilloscope.keyValue != KEYDPRESS);

	oscilloscope.keyValue = NoPRESS;
	LED_Off();
	UI_ShowHold(0);
}
