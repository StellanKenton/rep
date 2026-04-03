/************************************************************************************
* @file     : rep_config.h
* @brief    : Global repository configuration.
* @details  : Stores the current MCU platform, RTOS type, and compiled log level.
* @author   : GitHub Copilot
* @date     : 2026-03-31
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REP_CONFIG_H
#define REP_CONFIG_H

#if defined(__has_include)
#if __has_include("sdkconfig.h")
#include "sdkconfig.h"
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif


#define REP_MCU_PLATFORM_ESP32        1
#define REP_MCU_PLATFORM_STM32        2
#define REP_MCU_PLATFORM_GD32         3

#define REP_RTOS_FREERTOS             1
#define REP_RTOS_UCOSII               2
#define REP_RTOS_UCOSIII              3
#define REP_RTOS_NONE                 4

#define REP_LOG_LEVEL_NONE            0
#define REP_LOG_LEVEL_ERROR           1
#define REP_LOG_LEVEL_WARN            2
#define REP_LOG_LEVEL_INFO            3
#define REP_LOG_LEVEL_DEBUG           4

typedef enum eDrvStatus {
	DRV_STATUS_OK = 0,
	DRV_STATUS_INVALID_PARAM,
	DRV_STATUS_NOT_READY,
	DRV_STATUS_BUSY,
	DRV_STATUS_TIMEOUT,
	DRV_STATUS_NACK,
	DRV_STATUS_UNSUPPORTED,
    DRV_STATUS_ID_NOTMATCH,
	DRV_STATUS_ERROR,
} eDrvStatus;

#define REP_LOG_OUTPUT_PORT           0x02U         // number of active log interfaces

#ifndef REP_LOG_LEVEL
#define REP_LOG_LEVEL                 REP_LOG_LEVEL_INFO
#endif

#ifndef REP_MCU_PLATFORM
#define REP_MCU_PLATFORM              REP_MCU_PLATFORM_GD32
#endif

#ifndef REP_RTOS_SYSTEM
#define REP_RTOS_SYSTEM               REP_RTOS_FREERTOS
#endif


#if (REP_MCU_PLATFORM == REP_MCU_PLATFORM_ESP32)
#define MCU_PLATFORM_ESP32
#endif
#if (REP_MCU_PLATFORM == REP_MCU_PLATFORM_STM32)
#define MCU_PLATFORM_STM32
#endif
#if (REP_MCU_PLATFORM == REP_MCU_PLATFORM_GD32)
#define MCU_PLATFORM_GD32
#endif


#ifdef __cplusplus
}
#endif

#endif  // REP_CONFIG_H
/**************************End of file********************************/

