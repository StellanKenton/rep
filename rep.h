#ifndef REP_H
#define REP_H

#include <stddef.h>
#include <stdint.h>

typedef enum Rep_MCU_PLATFORM {
    REP_MCU_PLATFORM_GD32 = 1U,
    REP_MCU_PLATFORM_STM32 = 2U,
    REP_MCU_PLATFORM_ESP32 = 3U,
} eRepMcuPlatform;

typedef enum Rep_RtosSystem {
    REP_RTOS_NONE = 0U,
    REP_RTOS_FREERTOS = 1U,
    REP_RTOS_CUBEMX_FREERTOS = 2U,
    REP_RTOS_UCOSII = 3U,
    REP_RTOS_UCOSIII = 4U,
} eRepRtosSystem;

typedef enum Rep_LogLevel {
    REP_LOG_LEVEL_NONE = 0U,
    REP_LOG_LEVEL_ERROR = 1U,
    REP_LOG_LEVEL_WARN = 2U,
    REP_LOG_LEVEL_INFO = 3U,
    REP_LOG_LEVEL_DEBUG = 4U,
} eRepLogLevel;

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

#endif  // REP_H
/**************************End of file********************************/

