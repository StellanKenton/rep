/************************************************************************************
* @file     : drvgpio_port.h
* @brief    : Shared GPIO logical pin mapping definitions.
* @details  : This file keeps the project-level logical GPIO enumeration independent
*             from the driver interface declaration.
* @author   : GitHub Copilot
* @date     : 2026-03-30
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef DRVGPIO_PORT_H
#define DRVGPIO_PORT_H

#include "drvgpio_types.h"

#ifdef __cplusplus
extern "C" {
#endif


#ifndef DRVGPIO_LOG_SUPPORT
#define DRVGPIO_LOG_SUPPORT             1
#endif

#ifndef DRVGPIO_CONSOLE_SUPPORT
#define DRVGPIO_CONSOLE_SUPPORT         1
#endif

#ifdef __cplusplus
}
#endif

#endif  // DRVGPIO_PORT_H
/**************************End of file********************************/
