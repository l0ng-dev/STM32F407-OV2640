/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#include "dcmi.h"
#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"
#include "fsmc.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "lcd.h"
#include "sccb.h"
#include "ov2640.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define DCMI_FRAME_WIDTH       320U
#define DCMI_FRAME_HEIGHT      240U
#define DCMI_BYTES_PER_PIXEL   2U
#define DCMI_FRAME_WORD_COUNT  ((DCMI_FRAME_WIDTH * DCMI_FRAME_HEIGHT * \
                                 DCMI_BYTES_PER_PIXEL) / sizeof(uint32_t))

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static SCCB_Status_t ov2640_status;
static OV2640_Status_t ov2640_init_status;
static volatile uint8_t dcmi_frame_ready;
static volatile uint8_t dcmi_error_pending;
static volatile uint32_t dcmi_error_code;
static uint8_t camera_preview_ready;
extern DMA_HandleTypeDef hdma_dcmi;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void LCD_Test_ShowId(uint16_t id);
static HAL_StatusTypeDef DCMI_ConfigureLcdDirectDma(void);
static HAL_StatusTypeDef DCMI_StartRgb565Snapshot(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#if defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050)
__asm(".global __use_no_semihosting\n\t");
__asm(".global __ARM_use_no_argv\n\t");
#endif

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
  MX_DCMI_Init();
  MX_FSMC_Init();
  MX_TIM6_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  OV2640_HardwareReset();
  ov2640_status = SCCB_Init();
  if (ov2640_status != SCCB_STATUS_OK)
  {
    printf("OV2640 Init Failed\r\n");
    printf("OV2640 ERROR: SCCB init failed (%u)\r\n", (unsigned int)ov2640_status);
  }
  else
  {
    ov2640_init_status = OV2640_Init();
    if (ov2640_init_status != OV2640_STATUS_OK)
    {
      printf("OV2640 Init Failed\r\n");
      printf("OV2640 ERROR: init status (%u)\r\n", (unsigned int)ov2640_init_status);
    }
    else
    {
      printf("OV2640 Init OK\r\n");

      ov2640_status = OV2640_SetOutputSize(DCMI_FRAME_WIDTH,
                                          DCMI_FRAME_HEIGHT);
      if (ov2640_status != SCCB_STATUS_OK)
      {
        printf("OV2640 ERROR: output size failed (%u)\r\n",
               (unsigned int)ov2640_status);
      }
      else
      {
        camera_preview_ready = 1U;
      }
    }
  }

  /* FSMC初始化完成后再访问LCD。 */
  lcd_init();
  lcd_display_dir(1U);

  /* LCD基础显示测试：文字、控制器ID和RGB色块。 */
  lcd_clear(WHITE);
  lcd_show_string(20, 20, 200, 24, 24, "LCD TEST", BLACK);
  lcd_show_string(20, 50, 200, 24, 24, "FSMC OK", BLUE);
  LCD_Test_ShowId(lcddev.id);

  lcd_fill(20, 120, 70, 180, RED);
  lcd_fill(90, 120, 140, 180, GREEN);
  lcd_fill(160, 120, 210, 180, BLUE);

  if (camera_preview_ready != 0U)
  {
    if (DCMI_ConfigureLcdDirectDma() != HAL_OK)
    {
      printf("DCMI DMA Config Failed\r\n");
    }
    else if (DCMI_StartRgb565Snapshot() == HAL_OK)
    {
      printf("RGB565 Preview Start\r\n");
    }
    else
    {
      printf("DCMI Start Failed\r\n");
    }
  }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    if (dcmi_frame_ready != 0U)
    {
      dcmi_frame_ready = 0U;
      printf("DCMI Frame OK\r\n");
      printf("LCD Frame Drawn\r\n");

      if (DCMI_StartRgb565Snapshot() != HAL_OK)
      {
        printf("DCMI Restart Failed\r\n");
      }
    }

    if (dcmi_error_pending != 0U)
    {
      dcmi_error_pending = 0U;
      printf("DCMI ERROR: 0x%08lX\r\n", (unsigned long)dcmi_error_code);
    }
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void HAL_DCMI_FrameEventCallback(DCMI_HandleTypeDef *dcmi_handle)
{
  if (dcmi_handle->Instance == DCMI)
  {
    dcmi_frame_ready = 1U;
  }
}

void HAL_DCMI_ErrorCallback(DCMI_HandleTypeDef *dcmi_handle)
{
  if (dcmi_handle->Instance == DCMI)
  {
    dcmi_error_code = HAL_DCMI_GetError(dcmi_handle);
    dcmi_error_pending = 1U;
  }
}

#if defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050)
FILE __stdout;

int _ttywrch(int ch)
{
  return ch;
}

void _sys_exit(int status)
{
  (void)status;
}

char *_sys_command_string(char *command, int length)
{
  (void)command;
  (void)length;
  return NULL;
}
#endif

/**
  * @brief  将 printf 输出重定向到 USART1
  * @param  ch: 待发送字符
  * @param  stream: 标准输出流
  * @retval 成功时返回字符，失败时返回 EOF
  */
int fputc(int ch, FILE *stream)
{
  uint8_t data = (uint8_t)ch;

  (void)stream;
  if (HAL_UART_Transmit(&huart1, &data, 1U, 100U) != HAL_OK)
  {
    return EOF;
  }

  return ch;
}

/**
  * @brief  以十六进制显示LCD控制器ID
  * @param  id: LCD控制器ID
  * @retval None
  */
static void LCD_Test_ShowId(uint16_t id)
{
  static const char hex_digits[] = "0123456789ABCDEF";
  char id_text[] = "LCD ID: 0x0000";

  id_text[10] = hex_digits[(id >> 12) & 0x0FU];
  id_text[11] = hex_digits[(id >> 8) & 0x0FU];
  id_text[12] = hex_digits[(id >> 4) & 0x0FU];
  id_text[13] = hex_digits[id & 0x0FU];

  lcd_show_string(20, 80, 200, 24, 24, id_text, BLACK);
}

/**
 * @brief  将DCMI DMA配置为直接向LCD GRAM写入RGB565像素
 * @retval HAL状态
 */
static HAL_StatusTypeDef DCMI_ConfigureLcdDirectDma(void)
{
  if ((hdcmi.State == HAL_DCMI_STATE_BUSY) ||
      (hdma_dcmi.State == HAL_DMA_STATE_BUSY))
  {
    return HAL_BUSY;
  }

  hdma_dcmi.Init.MemInc = DMA_MINC_DISABLE;
  hdma_dcmi.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
  hdma_dcmi.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
  hdma_dcmi.Init.Mode = DMA_NORMAL;

  return HAL_DMA_Init(&hdma_dcmi);
}

/**
  * @brief  启动一次QVGA RGB565 Snapshot并由DMA直接写入LCD GRAM
  * @retval HAL状态
  */
static HAL_StatusTypeDef DCMI_StartRgb565Snapshot(void)
{
  /* 每帧重新设置窗口和GRAM起点，避免LCD内部写指针跨帧漂移。 */
  lcd_set_window(0U, 0U, DCMI_FRAME_WIDTH, DCMI_FRAME_HEIGHT);
  lcd_write_ram_prepare();

  __HAL_DCMI_CLEAR_FLAG(&hdcmi,
                        DCMI_FLAG_FRAMERI | DCMI_FLAG_OVRRI |
                        DCMI_FLAG_ERRRI);

  return HAL_DCMI_Start_DMA(&hdcmi,
                            DCMI_MODE_SNAPSHOT,
                            (uint32_t)&LCD->LCD_RAM,
                            DCMI_FRAME_WORD_COUNT);
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
