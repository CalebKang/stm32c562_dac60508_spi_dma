#ifndef DAC60508_PROTOCOL_H
#define DAC60508_PROTOCOL_H

#include <stdint.h>

#define DAC60508_REG_CONFIG 0x03U
#define DAC60508_REG_DAC0 0x08U
#define DAC60508_CONFIG_CRC_EN 0x0800U
#define DAC60508_CHANNEL_COUNT 8U
#define DAC60508_MAX_CODE 0x0FFFU

/* CRC-8-ATM/HEC: polynomial x^8+x^2+x+1, init 0, no reflection/xorout. */
static inline uint8_t dac60508_crc8(const uint8_t *data, uint32_t length)
{
  uint8_t crc = 0U;
  for (uint32_t i = 0U; i < length; ++i)
  {
    crc ^= data[i];
    for (uint32_t bit = 0U; bit < 8U; ++bit)
    {
      crc = (crc & 0x80U) != 0U ? (uint8_t)((crc << 1) ^ 0x07U)
                                : (uint8_t)(crc << 1);
    }
  }
  return crc;
}

#endif /* DAC60508_PROTOCOL_H */
