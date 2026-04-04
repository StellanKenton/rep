/************************************************************************************
* @file     : log_port.h
* @brief    : Log project port-layer declarations.
* @details  : Provides transport bindings that connect the reusable log core to
*             project-specific RTT and UART implementations.
***********************************************************************************/
#ifndef LOG_PORT_H
#define LOG_PORT_H

#include <stdint.h>

#include "log.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LOG_PORT_INTERFACE_COUNT
#define LOG_PORT_INTERFACE_COUNT         1U
#endif

#ifndef LOG_PORT_RTT_OUTPUT_ENABLE
#define LOG_PORT_RTT_OUTPUT_ENABLE       1
#endif

#ifndef LOG_PORT_RTT_INPUT_ENABLE
#define LOG_PORT_RTT_INPUT_ENABLE        1
#endif

#ifndef LOG_PORT_UART_OUTPUT_ENABLE
#define LOG_PORT_UART_OUTPUT_ENABLE      0
#endif

#ifndef LOG_PORT_UART_INPUT_ENABLE
#define LOG_PORT_UART_INPUT_ENABLE       0
#endif

const stLogInterface *logGetPlatformInterfaces(void);
uint32_t logGetPlatformInterfaceCount(void);

const stLogInterface *logPortGetInterfaces(void);
uint32_t logPortGetInterfaceCount(void);

#ifdef __cplusplus
}
#endif

#endif  // LOG_PORT_H
/**************************End of file********************************/
