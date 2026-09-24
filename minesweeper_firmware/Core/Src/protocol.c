#include "protocol.h"

/* CRC-8, polynomial 0x07 (CRC-8/SMBUS), initial value 0x00 */
uint8_t protocol_crc8(const uint8_t *data, uint16_t len)
{
  uint8_t crc = 0x00U;

  for (uint16_t i = 0U; i < len; i++)
  {
    crc ^= data[i];
    for (uint8_t bit = 0U; bit < 8U; bit++)
    {
      if ((crc & 0x80U) != 0U)
      {
        crc = (uint8_t)((crc << 1) ^ 0x07U);
      }
      else
      {
        crc = (uint8_t)(crc << 1);
      }
    }
  }

  return crc;
}
