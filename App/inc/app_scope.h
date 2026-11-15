/**
  ******************************************************************************
  * @file    app_scope.h
  * @brief   示波器核心数据结构与接口
  * @author  Alexia
  * @note    Pocket Scope —— STM32F103C8T6 + ST7735S 简易示波器
  *          代码许可：MIT（见仓库根目录 LICENSE）
  ******************************************************************************
  */
#ifndef __APP_SCOPE_H
#define __APP_SCOPE_H

#include "main.h"

#define ADC_VALUE_NUM     300U     /* 每帧采样点数 */

/* 峰峰值低于此值视为"没有信号"（ADC 噪声底）：
   波形不缩放、Vpp 显示 0.00V、频率/周期显示为 0 */
#define VPP_NOISE_FLOOR   0.03f

/* 采样缓冲：DMA 直接写入，主循环只读（处理前先拷到 voltageValue[]） */
extern uint16_t adc_value[];

/* 上电流程：先 ADC 自校准，再零点校准，最后才允许启动 DMA */
void  Scope_AdcCalibrate(void);
void  Scope_ZeroCalibrate(void);

/* 处理一帧：换算电压、统计峰峰值/占空比、找触发点、绘制波形 */
void  Scope_ProcessFrame(void);

/* 暂停 / 继续（冻结画面，再按一次恢复） */
void  Scope_Hold(void);

/* 用上一帧的缩放参数刷新触发电平标记（每轮主循环调用） */
void  Scope_UpdateTrigMark(void);

/* 取上一帧波形的幅值范围，用于限制触发电平的取值 */
uint8_t Scope_GetSignalRange(float *lo, float *hi);

#endif
