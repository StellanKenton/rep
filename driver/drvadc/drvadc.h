/************************************************************************************
* @file     : drvadc.h
* @brief    : Reusable ADC driver abstraction.
* @details  : This module exposes stable single-channel ADC read interfaces while
*             hiding board-specific controller details behind BSP hooks.
* @author   : GitHub Copilot
* @date     : 2026-04-03
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef DRVADC_H
#define DRVADC_H

#include <stdint.h>

#include "rep_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DRVADC_LOCK_WAIT_MS
#define DRVADC_LOCK_WAIT_MS                5U
#endif

#ifndef DRVADC_DEFAULT_TIMEOUT_MS
#define DRVADC_DEFAULT_TIMEOUT_MS          10U
#endif

#ifndef DRVADC_DEFAULT_RESOLUTION_BITS
#define DRVADC_DEFAULT_RESOLUTION_BITS     12U
#endif

#ifndef DRVADC_DEFAULT_REFERENCE_MV
#define DRVADC_DEFAULT_REFERENCE_MV        3300U
#endif

#ifndef DRVADC_BACKGROUND_PERIOD_MS
#define DRVADC_BACKGROUND_PERIOD_MS        20U
#endif

#ifndef DRVADC_FILTER_WINDOW_SIZE
#define DRVADC_FILTER_WINDOW_SIZE          4U
#endif

typedef eDrvStatus (*drvAdcBspInitFunc)(uint8_t adc);
typedef eDrvStatus (*drvAdcBspReadRawFunc)(uint8_t adc, uint16_t *value, uint32_t timeoutMs);

typedef struct stDrvAdcBspInterface {
    drvAdcBspInitFunc init;
    drvAdcBspReadRawFunc readRaw;
    uint32_t defaultTimeoutMs;
    uint16_t referenceMv;
    uint8_t resolutionBits;
} stDrvAdcBspInterface;

typedef struct stDrvAdcData {
    uint16_t raw;
    uint16_t mv;
    uint16_t rawFiltered;
    uint16_t mvFiltered;
} stDrvAdcData;

typedef struct stDrvAdcOps {
    const stDrvAdcBspInterface *(*getBspInterface)(void);
    stDrvAdcData *(*getData)(void);
} stDrvAdcOps;

eDrvStatus drvAdcInit(uint8_t adc);
eDrvStatus drvAdcReadRaw(uint8_t adc, uint16_t *value);
eDrvStatus drvAdcReadRawTimeout(uint8_t adc, uint16_t *value, uint32_t timeoutMs);
eDrvStatus drvAdcReadMv(uint8_t adc, uint16_t *valueMv);
eDrvStatus drvAdcReadMvTimeout(uint8_t adc, uint16_t *valueMv, uint32_t timeoutMs);
void drvAdcBackground(void);

#ifdef __cplusplus
}
#endif

#endif  // DRVADC_H
/**************************End of file********************************/
