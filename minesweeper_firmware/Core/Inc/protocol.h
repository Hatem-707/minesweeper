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
  uint16_t metal_detector_raw; /* digital level of the detector output: 0 = low, 1 = high */
  int16_t accel_x_raw;
  int16_t accel_y_raw;
  int16_t accel_z_raw;
  int16_t gyro_x_raw;
  int16_t gyro_y_raw;
  int16_t gyro_z_raw;
  int16_t mag_x_raw;
  int16_t mag_y_raw;
  int16_t mag_z_raw;
  uint8_t imu_status;   /* 0 = ok; else 1 + HAL_StatusTypeDef of the failing call (1 error, 2 busy, 3 timeout), or 4 = all-zero data */
  uint8_t mag_status;   /* 0 = ok; else 1 + HAL_StatusTypeDef of the failing call, or 4 = offline/not detected */
  uint8_t imu_who_am_i; /* last WHO_AM_I value read (0x68 for a genuine MPU6050) */
  uint8_t mag_who_am_i; /* magnetometer chip ID / type (0x48 for HMC5883L, 0xFF for QMC5883L, 0 = none) */
  uint16_t i2c_error;   /* hi2c2.ErrorCode */
  uint8_t crc;
} TelemetryPacket_t;

/* Pi -> STM32, expected at least once per watchdog period */
typedef struct
{
  uint8_t seq;
  int16_t linear_x;  /* mm/s, +forward */
  int16_t angular_z; /* mrad/s, +counter-clockwise */
  uint8_t flags;
  uint8_t crc;
} CommandPacket_t;

#pragma pack(pop)

uint8_t protocol_crc8(const uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* __PROTOCOL_H */
