/************************************************************************************
* @file     : drvadc_port.h
* @brief    : Shared ADC logical channel mapping definitions.
* @details  : This file keeps project-level ADC resource identifiers and default
*             timing/configuration independent from the public driver interface.
* @author   : GitHub Copilot
* @date     : 2026-04-03
* @version  : V1.0.0
***********************************************************************************/
#ifndef DRVADC_PORT_H
#define DRVADC_PORT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eDrvAdcPortMap {
    DRVADC_CH0 = 0,
    DRVADC_CH1 = 1,
    DRVADC_CH2 = 2,
    DRVADC_MAX,
} eDrvAdcPortMap;

#ifndef DRVADC_LOG_SUPPORT
#define DRVADC_LOG_SUPPORT                 1
#endif

#ifndef DRVADC_CONSOLE_SUPPORT
#define DRVADC_CONSOLE_SUPPORT             1
#endif

#define DRVADC_LOCK_WAIT_MS                5U
#define DRVADC_DEFAULT_TIMEOUT_MS          10U
#define DRVADC_DEFAULT_RESOLUTION_BITS     12U
#define DRVADC_DEFAULT_REFERENCE_MV        3300U

typedef struct stDrvAdcData {
    uint16_t raw;
    uint16_t mv;
} stDrvAdcData;

#ifdef __cplusplus
}
#endif

#endif  // DRVADC_PORT_H
/**************************End of file********************************/
