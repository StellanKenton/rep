/***********************************************************************************
* @file     : drvmcuflash_port.c
* @brief    : MCU flash port-layer BSP binding implementation.
* @details  : This file binds the generic MCU flash driver to the current GD32F4
*             FMC BSP hooks and provides the default writable flash area.
* @author   : GitHub Copilot
* @date     : 2026-04-02
* @version  : V1.0.0
**********************************************************************************/
#include "drvmcuflash.h"
#include "drvmcuflash_port.h"

#include <stddef.h>

#include "bspmcuflash.h"

static const stDrvMcuFlashAreaInfo gDrvMcuFlashAreas[DRVMCUFLASH_MAX] = {
    [DRVMCUFLASH_AREA_USER] = {
        .startAddress = DRVMCUFLASH_AREA_USER_START_ADDR,
        .size = DRVMCUFLASH_AREA_USER_SIZE,
    },
};

static stDrvMcuFlashBspInterface gDrvMcuFlashBspInterface = {
    .init = bspMcuFlashInit,
    .unlock = bspMcuFlashUnlock,
    .lock = bspMcuFlashLock,
    .eraseSector = bspMcuFlashEraseSector,
    .program = bspMcuFlashProgram,
    .getSectorInfo = bspMcuFlashGetSectorInfo,
};

const stDrvMcuFlashBspInterface *drvMcuFlashGetPlatformBspInterface(void)
{
    return &gDrvMcuFlashBspInterface;
}

eDrvStatus drvMcuFlashGetPlatformAreaInfo(eDrvMcuFlashAreaMap area, stDrvMcuFlashAreaInfo *info)
{
    if ((info == NULL) || (area >= DRVMCUFLASH_MAX)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if ((gDrvMcuFlashAreas[area].startAddress == 0U) || (gDrvMcuFlashAreas[area].size == 0U)) {
        return DRV_STATUS_NOT_READY;
    }

    *info = gDrvMcuFlashAreas[area];
    return DRV_STATUS_OK;
}

eDrvStatus drvMcuFlashPortGetAreaInfo(eDrvMcuFlashAreaMap area, stDrvMcuFlashAreaInfo *info)
{
    return drvMcuFlashGetPlatformAreaInfo(area, info);
}
/**************************End of file********************************/
