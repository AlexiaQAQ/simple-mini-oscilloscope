/**
  ******************************************************************************
  * @file    app_freq.c
  * @brief   频率测量：输入捕获 + 多周期平均
  * @author  Alexia
  * @note    Pocket Scope —— STM32F103C8T6 + ST7735S 简易示波器
  *          代码许可：MIT（见仓库根目录 LICENSE）
  ******************************************************************************
  */
#include "main.h"
#include "tim.h"
#include "app_freq.h"
static __IO uint16_t ccnumber = 0;
volatile uint32_t freqLastTick = 0;   /* 最近一次捕获的时间戳（ms） */
static __IO uint32_t freq = 0;
static __IO uint16_t readvalue1 = 0;
static __IO uint32_t count = 0;

/* 频率测量：累计 FREQ_AVG_NUM 个周期再算一次，抑制 ±1 计数的量化抖动 */
/* 1MHz 时基下，单周期测 47kHz 只有 21~22 个计数，误差 ±4.7%；          */
/* 累计 16 个周期后误差降到约 ±0.3%，显示就稳定了。                      */

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
	if(htim->Instance == TIM3)
	{
		uint16_t now = (uint16_t)HAL_TIM_ReadCapturedValue(&htim3, TIM_CHANNEL_3);
		uint32_t period;

		freqLastTick = HAL_GetTick();      /* 只要还有边沿就刷新时间戳 */

		if(ccnumber == 0)                 /* 第一个沿：只记起点 */
		{
			readvalue1 = now;
			ccnumber   = 1;
			count      = 0;               /* 累计计数清零 */
			return;
		}

		if(now > readvalue1)
		{
			period = (uint32_t)(now - readvalue1);
		}
		else                              /* 计满一轮回绕 */
		{
			period = (uint32_t)(0x10000UL - readvalue1 + now);
		}
		readvalue1 = now;
		count += period;                  /* 累计 N 个周期的总计数 */

		if(ccnumber < FREQ_AVG_NUM)
		{
			ccnumber++;
			return;
		}

		/* 凑满 N 个周期，算一次频率 */
		if(count > 0U)
		{
			freq = (1000000UL * FREQ_AVG_NUM) / count;
			oscilloscope.gatherFreq = freq;
		}
		ccnumber = 0;
		freq     = 0;
	}
}
/**
  * @brief  判断测频通道最近有没有收到边沿
  * @param  timeoutMs 超时时间（ms）
  * @retval 1 = 最近有捕获；0 = 超时没有捕获（信号太小或不跨比较器阈值）
  */
uint8_t Freq_IsAlive(uint32_t timeoutMs)
{
	return ((HAL_GetTick() - freqLastTick) < timeoutMs) ? 1U : 0U;
}
