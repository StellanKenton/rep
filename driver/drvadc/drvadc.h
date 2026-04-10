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

#ifndef DRVADC_LOG_SUPPORT
#define DRVADC_LOG_SUPPORT                 1
#endif

#ifndef DRVADC_CONSOLE_SUPPORT
#define DRVADC_CONSOLE_SUPPORT             1
#endif

#ifndef DRVADC_MAX
#define DRVADC_MAX                         3U
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
} stDrvAdcData;

eDrvStatus drvAdcInit(uint8_t adc);
eDrvStatus drvAdcReadRaw(uint8_t adc, uint16_t *value);
eDrvStatus drvAdcReadRawTimeout(uint8_t adc, uint16_t *value, uint32_t timeoutMs);
eDrvStatus drvAdcReadMv(uint8_t adc, uint16_t *valueMv);
eDrvStatus drvAdcReadMvTimeout(uint8_t adc, uint16_t *valueMv, uint32_t timeoutMs);

const stDrvAdcBspInterface *drvAdcGetPlatformBspInterface(void);
stDrvAdcData *drvAdcGetPlatformData(void);

#ifdef __cplusplus
}
#endif

#endif  // DRVADC_H
/**************************End of file********************************/
