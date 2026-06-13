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
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "dht11.h"
#include "hcsr04.h"
#include "com_debug.h"

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
extern volatile HCSR04_InfoTypeDef hcsr04;
DHT11_Data_TypeDef env_data;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// 绮楃暐鐨勫井绉掔骇寤舵椂 (72MHz 涓婚涓?)
void Delay_us_Rough(uint32_t us) {
    uint32_t count = us * (72 / 4); // 杩戜技璁＄畻锛岀敤浜庝骇鐢? Trig 鑴夊啿瓒冲浜?
    while(count--) {
        __NOP();
    }
}

// 鍙戦�佽Е鍙戣剦鍐?
void HCSR04_Trigger(void) {
    // 鍋囪浣犲湪 CubeMX 閲屾妸 Trig 寮曡剼鍛藉悕涓轰簡 HCSR04_TRIG
    HAL_GPIO_WritePin(HCSR04_TRIG_GPIO_Port, HCSR04_TRIG_Pin, GPIO_PIN_SET);
    Delay_us_Rough(15); // 缁? 15us 楂樼數骞筹紝纭繚瑙﹀彂
    HAL_GPIO_WritePin(HCSR04_TRIG_GPIO_Port, HCSR04_TRIG_Pin, GPIO_PIN_RESET);
}
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
  MX_TIM2_Init();
  MX_USART2_UART_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */
  HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_1);
	
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  // 在 while 外面定义一个静态变量，记录上次读取的时间
  static uint32_t last_dht11_time = 0;
 while (1) 
 {        
        /* USER CODE BEGIN 3 */
        
        // 1. 发起测距
        if (hcsr04.state == HCSR04_IDLE) {
            hcsr04.state = HCSR04_WAIT_RISING; // 设置状态机为等待上升沿
            HCSR04_Trigger();                  // 发射超声波
        }
        
        // 2. 检查测距是否完成
        if (hcsr04.state == HCSR04_FINISH) {
            // 声速 340m/s，转换为 0.017 cm/us 
            // 如果高电平时间超过约 25000 us (即超过 4 米)，视为无效数据或超时
            if(hcsr04.high_level_time_us < 25000) {
                hcsr04.distance_cm = hcsr04.high_level_time_us * 0.017f;
            } else {
                hcsr04.distance_cm = -1.0f; // 错误值标记
            }
            
            // 这里可以暂时代替 OLED，先用 Debug 调试模式看变量 hcsr04.distance_cm 的值
            // 或者通过 printf 发送到串口打印
            
            // 延时一段时间再进行下一次测距 (避免超声波余波干扰)
            HAL_Delay(100); 
            hcsr04.state = HCSR04_IDLE;
            debug_printf("Distance: %.2f cm\r\n", hcsr04.distance_cm);
        }      
        // --- 3. 读取温湿度 ---
        // 每隔 2000 毫秒 (2秒) 才去唤醒一次 DHT11
        if (HAL_GetTick() - last_dht11_time >= 2000) {
            last_dht11_time = HAL_GetTick(); // 更新时间戳
            
            if (DHT11_Read_Data(&env_data) == 0) {
                printf("[main.c:xxx] Temp: %d.%d C, Hum: %d.%d %%\r\n", 
                        env_data.temp_int, env_data.temp_dec, 
                        env_data.hum_int, env_data.hum_dec);
            } else {
                printf("[main.c:xxx] DHT11 Error!\r\n");
            }
        }
  /* USER CODE END WHILE */
  
  /* USER CODE BEGIN 3 */
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
}

/* USER CODE BEGIN 4 */

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
