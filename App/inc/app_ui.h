/**
  ******************************************************************************
  * @file    app_ui.h
  * @brief   界面刷新接口
  * @author  Alexia
  * @note    Pocket Scope —— STM32F103C8T6 + ST7735S 简易示波器
  *          代码许可：MIT（见仓库根目录 LICENSE）
  ******************************************************************************
  */
#ifndef __APP_UI_H
#define __APP_UI_H

#include "main.h"

/* 界面：顶部图例 + 波形区 + 三行读数 */
void UI_Init(void);
void UI_Refresh(volatile const struct Oscilloscope *value);
void UI_TrigMark(int16_t y, uint8_t erase);
void UI_ShowHold(uint8_t on);

#endif
