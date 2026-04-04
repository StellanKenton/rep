/***********************************************************************************
* @file     : drvgpio_port.c
* @brief    : GPIO port-layer BSP binding implementation.
* @details  : This file binds the generic GPIO driver interface to the board BSP.
* @author   : GitHub Copilot
* @date     : 2026-03-31
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "drvgpio.h"
#include "drvgpio_port.h"
#include "bspgpio.h"


stDrvGpioBspInterface gDrvGpioBspInterface = {
    .init = bspGpioInit,
    .write = bspGpioWrite,
    .read = bspGpioRead,
    .toggle = bspGpioToggle,
};

    const stDrvGpioBspInterface *drvGpioGetPlatformBspInterface(void)
    {
        return &gDrvGpioBspInterface;
    }

/**************************End of file********************************/

