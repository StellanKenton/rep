/************************************************************************************
* @file     : drvgpio_debug.h
* @brief    : DrvGpio debug helpers.
* @details  : Exposes optional console registration for GPIO debug commands.
* @author   : GitHub Copilot
* @date     : 2026-04-27
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef DRVGPIO_DEBUG_H
#define DRVGPIO_DEBUG_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool drvGpioDebugConsoleRegister(void);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/

