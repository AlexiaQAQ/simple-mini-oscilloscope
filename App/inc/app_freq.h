/**
  ******************************************************************************
  * @file    app_freq.h
  * @brief   频率测量接口
  * @author  Alexia
  * @note    Pocket Scope —— STM32F103C8T6 + ST7735S 简易示波器
  *          代码许可：MIT（见仓库根目录 LICENSE）
  ******************************************************************************
  */
#ifndef __APP_FREQ_H
#define __APP_FREQ_H

#include "main.h"

/* 测频：TIM3 CH3 输入捕获，结果写入 oscilloscope.gatherFreq
   实现位于 app_freq.c 的 HAL_TIM_IC_CaptureCallback（HAL 弱回调） */

#define FREQ_AVG_NUM   16U   /* 累计多少个周期算一次，抑制 ±1 计数抖动 */

/* 测频通道是否还在收到边沿（超时则说明没有可用输入） */
uint8_t Freq_IsAlive(uint32_t timeoutMs);

#endif
