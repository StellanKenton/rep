/***********************************************************************************
* @file     : system.c
* @brief    : System mode management.
* @details  :
* @author   : \.rumi
* @date     : 2026-03-31
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "system.h"

#include "log.h"

static eSystemMode gSystemMode = eSYSTEM_INIT_MODE;

/**
* @brief : Check whether the provided system mode is valid.
* @param : mode - target system mode.
* @return: true when the mode is within the supported range.
**/
bool systemIsValidMode(eSystemMode mode)
{
    return mode < eSYSTEM_MODE_MAX;
}

/**
* @brief : Get current system mode.
* @param : None
* @return: Current system mode.
**/
eSystemMode systemGetMode(void)
{
    return gSystemMode;
}

/**
* @brief : Set system mode.
* @param : mode - target system mode.
* @return: None
**/
void systemSetMode(eSystemMode mode)
{
    if (!systemIsValidMode(mode)) {
        LOG_W(SYSTEM_TAG, "Reject invalid system mode: %d", (int)mode);
        return;
    }

    if (gSystemMode == mode) {
        return;
    }

    gSystemMode = mode;
    LOG_I(SYSTEM_TAG, "System mode set to %s", systemGetModeString(mode));
}

/**
* @brief : Get string representation of system mode.
* @param : mode - system mode enum value.
* @return: String representation of the system mode.
**/
const char *systemGetModeString(eSystemMode mode)
{
    switch (mode) {
        case eSYSTEM_INIT_MODE:
            return "INIT_MODE";
        case eSYSTEM_SELF_CHECK_MODE:
            return "SELF_CHECK_MODE";
        case eSYSTEM_STANDBY_MODE:
            return "STANDBY_MODE";
        case eSYSTEM_NORMAL_MODE:
            return "NORMAL_MODE";
        case eSYSTEM_UPDATE_MODE:
            return "UPDATE_MODE";
        case eSYSTEM_DIAGNOSTIC_MODE:
            return "DIAGNOSTIC_MODE";
        default:
            return "UNKNOWN_MODE";
    }
}

/**
* @brief : Get firmware version string.
* @param : None
* @return: Firmware version string.
**/
const char *systemGetFirmwareVersion(void)
{
    return FIRMWARE_VERSION;
}

/**
* @brief : Get hardware version string.
* @param : None
* @return: Hardware version string.
**/
const char *systemGetHardwareVersion(void)
{
    return HARDWARE_VERSION;
}


/**************************End of file********************************/
