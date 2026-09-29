/**
  ******************************************************************************
  * file           : main.c
  * brief          : Main program body
  *                  Calls target system initialization then loop in main.
  ******************************************************************************
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dac60508_master.h"
#include "dac60508_slave.h"
#include <math.h>

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define FIBONACCI_CALIBRATION_ITERATIONS 100000U
#define TRIG_ITERATIONS 1000U

/* Keep the two Fibonacci loops identical; this loop has no flash calls. */
#define FIBONACCI_WORK_BODY(iterations, sink)           \
  do                                                   \
  {                                                    \
    volatile uint32_t previous = 0U;                   \
    volatile uint32_t current = 1U;                    \
    for (uint32_t i = 0U; i < (iterations); ++i)        \
    {                                                  \
      const uint32_t next = previous + current;        \
      previous = current;                              \
      current = next;                                  \
    }                                                  \
    (sink) = current;                                   \
  } while (0)

/* Vary volatile input and retain the sum so every math call is needed. */
#define TRIG_WORK_BODY(iterations, sink)                            \
  do                                                               \
  {                                                                \
    const double base = (double)((iterations) & 0xFFFFU) / 65536.0; \
    double sum = 0.0;                                              \
    for (uint32_t trig_index = 0U; trig_index < TRIG_ITERATIONS; ++trig_index) \
    {                                                              \
      volatile double x = base + (double)trig_index * 0.001;       \
      const double sine = sin(x);                                  \
      const double cosine = cos(x + 0.25);                         \
      sum += atan2(sine, cosine);                                  \
    }                                                              \
    (sink) = sum;                                                  \
  } while (0)

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* DWT cycles; at 144 MHz, 72,000,000 cycles correspond to 500 ms. */
volatile uint32_t fibonacci_elapsed_cycles;
volatile uint32_t fibonacci_sram2_elapsed_cycles;
volatile uint32_t fibonacci_last_value;
volatile uint32_t fibonacci_sram2_last_value;
volatile double fibonacci_trig_result;
volatile double fibonacci_sram2_trig_result;

/* Private functions prototype -----------------------------------------------*/
static void __attribute__((noinline, noclone, optimize("O3")))
fibonacci_workload_500ms(uint32_t iterations);
/* Optimize only the SRAM2 copy; the flash copy keeps the project's -O0. */
static void __attribute__((noinline, noclone, used, section(".sram2_text"), optimize("O3")))
fibonacci_workload_500ms_sram2(uint32_t iterations);
static void fibonacci_dma_tc(const void *context);

static void fibonacci_workload_500ms(uint32_t iterations)
{
  FIBONACCI_WORK_BODY(iterations, fibonacci_last_value);
  TRIG_WORK_BODY(iterations, fibonacci_trig_result);
}

static void fibonacci_workload_500ms_sram2(uint32_t iterations)
{
  FIBONACCI_WORK_BODY(iterations, fibonacci_sram2_last_value);
  TRIG_WORK_BODY(iterations, fibonacci_sram2_trig_result);
}

static void fibonacci_dma_tc(const void *context)
{
  /* Both benchmarks run in the same DMA TC interrupt. */
  const uint32_t iterations = *(const uint32_t *)context;
  uint32_t start_cycles = DWT->CYCCNT;
  fibonacci_workload_500ms(iterations);
  fibonacci_elapsed_cycles = (uint32_t)(DWT->CYCCNT - start_cycles);

  start_cycles = DWT->CYCCNT;
  fibonacci_workload_500ms_sram2(iterations);
  fibonacci_sram2_elapsed_cycles = (uint32_t)(DWT->CYCCNT - start_cycles);
}

/**
  * brief:  The application entry point.
  * retval: none but we specify int to comply with C99 standard
  */
int main(void)
{
  /** System Init: this code placed in targets folder initializes your system.
    * It calls the initialization (and sets the initial configuration) of the peripherals.
    * You can use STM32CubeMX to generate and call this code or not in this project.
    * It also contains the HAL initialization and the initial clock configuration.
    */
  if (mx_system_init() != SYSTEM_OK)
  {
    return (-1);
  }
  else
  {
    dac60508_slave_init();
    if (dac60508_master_init() != DAC60508_MASTER_OK)
    {
      return (-1);
    }

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    if ((DWT->CTRL & DWT_CTRL_NOCYCCNT_Msk) != 0U)
    {
      return (-1);
    }
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    /* Calibrate a fixed amount of work once; then compare both memories. */
    const uint32_t calibration_start = DWT->CYCCNT;
    fibonacci_workload_500ms(FIBONACCI_CALIBRATION_ITERATIONS);
    const uint32_t calibration_cycles = (uint32_t)(DWT->CYCCNT - calibration_start);
    if (calibration_cycles == 0U)
    {
      return (-1);
    }
    /* Whole calibration batches avoid 64-bit division and stay near 500 ms. */
    uint32_t batches = (SystemCoreClock / 2U) / calibration_cycles;
    if (batches == 0U)
    {
      batches = 1U;
    }
    const uint32_t iterations = FIBONACCI_CALIBRATION_ITERATIONS * batches;
    fibonacci_workload_500ms_sram2(100U); /* Warm the SRAM2 code path once. */
    dac60508_master_set_dma_tc_callback(fibonacci_dma_tc, &iterations);

    uint16_t code = 0U;
    uint32_t last_update = HAL_GetTick();
    while (1)
    {
      if ((uint32_t)(HAL_GetTick() - last_update) >= 1000U)
      {
        last_update = HAL_GetTick();
        if (dac60508_master_set_channel(0U, code) != DAC60508_MASTER_OK)
        {
          return (-1);
        }
        code = (uint16_t)((code + 0x100U) & DAC60508_MAX_CODE);
      }
    }
  }
} /* end main */







