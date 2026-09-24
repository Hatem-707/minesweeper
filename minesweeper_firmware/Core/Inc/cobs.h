/* Consistent Overhead Byte Stuffing (COBS) encode/decode */
#ifndef __COBS_H
#define __COBS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

/* Usable in static array size declarations, unlike cobs_max_encoded_size(). */
#define COBS_MAX_ENCODED_SIZE(len) ((len) + ((len) / 254U) + 1U)

/* dst must be at least cobs_max_encoded_size(len) bytes. Does not append the
 * trailing 0x00 frame delimiter; caller writes that when placing on the wire. */
size_t cobs_encode(const uint8_t *src, size_t len, uint8_t *dst);

/* Decodes one COBS block (no delimiter byte included in src/len).
 * Returns decoded length, or (size_t)-1 on malformed input. dst must be at
 * least len bytes. */
size_t cobs_decode(const uint8_t *src, size_t len, uint8_t *dst);

static inline size_t cobs_max_encoded_size(size_t len)
{
  return len + (len / 254U) + 1U;
}

#ifdef __cplusplus
}
#endif

#endif /* __COBS_H */
