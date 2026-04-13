/************************************************************************************
* @file     : drvadc_debug.h
* @brief    : DrvAdc debug helpers.
* @details  : Exposes optional console registration for ADC debug commands.
* @author   : GitHub Copilot
* @date     : 2026-04-13
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef DRVADC_DEBUG_H
#define DRVADC_DEBUG_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool drvAdcDebugConsoleRegister(void);

#ifdef __cplusplus
}
#endif

#endif  // DRVADC_DEBUG_H
/**************************End of file********************************/
