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

void mpu6050PortGetDefBind(stMpu6050PortIicBinding *bind);
void mpu6050PortGetDefCfg(eMPU6050MapType device, stMpu6050Cfg *cfg);
eDrvStatus mpu6050PortSetSoftIic(stMpu6050PortIicBinding *bind, eDrvAnlogIicPortMap iic);
eDrvStatus mpu6050PortSetHardIic(stMpu6050PortIicBinding *bind, eDrvIicPortMap iic);
bool mpu6050PortIsValidBind(const stMpu6050PortIicBinding *bind);
bool mpu6050PortHasValidIicIf(const stMpu6050PortIicBinding *bind);
const stMpu6050PortIicInterface *mpu6050PortGetIicIf(const stMpu6050PortIicBinding *bind);
void mpu6050PortDelayMs(uint32_t delayMs);

#ifdef __cplusplus
}
#endif

#endif  // MPU6050_PORT_H
/**************************End of file********************************/

