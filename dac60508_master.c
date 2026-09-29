#include "dac60508_master.h"
#include "stm32_ll.h"
#include "stm32_hal.h"

#define DAC60508_CS_PORT GPIOA
#define DAC60508_CS_PIN LL_GPIO_PIN_4
#define DAC60508_TRANSFER_TIMEOUT_MS 20U

static uint8_t tx_frame[4]; /* DMA source must remain valid until EOT. */
static volatile uint8_t dma_complete;
static volatile uint8_t dma_error;
static uint8_t initialized;
static dac60508_dma_tc_callback_t dma_tc_callback;
static const void *dma_tc_context;

static void dac60508_master_close_transfer(void)
{
  LL_DMA_DisableChannel(LPDMA1_CH0);
  LL_SPI_Disable(SPI1);
  LL_GPIO_SetOutputPin(DAC60508_CS_PORT, DAC60508_CS_PIN);
  LL_SPI_ClearFlag_EOT(SPI1);
  LL_SPI_ClearFlag_TXTF(SPI1);
}

static dac60508_master_status_t dac60508_master_write(uint8_t reg,
                                                       uint16_t value,
                                                       uint8_t crc_enabled)
{
  const uint32_t length = crc_enabled != 0U ? 4U : 3U;
  const uint32_t start = HAL_GetTick();
  dac60508_master_status_t result = DAC60508_MASTER_OK;

  tx_frame[0] = reg & 0x0FU; /* RW=0, reserved bits=0. */
  tx_frame[1] = (uint8_t)(value >> 8);
  tx_frame[2] = (uint8_t)value;
  tx_frame[3] = dac60508_crc8(tx_frame, 3U);

  dma_complete = 0U;
  dma_error = 0U;
  LL_DMA_ClearFlag(LPDMA1_CH0, LL_DMA_FLAG_ALL);
  /* CSAR and BNDT are transfer state; re-arm them for every DMA block. */
  LL_DMA_SetSrcAddress(LPDMA1_CH0, (uint32_t)(uintptr_t)tx_frame);
  LL_DMA_SetBlkDataLength(LPDMA1_CH0, length);

  LL_GPIO_ResetOutputPin(DAC60508_CS_PORT, DAC60508_CS_PIN);
  /* DMA can fill the four-byte FIFO before CSTART; defer its IRQ until then. */
  NVIC_DisableIRQ(LPDMA1_CH0_IRQn);
  NVIC_ClearPendingIRQ(LPDMA1_CH0_IRQn);
  LL_DMA_EnableChannel(LPDMA1_CH0);
  LL_SPI_Enable(SPI1);
  LL_SPI_StartMasterTransfer(SPI1);
  NVIC_EnableIRQ(LPDMA1_CH0_IRQn);

  while (dma_complete == 0U)
  {
    if ((dma_error != 0U) || (LL_DMA_IsActiveFlag_DTE(LPDMA1_CH0) != 0U) ||
        (LL_DMA_IsActiveFlag_ULE(LPDMA1_CH0) != 0U) ||
        (LL_DMA_IsActiveFlag_USE(LPDMA1_CH0) != 0U))
    {
      result = DAC60508_MASTER_DMA_ERROR;
      break;
    }
    if ((uint32_t)(HAL_GetTick() - start) >= DAC60508_TRANSFER_TIMEOUT_MS)
    {
      result = DAC60508_MASTER_TIMEOUT;
      break;
    }
  }

  if (result != DAC60508_MASTER_OK)
  {
    dac60508_master_close_transfer();
  }
  return result;
}

void dac60508_master_set_dma_tc_callback(dac60508_dma_tc_callback_t callback,
                                        const void *context)
{
  dma_tc_context = context;
  dma_tc_callback = callback;
}

dac60508_master_status_t dac60508_master_init(void)
{
  /* PA4 is unused by CubeMX2; wire it to a real DAC's CS when fitted. */
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);
  LL_GPIO_SetOutputPin(DAC60508_CS_PORT, DAC60508_CS_PIN);
  LL_GPIO_SetPinOutputType(DAC60508_CS_PORT, DAC60508_CS_PIN, LL_GPIO_OUTPUT_PUSHPULL);
  LL_GPIO_SetPinSpeed(DAC60508_CS_PORT, DAC60508_CS_PIN, LL_GPIO_SPEED_FREQ_HIGH);
  LL_GPIO_SetPinMode(DAC60508_CS_PORT, DAC60508_CS_PIN, LL_GPIO_MODE_OUTPUT);

  /* SPI2 uses one RX interrupt per byte; slow down SPI1 for this loopback test. */
  LL_SPI_SetBaudRatePrescaler(SPI1, LL_SPI_BAUD_RATE_PRESCALER_256);
  LL_SPI_SetTransferDirection(SPI1, LL_SPI_SIMPLEX_TX);
  LL_SPI_SetTransferSize(SPI1, 3U);
  LL_SPI_EnableDMAReq_TX(SPI1);
  LL_DMA_DisableChannel(LPDMA1_CH0);
  LL_DMA_ClearFlag(LPDMA1_CH0, LL_DMA_FLAG_ALL);
  LL_DMA_SetDestAddress(LPDMA1_CH0, (uint32_t)(uintptr_t)&SPI1->TXDR);
  LL_DMA_EnableIT_TC(LPDMA1_CH0);
  LL_DMA_EnableIT_DTE(LPDMA1_CH0);
  /* Let SPI2 RX and SysTick preempt the long benchmark in the DMA TC IRQ. */
  NVIC_SetPriority(SysTick_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 9, 0));
  NVIC_SetPriority(LPDMA1_CH0_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 11, 0));
  dma_tc_callback = NULL;
  dma_tc_context = NULL;
  initialized = 0U;

  /* CRC-EN powers up clear, so this first CONFIG write is a 24-bit cycle. */
  dac60508_master_status_t result = dac60508_master_write(DAC60508_REG_CONFIG,
                                                            DAC60508_CONFIG_CRC_EN, 0U);
  if (result == DAC60508_MASTER_OK)
  {
    /* All subsequent CRC-protected writes use four 8-bit frames. */
    LL_SPI_SetTransferSize(SPI1, 4U);
    initialized = 1U;
  }
  return result;
}

dac60508_master_status_t dac60508_master_set_channel(uint8_t channel, uint16_t code)
{
  if (channel >= DAC60508_CHANNEL_COUNT)
  {
    return DAC60508_MASTER_BAD_CHANNEL;
  }
  if (code > DAC60508_MAX_CODE)
  {
    return DAC60508_MASTER_BAD_CODE;
  }
  if (initialized == 0U)
  {
    return DAC60508_MASTER_DMA_ERROR;
  }
  /* DAC60508 uses the upper 12 bits of the 16-bit data register. */
  return dac60508_master_write((uint8_t)(DAC60508_REG_DAC0 + channel),
                                (uint16_t)(code << 4), 1U);
}

void LPDMA1_CH0_IRQHandler(void)
{
  if (dma_error != 0U)
  {
    LL_DMA_ClearFlag(LPDMA1_CH0, LL_DMA_FLAG_ALL);
    return;
  }
  if ((LL_DMA_IsActiveFlag_DTE(LPDMA1_CH0) != 0U) ||
      (LL_DMA_IsActiveFlag_ULE(LPDMA1_CH0) != 0U) ||
      (LL_DMA_IsActiveFlag_USE(LPDMA1_CH0) != 0U))
  {
    LL_DMA_ClearFlag(LPDMA1_CH0, LL_DMA_FLAG_ALL);
    dma_error = 1U;
    return;
  }
  if (LL_DMA_IsActiveFlag_TC(LPDMA1_CH0) != 0U)
  {
    LL_DMA_ClearFlag_TC(LPDMA1_CH0);
    const uint32_t start = HAL_GetTick();
    while (LL_SPI_IsActiveFlag_EOT(SPI1) == 0U)
    {
      if ((uint32_t)(HAL_GetTick() - start) >= DAC60508_TRANSFER_TIMEOUT_MS)
      {
        dma_error = 1U;
        dac60508_master_close_transfer();
        return;
      }
    }
    dac60508_master_close_transfer();
    dma_complete = 1U;
    if (dma_tc_callback != NULL)
    {
      dma_tc_callback(dma_tc_context);
    }
  }
}
