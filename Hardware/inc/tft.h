/**
  ******************************************************************************
  * @file    tft.h
  * @brief   TFT 驱动接口与屏幕参数
  * @author  Alexia
  * @note    Pocket Scope —— STM32F103C8T6 + ST7735S 简易示波器
  *          代码许可：MIT（见仓库根目录 LICENSE）
  ******************************************************************************
  */
#ifndef __TFT_H
#define __TFT_H

#include "main.h"

/* 波形区几何：x 0..WAVE_XMAX, y WAVE_TOP..WAVE_YMAX（y 由 raw 值翻转得到） */
#define WAVE_XMAX   153
#define WAVE_TOP    16      /* 顶部留给状态栏（AC/DC、1X/10X） */
#define WAVE_YMAX   71
#define WAVE_H      (WAVE_YMAX - WAVE_TOP)
#define WAVE_MID    27      /* 波形垂直中心对应的 raw 值 */

/* 画笔颜色（RGB565） */
#define WHITE         0xFFFF
#define BLACK         0x0000
#define BLUE          0x001F
#define BRED          0XF81F
#define GRED          0XFFE0
#define GBLUE         0X07FF
#define RED           0xF800
#define MAGENTA       0xF81F
#define GREEN         0x07E0
#define CYAN          0x7FFF
#define YELLOW        0xFFE0
#define PURPLE        0x780F
#define BROWN         0XBC40
#define BRRED         0XFC07
#define GRAY          0X8430
#define DARKBLUE      0X01CF
#define LIGHTBLUE     0X7D7C
#define GRAYBLUE      0X5458
#define LIGHTGREEN    0X841F
#define LGRAY         0XC618
#define LGRAYBLUE     0XA651
#define LBBLUE        0X2B12

void TFT_Fill(uint16_t xsta,uint16_t ysta,uint16_t xend,uint16_t yend,uint16_t color);
void TFT_DrawPoint(uint16_t x,uint16_t y, uint16_t color);
void TFT_DrawLine(uint16_t x1,uint16_t y1,uint16_t x2,uint16_t y2,uint16_t color);
void TFT_DrawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,uint16_t color);
void Draw_Circle(uint16_t x0,uint16_t y0,uint8_t r,uint16_t color);
void TFT_ShowChar(uint16_t x,uint16_t y,uint8_t num,uint16_t fc,uint16_t bc,uint8_t sizey,uint8_t mode);
void TFT_ShowString(uint16_t x,uint16_t y,const uint8_t *p,uint16_t fc,uint16_t bc,uint8_t sizey,uint8_t mode);
void TFT_ShowChinese(uint16_t x,uint16_t y,uint8_t *s,uint16_t fc,uint16_t bc,uint8_t sizey,uint8_t mode);
void TFT_ShowChinese12x12(uint16_t x,uint16_t y,uint8_t *s,uint16_t fc,uint16_t bc,uint8_t sizey,uint8_t mode);
void TFT_ShowChinese16x16(uint16_t x,uint16_t y,uint8_t *s,uint16_t fc,uint16_t bc,uint8_t sizey,uint8_t mode);
void TFT_ShowChinese24x24(uint16_t x,uint16_t y,uint8_t *s,uint16_t fc,uint16_t bc,uint8_t sizey,uint8_t mode);
void drawCurve(uint8_t yOffset,short int rawValue);

#endif
