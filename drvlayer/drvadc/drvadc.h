/************************************************************************************
* @file     : drvadc.h
* @brief    : Reusable ADC driver abstraction.
* @details  : This module exposes stable single-channel ADC read interfaces while
*             hiding board-specific controller details behind BSP hooks.
* @author   : GitHub Copilot
* @date     : 2026-04-03
* @version  : V1.0.0
***********************************************************************************/
#ifndef DRVADC_H
#define DRVADC_H

#include <stdint.h>

#include "rep_config.h"
#include "drvadc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef eDrvStatus (*drvAdcBspInitFunc)(eDrvAdcPortMap adc);
typedef eDrvStatus (*drvAdcBspReadRawFunc)(eDrvAdcPortMap adc, uint16_t *value, uint32_t timeoutMs);

typedef struct stDrvAdcBspInterface {
    drvAdcBspInitFunc init;
    drvAdcBspReadRawFunc readRaw;
    uint32_t defaultTimeoutMs;
    uint16_t referenceMv;
    uint8_t resolutionBits;
} stDrvAdcBspInterface;

eDrvStatus drvAdcInit(eDrvAdcPortMap adc);
eDrvStatus drvAdcReadRaw(eDrvAdcPortMap adc, uint16_t *value);
eDrvStatus drvAdcReadRawTimeout(eDrvAdcPortMap adc, uint16_t *value, uint32_t timeoutMs);
eDrvStatus drvAdcReadMv(eDrvAdcPortMap adc, uint16_t *valueMv);
eDrvStatus drvAdcReadMvTimeout(eDrvAdcPortMap adc, uint16_t *valueMv, uint32_t timeoutMs);

#ifdef __cplusplus
}
#endif

#endif  // DRVADC_H
/**************************End of file********************************/
