/************************************************************************************
* @file     : mpu6050_port.h
* @brief    : MPU6050 project port-layer definitions.
* @details  : This file keeps project-level bus mapping and underlying IIC
*             selection independent from the reusable MPU6050 core.
* @author   : GitHub Copilot
* @date     : 2026-04-01
* @version  : V1.0.0
***********************************************************************************/
#ifndef MPU6050_PORT_H
#define MPU6050_PORT_H

#include <stdbool.h>
#include <stdint.h>

#include "mpu6050.h"
#include "mpu6050_assembly.h"
#ifdef __cplusplus
extern "C" {
#endif

#ifndef MPU6050_CONSOLE_SUPPORT
#define MPU6050_CONSOLE_SUPPORT              1
#endif

#ifndef MPU6050_PORT_RESET_DELAY_MS
#define MPU6050_PORT_RESET_DELAY_MS            100U
#endif

#ifndef MPU6050_PORT_WAKE_DELAY_MS
#define MPU6050_PORT_WAKE_DELAY_MS             10U
#endif

typedef stMpu6050IicInterface stMpu6050PortIicInterface;
typedef stMpu6050AssembleCfg stMpu6050PortAssembleCfg;

eDrvStatus mpu6050PortGetDefAssembleCfg(eMPU6050MapType device, stMpu6050PortAssembleCfg *cfg);
eDrvStatus mpu6050PortGetAssembleCfg(eMPU6050MapType device, stMpu6050PortAssembleCfg *cfg);
eDrvStatus mpu6050PortSetAssembleCfg(eMPU6050MapType device, const stMpu6050PortAssembleCfg *cfg);
eDrvStatus mpu6050PortAssembleSoftIic(stMpu6050PortAssembleCfg *cfg, uint8_t iic);
eDrvStatus mpu6050PortAssembleHardIic(stMpu6050PortAssembleCfg *cfg, uint8_t iic);
bool mpu6050PortIsValidAssembleCfg(const stMpu6050PortAssembleCfg *cfg);
bool mpu6050PortHasValidIicIf(const stMpu6050PortAssembleCfg *cfg);
const stMpu6050PortIicInterface *mpu6050PortGetIicIf(const stMpu6050PortAssembleCfg *cfg);
void mpu6050PortDelayMs(uint32_t delayMs);

#ifdef __cplusplus
}
#endif

#endif  // MPU6050_PORT_H
/**************************End of file********************************/

