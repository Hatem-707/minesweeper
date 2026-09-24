#include "mpu6050.h"

#define MPU6050_REG_SMPLRT_DIV   0x19U
#define MPU6050_REG_CONFIG       0x1AU
#define MPU6050_REG_GYRO_CONFIG  0x1BU
#define MPU6050_REG_ACCEL_CONFIG 0x1CU
#define MPU6050_REG_ACCEL_XOUT_H 0x3BU
#define MPU6050_REG_PWR_MGMT_1   0x6BU
#define MPU6050_REG_WHO_AM_I     0x75U

#define MPU6050_I2C_TIMEOUT_MS   100U

/* +/-2g -> 16384 LSB/g, +/-250 dps -> 131 LSB/(deg/s) */
#define MPU6050_ACCEL_SENSITIVITY 16384.0f
#define MPU6050_GYRO_SENSITIVITY  131.0f

static HAL_StatusTypeDef MPU6050_WriteReg(I2C_HandleTypeDef *hi2c, uint16_t dev_addr,
                                           uint8_t reg, uint8_t value)
{
  return HAL_I2C_Mem_Write(hi2c, dev_addr, reg, I2C_MEMADD_SIZE_8BIT,
                            &value, 1U, MPU6050_I2C_TIMEOUT_MS);
}

HAL_StatusTypeDef MPU6050_Init(I2C_HandleTypeDef *hi2c, uint16_t dev_addr)
{
  uint8_t who_am_i = 0U;
  HAL_StatusTypeDef status;

  status = HAL_I2C_Mem_Read(hi2c, dev_addr, MPU6050_REG_WHO_AM_I,
                             I2C_MEMADD_SIZE_8BIT, &who_am_i, 1U,
                             MPU6050_I2C_TIMEOUT_MS);
  if (status != HAL_OK)
  {
    return status;
  }

  /* Wake up the device from sleep mode and use internal 8MHz oscillator */
  status = MPU6050_WriteReg(hi2c, dev_addr, MPU6050_REG_PWR_MGMT_1, 0x00U);
  if (status != HAL_OK)
  {
    return status;
  }

  /* Sample rate = gyro rate / (1 + divider) = 1kHz / (1+7) = 125Hz */
  status = MPU6050_WriteReg(hi2c, dev_addr, MPU6050_REG_SMPLRT_DIV, 0x07U);
  if (status != HAL_OK)
  {
    return status;
  }

  /* Digital low pass filter ~44Hz bandwidth */
  status = MPU6050_WriteReg(hi2c, dev_addr, MPU6050_REG_CONFIG, 0x03U);
  if (status != HAL_OK)
  {
    return status;
  }

  /* Gyro full scale range: +/-250 deg/s */
  status = MPU6050_WriteReg(hi2c, dev_addr, MPU6050_REG_GYRO_CONFIG, 0x00U);
  if (status != HAL_OK)
  {
    return status;
  }

  /* Accel full scale range: +/-2g */
  status = MPU6050_WriteReg(hi2c, dev_addr, MPU6050_REG_ACCEL_CONFIG, 0x00U);

  return status;
}

HAL_StatusTypeDef MPU6050_ReadAll(I2C_HandleTypeDef *hi2c, uint16_t dev_addr,
                                   MPU6050_Data_t *data)
{
  uint8_t raw[14];
  HAL_StatusTypeDef status;

  status = HAL_I2C_Mem_Read(hi2c, dev_addr, MPU6050_REG_ACCEL_XOUT_H,
                             I2C_MEMADD_SIZE_8BIT, raw, sizeof(raw),
                             MPU6050_I2C_TIMEOUT_MS);
  if (status != HAL_OK)
  {
    return status;
  }

  data->accel_x_raw = (int16_t)((raw[0] << 8) | raw[1]);
  data->accel_y_raw = (int16_t)((raw[2] << 8) | raw[3]);
  data->accel_z_raw = (int16_t)((raw[4] << 8) | raw[5]);
  data->temp_raw    = (int16_t)((raw[6] << 8) | raw[7]);
  data->gyro_x_raw  = (int16_t)((raw[8] << 8) | raw[9]);
  data->gyro_y_raw  = (int16_t)((raw[10] << 8) | raw[11]);
  data->gyro_z_raw  = (int16_t)((raw[12] << 8) | raw[13]);

  data->accel_x_g = (float)data->accel_x_raw / MPU6050_ACCEL_SENSITIVITY;
  data->accel_y_g = (float)data->accel_y_raw / MPU6050_ACCEL_SENSITIVITY;
  data->accel_z_g = (float)data->accel_z_raw / MPU6050_ACCEL_SENSITIVITY;
  data->temp_c    = ((float)data->temp_raw / 340.0f) + 36.53f;
  data->gyro_x_dps = (float)data->gyro_x_raw / MPU6050_GYRO_SENSITIVITY;
  data->gyro_y_dps = (float)data->gyro_y_raw / MPU6050_GYRO_SENSITIVITY;
  data->gyro_z_dps = (float)data->gyro_z_raw / MPU6050_GYRO_SENSITIVITY;

  return HAL_OK;
}
