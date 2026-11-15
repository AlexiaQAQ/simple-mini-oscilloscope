/**
  ******************************************************************************
  * @file    app_ui.c
  * @brief   底部读数刷新、触发电平标记、HOLD 提示
  * @author  Alexia
  * @note    Pocket Scope —— STM32F103C8T6 + ST7735S 简易示波器
  *          代码许可：MIT（见仓库根目录 LICENSE）
  ******************************************************************************
  */
#include "main.h"
#include "tft.h"
#include "app_ui.h"
#include "app_scope.h"
#include "app_freq.h"
#include <stdio.h>
#include <string.h>

/*
*   函数内容：触发电平标记（指向左侧的实心三角形）
*   函数参数：y       三角形尖端所在行
*             erase   1 = 擦除（把整块填黑），0 = 画红色三角
*   返回值：  无
*/
void UI_TrigMark(int16_t y, uint8_t erase)
{
	int16_t col, row, yy;

	for(col = 0; col < 4; col++)          /* 尖端在 x=155，往右逐列加宽 */
	{
		for(row = -3; row <= 3; row++)
		{
			yy = y + row;
			if((yy < WAVE_TOP) || (yy > WAVE_YMAX))
			{
				continue;
			}

			if(erase || ((row >= -col) && (row <= col)))
			{
				TFT_DrawPoint((uint16_t)(155 + col), (uint16_t)yy,
				              erase ? BLACK : RED);
			}
		}
	}
}

void UI_Init(void)
{
    uint16_t i=0;

    /* 顶部图例：只做示意，实际状态显示在底部第三行 */
    TFT_ShowString(0,0,(uint8_t *)"AC/DC",LGRAY,BLACK,16,0);
    TFT_ShowString(112,0,(uint8_t *)"10X/1X",LGRAY,BLACK,16,0);

    /* 波形区坐标轴 */
    for(i=0;i<=WAVE_XMAX;i++)
    {
        TFT_DrawPoint(i,WAVE_YMAX+1,GREEN);          /* 横轴 */
    }
    for(i=WAVE_TOP;i<=WAVE_YMAX;i++)
    {
        TFT_DrawPoint(0,i,GREEN);                    /* 纵轴 */
    }
    for(i=0;i<=14;i++)                               /* 横轴刻度，每 10 像素一格 */
    {
        TFT_DrawPoint((i*10)+2,WAVE_YMAX+2,GREEN);
        TFT_DrawPoint((i*10)+3,WAVE_YMAX+2,GREEN);
    }
    for(i=WAVE_TOP;i<=WAVE_YMAX;i++)
    {
        TFT_DrawPoint(WAVE_XMAX+1,i,YELLOW);         /* 触发电平标记栏分隔线 */
    }
}

/* 把字符串补空格到固定宽度：数值变短时用空格盖掉上一次的残留字符 */
static void Ui_Pad(char *buf, uint8_t width)
{
	uint8_t n = (uint8_t)strlen(buf);

	if(n >= width)
	{
		return;
	}
	while(n < width)
	{
		buf[n++] = ' ';
	}
	buf[width] = '\0';
}

/*
*   函数内容：  刷新底部读数（内部做变化检测，只在内容改变时重画）
*   函数参数：  示波器上下文
*   返回值：    无
*/
void UI_Refresh(volatile const struct Oscilloscope *value)
{	
	char buf[24];
	uint32_t freq;
	float    vpp;
	static char lastVpp[10]  = "";
	static char lastFrq[10]  = "";
	static char lastPer[10]  = "";
	static char lastDty[8]   = "";
	static char lastRng[8]   = "";
	static char lastTdv[8]   = "";
	static char lastTrg[10]  = "";
	static uint8_t lastSel   = 0xFF;
	static uint8_t lastClip  = 0xFF;
	static uint8_t labelsDone = 0;

	/* 噪声底以内视为无信号：Vpp 显示 0，频率与周期也按 0 处理 */
	vpp = (*value).vpp;
	if(vpp < VPP_NOISE_FLOOR)
	{
		vpp = 0.0f;
	}

	freq = (*value).gatherFreq;

	/* 测频通道 1.5 s 没收到边沿 = 没有可用信号：
	   信号不跨比较器阈值时不会有捕获，不处理就会一直显示上次的值 */
	if((vpp <= 0.0f) || (Freq_IsAlive(1500U) == 0U))
	{
		freq = 0U;
	}

	/* 固定标签只画一次：灰字放在每列的左端 */
	if(!labelsDone)
	{
		labelsDone = 1;
		TFT_ShowString(0,78,(uint8_t *)"VPP",LGRAY,BLACK,16,0);
		TFT_ShowString(80,78,(uint8_t *)"FRQ",LGRAY,BLACK,16,0);
		TFT_ShowString(0,94,(uint8_t *)"PER",LGRAY,BLACK,16,0);
		TFT_ShowString(80,94,(uint8_t *)"DTY",LGRAY,BLACK,16,0);
	}

	/* ---- 第一行左：Vpp（ADC 饱和时变红） ---- */
	if(vpp >= 10.0f)
	{
		sprintf(buf,"%5.1fV",vpp);
	}
	else
	{
		sprintf(buf,"%5.2fV",vpp);
	}
	if((strcmp(buf,lastVpp) != 0) || (lastClip != (*value).clipFlag))
	{
		strcpy(lastVpp,buf);
		lastClip = (*value).clipFlag;
		TFT_ShowString(24,78,(uint8_t *)buf,
			(*value).clipFlag ? RED : GREEN,BLACK,16,0);
	}

	/* ---- 第一行右：频率 ---- */
	/* 统一保留 3 位有效数字：9.90KHz / 47.0KHz / 470KHz */
	if(freq >= 100000U)
	{
		sprintf(buf,"%dKHz",freq / 1000U);
	}
	else if(freq >= 10000U)
	{
		sprintf(buf,"%d.%dKHz",freq / 1000U, (freq % 1000U) / 100U);
	}
	else if(freq >= 1000U)
	{
		sprintf(buf,"%d.%02dKHz",freq / 1000U, (freq % 1000U) / 10U);
	}
	else
	{
		sprintf(buf,"%dHz",freq);
	}
	Ui_Pad(buf,7);
	if(strcmp(buf,lastFrq) != 0)
	{
		strcpy(lastFrq,buf);
		TFT_ShowString(104,78,(uint8_t *)buf,GREEN,BLACK,16,0);
	}

	/* ---- 第二行左：周期 ---- */
	if(freq == 0U)
	{
		sprintf(buf,"  ---- ");
	}
	else
	{
		float periodUs = 1000000.0f / (float)freq;

		if(periodUs < 1000.0f)
		{
			sprintf(buf,"%5.1fus",periodUs);
		}
		else
		{
			sprintf(buf,"%5.2fms",periodUs / 1000.0f);
		}
	}
	if(strcmp(buf,lastPer) != 0)
	{
		strcpy(lastPer,buf);
		TFT_ShowString(24,94,(uint8_t *)buf,GREEN,BLACK,16,0);
	}

	/* ---- 第二行右：占空比 ---- */
	sprintf(buf,"%3d%%",(*value).duty);
	if(strcmp(buf,lastDty) != 0)
	{
		strcpy(lastDty,buf);
		TFT_ShowString(104,94,(uint8_t *)buf,GREEN,BLACK,16,0);
	}

	/* ---- 第三行：量程 / 时基 / 触发电平（选中的黄底高亮） ---- */

	/* 量程 */
	sprintf(buf,"%s",(*value).range10x ? "10X" : "1X ");
	if((strcmp(buf,lastRng) != 0) || (lastSel != (*value).uiSel))
	{
		strcpy(lastRng,buf);
		TFT_ShowString(0,110,(uint8_t *)buf,
			((*value).uiSel == SEL_RANGE) ? BLACK : GREEN,
			((*value).uiSel == SEL_RANGE) ? YELLOW: BLACK,16,0);
	}

	/* 时基 */
	/* 时基：T/div = 10 × 采样间隔（波形区每格 10 个采样点） */
	{
		uint32_t tdUs = (*value).samplePeriodUs * 10U;

		if(tdUs < 1000U)
		{
			sprintf(buf,"%3dus",(int)tdUs);
		}
		else
		{
			sprintf(buf,"%3dms",(int)(tdUs / 1000U));
		}
	}
	if((strcmp(buf,lastTdv) != 0) || (lastSel != (*value).uiSel))
	{
		strcpy(lastTdv,buf);
		TFT_ShowString(44,110,(uint8_t *)buf,
			((*value).uiSel == SEL_TIMEBASE) ? BLACK : GREEN,
			((*value).uiSel == SEL_TIMEBASE) ? YELLOW: BLACK,16,0);
	}

	sprintf(buf,"T%1.2fV",(*value).trigLevel);
	Ui_Pad(buf,7);
	if((strcmp(buf,lastTrg) != 0) || (lastSel != (*value).uiSel))
	{
		strcpy(lastTrg,buf);
		TFT_ShowString(104,110,(uint8_t *)buf,
			((*value).uiSel == SEL_TRIG) ? BLACK : GREEN,
			((*value).uiSel == SEL_TRIG) ? YELLOW: BLACK,16,0);
	}

	lastSel = (*value).uiSel;
}

/**
  * @brief  显示 / 清除顶部的 HOLD 提示
  * @param  on 1 = 显示，0 = 清除
  * @retval None
  */
void UI_ShowHold(uint8_t on)
{
	if(on)
	{
		TFT_ShowString(48,0,(uint8_t *)"HOLD",RED,BLACK,16,0);
	}
	else
	{
		TFT_ShowString(48,0,(uint8_t *)"    ",BLACK,BLACK,16,0);
	}
}
