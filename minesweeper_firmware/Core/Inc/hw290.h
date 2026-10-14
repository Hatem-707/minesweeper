/* HW-290 (GY-87) 10-DOF IMU driver (MPU6050 + HMC5883L/QMC5883L + BMP180) */
#ifndef __HW290_H
#define __HW290_H

#include "stm32f4xx_hal.h"
#include "mpu6050.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HW290_MPU6050_ADDR_DEFAULT  (0x68U << 1)
#define HW290_HMC5883L_ADDR         (0x1EU << 1)
#define HW290_QMC5883L_ADDR         (0x0DU << 1)

typedef enum
{
  HW290_MAG_NONE = 0,
  HW290_MAG_HMC5883L = 1,
  HW290_MAG_QMC5883L = 2
} HW290_MagType_t;

typedef struct
{
  int16_t accel_x_raw;
  int16_t accel_y_raw;
  int16_t accel_z_raw;
  int16_t temp_raw;
  int16_t gyro_x_raw;
  int16_t gyro_y_raw;
  int16_t gyro_z_raw;
  int16_t mag_x_raw;
  int16_t mag_y_raw;
  int16_t mag_z_raw;

  float accel_x_g;
  float accel_y_g;
  float accel_z_g;
  float temp_c;
  float gyro_x_dps;
  float gyro_y_dps;
  float gyro_z_dps;
  float mag_x_ut;
  float mag_y_ut;
  float mag_z_ut;
} HW290_Data_t;

extern uint8_t HW290_ImuWhoAmI;
extern uint8_t HW290_MagWhoAmI;
extern HW290_MagType_t HW290_MagType;

HAL_StatusTypeDef HW290_Init(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef HW290_InitMag(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef HW290_ReadAll(I2C_HandleTypeDef *hi2c, HW290_Data_t *data,
                                uint8_t *imu_status_out, uint8_t *mag_status_out);

#ifdef __cplusplus
}
#endif

#endif /* __HW290_H */
