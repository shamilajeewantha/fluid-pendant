/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "flip_sim_c/scene.h"
#include "flip_sim_c/flip_utils.h"
#include "flip_sim_c/flip_fluid.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

// ADXL SPI Commands
#define ADXL_READ_CMD 0x0B
#define ADXL_WRITE_CMD 0x0A

// ADXL SPI registers
#define ADXL_DEVID_AD_REG 0x00
#define ADXL_STATUS_REG 0x0B
#define ADXL362_POWER_CTL_REG 0x2D
// ADXL362 Data registers
#define ADXL_XDATA_REG 0x08
#define ADXL_YDATA_REG 0x09
#define ADXL_ZDATA_REG 0x0A

// ADXL SPI register options
#define ADXL362_MEASURE 0x02

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;
SPI_HandleTypeDef hspi2;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
uint32_t counter = 0;  // global counter
static Scene g_scene;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_SPI2_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */

int _write(int file, char *ptr, int len)
{
    HAL_UART_Transmit(&huart1, (uint8_t*)ptr, len, HAL_MAX_DELAY);
    return len;
}

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

uint8_t ADXL362_ReadRegister(uint8_t reg_addr)
{
    uint8_t tx[3], rx[3];
    uint8_t value;

    // CS LOW
    HAL_GPIO_WritePin(CS2_GPIO_Port, CS2_Pin, GPIO_PIN_RESET);

    // Prepare SPI command
    tx[0] = ADXL_READ_CMD;      // Read command
    tx[1] = reg_addr;  // Register address
    tx[2] = 0x00;      // Dummy byte to receive data

    HAL_SPI_TransmitReceive(&hspi2, tx, rx, 3, HAL_MAX_DELAY);

    // CS HIGH
    HAL_GPIO_WritePin(CS2_GPIO_Port, CS2_Pin, GPIO_PIN_SET);

    value = rx[2];  // The third byte contains the register value

    // Print TX and RX for debugging
//    printf("TX Data: 0x%02X 0x%02X 0x%02X\r\n", tx[0], tx[1], tx[2]);
//    printf("RX Data[0]: 0x%02X\r\nRX Data[1]: 0x%02X\r\nRX Data[2]: 0x%02X\r\n", rx[0], rx[1], rx[2]);

    return value;
}
void ADXL362_WriteRegister(uint8_t reg_addr, uint8_t value)
{
    uint8_t tx[3];

    // CS LOW
    HAL_GPIO_WritePin(CS2_GPIO_Port, CS2_Pin, GPIO_PIN_RESET);

    // Prepare SPI write command
    tx[0] = ADXL_WRITE_CMD;  // 0x0A
    tx[1] = reg_addr;        // register address
    tx[2] = value;           // value to write

    HAL_SPI_Transmit(&hspi2, tx, 3, HAL_MAX_DELAY);

    // CS HIGH
    HAL_GPIO_WritePin(CS2_GPIO_Port, CS2_Pin, GPIO_PIN_SET);

    // Debug print
    printf("Write Reg 0x%02X = 0x%02X\r\n", reg_addr, value);
    printf("TX Data: 0x%02X 0x%02X 0x%02X\r\n", tx[0], tx[1], tx[2]);
}


/**
 * @brief  Read Device ID
 */
void ADXL362_ReadID(void)
{
    uint8_t devid = ADXL362_ReadRegister(ADXL_DEVID_AD_REG);
    printf("ADXL362 Device ID: 0x%02X\r\n", devid);
}

/**
 * @brief  Read STATUS register and print human-readable bits
 */
void ADXL362_ReadStatus(void)
{
    uint8_t status = ADXL362_ReadRegister(ADXL_STATUS_REG);
    printf("ADXL362 STATUS: 0x%02X\r\n", status);
    printf("  ERROR        : %s\r\n", (status & 0x80) ? "1" : "0");
    printf("  READY        : %s\r\n", (status & 0x40) ? "1" : "0");
    printf("  ACTIVITY     : %s\r\n", (status & 0x20) ? "1" : "0");
    printf("  INACTIVITY   : %s\r\n", (status & 0x10) ? "1" : "0");
    printf("  FIFO_OVERRUN : %s\r\n", (status & 0x08) ? "1" : "0");
    printf("  FIFO_WATERMARK: %s\r\n", (status & 0x04) ? "1" : "0");
    printf("  FIFO_READY   : %s\r\n", (status & 0x02) ? "1" : "0");
    printf("  RESERVED     : %s\r\n", (status & 0x01) ? "1" : "0");
}

void ADXL362_ReadXYZ(void)
{
    uint8_t x = ADXL362_ReadRegister(ADXL_XDATA_REG);
    uint8_t y = ADXL362_ReadRegister(ADXL_YDATA_REG);
    uint8_t z = ADXL362_ReadRegister(ADXL_ZDATA_REG);

    printf("X: %d\tY: %d\tZ: %d\r\n", x, y, z);
}









const uint8_t charData[][8] = {
		{0x00, 0x1c, 0x22, 0x22, 0x22, 0x22, 0x1c, 0x00},  // 0
		{0x08, 0x0c, 0x08, 0x08, 0x08, 0x08, 0x3e, 0x00},  // 1
		{0x1c, 0x22, 0x20, 0x18, 0x04, 0x02, 0x3e, 0x00},  // 2
		{0x1c, 0x22, 0x20, 0x1c, 0x20, 0x22, 0x1c, 0x00},  // 3
		{0x20, 0x30, 0x28, 0x24, 0x7c, 0x20, 0x20, 0x00},  // 4
		{0x3e, 0x02, 0x1e, 0x20, 0x20, 0x20, 0x1e, 0x00},  // 5
		{0x18, 0x04, 0x02, 0x1e, 0x22, 0x22, 0x1c, 0x00},  // 6
		{0x3e, 0x20, 0x10, 0x08, 0x04, 0x04, 0x04, 0x00},  // 7
		{0x1c, 0x22, 0x22, 0x1c, 0x22, 0x22, 0x1c, 0x00},  // 8
		{0x1c, 0x22, 0x22, 0x3c, 0x20, 0x10, 0x0c, 0x00},  // 9
};


void max7219_write (uint8_t Addr, uint8_t data)
{
	uint16_t writeData = (Addr<<8)|data;
	HAL_GPIO_WritePin(CS1_GPIO_Port, CS1_Pin, 0);  // enable slave
	HAL_SPI_Transmit(&hspi1, (uint8_t *)&writeData, 1, 100);
	HAL_GPIO_WritePin(CS1_GPIO_Port, CS1_Pin, 1);  // disable slave
}

void matrixInit (void)
{
	max7219_write(0x09, 0);  // no decoding
	max7219_write(0x0a, 0x01);  // 3/32 intensity
	max7219_write(0x0B, 0x07);  // scan all 7 columns
	max7219_write(0x0C, 0x01);  // normal operation
	max7219_write(0x0F, 0);     // No display test
}

static void matrixShowRows(uint8_t rows[8])
{
	for (int i = 1; i <= 8; i++)
	{
		max7219_write(i, rows[8 - i]);
	}
}

void matrixData(int num)
{
	for (int i=1; i<=8; i++)
	{
		max7219_write(i, charData[num][8-i]);
	}
}

static void matrixFromFluid(void)
{
	if (!g_scene.fluid || !g_scene.fluid->cellColor) return;

	uint8_t rows[8] = {0};
	for (int row = 1; row <= 8; row++)
	{
		uint8_t bits = 0;
		for (int col = 1; col <= 8; col++)
		{
			int idx = row * 10 + col;
			float v = g_scene.fluid->cellColor[idx];
			if (v > 0.01f)
			{
				bits |= (1u << (col - 1));
			}
		}
		rows[row - 1] = bits;
	}

	matrixShowRows(rows);
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
  MX_SPI1_Init();
  MX_SPI2_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */

  HAL_Delay(1000); // 1000 ms delay to let UART buffers settle
  printf("Reading device id :\r\n");
  ADXL362_ReadID();
  printf("Reading STATUS before measuring :\r\n");
  ADXL362_ReadStatus();

  HAL_Delay(1000); // 1000 ms delay to read the prev status
  /* After reading device ID and status */
  printf("Enabling measurement mode...\r\n");
  ADXL362_WriteRegister(ADXL362_POWER_CTL_REG, ADXL362_MEASURE);

  printf("Reading STATUS after measuring :\r\n");
  ADXL362_ReadStatus();



  matrixInit();



  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

		// Toggle LED on PC13
		HAL_GPIO_TogglePin(GPIOC, STATUS_LED_Pin);

		// Print counter
		printf("Counter: %lu, LED toggled!\r\n", counter);

		// Increment counter
		counter++;

	//	ADXL362_ReadStatus();
		// Read raw X/Y and map to gravity
		uint8_t x_raw = ADXL362_ReadRegister(ADXL_XDATA_REG);
		uint8_t y_raw = ADXL362_ReadRegister(ADXL_YDATA_REG);
		double gx = ((int)x_raw - 128) / 64.0 * 9.81;
		double gy = ((int)y_raw - 128) / 64.0 * 9.81;

		// Lazy init scene on first loop
		if (g_scene.fluid == NULL)
		{
			g_scene = scene_default();
			setupScene(&g_scene);
			g_scene.paused = false;
		}

		// Step fluid and update matrix
		simulateFlipFluid(
			g_scene.fluid,
			g_scene.dt,
			gx,
			gy,
			g_scene.flipRatio,
			g_scene.numPressureIters,
			g_scene.numParticleIters,
			g_scene.overRelaxation,
			g_scene.compensateDrift,
			g_scene.separateParticles
		);

		matrixFromFluid();

		HAL_Delay(50);

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

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_1LINE;
  hspi1.Init.DataSize = SPI_DATASIZE_16BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_HIGH;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

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
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(STATUS_LED_GPIO_Port, STATUS_LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, CS1_Pin|CS2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : STATUS_LED_Pin */
  GPIO_InitStruct.Pin = STATUS_LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(STATUS_LED_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : CS1_Pin CS2_Pin */
  GPIO_InitStruct.Pin = CS1_Pin|CS2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
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

#ifdef  USE_FULL_ASSERT
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
