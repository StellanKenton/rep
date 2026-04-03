/************************************************************************************
* @file     : mpu6050_debug.h
* @brief    : MPU6050 debug helpers.
* @details  : Exposes optional console registration for MPU6050 debug commands.
***********************************************************************************/
#ifndef MPU6050_DEBUG_H
#define MPU6050_DEBUG_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool mpu6050DebugConsoleRegister(void);

#ifdef __cplusplus
}
#endif

#endif  // MPU6050_DEBUG_H
/**************************End of file********************************/
