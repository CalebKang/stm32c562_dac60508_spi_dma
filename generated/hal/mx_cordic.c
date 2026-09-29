/**
  ******************************************************************************
  * @file           : mx_cordic.c
  * @brief          : CORDIC Peripheral initialization
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the mx_stm32c5xx_hal_drivers_license.md file
  * in the same directory as the generated code.
  * If no mx_stm32c5xx_hal_drivers_license.md file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "mx_cordic.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private functions prototypes ----------------------------------------------*/
/* Exported variables by reference -------------------------------------------*/

/******************************************************************************/
/* Exported functions for CORDIC in LL layer                                 */
/******************************************************************************/
CORDIC_TypeDef *mx_cordic_init(void)
{
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_CORDIC);
  /* LL_CORDIC_SetFunction(CORDIC, LL_CORDIC_FUNCTION_COSINE); */ /* Configuration matches register reset state at startup. */
  /* LL_CORDIC_SetPrecision(CORDIC, LL_CORDIC_PRECISION_5_CYCLE); */ /* Configuration matches register reset state at startup. */
  /* LL_CORDIC_SetScale(CORDIC, LL_CORDIC_SCALING_FACTOR_0); */ /* Configuration matches register reset state at startup. */
  LL_CORDIC_SetNbWrite(CORDIC, LL_CORDIC_NBWRITE_2);
  LL_CORDIC_SetNbRead(CORDIC, LL_CORDIC_NBREAD_2);
  /* LL_CORDIC_SetInWidth(CORDIC, LL_CORDIC_INWIDTH_32_BIT); */ /* Configuration matches register reset state at startup. */
  /* LL_CORDIC_SetOutWidth(CORDIC, LL_CORDIC_OUTWIDTH_32_BIT); */ /* Configuration matches register reset state at startup. */
  return CORDIC;
}
void mx_cordic_deinit(void)
{
  LL_AHB1_GRP1_ForceReset(LL_AHB1_GRP1_PERIPH_CORDIC);
  LL_AHB1_GRP1_ReleaseReset(LL_AHB1_GRP1_PERIPH_CORDIC);

  LL_AHB1_GRP1_DisableClock(LL_AHB1_GRP1_PERIPH_CORDIC);
}
