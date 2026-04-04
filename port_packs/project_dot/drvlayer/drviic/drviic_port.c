/***********************************************************************************
* @file     : drviic_port.c
* @brief    : Hardware IIC port-layer BSP binding implementation.
* @details  : This file owns the project-level hardware IIC map and must be
*             updated with board-specific controller hooks before the bus can be
*             used by upper modules.
* @author   : GitHub Copilot
* @date     : 2026-04-01
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "drviic.h"
#include "drviic_port.h"

#include <stddef.h>

#include "bsphardiic.h"

stDrvIicBspInterface gDrvIicBspInterface[DRVIIC_MAX] = {
    [DRVIIC_BUS0] = {
        .init = bspHardIicInit,
        .transfer = bspHardIicTransfer,
        .recoverBus = bspHardIicRecoverBus,
        .defaultTimeoutMs = DRVIIC_DEFAULT_TIMEOUT_MS,
    },
};

    const stDrvIicBspInterface *drvIicGetPlatformBspInterfaces(void)
    {
        return gDrvIicBspInterface;
    }

/**************************End of file********************************/
