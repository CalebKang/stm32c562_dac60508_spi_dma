#include "dac60508_slave.h"
#include "stm32_ll.h"

volatile uint32_t dac60508_slave_last_frame;
volatile uint32_t dac60508_slave_frame_count;
volatile uint32_t dac60508_slave_crc_error_count;
volatile uint32_t dac60508_slave_overrun_count;
volatile uint16_t dac60508_slave_channel_code[DAC60508_CHANNEL_COUNT];
volatile uint16_t dac60508_slave_config;

static uint8_t rx_frame[4];
static uint8_t rx_count;
static uint8_t crc_enabled;

static void dac60508_slave_accept_frame(void)
{
  const uint8_t reg = rx_frame[0] & 0x0FU;
  const uint16_t value = (uint16_t)(((uint16_t)rx_frame[1] << 8) | rx_frame[2]);
  uint32_t frame = ((uint32_t)rx_frame[0] << 16) |
                   ((uint32_t)rx_frame[1] << 8) | rx_frame[2];

  if (crc_enabled != 0U)
  {
    frame = (frame << 8) | rx_frame[3];
    if (dac60508_crc8(rx_frame, 3U) != rx_frame[3])
    {
      ++dac60508_slave_crc_error_count;
      dac60508_slave_last_frame = frame;
      return;
    }
  }
  dac60508_slave_last_frame = frame;
  ++dac60508_slave_frame_count;

  if ((rx_frame[0] & 0xF0U) != 0U) /* Only writes with zero reserved bits. */
  {
    return;
  }
  if (reg == DAC60508_REG_CONFIG)
  {
    dac60508_slave_config = value;
    crc_enabled = (value & DAC60508_CONFIG_CRC_EN) != 0U ? 1U : 0U;
  }
  else if ((reg >= DAC60508_REG_DAC0) &&
           (reg < DAC60508_REG_DAC0 + DAC60508_CHANNEL_COUNT))
  {
    dac60508_slave_channel_code[reg - DAC60508_REG_DAC0] = value >> 4;
  }
}

void dac60508_slave_init(void)
{
  rx_count = 0U;
  crc_enabled = 0U;
  dac60508_slave_last_frame = 0U;
  dac60508_slave_frame_count = 0U;
  dac60508_slave_crc_error_count = 0U;
  dac60508_slave_overrun_count = 0U;
  dac60508_slave_config = 0U;
  for (uint32_t i = 0U; i < DAC60508_CHANNEL_COUNT; ++i)
  {
    dac60508_slave_channel_code[i] = 0U;
  }

  /* No SPI2 TX request/interrupt: MISO remains unused. */
  LL_SPI_SetTransferDirection(SPI2, LL_SPI_SIMPLEX_RX);
  LL_SPI_EnableIT_RXP(SPI2);
  LL_SPI_EnableIT_OVR(SPI2);
  LL_SPI_Enable(SPI2);
}

void SPI2_IRQHandler(void)
{
  if (LL_SPI_IsActiveFlag_OVR(SPI2) != 0U)
  {
    LL_SPI_ClearFlag_OVR(SPI2);
    rx_count = 0U;
    ++dac60508_slave_overrun_count;
  }
  while (LL_SPI_IsActiveFlag_RXP(SPI2) != 0U)
  {
    const uint8_t byte = LL_SPI_ReceiveData8(SPI2);
    rx_frame[rx_count++] = byte;
    if (rx_count == (crc_enabled != 0U ? 4U : 3U))
    {
      dac60508_slave_accept_frame();
      rx_count = 0U;
    }
  }
}
