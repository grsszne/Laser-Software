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
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

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

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
static void MX_GPIO_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

volatile uint8_t arm_dryrun_request = 0U; /* 1 = pulse, 2 = force off */
volatile uint8_t optical_tx_active = 0U;
static uint8_t arm_dryrun_active = 0U;
static uint32_t arm_dryrun_started_ms = 0U;
#define ARM_DRYRUN_PULSE_MS 500U

static void ArmDryRun_Off(void)
{
  HAL_GPIO_WritePin(LASER_EN_GPIO_Port, LASER_EN_Pin, GPIO_PIN_RESET);
  arm_dryrun_active = 0U;
}

static uint8_t power_dryrun_active = 0U;
static uint32_t power_dryrun_started_ms = 0U;
#define POWER_DRYRUN_PULSE_MS 5000U


volatile uint8_t dummy_test_active = 0U;
static uint8_t dummy_phase = 0U;
static uint32_t dummy_phase_ms = 0U;
static uint32_t dummy_start_ms = 0U;

static void DummyTest_Off(void)
{
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LASER_EN_GPIO_Port, LASER_EN_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(TX_PWR_EN_GPIO_Port, TX_PWR_EN_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(TX_PWR_LED_GPIO_Port, TX_PWR_LED_Pin, GPIO_PIN_RESET);

  arm_dryrun_active = 0U;
  power_dryrun_active = 0U;
  dummy_phase = 0U;
  dummy_test_active = 0U;
}
static void ArmDryRun_Task(void)
{
  if (optical_tx_active) {
    arm_dryrun_request = 0U;
    return;
}
  uint8_t request;

  __disable_irq();
  request = arm_dryrun_request;
  arm_dryrun_request = 0U;
  __enable_irq();

  uint32_t now = HAL_GetTick();

  
  /* B6 / B8: software shutdown */
  if (request == 2U || request == 4U) {
    DummyTest_Off();
    return;
  }

  /* Execute the timed dummy-load test */
  if (dummy_test_active) {
    /* Overall timeout */
    if ((uint32_t)(now - dummy_start_ms) >= 1800U) {
      DummyTest_Off();
      return;
    }

    if (dummy_phase == 1U &&
        (uint32_t)(now - dummy_phase_ms) >= 250U) {
      /* Power settled: arm TX-A */
      HAL_GPIO_WritePin(
          LASER_EN_GPIO_Port, LASER_EN_Pin, GPIO_PIN_SET);
      dummy_phase = 2U;
      dummy_phase_ms = now;
    }
    else if (dummy_phase == 2U &&
             (uint32_t)(now - dummy_phase_ms) >= 100U) {
      /* Assert modulation for one second */
      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_SET);
      HAL_GPIO_WritePin(
          LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET);
      dummy_phase = 3U;
      dummy_phase_ms = now;
    }
    else if (dummy_phase == 3U &&
             (uint32_t)(now - dummy_phase_ms) >= 1000U) {
      DummyTest_Off();
    }

    return;
  }

  /* B9: begin single-shot dummy-load test */
  if (request == 5U) {
    arm_dryrun_active = 0U;
    power_dryrun_active = 0U;

    HAL_GPIO_WritePin(
        LASER_EN_GPIO_Port, LASER_EN_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(
        LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET);

    dummy_test_active = 1U;
    dummy_phase = 1U;
    dummy_phase_ms = now;
    dummy_start_ms = now;

    HAL_GPIO_WritePin(
        TX_PWR_EN_GPIO_Port, TX_PWR_EN_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(
        TX_PWR_LED_GPIO_Port, TX_PWR_LED_Pin, GPIO_PIN_SET);
    return;
  }


  /* B6 or B8: turn both controls off */
  if (request == 2U || request == 4U) {
    power_dryrun_active = 0U;
    ArmDryRun_Off();
  }

  /* B7: start the switched-power test */
  if (request == 3U) {
    ArmDryRun_Off();  // PC5 LOW
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);

    power_dryrun_started_ms = now;
    power_dryrun_active = 1U;
  }

  /* Automatically end the power pulse */
  if (power_dryrun_active &&
      (uint32_t)(now - power_dryrun_started_ms)
          >= POWER_DRYRUN_PULSE_MS) {
    power_dryrun_active = 0U;
  }

  if (power_dryrun_active) {
    ArmDryRun_Off();
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);

    HAL_GPIO_WritePin(
        TX_PWR_EN_GPIO_Port, TX_PWR_EN_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(
        TX_PWR_LED_GPIO_Port, TX_PWR_LED_Pin, GPIO_PIN_SET);
    return;
  }

  /* Default: TX power OFF */
  HAL_GPIO_WritePin(
      TX_PWR_EN_GPIO_Port, TX_PWR_EN_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(
      TX_PWR_LED_GPIO_Port, TX_PWR_LED_Pin, GPIO_PIN_RESET);

  /* Preserve the existing PC5 dry-run command */
  if (request == 1U) {
    arm_dryrun_started_ms = now;
    arm_dryrun_active = 1U;
    HAL_GPIO_WritePin(
        LASER_EN_GPIO_Port, LASER_EN_Pin, GPIO_PIN_SET);
  }

  if (arm_dryrun_active &&
      (uint32_t)(now - arm_dryrun_started_ms)
          >= ARM_DRYRUN_PULSE_MS) {
    ArmDryRun_Off();
  }
}

#define PATTERN_DEMO_BIT_MS 200U
#define PATTERN_DEMO_MAX_BYTES 24U
#define PATTERN_DEMO_MAX_MS (PATTERN_DEMO_MAX_BYTES * 8U * PATTERN_DEMO_BIT_MS + 500U)

extern volatile uint8_t optical_tx_active;
volatile uint8_t pattern_demo_request = 0U; /* 1=HELLO, 2=stop, 3=payload */
volatile uint8_t pattern_demo_pending_len = 0U;
uint8_t pattern_demo_pending[PATTERN_DEMO_MAX_BYTES];

static uint8_t pattern_demo_text[PATTERN_DEMO_MAX_BYTES];
static uint8_t pattern_demo_text_len = 0U;
static uint8_t pattern_demo_active = 0U;
static uint32_t pattern_demo_bit = 0U;
static uint32_t pattern_demo_start_ms = 0U;
static uint32_t pattern_demo_next_ms = 0U;


#define OPTICAL_TX_MAX_BYTES 16U
#define OPTICAL_POWER_SETTLE_MS 250U
#define OPTICAL_ARM_SETTLE_MS 100U
#define OPTICAL_MAX_MS (OPTICAL_POWER_SETTLE_MS + OPTICAL_ARM_SETTLE_MS + \
                        OPTICAL_TX_MAX_BYTES * 8U * PATTERN_DEMO_BIT_MS + 750U)

static uint8_t optical_phase = 0U;
static uint32_t optical_phase_started_ms = 0U;

static void PatternDemo_Stop(void)
{
    /* Data off, then arm off, then power off. */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LASER_EN_GPIO_Port, LASER_EN_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(TX_PWR_EN_GPIO_Port, TX_PWR_EN_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(TX_PWR_LED_GPIO_Port, TX_PWR_LED_Pin, GPIO_PIN_RESET);

    pattern_demo_active = 0U;
    optical_phase = 0U;
    optical_tx_active = 0U;
}

static void PatternDemo_Task(void)
{
    uint8_t request;
    uint8_t payload_len = 0U;
    uint8_t payload[PATTERN_DEMO_MAX_BYTES];

    __disable_irq();
    request = pattern_demo_request;

    if (request == 3U || request == 4U) {
        payload_len = pattern_demo_pending_len;
        if (payload_len > PATTERN_DEMO_MAX_BYTES)
            payload_len = 0U;

        for (uint8_t i = 0; i < payload_len; ++i)
            payload[i] = pattern_demo_pending[i];
    }

    pattern_demo_request = 0U;
    __enable_irq();

    if (request == 2U) {
        PatternDemo_Stop();
        return;
    }

    /* Never overlap with the older dummy-load test. */
    if (dummy_test_active) {
        if (pattern_demo_active) PatternDemo_Stop();
        return;
    }

    if (request == 1U || request == 3U || request == 4U) {
        if (optical_tx_active) return;

        if (request == 4U &&
            (arm_dryrun_active || power_dryrun_active ||
             payload_len > OPTICAL_TX_MAX_BYTES))
            return;

        PatternDemo_Stop();

        if (request == 1U) {
            const uint8_t hello[] = "HELLO";
            payload_len = sizeof(hello) - 1U;
            for (uint8_t i = 0; i < payload_len; ++i)
                payload[i] = hello[i];
        }

        if (payload_len == 0U) return;

        for (uint8_t i = 0; i < payload_len; ++i)
            pattern_demo_text[i] = payload[i];

        pattern_demo_text_len = payload_len;
        pattern_demo_bit = 0U;
        pattern_demo_start_ms = HAL_GetTick();
        pattern_demo_next_ms = pattern_demo_start_ms;
        pattern_demo_active = 1U;

        if (request == 4U) {
            /* Optical mode: power first, with arm and data LOW. */
            optical_tx_active = 1U;
            optical_phase = 1U;
            optical_phase_started_ms = pattern_demo_start_ms;
            arm_dryrun_request = 0U;

            HAL_GPIO_WritePin(
                TX_PWR_EN_GPIO_Port, TX_PWR_EN_Pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(
                TX_PWR_LED_GPIO_Port, TX_PWR_LED_Pin, GPIO_PIN_SET);
            return;
        }
    }

    if (!pattern_demo_active) return;

    uint32_t now = HAL_GetTick();

    if (optical_tx_active) {
        if ((uint32_t)(now - pattern_demo_start_ms)
                >= OPTICAL_MAX_MS) {
            PatternDemo_Stop();
            return;
        }

        if (optical_phase == 1U) {
            if ((uint32_t)(now - optical_phase_started_ms)
                    >= OPTICAL_POWER_SETTLE_MS) {
                HAL_GPIO_WritePin(
                    LASER_EN_GPIO_Port, LASER_EN_Pin, GPIO_PIN_SET);
                optical_phase = 2U;
                optical_phase_started_ms = now;
            }
            return;
        }

        if (optical_phase == 2U) {
            if ((uint32_t)(now - optical_phase_started_ms)
                    >= OPTICAL_ARM_SETTLE_MS) {
                optical_phase = 3U;
                pattern_demo_next_ms = now;
            }
            return;
        }

        if (optical_phase != 3U) {
            PatternDemo_Stop();
            return;
        }
    }
    else if ((uint32_t)(now - pattern_demo_start_ms)
                 >= PATTERN_DEMO_MAX_MS) {
        PatternDemo_Stop();
        return;
    }

    if ((int32_t)(now - pattern_demo_next_ms) < 0)
        return;

    if (pattern_demo_bit >=
        (uint32_t)pattern_demo_text_len * 8U) {
        PatternDemo_Stop();
        return;
    }

    uint32_t byte_idx = pattern_demo_bit / 8U;
    uint32_t shift = 7U - (pattern_demo_bit % 8U);

    uint8_t bit = (pattern_demo_text[byte_idx] >> shift) & 1U;
    GPIO_PinState level =
        bit ? GPIO_PIN_SET : GPIO_PIN_RESET;

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, level);
    HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, level);

    ++pattern_demo_bit;
    pattern_demo_next_ms = now + PATTERN_DEMO_BIT_MS;
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

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  SCB_EnableICache();
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USB_DEVICE_Init();
  /* USER CODE BEGIN 2 */

  /* Keep the transmitter disabled until explicitly commanded. */
  HAL_GPIO_WritePin(TX_PWR_EN_GPIO_Port, TX_PWR_EN_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(TX_PWR_LED_GPIO_Port, TX_PWR_LED_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LASER_EN_GPIO_Port, LASER_EN_Pin, GPIO_PIN_RESET);

  /* Firmware-running indicator. */
  HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET);

  /* Ensure demo GPIO is LOW after startup. */
  PatternDemo_Stop();
  ArmDryRun_Off();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    PatternDemo_Task();
    ArmDryRun_Task();
    /*
       HAL_GPIO_WritePin(TX_PWR_EN_GPIO_Port, TX_PWR_EN_Pin, GPIO_PIN_SET);
       HAL_GPIO_WritePin(TX_PWR_LED_GPIO_Port, TX_PWR_LED_Pin, GPIO_PIN_SET);
       HAL_Delay(2500);

       HAL_GPIO_WritePin(TX_PWR_EN_GPIO_Port, TX_PWR_EN_Pin, GPIO_PIN_RESET);
       HAL_GPIO_WritePin(TX_PWR_LED_GPIO_Port, TX_PWR_LED_Pin, GPIO_PIN_RESET);
       HAL_Delay(2500);
       */
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
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 432;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Activate the overdrive mode mode (216 MHz)
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
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

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_7) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, TX_PWR_EN_Pin|LED1_Pin|LED2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, TX_PWR_LED_Pin|LASER_EN_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : TX_PWR_EN_Pin LED1_Pin LED2_Pin */
  GPIO_InitStruct.Pin = TX_PWR_EN_Pin|LED1_Pin|LED2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pins : TX_PWR_LED_Pin LASER_EN_Pin */
  GPIO_InitStruct.Pin = TX_PWR_LED_Pin|LASER_EN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin = GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

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
