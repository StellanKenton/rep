/************************************************************************************
* @file     : drvuart_debug.h
* @brief    : DrvUart debug helpers.
* @details  : Exposes optional console registration for UART debug commands.
* @author   : GitHub Copilot
* @date     : 2026-04-01
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef DRVUART_DEBUG_H
#define DRVUART_DEBUG_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool drvUartDebugConsoleRegister(void);

#ifdef __cplusplus
}
#endif

#endif  // DRVUART_DEBUG_H
/**************************End of file********************************/
