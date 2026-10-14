/* MPU6050 accelerometer/gyroscope driver (I2C) */
#ifndef __MPU6050_H
#define __MPU6050_H

#include "stm32f4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif


#include <stdint.h>

#define MPU6050_ADDR_DEFAULT   (0x68U << 1) /* AD0 pin low */

typedef struct
{
  int16_t accel_x_raw;
  int16_t accel_y_raw;
  int16_t accel_z_raw;
  int16_t temp_raw;
  int16_t gyro_x_raw;
  int16_t gyro_y_raw;
  int16_t gyro_z_raw;

  float accel_x_g;
  float accel_y_g;
  float accel_z_g;
  float temp_c;
  float gyro_x_dps;
  float gyro_y_dps;
  float gyro_z_dps;
} MPU6050_Data_t;

extern uint8_t MPU6050_LastWhoAmI;

HAL_StatusTypeDef MPU6050_Init(I2C_HandleTypeDef *hi2c, uint16_t dev_addr);
HAL_StatusTypeDef MPU6050_SetBypass(I2C_HandleTypeDef *hi2c, uint16_t dev_addr,
                                    uint8_t enable);
HAL_StatusTypeDef MPU6050_ReadAll(I2C_HandleTypeDef *hi2c, uint16_t dev_addr,
                                   MPU6050_Data_t *data);

#ifdef __cplusplus
}
#endif

#endif /* __MPU6050_H */
