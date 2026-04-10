/************************************************************************************
* @file     : lsm6.h
* @brief    : LSM6 family IMU module public interface.
* @details  : This module provides a small blocking interface for common LSM6
*             devices that share the register map used by LSM6DS3/LSM6DSL/
*             LSM6DSO class sensors.
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef LSM6_H
#define LSM6_H

#include <stdbool.h>
#include <stdint.h>

#include "drviic.h"
#include "drvanlogiic.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eLsm6DevMap {
    LSM6_DEV0 = 0,
    LSM6_DEV_MAX,
} eLsm6MapType;

#define LSM6_IIC_ADDRESS_LOW              0x6AU
#define LSM6_IIC_ADDRESS_HIGH             0x6BU

#define LSM6_WHO_AM_I_LSM6DS3             0x69U
#define LSM6_WHO_AM_I_LSM6DSL             0x6AU
#define LSM6_WHO_AM_I_LSM6DSM             0x6BU
#define LSM6_WHO_AM_I_LSM6DSO             0x6CU

#define LSM6_REG_WHO_AM_I                 0x0FU
#define LSM6_REG_CTRL1_XL                 0x10U
#define LSM6_REG_CTRL2_G                  0x11U
#define LSM6_REG_CTRL3_C                  0x12U
#define LSM6_REG_STATUS                   0x1EU
#define LSM6_REG_OUT_TEMP_L               0x20U

#define LSM6_CTRL3_SW_RESET_BIT           0x01U
#define LSM6_CTRL3_IF_INC_BIT             0x04U
#define LSM6_CTRL3_BDU_BIT                0x40U

#define LSM6_SAMPLE_BYTES                 14U

#define LSM6_STATUS_OK                    DRV_STATUS_OK
#define LSM6_STATUS_INVALID_PARAM         DRV_STATUS_INVALID_PARAM
#define LSM6_STATUS_NOT_READY             DRV_STATUS_NOT_READY
#define LSM6_STATUS_BUSY                  DRV_STATUS_BUSY
#define LSM6_STATUS_TIMEOUT               DRV_STATUS_TIMEOUT
#define LSM6_STATUS_NACK                  DRV_STATUS_NACK
#define LSM6_STATUS_UNSUPPORTED           DRV_STATUS_UNSUPPORTED
#define LSM6_STATUS_DEVICE_ID_MISMATCH    DRV_STATUS_ID_NOTMATCH
#define LSM6_STATUS_ERROR                 DRV_STATUS_ERROR

typedef enum eLsm6AccelDataRate {
    LSM6_ACCEL_ODR_OFF = 0x00,
    LSM6_ACCEL_ODR_12P5HZ = 0x01,
    LSM6_ACCEL_ODR_26HZ = 0x02,
    LSM6_ACCEL_ODR_52HZ = 0x03,
    LSM6_ACCEL_ODR_104HZ = 0x04,
    LSM6_ACCEL_ODR_208HZ = 0x05,
    LSM6_ACCEL_ODR_416HZ = 0x06,
    LSM6_ACCEL_ODR_833HZ = 0x07,
    LSM6_ACCEL_ODR_1660HZ = 0x08,
} eLsm6AccelDataRate;

typedef enum eLsm6GyroDataRate {
    LSM6_GYRO_ODR_OFF = 0x00,
    LSM6_GYRO_ODR_12P5HZ = 0x01,
    LSM6_GYRO_ODR_26HZ = 0x02,
    LSM6_GYRO_ODR_52HZ = 0x03,
    LSM6_GYRO_ODR_104HZ = 0x04,
    LSM6_GYRO_ODR_208HZ = 0x05,
    LSM6_GYRO_ODR_416HZ = 0x06,
    LSM6_GYRO_ODR_833HZ = 0x07,
    LSM6_GYRO_ODR_1660HZ = 0x08,
} eLsm6GyroDataRate;

typedef enum eLsm6AccelRange {
    LSM6_ACCEL_RANGE_2G = 0x00,
    LSM6_ACCEL_RANGE_16G = 0x01,
    LSM6_ACCEL_RANGE_4G = 0x02,
    LSM6_ACCEL_RANGE_8G = 0x03,
} eLsm6AccelRange;

typedef enum eLsm6GyroRange {
    LSM6_GYRO_RANGE_250DPS = 0x00,
    LSM6_GYRO_RANGE_500DPS = 0x01,
    LSM6_GYRO_RANGE_1000DPS = 0x02,
    LSM6_GYRO_RANGE_2000DPS = 0x03,
} eLsm6GyroRange;

typedef struct stLsm6Cfg {
    uint8_t address;
    eLsm6AccelDataRate accelDataRate;
    eLsm6GyroDataRate gyroDataRate;
    eLsm6AccelRange accelRange;
    eLsm6GyroRange gyroRange;
    bool blockDataUpdate;
    bool autoIncrement;
} stLsm6Cfg;

typedef struct stLsm6RawSample {
    int16_t temperature;
    int16_t gyroX;
    int16_t gyroY;
    int16_t gyroZ;
    int16_t accelX;
    int16_t accelY;
    int16_t accelZ;
} stLsm6RawSample;

typedef struct stLsm6Dev {
    stLsm6Cfg cfg;
    stLsm6RawSample data;
    uint8_t lastWhoAmI;
    bool isReady;
} stLsm6Device;

eDrvStatus lsm6GetDefCfg(eLsm6MapType device, stLsm6Cfg *cfg);
eDrvStatus lsm6GetCfg(eLsm6MapType device, stLsm6Cfg *cfg);
eDrvStatus lsm6SetCfg(eLsm6MapType device, const stLsm6Cfg *cfg);
eDrvStatus lsm6Init(eLsm6MapType device);
bool lsm6IsReady(eLsm6MapType device);
eDrvStatus lsm6ReadId(eLsm6MapType device, uint8_t *devId);
eDrvStatus lsm6ReadReg(eLsm6MapType device, uint8_t regAddr, uint8_t *value);
eDrvStatus lsm6WriteReg(eLsm6MapType device, uint8_t regAddr, uint8_t value);
eDrvStatus lsm6ReadStatus(eLsm6MapType device, uint8_t *status);
eDrvStatus lsm6ReadRaw(eLsm6MapType device, stLsm6RawSample *sample);
eDrvStatus lsm6ReadTempCdC(eLsm6MapType device, int32_t *tempCdC);

#ifdef __cplusplus
}
#endif

#endif  // LSM6_H
/**************************End of file********************************/
