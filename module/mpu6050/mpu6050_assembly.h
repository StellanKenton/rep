/************************************************************************************
* @file     : mpu6050_assembly.h
* @brief    : MPU6050 assembly-time contract shared by core and port.
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef MPU6050_ASSEMBLY_H
#define MPU6050_ASSEMBLY_H

#include <stdint.h>

#include "mpu6050.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eMpu6050TransportType {
    MPU6050_TRANSPORT_TYPE_NONE = 0,
    MPU6050_TRANSPORT_TYPE_SOFTWARE,
    MPU6050_TRANSPORT_TYPE_HARDWARE,
    MPU6050_TRANSPORT_TYPE_MAX,
} eMpu6050TransportType;

typedef eDrvStatus (*mpu6050IicInitFunc)(uint8_t bus);
typedef eDrvStatus (*mpu6050IicWriteRegFunc)(uint8_t bus, uint8_t address, const uint8_t *regBuf, uint16_t regLen, const uint8_t *buffer, uint16_t length);
typedef eDrvStatus (*mpu6050IicReadRegFunc)(uint8_t bus, uint8_t address, const uint8_t *regBuf, uint16_t regLen, uint8_t *buffer, uint16_t length);

typedef struct stMpu6050IicInterface {
    mpu6050IicInitFunc init;
    mpu6050IicWriteRegFunc writeReg;
    mpu6050IicReadRegFunc readReg;
} stMpu6050IicInterface;

typedef struct stMpu6050AssembleCfg {
    eMpu6050TransportType transportType;
    uint8_t linkId;
} stMpu6050AssembleCfg;

typedef struct stMpu6050Ops {
    void (*loadDefaultCfg)(eMPU6050MapType device, stMpu6050Cfg *cfg);
    const stMpu6050IicInterface *(*getIicInterface)(eMPU6050MapType device);
    bool (*isValidAssemble)(eMPU6050MapType device);
    uint8_t (*getLinkId)(eMPU6050MapType device);
    uint32_t (*getResetDelayMs)(void);
    uint32_t (*getWakeDelayMs)(void);
    void (*delayMs)(uint32_t delayMs);
} stMpu6050Ops;

const stMpu6050Ops *mpu6050PortGetOps(void);

#ifdef __cplusplus
}
#endif

#endif  // MPU6050_ASSEMBLY_H
/**************************End of file********************************/
