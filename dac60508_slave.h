#ifndef DAC60508_SLAVE_H
#define DAC60508_SLAVE_H

#include <stdint.h>
#include "dac60508_protocol.h"

/* Observe these in the debugger. Each accepted DAC code is 0..4095. */
extern volatile uint32_t dac60508_slave_last_frame;
extern volatile uint32_t dac60508_slave_frame_count;
extern volatile uint32_t dac60508_slave_crc_error_count;
extern volatile uint32_t dac60508_slave_overrun_count;
extern volatile uint16_t dac60508_slave_channel_code[DAC60508_CHANNEL_COUNT];
extern volatile uint16_t dac60508_slave_config;

/* Must be called before the master's first transfer. */
void dac60508_slave_init(void);

#endif /* DAC60508_SLAVE_H */
