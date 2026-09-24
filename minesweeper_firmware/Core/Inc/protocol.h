/* Binary wire protocol between Raspberry Pi and the STM32 real-time controller.
 * Frames are COBS-encoded and delimited by a single 0x00 byte on the wire. */
#ifndef __PROTOCOL_H
#define __PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define PROTOCOL_CMD_FLAG_MAGNET_ON 0x01U

#pragma pack(push, 1)

/* STM32 -> Pi, sent periodically */
typedef struct
{
  uint8_t seq;
  int32_t encoder_left;
  int32_t encoder_right;
  uint16_t metal_detector_raw;
  int16_t accel_x_raw;
  int16_t accel_y_raw;
  int16_t accel_z_raw;
  int16_t gyro_x_raw;
  int16_t gyro_y_raw;
  int16_t gyro_z_raw;
  uint8_t crc;
} TelemetryPacket_t;

/* Pi -> STM32, expected at least once per watchdog period */
typedef struct
{
  uint8_t seq;
  int16_t left_duty;  /* -1000..1000 */
  int16_t right_duty; /* -1000..1000 */
  uint8_t flags;
  uint8_t crc;
} CommandPacket_t;

#pragma pack(pop)

uint8_t protocol_crc8(const uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* __PROTOCOL_H */
