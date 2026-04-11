/************************************************************************************
* @file     : system.h
* @brief    : System mode management.
* @details  :
* @author   : \.rumi
* @date     : 2026-03-31
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef SYSTEM_H
#define SYSTEM_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SYSTEM_TAG "System"

typedef enum eSystemMode {
    eSYSTEM_INIT_MODE = 0,
    eSYSTEM_POWERUP_SELFCHECK_MODE,
    eSYSTEM_STANDBY_MODE,
    eSYSTEM_NORMAL_MODE,
    eSYSTEM_SELF_CHECK_MODE,
    eSYSTEM_UPDATE_MODE,
    eSYSTEM_DIAGNOSTIC_MODE,
    eSYSTEM_EOL_MODE,
    eSYSTEM_MODE_MAX,
} eSystemMode;

typedef eSystemMode System_Mode_EnumDef;

bool systemIsValidMode(eSystemMode mode);
eSystemMode systemGetMode(void);
void systemSetMode(eSystemMode mode);
const char *systemGetModeString(eSystemMode mode);
const char *systemGetFirmwareVersion(void);
const char *systemGetHardwareVersion(void);
#ifdef __cplusplus
}
#endif
#endif  // SYSTEM_H
/**************************End of file********************************/
