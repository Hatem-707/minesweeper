#include "cobs.h"

size_t cobs_encode(const uint8_t *src, size_t len, uint8_t *dst)
{
  size_t read_idx = 0U;
  size_t write_idx = 1U;
  size_t code_idx = 0U;
  uint8_t code = 1U;

  while (read_idx < len)
  {
    if (src[read_idx] == 0U)
    {
      dst[code_idx] = code;
      code = 1U;
      code_idx = write_idx++;
      read_idx++;
    }
    else
    {
      dst[write_idx++] = src[read_idx++];
      code++;
      if (code == 0xFFU)
      {
        dst[code_idx] = code;
        code = 1U;
        code_idx = write_idx++;
      }
    }
  }

  dst[code_idx] = code;

  return write_idx;
}

size_t cobs_decode(const uint8_t *src, size_t len, uint8_t *dst)
{
  size_t read_idx = 0U;
  size_t write_idx = 0U;

  while (read_idx < len)
  {
    uint8_t code = src[read_idx];

    if (code == 0U || (read_idx + code - 1U) > len)
    {
      return (size_t)-1;
    }

    read_idx++;

    for (uint8_t i = 1U; i < code; i++)
    {
      if (read_idx >= len)
      {
        return (size_t)-1;
      }
      dst[write_idx++] = src[read_idx++];
    }

    if (code != 0xFFU && read_idx < len)
    {
      dst[write_idx++] = 0U;
    }
  }

  return write_idx;
}
