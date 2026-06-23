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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
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
SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim1;
DMA_HandleTypeDef hdma_tim1_up;
DMA_HandleTypeDef hdma_tim1_ch1;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */

uint32_t blinkCounter = 0;
char msg[128];

/* Charlieplex hardware bring-up scan. This board has NO per-leg current-limiting
   resistors on PB0..PB15 (confirmed), so the only thing protecting the GPIO pins
   from a software bug is the software itself -- see ChargePlex_ValidateScanTable
   and ChargePlex_CheckAlive below. */
#define CP_PIN_COUNT  16
#define CP_SLOT_COUNT (CP_PIN_COUNT * (CP_PIN_COUNT - 1)) /* 240: every ordered (src,dst) pin pair */

/* This board's wiring (confirmed by user): each of the 16 PB pins takes a turn as one row's
   single source/driving pin, with the other 15 pins as that row's sink/column pins -- the
   standard charlieplex row/column convention. ChargePlex_BuildScanTable's enumeration order
   (outer loop over src, inner loop over dst) already groups slots into exactly CP_ROWS
   contiguous blocks of CP_COLS slots each as a direct result -- row r occupies slots
   [r*CP_COLS, r*CP_COLS + CP_COLS - 1]. */
#define CP_ROWS CP_PIN_COUNT       /* 16 */
#define CP_COLS (CP_PIN_COUNT - 1) /* 15 */

static uint32_t bsrr_buf[CP_SLOT_COUNT];
static uint32_t moder_buf[CP_SLOT_COUNT];
static volatile uint8_t chargePlexFaulted = 0;
static uint8_t chargePlexFaultReported = 0;

/* Set true only after HAL_DMA_Start has actually run on both channels in main(), below.
   SysTick (and therefore ChargePlex_CheckAlive, which it calls every 1ms) starts running at
   HAL_Init(), several peripheral inits before MX_TIM1_Init() gives hdma_tim1_up/ch1 a real
   .Instance (set inside HAL_TIM_Base_MspInit, stm32l4xx_hal_msp.c). Without this guard,
   ChargePlex_CheckAlive reads __HAL_DMA_GET_COUNTER on a NULL .Instance during that gap --
   garbage memory, not a real transfer counter -- and a false "stalled" reading there
   permanently latches chargePlexFaulted before the real scan table even exists, silently
   disabling this watchdog for the rest of the session. */
static volatile uint8_t chargePlexScanStarted = 0;

/* Immutable per-slot "this is what moder_buf[slot] would be if this slot is on" reference,
   filled once by ChargePlex_BuildScanTable and never modified afterward -- moder_buf itself
   gets zeroed for "off" slots every frame, so this is what lets a slot be safely turned back
   on later without losing its source/sink pin-pair identity. */
static uint32_t moder_template[CP_SLOT_COUNT];

/* Firmware-local stand-in for DisplayFrame/cellColor until the physics core is ported
   onto this board (research.md Decision 17, data-model.md AxelorFrameBuffer). Shaped as an
   actual [row][col] matrix (not a flat 240 array) to match the eventual real DisplayFrame/
   cellColor shape directly -- no flattening will be needed when that swap happens. Filled by
   a placeholder pattern generator for now; NOT read by DMA -- only ChargePlex_ApplyFrameBuffer()
   translates it into moder_buf (via the row/col -> slot formula above), and only after
   re-validating. The DMA scanner, bsrr_buf, moder_template, and the validator are untouched by
   this -- this buffer is purely upstream of ChargePlex_ApplyFrameBuffer's existing gate. */
static uint8_t cpFrameBuf[CP_ROWS][CP_COLS];
static uint32_t cpCandidateModer[CP_SLOT_COUNT];

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_TIM1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
uint8_t mpuTxData[2];
uint8_t mpuRxData[2];

void MPU6500_ReadWhoAmI(void)
{
    uint8_t whoami;
    HAL_StatusTypeDef status;

    sprintf(msg, "[MPU] Starting WHO_AM_I read...\r\n");
    HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);

    sprintf(msg, "[MPU] CS (PA5) -> LOW\r\n");
    HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET); // CS low

    mpuTxData[0] = 0x75 | 0x80; // WHO_AM_I register (0x75), read bit set
    mpuTxData[1] = 0x00;        // dummy byte to receive data

    status = HAL_SPI_TransmitReceive(&hspi1, mpuTxData, mpuRxData, 2, HAL_MAX_DELAY);

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET); // CS high

    /* Snapshot results immediately, before any sprintf can touch these buffers */
    uint8_t tx0 = mpuTxData[0], tx1 = mpuTxData[1];
    uint8_t rx0 = mpuRxData[0], rx1 = mpuRxData[1];
    whoami = rx1;

    sprintf(msg, "[MPU] CS (PA5) -> HIGH\r\n");
    HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);

    sprintf(msg, "[MPU] HAL_SPI_TransmitReceive status: %d (0=OK,1=ERROR,2=BUSY,3=TIMEOUT)\r\n", status);
    HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);

    sprintf(msg, "[MPU] tx=[0x%02X 0x%02X] rx=[0x%02X 0x%02X]\r\n", tx0, tx1, rx0, rx1);
    HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);

    sprintf(msg, "[MPU] WHO_AM_I: 0x%02X (expect 0x70) -> %s\r\n",
            whoami, (whoami == 0x70) ? "PASS" : "FAIL");
    HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
}

void MPU6500_WriteReg(uint8_t reg, uint8_t value)
{
    uint8_t tx[2] = { (uint8_t)(reg & 0x7F), value }; // write: MSB of addr = 0
    uint8_t rx[2];

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
    HAL_SPI_TransmitReceive(&hspi1, tx, rx, 2, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
}

void MPU6500_ReadRegs(uint8_t reg, uint8_t *buf, uint8_t len)
{
    uint8_t tx[8] = {0};
    uint8_t rx[8] = {0};

    tx[0] = reg | 0x80; // read: MSB of addr = 1

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
    HAL_SPI_TransmitReceive(&hspi1, tx, rx, len + 1, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);

    memcpy(buf, &rx[1], len);
}

void MPU6500_Init(void)
{
    MPU6500_WriteReg(0x6A, 0x10); // USER_CTRL: I2C_IF_DIS = 1, lock to SPI-only mode
    HAL_Delay(1);
    MPU6500_WriteReg(0x6B, 0x01); // PWR_MGMT_1: clear SLEEP, select PLL clock source
    HAL_Delay(10);
    MPU6500_WriteReg(0x1C, 0x00); // ACCEL_CONFIG: AFS_SEL = 0 -> +/-2g, 16384 LSB/g

    sprintf(msg, "[MPU] Init done: SPI-only, awake, accel range +/-2g\r\n");
    HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
}

uint8_t accelRaw[6];
int16_t accelX_raw, accelY_raw, accelZ_raw;

void MPU6500_ReadAccel(void)
{
    MPU6500_ReadRegs(0x3B, accelRaw, 6); // ACCEL_XOUT_H..ACCEL_ZOUT_L

    accelX_raw = (int16_t)((accelRaw[0] << 8) | accelRaw[1]);
    accelY_raw = (int16_t)((accelRaw[2] << 8) | accelRaw[3]);
    accelZ_raw = (int16_t)((accelRaw[4] << 8) | accelRaw[5]);

    // milli-g via integer math (avoids needing float-in-printf support)
    long accelX_mg = ((long)accelX_raw * 1000) / 16384;
    long accelY_mg = ((long)accelY_raw * 1000) / 16384;
    long accelZ_mg = ((long)accelZ_raw * 1000) / 16384;

    sprintf(msg, "[ACCEL] raw=[X=%d Y=%d Z=%d] mg=[X=%ld Y=%ld Z=%ld] (X,Y used by sim; Z ignored)\r\n",
            accelX_raw, accelY_raw, accelZ_raw, accelX_mg, accelY_mg, accelZ_mg);
    HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);

    // m/s^2 -- the exact unit/scale MotionInput.x/y and Scene.gravity_x/y will need once a
    // simulation exists on this board (research.md Decision 16). This is NOT read by any
    // simulation code yet -- printed only so the conversion and this board's MPU-6500 axis/sign
    // orientation can be verified against real tilts before anything consumes it.
    float accelX_mps2 = (accelX_mg / 1000.0f) * 9.81f;
    float accelY_mps2 = (accelY_mg / 1000.0f) * 9.81f;
    float accelZ_mps2 = (accelZ_mg / 1000.0f) * 9.81f;

    // Printed as milli-(m/s^2) integers, same float-in-printf avoidance as the mg line above --
    // the underlying accel*_mps2 floats are the real value a future Scene.gravity_x/y assignment
    // would use; only the *display* stays integer-only.
    long accelX_mmps2 = (long)(accelX_mps2 * 1000.0f);
    long accelY_mmps2 = (long)(accelY_mps2 * 1000.0f);
    long accelZ_mmps2 = (long)(accelZ_mps2 * 1000.0f);

    sprintf(msg, "[ACCEL] mm/s2=[X=%ld Y=%ld Z=%ld] (=m/s2 x1000; X->gravity_x, Y->gravity_y, Z ignored)\r\n",
            accelX_mmps2, accelY_mmps2, accelZ_mmps2);
    HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
}

/* Fills bsrr_buf/moder_buf with every one of the 240 ordered (source,sink) pin
   pairs on PB0..PB15, generated from the charlieplex formula instead of hand-typed
   hex -- a transcription error in a 240-entry literal table is exactly the kind of
   mistake that, on this resistor-less board, could drive two pins into each other.
   Bit-pattern verified by hand against the working 3-pin proof of concept in
   stm_projects/sample-charlieplexing (e.g. its LED1 = PB0 source/PB6 sink encodes
   to moder=0x00001001/bsrr=0x00400001, which this formula reproduces exactly for
   src=0,dst=6). */
static void ChargePlex_BuildScanTable(void)
{
    int slot = 0;
    for (int src = 0; src < CP_PIN_COUNT; src++)
    {
        for (int dst = 0; dst < CP_PIN_COUNT; dst++)
        {
            if (src == dst) continue;
            bsrr_buf[slot]      = (1UL << src) | (1UL << (dst + 16));
            moder_template[slot] = (1UL << (src * 2)) | (1UL << (dst * 2));
            moder_buf[slot]      = moder_template[slot]; /* initial state: every slot on */
            slot++;
        }
    }
}

/* Confirms every one of the 240 slots is either a valid "on" slot (exactly one source
   pin high, one different sink pin low, every other pin plain input) or a valid "off"
   slot (every pin plain input, nothing driven) -- checked before MOE/DMA are ever
   enabled, and again before any regenerated frame buffer is allowed to replace the
   live moder_buf (research.md Decision 17), so a table bug fails safe (matrix doesn't
   start, or the previous known-good frame is kept) instead of silently energizing an
   unintended pin pair. */
static int ChargePlex_ValidateScanTable(const uint32_t *moderToCheck)
{
    for (int slot = 0; slot < CP_SLOT_COUNT; slot++)
    {
        uint32_t moder = moderToCheck[slot];
        uint32_t bsrr  = bsrr_buf[slot]; /* bsrr_buf never changes after boot -- only moder does */
        int outputCount = 0;
        int srcPin = -1, dstPin = -1;

        for (int pin = 0; pin < CP_PIN_COUNT; pin++)
        {
            uint32_t field  = (moder >> (pin * 2)) & 0x3UL;
            int      isSet  = (bsrr & (1UL << pin)) != 0;
            int      isReset = (bsrr & (1UL << (pin + 16))) != 0;

            if (field == 0x1) /* configured as output for this slot */
            {
                outputCount++;
                if (isReset)    dstPin = pin; /* RESET has priority over SET in BSRR on real silicon */
                else if (isSet) srcPin = pin;
                else            return 0; /* output pin with no defined level -- reject */
            }
            else if (field != 0x0)
            {
                return 0; /* AF/analog should never appear in this table -- reject */
            }
            /* field == 0x0 (input): nothing to check. bsrr_buf is immutable and permanently
               holds this slot's 2 designated pins regardless of whether this slot is currently
               on or off -- an input pin is electrically disconnected from BSRR/ODR at the
               hardware level, so a "set"/"reset" bit sitting there for an off slot is the
               expected, harmless resting state, not a defect (this was incorrectly rejected
               before Round 8's off-slot support was added, which made every off slot fail). */
        }

        if (outputCount == 0)
        {
            continue; /* valid "off" slot -- nothing driven this cycle (research.md Decision 17) */
        }
        if (outputCount != 2 || srcPin < 0 || dstPin < 0 || srcPin == dstPin)
            return 0;
    }
    return 1;
}

/* Row/column mapping calibration sweep: lights one full RAW (un-rotated, un-corrected) row at
   a time -- row index 0 first, advancing 0,1,2,...,15 -- then, once all CP_ROWS rows have been
   shown, switches to lighting one full RAW column at a time -- column index 0 first, advancing
   0,1,2,...,14 -- then loops back to rows again, forever. Every step prints exactly which raw
   index it just lit, 1-indexed for easy counting against the physical board ("ROW 1 of 16",
   "COLUMN 1 of 15", etc). No offset/rotation is applied anywhere in this function -- the whole
   point is to read the board's true, unmodified row/column wiring order directly off the UART
   log, the same way the original per-row sweep already revealed the row-8 offset that axelor's
   real simulation now corrects for. The DMA scanner is completely unaware of any of this -- it
   still only ever drives one (src,dst) pin pair at any given instant, exactly as before; marking
   a whole row/column "on" here just means the scanner's existing one-at-a-time sweep spends this
   ~0.5s tick cycling only through that row's/column's slots, so persistence of vision reads it as
   the whole row/column being lit. */
static void ChargePlex_GeneratePattern(void)
{
    static int calibStep = 0; /* 0..CP_ROWS-1 = rows, CP_ROWS..CP_ROWS+CP_COLS-1 = columns */

    memset(cpFrameBuf, 0, sizeof(cpFrameBuf));

    if (calibStep < CP_ROWS)
    {
        int row = calibStep;
        for (int col = 0; col < CP_COLS; col++)
        {
            cpFrameBuf[row][col] = 1;
        }
        sprintf(msg, "[CHARLIEPLEX] ROW %d of %d lit (raw row index %d, %d LEDs)\r\n",
                calibStep + 1, CP_ROWS, row, CP_COLS);
    }
    else
    {
        int col = calibStep - CP_ROWS;
        for (int row = 0; row < CP_ROWS; row++)
        {
            cpFrameBuf[row][col] = 1;
        }
        sprintf(msg, "[CHARLIEPLEX] COLUMN %d of %d lit (raw col index %d, %d LEDs)\r\n",
                col + 1, CP_COLS, col, CP_ROWS);
    }
    HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);

    calibStep = (calibStep + 1) % (CP_ROWS + CP_COLS);
}

/* Translates cpFrameBuf into a candidate moder_buf (on slots get their immutable
   moder_template pattern back; off slots get 0/all-input), validates that candidate in
   full, and only then commits it over the live moder_buf -- exactly the same fail-safe
   shape as the boot-time check, applied to every regenerated frame (research.md Decision
   17). A rejected frame is not a stall (the existing scan is still known-good and keeps
   running); it just means this particular new frame didn't pass and is discarded. */
static void ChargePlex_ApplyFrameBuffer(void)
{
    for (int row = 0; row < CP_ROWS; row++)
    {
        for (int col = 0; col < CP_COLS; col++)
        {
            int slot = row * CP_COLS + col; /* matches ChargePlex_BuildScanTable's row-major (src,dst) enumeration exactly */
            cpCandidateModer[slot] = cpFrameBuf[row][col] ? moder_template[slot] : 0;
        }
    }

    if (ChargePlex_ValidateScanTable(cpCandidateModer))
    {
        memcpy(moder_buf, cpCandidateModer, sizeof(moder_buf));
    }
    else
    {
        sprintf(msg, "[CHARLIEPLEX] frame rejected, keeping previous frame\r\n");
        HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
    }
}

/* Drives every PB pin back to plain input (HiZ) directly via register write,
   bypassing TIM1/DMA entirely. Deliberately avoids any HAL call that polls against
   HAL_GetTick() (e.g. HAL_DMA_Abort) -- this can be invoked from SysTick_Handler
   itself, where the tick is not advancing, so any such call would spin forever
   instead of completing. */
static void ChargePlex_ForceSafeState(void)
{
    __HAL_TIM_MOE_DISABLE(&htim1);
    __HAL_TIM_DISABLE(&htim1);
    hdma_tim1_up.Instance->CCR  &= ~DMA_CCR_EN;
    hdma_tim1_ch1.Instance->CCR &= ~DMA_CCR_EN;
    GPIOB->MODER = 0x00000000;
}

/* Called every 1ms from SysTick_Handler (stm32l4xx_it.c) so stall-detection latency
   never depends on whatever the main loop happens to be blocked in (HAL_Delay,
   blocking UART/SPI transfers, etc). Compares each DMA channel's remaining-transfer
   counter against the value read on the *previous* 1ms tick: at the real scan rate
   (~80MHz/2776 =~ 28.8 kHz/slot) the counter advances by roughly 28 of its 240
   states every millisecond, so an unchanged reading one tick later means the scan
   has stalled -- and with no series resistors on this board, a stalled scan means
   whatever pin pair was active is now driven continuously instead of multiplexed. */
void ChargePlex_CheckAlive(void)
{
    static uint32_t lastUp  = 0xFFFFFFFFUL; /* sentinel: no prior sample yet */
    static uint32_t lastCh1 = 0xFFFFFFFFUL;

    if (chargePlexFaulted || !chargePlexScanStarted) return;

    uint32_t up  = __HAL_DMA_GET_COUNTER(&hdma_tim1_up);
    uint32_t ch1 = __HAL_DMA_GET_COUNTER(&hdma_tim1_ch1);

    if (lastUp != 0xFFFFFFFFUL && (up == lastUp || ch1 == lastCh1))
    {
        chargePlexFaulted = 1;
        ChargePlex_ForceSafeState();
    }

    lastUp  = up;
    lastCh1 = ch1;
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
  MX_DMA_Init();
  MX_SPI1_Init();
  MX_USART1_UART_Init();
  MX_TIM1_Init();
  /* USER CODE BEGIN 2 */
  sprintf(msg, "\r\n>>> BUILD MARKER: AXELOR-001 <<<\r\n");
  HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);

  HAL_Delay(100); // MPU-6500 register read/write start-up time
  MPU6500_ReadWhoAmI();
  MPU6500_Init();

  ChargePlex_BuildScanTable();
  if (!ChargePlex_ValidateScanTable(moder_buf))
  {
      sprintf(msg, "[CHARLIEPLEX] FAULT: scan table failed self-check -- matrix NOT started\r\n");
      HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
      chargePlexFaulted = 1;
      chargePlexFaultReported = 1;
      ChargePlex_ForceSafeState(); /* belt-and-suspenders: PB pins should already be input from MX_GPIO_Init */
  }
  else
  {
      sprintf(msg, "[CHARLIEPLEX] scan table OK (240/240 slots), starting matrix scan\r\n");
      HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);

      __HAL_TIM_MOE_ENABLE(&htim1);
      HAL_DMA_Start(&hdma_tim1_ch1, (uint32_t)bsrr_buf, (uint32_t)&GPIOB->BSRR, CP_SLOT_COUNT);
      HAL_DMA_Start(&hdma_tim1_up, (uint32_t)moder_buf, (uint32_t)&GPIOB->MODER, CP_SLOT_COUNT);
      HAL_TIM_OC_Start(&htim1, TIM_CHANNEL_1);
      __HAL_TIM_ENABLE_DMA(&htim1, TIM_DMA_CC1);
      __HAL_TIM_ENABLE_DMA(&htim1, TIM_DMA_UPDATE);
      HAL_TIM_Base_Start(&htim1);
      chargePlexScanStarted = 1; /* only now is it safe for ChargePlex_CheckAlive to read the DMA counters */
  }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    if (chargePlexFaulted && !chargePlexFaultReported)
    {
        sprintf(msg, "[CHARLIEPLEX] FAULT: scan stalled -- matrix forced to safe input state\r\n");
        HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
        chargePlexFaultReported = 1;
    }
    else if (!chargePlexFaulted)
    {
        /* Reuses this existing ~500ms tick as the pattern-refresh cadence (research.md
           Decision 17) -- deliberately not a new timer/ISR. Skipped once faulted: TIM1/DMA
           are already stopped by ChargePlex_ForceSafeState, so there is nothing left to
           apply a new frame to. */
        ChargePlex_GeneratePattern();
        ChargePlex_ApplyFrameBuffer();
    }

    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
    blinkCounter++;
    sprintf(msg, "Counter: %lu\r\n", blinkCounter);
    HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
    MPU6500_ReadAccel();
    HAL_Delay(500);
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
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 40;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
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
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_128;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 0;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 2775;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_OC_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_TIMING;
  sConfigOC.Pulse = 2625;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_OC_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.BreakFilter = 0;
  sBreakDeadTimeConfig.Break2State = TIM_BREAK2_DISABLE;
  sBreakDeadTimeConfig.Break2Polarity = TIM_BREAK2POLARITY_HIGH;
  sBreakDeadTimeConfig.Break2Filter = 0;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */

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
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel2_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel2_IRQn);
  /* DMA1_Channel6_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel6_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel6_IRQn);

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
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PA5 */
  GPIO_InitStruct.Pin = GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB0 PB1 PB2 PB10
                           PB11 PB12 PB13 PB14
                           PB15 PB3 PB4 PB5
                           PB6 PB7 PB8 PB9 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_10
                          |GPIO_PIN_11|GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14
                          |GPIO_PIN_15|GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5
                          |GPIO_PIN_6|GPIO_PIN_7|GPIO_PIN_8|GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

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
