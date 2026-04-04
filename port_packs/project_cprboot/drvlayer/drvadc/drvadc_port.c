/***********************************************************************************
* @file     : drvadc_port.c
* @brief    : ADC port-layer BSP binding implementation.
* @details  : This file owns the project-level ADC channel map and binds each
*             enabled logical channel to its BSP implementation.
* @author   : GitHub Copilot
* @date     : 2026-04-03
* @version  : V1.0.0
**********************************************************************************/
#include "drvadc.h"
#include "drvadc_port.h"

#include <stddef.h>

#include "bspadc.h"

stDrvAdcBspInterface gDrvAdcBspInterface = {
    .init = bspAdcInit,
    .readRaw = bspAdcReadRaw,
    .defaultTimeoutMs = DRVADC_DEFAULT_TIMEOUT_MS,
    .referenceMv = DRVADC_DEFAULT_REFERENCE_MV,
    .resolutionBits = DRVADC_DEFAULT_RESOLUTION_BITS,
};

stDrvAdcData gDrvAdcData[DRVADC_MAX] = {0};

    const stDrvAdcBspInterface *drvAdcGetPlatformBspInterface(void)
    {
        return &gDrvAdcBspInterface;
    }

    stDrvAdcData *drvAdcGetPlatformData(void)
    {
        return gDrvAdcData;
    }

/**************************End of file********************************/
