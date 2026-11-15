/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : 应用入口：初始化 -> 主循环调度
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "spi.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "tft.h"
#include "tft_init.h"
#include "app_ui.h"
#include "app_scope.h"
#include "app_key.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* TFT 初始化次数：0 = 只初始化一次（TFT_Init 内部已加软件复位 SWRESET）
                    1 = 连续初始化两次（旧版保险做法，启动多约 0.5s）
   如果改成 0 后出现白屏，说明软件复位仍未让面板就绪，改回 1。 */
#define TFT_INIT_TWICE   0

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void Init_Oscilloscope(volatile struct Oscilloscope *value);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
volatile struct Oscilloscope oscilloscope = {0};
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
	
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_SPI1_Init();
  MX_TIM3_Init();
  MX_TIM1_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
	HAL_TIM_IC_Start_IT(&htim3, TIM_CHANNEL_3);   /* 输入捕获测频   */
	HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);     /* PWM 输出，常开 */
	HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);     /* ADC 采样触发源 */

	Init_Oscilloscope(&oscilloscope);
	TIME_Init();                                  /* 应用默认时基（含 TIM2 周期）*/

	Scope_AdcCalibrate();                         /* ADC 硬件自校准 */

	HAL_Delay(100);                               /* 等 TFT 上电稳定 */
	TFT_Init();
#if TFT_INIT_TWICE
	HAL_Delay(100);
	TFT_Init();                                   /* 旧版保险做法：初始化两次 */
#endif
	TFT_Fill(0,0,160,128,BLACK);
	UI_Init();

	Scope_ZeroCalibrate();                        /* 含等待 -5V 稳定 */

	LED_Off();                                    /* 运行中灯灭 */
	HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_value, ADC_VALUE_NUM);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
		KEY_Scan(&oscilloscope);                  /* 三个功能键      */
		KEY_Handle(&oscilloscope);                /* 执行按键动作    */

		KEY_EncScan(&oscilloscope);               /* 编码器按键      */
		if(oscilloscope.keyValue == KEYDPRESS)
		{
			oscilloscope.keyValue = NoPRESS;
			Scope_Hold();                         /* 暂停 / 继续     */
		}

		if(oscilloscope.showbit == 1)
		{
			oscilloscope.showbit = 0;
			Scope_ProcessFrame();                 /* 换算、测量、绘制 */
		}

		Scope_UpdateTrigMark();                   /* 标记独立刷新，慢档也跟手 */
		UI_Refresh(&oscilloscope);                /* 内部做变化检测   */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
/**
  * @brief  运行时上下文的初始值
  * @param  value 上下文指针
  * @retval None
  */
static void Init_Oscilloscope(volatile struct Oscilloscope *value)
{
    (*value).showbit    = 0;
    (*value).keyValue   = 0;
    (*value).range10x   = 0;
    (*value).uiSel      = SEL_RANGE;
    (*value).clipFlag   = 0;
    (*value).duty       = 0;
    (*value).sampletime = ADC_SAMPLETIME_239CYCLES_5;
    (*value).gatherFreq = 0;
    (*value).vpp        = 0.0f;
    (*value).trigLevel  = 1.0f;
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
