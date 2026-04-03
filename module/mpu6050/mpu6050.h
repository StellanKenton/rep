/************************************************************************************
* @file     : mpu6050.h
* @brief    : MPU6050 sensor driver built on the shared IIC drv layer.
* @details  : This module exposes a small blocking interface for basic device
*             initialization and raw sensor data acquisition through either
*             the software or hardware IIC drv implementation.
* @author   : GitHub Copilot
* @date     : 2026-04-01
* @version  : V1.0.0
***********************************************************************************/
#ifndef MPU6050_H
#define MPU6050_H

#include <stdbool.h>
#include <stdint.h>

#include "drviic.h"
#include "drvanlogiic.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef enum eMPU6050DevMap {
    MPU6050_DEV0 = 0,
    MPU6050_DEV1,
    MPU6050_DEV_MAX,
} eMPU6050MapType;

#define MPU6050_IIC_ADDRESS_LOW          0x68U
#define MPU6050_IIC_ADDRESS_HIGH         0x69U
#define MPU6050_WHO_AM_I_EXPECTED        0x68U
#define MPU6050_WHO_AM_I_COMPATIBLE_6500 0x70U

#define MPU6050_REG_SMPLRT_DIV           0x19U
#define MPU6050_REG_CONFIG               0x1AU
#define MPU6050_REG_GYRO_CONFIG          0x1BU
#define MPU6050_REG_ACCEL_CONFIG         0x1CU
#define MPU6050_REG_ACCEL_XOUT_H         0x3BU
#define MPU6050_REG_TEMP_OUT_H           0x41U
#define MPU6050_REG_PWR_MGMT_1           0x6BU
#define MPU6050_REG_PWR_MGMT_2           0x6CU
#define MPU6050_REG_WHO_AM_I             0x75U

#define MPU6050_PWR1_DEVICE_RESET_BIT    0x80U
#define MPU6050_PWR1_SLEEP_BIT           0x40U
#define MPU6050_PWR1_CLKSEL_PLL_XGYRO    0x01U

#define MPU6050_SAMPLE_BYTES             14U

#define MPU6050_STATUS_OK                 DRV_STATUS_OK
#define MPU6050_STATUS_INVALID_PARAM      DRV_STATUS_INVALID_PARAM
#define MPU6050_STATUS_NOT_READY          DRV_STATUS_NOT_READY
#define MPU6050_STATUS_BUSY               DRV_STATUS_BUSY
#define MPU6050_STATUS_TIMEOUT            DRV_STATUS_TIMEOUT
#define MPU6050_STATUS_NACK               DRV_STATUS_NACK
#define MPU6050_STATUS_UNSUPPORTED        DRV_STATUS_UNSUPPORTED
#define MPU6050_STATUS_DEVICE_ID_MISMATCH DRV_STATUS_ID_NOTMATCH
#define MPU6050_STATUS_ERROR              DRV_STATUS_ERROR

typedef enum eMpu6050AccelRange {
    MPU6050_ACCEL_RANGE_2G = 0,
    MPU6050_ACCEL_RANGE_4G,
    MPU6050_ACCEL_RANGE_8G,
    MPU6050_ACCEL_RANGE_16G,
    MPU6050_ACCEL_RANGE_MAX,
} eMpu6050AccelRange;

typedef enum eMpu6050GyroRange {
    MPU6050_GYRO_RANGE_250DPS = 0,
    MPU6050_GYRO_RANGE_500DPS,
    MPU6050_GYRO_RANGE_1000DPS,
    MPU6050_GYRO_RANGE_2000DPS,
    MPU6050_GYRO_RANGE_MAX,
} eMpu6050GyroRange;

typedef enum eMpu6050PortIicType {
    MPU6050_PORT_IIC_TYPE_NONE = 0,
    MPU6050_PORT_IIC_TYPE_SOFTWARE,
    MPU6050_PORT_IIC_TYPE_HARDWARE,
    MPU6050_PORT_IIC_TYPE_MAX,
} eMpu6050PortIicType;

typedef eDrvStatus (*mpu6050PortIicInitFunc)(uint8_t bus);
typedef eDrvStatus (*mpu6050PortIicWriteRegFunc)(uint8_t bus, uint8_t address, const uint8_t *regBuf, uint16_t regLen, const uint8_t *buffer, uint16_t length);
typedef eDrvStatus (*mpu6050PortIicReadRegFunc)(uint8_t bus, uint8_t address, const uint8_t *regBuf, uint16_t regLen, uint8_t *buffer, uint16_t length);

typedef struct stMpu6050PortIicInterface {
    mpu6050PortIicInitFunc init;
    mpu6050PortIicWriteRegFunc writeReg;
    mpu6050PortIicReadRegFunc readReg;
} stMpu6050PortIicInterface;

typedef struct stMpu6050PortIicBinding {
    eMpu6050PortIicType type;
    uint8_t bus;
    const stMpu6050PortIicInterface *iicIf;
} stMpu6050PortIicBinding;

typedef struct stMpu6050Cfg {
    stMpu6050PortIicBinding iicBind;
    uint8_t address;
    uint8_t sampleRateDiv;
    uint8_t dlpfCfg;
    eMpu6050AccelRange accelRange;
    eMpu6050GyroRange gyroRange;
} stMpu6050Cfg;

typedef struct stMpu6050RawSample {
    int16_t accelX;
    int16_t accelY;
    int16_t accelZ;
    int16_t temperature;
    int16_t gyroX;
    int16_t gyroY;
    int16_t gyroZ;
} stMpu6050RawSample;

typedef struct stMpu6050Dev {
    stMpu6050Cfg cfg;
    stMpu6050RawSample data;
    bool isReady;
} stMpu6050Device;

eDrvStatus mpu6050GetDefCfg(eMPU6050MapType device);
eDrvStatus mpu6050Init(eMPU6050MapType device);
bool mpu6050IsReady(eMPU6050MapType device);
eDrvStatus mpu6050ReadId(eMPU6050MapType device, uint8_t *devId);
eDrvStatus mpu6050ReadReg(eMPU6050MapType device, uint8_t regAddr, uint8_t *value);
eDrvStatus mpu6050WriteReg(eMPU6050MapType device, uint8_t regAddr, uint8_t value);
eDrvStatus mpu6050SetSleep(eMPU6050MapType device, bool enable);
eDrvStatus mpu6050ReadRaw(eMPU6050MapType device, stMpu6050RawSample *sample);
eDrvStatus mpu6050ReadTempCdC(eMPU6050MapType device, int32_t *tempCdC);

#ifdef __cplusplus
}
#endif

#endif  // MPU6050_H
/**************************End of file********************************/
