#ifndef DAC60508_MASTER_H
#define DAC60508_MASTER_H

#include <stdint.h>
#include "dac60508_protocol.h"

typedef enum
{
  DAC60508_MASTER_OK = 0,
  DAC60508_MASTER_BAD_CHANNEL,
  DAC60508_MASTER_BAD_CODE,
  DAC60508_MASTER_DMA_ERROR,
  DAC60508_MASTER_TIMEOUT
} dac60508_master_status_t;

/* mx_system_init() must already have initialized SPI1 and LPDMA1_CH0. */
dac60508_master_status_t dac60508_master_init(void);
dac60508_master_status_t dac60508_master_set_channel(uint8_t channel, uint16_t code);

/* Called from the SPI1 TX DMA complete IRQ, after SPI EOT and CS release. */
typedef void (*dac60508_dma_tc_callback_t)(const void *context);
void dac60508_master_set_dma_tc_callback(dac60508_dma_tc_callback_t callback,
                                        const void *context);

#endif /* DAC60508_MASTER_H */
