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

#define SYSTEM_STRINGIFY_IMPL(value)    #value
#define SYSTEM_STRINGIFY(value)         SYSTEM_STRINGIFY_IMPL(value)

#define FW_VER_MAJOR                    0
#define FW_VER_MINOR                    0
#define FW_VER_PATCH                    1

#define HW_VER_MAJOR                    0
#define HW_VER_MINOR                    0
#define HW_VER_PATCH                    1

#define FIRMWARE_VERSION                "SoftVer" SYSTEM_STRINGIFY(FW_VER_MAJOR) "." SYSTEM_STRINGIFY(FW_VER_MINOR) "." SYSTEM_STRINGIFY(FW_VER_PATCH)
#define HARDWARE_VERSION                "HardVer" SYSTEM_STRINGIFY(HW_VER_MAJOR) "." SYSTEM_STRINGIFY(HW_VER_MINOR) "." SYSTEM_STRINGIFY(HW_VER_PATCH)

typedef enum eSystemMode {
    eSYSTEM_INIT_MODE = 0,
    eSYSTEM_SELF_CHECK_MODE,
    eSYSTEM_STANDBY_MODE,
    eSYSTEM_NORMAL_MODE,
    eSYSTEM_UPDATE_MODE,
    eSYSTEM_DIAGNOSTIC_MODE,
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
