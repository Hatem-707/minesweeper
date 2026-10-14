#include "hw290.h"
#include <string.h>

#define HW290_I2C_TIMEOUT_MS     10U

/* HMC5883L Register Map */
#define HMC5883L_REG_CRA         0x00U
#define HMC5883L_REG_CRB         0x01U
#define HMC5883L_REG_MODE        0x02U
#define HMC5883L_REG_DATA_OUT    0x03U
#define HMC5883L_REG_STATUS      0x09U
#define HMC5883L_REG_IDA         0x0AU

/* QMC5883L Register Map */
#define QMC5883L_REG_DATA_OUT    0x00U
#define QMC5883L_REG_STATUS      0x06U
#define QMC5883L_REG_CTRL1       0x09U
#define QMC5883L_REG_CTRL2       0x0AU
#define QMC5883L_REG_FBR         0x0BU
#define QMC5883L_REG_CHIP_ID     0x0DU

uint8_t HW290_ImuWhoAmI = 0U;
uint8_t HW290_MagWhoAmI = 0U;
HW290_MagType_t HW290_MagType = HW290_MAG_NONE;

static HAL_StatusTypeDef HW290_WriteReg(I2C_HandleTypeDef *hi2c, uint16_t dev_addr,
                                        uint8_t reg, uint8_t value)
{
  return HAL_I2C_Mem_Write(hi2c, dev_addr, reg, I2C_MEMADD_SIZE_8BIT,
                            &value, 1U, HW290_I2C_TIMEOUT_MS);
}

HAL_StatusTypeDef HW290_InitMag(I2C_HandleTypeDef *hi2c)
{
  HAL_StatusTypeDef status;
  uint8_t id[3] = {0};

  /* 1. Probe for genuine HMC5883L at 7-bit 0x1E (8-bit 0x3C) */
  status = HAL_I2C_Mem_Read(hi2c, HW290_HMC5883L_ADDR, HMC5883L_REG_IDA,
                             I2C_MEMADD_SIZE_8BIT, id, 3U, HW290_I2C_TIMEOUT_MS);
  if (status == HAL_OK && id[0] == 0x48U) /* 'H' */
  {
    /* HMC5883L detected:
     * CRA: 8-average, 75 Hz ODR, normal measurement = 0x78
     * CRB: +/-1.3 Gauss range (1090 LSB/Gauss) = 0x20
     * Mode: Continuous measurement mode = 0x00
     */
    status = HW290_WriteReg(hi2c, HW290_HMC5883L_ADDR, HMC5883L_REG_CRA, 0x78U);
    if (status != HAL_OK)
    {
      return status;
    }

    status = HW290_WriteReg(hi2c, HW290_HMC5883L_ADDR, HMC5883L_REG_CRB, 0x20U);
    if (status != HAL_OK)
    {
      return status;
    }

    status = HW290_WriteReg(hi2c, HW290_HMC5883L_ADDR, HMC5883L_REG_MODE, 0x00U);
    if (status != HAL_OK)
    {
      return status;
    }

    HW290_MagType = HW290_MAG_HMC5883L;
    HW290_MagWhoAmI = id[0]; /* 0x48 */
    return HAL_OK;
  }

  /* 2. Probe for QMC5883L clone at 7-bit 0x0D (8-bit 0x1A) */
  uint8_t qmc_id = 0U;
  status = HAL_I2C_Mem_Read(hi2c, HW290_QMC5883L_ADDR, QMC5883L_REG_CHIP_ID,
                             I2C_MEMADD_SIZE_8BIT, &qmc_id, 1U, HW290_I2C_TIMEOUT_MS);
  if (status == HAL_OK)
  {
    /* QMC5883L detected:
     * CTRL2: Soft reset (0x80)
     */
    (void)HW290_WriteReg(hi2c, HW290_QMC5883L_ADDR, QMC5883L_REG_CTRL2, 0x80U);
    HAL_Delay(2U);

    /* FBR: Set/Reset period recommendation = 0x01 */
    status = HW290_WriteReg(hi2c, HW290_QMC5883L_ADDR, QMC5883L_REG_FBR, 0x01U);
    if (status != HAL_OK)
    {
      return status;
    }

    /* CTRL1: OSR=512, RNG=+/-8G, ODR=100Hz, Mode=Continuous = 0x19 */
    status = HW290_WriteReg(hi2c, HW290_QMC5883L_ADDR, QMC5883L_REG_CTRL1, 0x19U);
    if (status != HAL_OK)
    {
      return status;
    }

    HW290_MagType = HW290_MAG_QMC5883L;
    HW290_MagWhoAmI = (qmc_id != 0U) ? qmc_id : 0xFFU;
    return HAL_OK;
  }

  /* 3. Neither responded */
  HW290_MagType = HW290_MAG_NONE;
  HW290_MagWhoAmI = 0x00U;
  return HAL_ERROR;
}

HAL_StatusTypeDef HW290_Init(I2C_HandleTypeDef *hi2c)
{
  HAL_StatusTypeDef status;

  /* Initialize MPU-6050 (wakes device, sets DLPF, enables aux I2C bypass) */
  status = MPU6050_Init(hi2c, HW290_MPU6050_ADDR_DEFAULT);
  if (status != HAL_OK)
  {
    HW290_ImuWhoAmI = MPU6050_LastWhoAmI;
    HW290_MagType = HW290_MAG_NONE;
    HW290_MagWhoAmI = 0x00U;
    return status;
  }
  HW290_ImuWhoAmI = MPU6050_LastWhoAmI;

  /* Initialize the onboard magnetometer (auto-detects HMC5883L vs QMC5883L) */
  (void)HW290_InitMag(hi2c);

  /* Return HAL_OK: MPU6050 is initialized; magnetometer status is tracked independently */
  return HAL_OK;
}

HAL_StatusTypeDef HW290_ReadAll(I2C_HandleTypeDef *hi2c, HW290_Data_t *data,
                                uint8_t *imu_status_out, uint8_t *mag_status_out)
{
  MPU6050_Data_t mpu_data;
  HAL_StatusTypeDef imu_res;

  /* 1. Read MPU6050 (Accel, Gyro, Temp) */
  imu_res = MPU6050_ReadAll(hi2c, HW290_MPU6050_ADDR_DEFAULT, &mpu_data);
  if (imu_res == HAL_OK)
  {
    if ((mpu_data.accel_x_raw | mpu_data.accel_y_raw | mpu_data.accel_z_raw |
         mpu_data.gyro_x_raw | mpu_data.gyro_y_raw | mpu_data.gyro_z_raw) == 0)
    {
      *imu_status_out = 4U; /* All-zero: sensor sleeping or in reset */
    }
    else
    {
      *imu_status_out = 0U; /* OK */
    }
  }
  else
  {
    *imu_status_out = (uint8_t)(1U + imu_res);
  }

  data->accel_x_raw = mpu_data.accel_x_raw;
  data->accel_y_raw = mpu_data.accel_y_raw;
  data->accel_z_raw = mpu_data.accel_z_raw;
  data->temp_raw    = mpu_data.temp_raw;
  data->gyro_x_raw  = mpu_data.gyro_x_raw;
  data->gyro_y_raw  = mpu_data.gyro_y_raw;
  data->gyro_z_raw  = mpu_data.gyro_z_raw;

  data->accel_x_g  = mpu_data.accel_x_g;
  data->accel_y_g  = mpu_data.accel_y_g;
  data->accel_z_g  = mpu_data.accel_z_g;
  data->temp_c     = mpu_data.temp_c;
  data->gyro_x_dps = mpu_data.gyro_x_dps;
  data->gyro_y_dps = mpu_data.gyro_y_dps;
  data->gyro_z_dps = mpu_data.gyro_z_dps;

  /* 2. Read Magnetometer based on detected chip type */
  if (HW290_MagType == HW290_MAG_HMC5883L)
  {
    uint8_t raw[6];
    HAL_StatusTypeDef mag_res = HAL_I2C_Mem_Read(hi2c, HW290_HMC5883L_ADDR,
                                                  HMC5883L_REG_DATA_OUT,
                                                  I2C_MEMADD_SIZE_8BIT,
                                                  raw, 6U, HW290_I2C_TIMEOUT_MS);
    if (mag_res == HAL_OK)
    {
      /* HMC5883L register order: X MSB, X LSB, Z MSB, Z LSB, Y MSB, Y LSB */
      data->mag_x_raw = (int16_t)((raw[0] << 8) | raw[1]);
      data->mag_z_raw = (int16_t)((raw[2] << 8) | raw[3]);
      data->mag_y_raw = (int16_t)((raw[4] << 8) | raw[5]);

      /* 1090 LSB/Gauss -> 1 Gauss = 100 uT -> 10.90 LSB/uT */
      data->mag_x_ut = (float)data->mag_x_raw / 10.90f;
      data->mag_y_ut = (float)data->mag_y_raw / 10.90f;
      data->mag_z_ut = (float)data->mag_z_raw / 10.90f;
      *mag_status_out = 0U; /* OK */
    }
    else
    {
      data->mag_x_raw = 0;
      data->mag_y_raw = 0;
      data->mag_z_raw = 0;
      data->mag_x_ut = 0.0f;
      data->mag_y_ut = 0.0f;
      data->mag_z_ut = 0.0f;
      *mag_status_out = (uint8_t)(1U + mag_res);
    }
  }
  else if (HW290_MagType == HW290_MAG_QMC5883L)
  {
    uint8_t raw[6];
    HAL_StatusTypeDef mag_res = HAL_I2C_Mem_Read(hi2c, HW290_QMC5883L_ADDR,
                                                  QMC5883L_REG_DATA_OUT,
                                                  I2C_MEMADD_SIZE_8BIT,
                                                  raw, 6U, HW290_I2C_TIMEOUT_MS);
    if (mag_res == HAL_OK)
    {
      /* QMC5883L register order: X LSB, X MSB, Y LSB, Y MSB, Z LSB, Z MSB (little-endian) */
      data->mag_x_raw = (int16_t)((raw[1] << 8) | raw[0]);
      data->mag_y_raw = (int16_t)((raw[3] << 8) | raw[2]);
      data->mag_z_raw = (int16_t)((raw[5] << 8) | raw[4]);

      /* 3000 LSB/Gauss (+/-8G range) -> 1 Gauss = 100 uT -> 30.0 LSB/uT */
      data->mag_x_ut = (float)data->mag_x_raw / 30.0f;
      data->mag_y_ut = (float)data->mag_y_raw / 30.0f;
      data->mag_z_ut = (float)data->mag_z_raw / 30.0f;
      *mag_status_out = 0U; /* OK */
    }
    else
    {
      data->mag_x_raw = 0;
      data->mag_y_raw = 0;
      data->mag_z_raw = 0;
      data->mag_x_ut = 0.0f;
      data->mag_y_ut = 0.0f;
      data->mag_z_ut = 0.0f;
      *mag_status_out = (uint8_t)(1U + mag_res);
    }
  }
  else
  {
    /* No magnetometer detected */
    data->mag_x_raw = 0;
    data->mag_y_raw = 0;
    data->mag_z_raw = 0;
    data->mag_x_ut = 0.0f;
    data->mag_y_ut = 0.0f;
    data->mag_z_ut = 0.0f;
    *mag_status_out = 4U; /* Offline/Not detected */
  }

  return imu_res;
}
