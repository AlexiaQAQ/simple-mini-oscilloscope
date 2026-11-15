/**
  ******************************************************************************
  * @file    tft_init.h
  * @brief   TFT 初始化接口
  * @author  Alexia
  * @note    Pocket Scope —— STM32F103C8T6 + ST7735S 简易示波器
  *          代码许可：MIT（见仓库根目录 LICENSE）
  ******************************************************************************
  */
#ifndef TFT_INIT_H
#define TFT_INIT_H

#include "main.h"

#define USE_HORIZONTAL 2  //���ú�������������ʾ 0��1Ϊ���� 2��3Ϊ����

#if USE_HORIZONTAL==0||USE_HORIZONTAL==1
#define LCD_W 128
#define LCD_H 160

#else
#define LCD_W 160
#define LCD_H 128
#endif

void TFT_WR_DATA8(uint8_t data);
void TFT_WR_DATA(uint16_t data);
void TFT_WR_REG(uint8_t reg);
void TFT_Address_Set(uint16_t x1,uint16_t y1,uint16_t x2,uint16_t y2);
void TFT_Init(void);

#endif
