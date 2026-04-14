#ifndef REP_H
#define REP_H

#include <stddef.h>
#include <stdint.h>

#ifndef REP_STM32_F0
#define REP_STM32_F0 1U
#endif
#ifndef REP_STM32_F1
#define REP_STM32_F1 2U
#endif
#ifndef REP_STM32_F4
#define REP_STM32_F4 3U
#endif
#ifndef REP_STM32_F7
#define REP_STM32_F7 4U
#endif
#ifndef REP_STM32_H7
#define REP_STM32_H7 5U
#endif
#ifndef REP_STM32_G4
#define REP_STM32_G4 6U
#endif

#ifndef REP_GD32F1
#define REP_GD32F1 1U
#endif
#ifndef REP_GD32F3
#define REP_GD32F3 2U
#endif
#ifndef REP_GD32F4
#define REP_GD32F4 3U
#endif
#ifndef REP_GD32F7
#define REP_GD32F7 4U
#endif

#ifndef REP_ESP32
#define REP_ESP32 1U
#endif
#ifndef REP_ESP32C5
#define REP_ESP32C5 2U
#endif
#ifndef REP_ESP32S3
#define REP_ESP32S3 3U
#endif

#ifndef REP_MCU_PLATFORM_GD32
#define REP_MCU_PLATFORM_GD32 1U
#endif
#ifndef REP_MCU_PLATFORM_STM32
#define REP_MCU_PLATFORM_STM32 2U
#endif
#ifndef REP_MCU_PLATFORM_ESP32
#define REP_MCU_PLATFORM_ESP32 3U
#endif

#ifndef REP_RTOS_NONE
#define REP_RTOS_NONE 0U
#endif
#ifndef REP_RTOS_FREERTOS
#define REP_RTOS_FREERTOS 1U
#endif
#ifndef REP_RTOS_CUBEMX_FREERTOS
#define REP_RTOS_CUBEMX_FREERTOS 2U
#endif
#ifndef REP_RTOS_UCOSII
#define REP_RTOS_UCOSII 3U
#endif
#ifndef REP_RTOS_UCOSIII
#define REP_RTOS_UCOSIII 4U
#endif

#ifndef REP_LOG_LEVEL_NONE
#define REP_LOG_LEVEL_NONE 0U
#endif
#ifndef REP_LOG_LEVEL_ERROR
#define REP_LOG_LEVEL_ERROR 1U
#endif
#ifndef REP_LOG_LEVEL_WARN
#define REP_LOG_LEVEL_WARN 2U
#endif
#ifndef REP_LOG_LEVEL_INFO
#define REP_LOG_LEVEL_INFO 3U
#endif
#ifndef REP_LOG_LEVEL_DEBUG
#define REP_LOG_LEVEL_DEBUG 4U
#endif

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

