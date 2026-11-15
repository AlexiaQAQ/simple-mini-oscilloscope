/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    main.h
  * @brief   Header for main.c file.
  *          This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/

/* USER CODE BEGIN Private defines */

/* 当前选中的可调项 */
#define SEL_RANGE      0     /* 量程   1X / 10X  */
#define SEL_TIMEBASE   1     /* 时基   T/div     */
#define SEL_TRIG       2     /* 触发电平          */

/* 示波器运行时上下文：前台（中断）只写少数几个字段，其余由主循环维护 */
struct Oscilloscope
{
   uint8_t  showbit;          /* 1 = 一帧采样数据就绪（DMA 完成中断置位） */
   uint8_t  keyValue;         /* 当前键值，由中断/扫描函数写入，主循环消费 */
   uint8_t  range10x;         /* 0 = 1X 档，1 = 10X 档                   */
   uint8_t  uiSel;            /* 当前选中的可调项（SEL_xxx）              */
   uint8_t  clipFlag;         /* 1 = ADC 码值贴边，Vpp 读数不可信         */
   uint8_t  duty;             /* 占空比，单位 %                          */
   uint32_t sampletime;       /* ADC 采样时间档位                         */
   uint32_t samplePeriodUs;   /* ADC 触发周期（TIM2），单位 us            */
   uint32_t gatherFreq;       /* 输入频率，单位 Hz                        */
   float    vpp;              /* 峰峰值，单位 V                           */
   float    trigLevel;        /* 触发电平，单位 V                         */
   float    voltageValue[300];/* 一帧的电压换算结果                       */
};

extern volatile struct Oscilloscope oscilloscope;

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
