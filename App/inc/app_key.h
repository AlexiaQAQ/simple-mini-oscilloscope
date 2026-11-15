/**
  ******************************************************************************
  * @file    app_key.h
  * @brief   按键接口与键值编码
  * @author  Alexia
  * @note    Pocket Scope —— STM32F103C8T6 + ST7735S 简易示波器
  *          代码许可：MIT（见仓库根目录 LICENSE）
  ******************************************************************************
  */
#ifndef __APP_KEY_H
#define __APP_KEY_H

#include "main.h"

/* 键值编码 */
#define NoPRESS      0x04
#define KEY1PRESS    0x05    /* PB13：选量程     */
#define KEY2PRESS    0x06    /* PB14：选时基     */
#define KEY3PRESS    0x07    /* PB15：选触发电平 */
#define KEYAPRESS    0x08    /* 编码器逆时针     */
#define KEYBPRESS    0x09    /* 编码器顺时针     */
#define KEYDPRESS    0x10    /* 编码器按键       */

/* 编码器按键所在引脚（本板 PA15，上拉输入，按下接地） */
#define ENC_BTN_PORT    GPIOA
#define ENC_BTN_PIN     GPIO_PIN_15

/* 三个功能键 + 编码器按键扫描 */
void KEY_Scan(volatile struct Oscilloscope *value);
void KEY_Handle(volatile struct Oscilloscope *value);
void KEY_EncScan(volatile struct Oscilloscope *value);

/* 时基：初始化默认档位（上电调用一次） */
void TIME_Init(void);

/* 板载 LED（PC13，低电平点亮） */
void LED_On(void);
void LED_Off(void);

#endif
