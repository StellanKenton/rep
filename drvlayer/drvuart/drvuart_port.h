/************************************************************************************
* @file     : drvuart_portmap.h
* @brief    : Shared UART logical port mapping definitions.
* @details  : This file keeps the project-level logical UART identifiers independent
*             from the driver interface declaration.
* @author   : GitHub Copilot
* @date     : 2026-03-30
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef DRVUART_PORTMAP_H
#define DRVUART_PORTMAP_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DRVUART_LOG_SUPPORT
#define DRVUART_LOG_SUPPORT             1
#endif

#ifndef DRVUART_CONSOLE_SUPPORT
#define DRVUART_CONSOLE_SUPPORT         1
#endif

/* Keep legacy macro names as compatibility aliases for existing code. */
#ifndef DRVGPIO_LOG_SUPPORT
#define DRVGPIO_LOG_SUPPORT             DRVUART_LOG_SUPPORT
#endif

#ifndef DRVGPIO_CONSOLE_SUPPORT
#define DRVGPIO_CONSOLE_SUPPORT         DRVUART_CONSOLE_SUPPORT
#endif

#define DRVUART_RECVLEN_DEBUGUART    1024U

typedef enum eDrvUartPortMapTable {
    DRVUART_WIRELESS = 0,      // PC10 PC11
    DRVUART_MAX,
} eDrvUartPortMap;

#ifdef __cplusplus
}
#endif

#endif  // DRVUART_PORTMAP_H
/**************************End of file********************************/
