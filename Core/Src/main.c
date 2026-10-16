/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "bsp_dwt.h"
#include "bsp_i2c.h"
#include "bsp_oled.h"
#include "bsp_sht30.h"
#include "app_frame.h"
#include "bsp_esp_uart.h"
#include "bsp_esp.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
i2c_soft_t bus_oled = {GPIOB, GPIO_PIN_8, GPIOB, GPIO_PIN_9};
i2c_soft_t bus_sht30 = {GPIOB, GPIO_PIN_6, GPIOB, GPIO_PIN_7};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  dwt_init();
  i2c_soft_init(&bus_oled);
  i2c_soft_init(&bus_sht30);

  oled_init(&bus_oled);
  sht30_init(&bus_sht30);
  esp_uart_init();
  esp_init();
  oled_clear();
  oled_show_string(0, 16, "T x10:", 16);
  oled_show_string(0, 32, "H x10:", 16);
  oled_refresh();

  uint32_t t_read = 0;          // 上次读取传感器的时间
  uint32_t t_led = 0;           // 上次翻转 LED 的时间
  uint32_t disp_t = 0xFFFFFFFF; // 上次显示的温度
  uint32_t disp_h = 0xFFFFFFFF; // 上次显示的湿度

  uint32_t t_report = 0;   // 上次上报的时刻
  uint32_t seq      = 0;   // 上报包号
  int32_t  t_x100   = 0;   // 温度 ×100（定点）
  int32_t  h_x100   = 0;   // 湿度 ×100（定点）
  uint8_t  s_valid  = 0;   // 是否已有有效数据
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    uint32_t now = HAL_GetTick();

    /* ---- 1. 传感器：1 秒触发一次采集，状态机每轮推进 ---- */
    if (now - t_read >= 1000)
    {
      t_read = now;
      sht30_start();
    }
    sht30_task();

    /* ---- 2. 温湿度显示：只在数值变化时重画 ---- */
    float t, h;
    if (sht30_get(&t, &h) == 0)
    {
      t_x100=(int32_t)(t*100.0f+(t>=0?0.5f:-0.5f));
      h_x100=(int32_t)(h*100.0f+(h>=0?0.5f:-0.5f));
      s_valid=1;

      uint32_t t10 = (uint32_t)(t * 10);
      uint32_t h10 = (uint32_t)(h * 10);
      if (t10 != disp_t || h10 != disp_h)
      {
        disp_t = t10;
        disp_h = h10;
        oled_show_num(48, 16, t10, 3, 16);
        oled_show_num(48, 32, h10, 3, 16);
        oled_refresh();
      }
    }

    /* ---- 3. ESP 状态机：必须每轮调用 ---- */
    esp_task();
    /*上报*/
    if(now-t_report>=1000){
      t_report=now;
      if(s_valid){
        char buf[96];   /* 必须 >= 48：app_frame_json 在 out_sz<48 时直接返回 -1 */
        seq++;
        int n=app_frame_json(buf,sizeof(buf),seq,t_x100,h_x100);
        if(n>0){
          esp_send((uint8_t*)buf,(uint16_t)n);
        }
      }
    }
    
    /* ---- 5. 已发包数（只在变化时重画） ---- */
    static uint32_t last_tx = 0xFFFFFFFF;
    uint32_t tx = esp_get_ok_count();
    if (tx != last_tx)
    {
      last_tx = tx;
      oled_show_num(24, 0, tx, 5, 16);
      oled_refresh();
    }

    /* ---- 6. WiFi 状态行：只在状态变化时重画 ---- */
    static esp_state_t last_esp = (esp_state_t)0xFF;
    if (esp_get_state() != last_esp)
    {
      last_esp = esp_get_state();
      char sst[16];
      snprintf(sst, sizeof sst, "WiFi:%-5s", esp_state_str());
      oled_show_string(0, 48, sst, 12);
      oled_refresh();
    }

    /* ---- 7. LED 心跳：500ms 翻转 ---- */
    if (now - t_led >= 500)
    {
      t_led = now;
      HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
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
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
/**重定向 printf 到 USART1
 *已勾 MicroLIB 无需处理半主机模式
 */
int fputc(int ch, FILE *f)
{
  uint8_t byte = (uint8_t)ch;
  HAL_UART_Transmit(&huart1, &byte, 1, 100);
  return ch;
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

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
