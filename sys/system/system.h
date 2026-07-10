/************************************************************************************
* @file     : system.h
* @brief    : Reusable system mode declarations.
* @details  : Exposes common system mode state and version string helpers.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_SYSTEM_H
#define REBUILDCPR_SYSTEM_H

#include <stdbool.h>
#include <stdint.h>

#include "rep_config.h"


#ifdef __cplusplus
extern "C" {
#endif

#define SYSTEM_STRINGIFY_IMPL(value)    #value
#define SYSTEM_STRINGIFY(value)         SYSTEM_STRINGIFY_IMPL(value)

#define FW_VER_MAJOR                    SYSTEM_FW_VER_MAJOR
#define FW_VER_MINOR                    SYSTEM_FW_VER_MINOR
#define FW_VER_PATCH                    SYSTEM_FW_VER_PATCH

#define HW_VER_MAJOR                    SYSTEM_HW_VER_MAJOR
#define HW_VER_MINOR                    SYSTEM_HW_VER_MINOR
#define HW_VER_PATCH                    SYSTEM_HW_VER_PATCH

#define FIRMWARE_NAME                   SYSTEM_FIRMWARE_NAME
#define FIRMWARE_VERSION                "SoftVer" SYSTEM_STRINGIFY(SYSTEM_FW_VER_MAJOR) "." SYSTEM_STRINGIFY(SYSTEM_FW_VER_MINOR) "." SYSTEM_STRINGIFY(SYSTEM_FW_VER_PATCH)
#define HARDWARE_VERSION                "HardVer" SYSTEM_STRINGIFY(SYSTEM_HW_VER_MAJOR) "." SYSTEM_STRINGIFY(SYSTEM_HW_VER_MINOR) "." SYSTEM_STRINGIFY(SYSTEM_HW_VER_PATCH)


typedef enum {
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

eSystemMode systemGetMode(void);
void systemSetMode(eSystemMode mode);
const char *systemGetModeString(eSystemMode mode);
const char *systemGetFirmwareName(void);
const char *systemGetFirmwareVersion(void);
const char *systemGetHardwareVersion(void);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/