/************************************************************************************
* @file     : power.c
* @brief    : Power service manager.
* @details  : Maintains lightweight power-service state for scheduled processing.
* @author   : GitHub Copilot
* @date     : 2026-04-04
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "power.h"

#include "log.h"

static stPowerStatus gPowerStatus = {
    .lifecycle = {
        .classType = eMANAGER_SERVICE_CLASS_ACTIVE_SERVICE,
        .state = eMANAGER_LIFECYCLE_STATE_UNINIT,
        .lastError = eMANAGER_LIFECYCLE_ERROR_NONE,
        .initCount = 0U,
        .startCount = 0U,
        .stopCount = 0U,
        .processCount = 0U,
        .recoverCount = 0U,
        .isReady = false,
        .isStarted = false,
        .hasFault = false,
    },
    .state = ePOWER_STATE_UNINIT,
    .isLowPowerRequested = false,
};

bool powerInit(void)
{
    if (managerLifecycleInit(&gPowerStatus.lifecycle) == false) {
        gPowerStatus.state = ePOWER_STATE_FAULT;
        return false;
    }

    if (gPowerStatus.state != ePOWER_STATE_UNINIT) {
        return true;
    }

    gPowerStatus.state = ePOWER_STATE_READY;
    gPowerStatus.isLowPowerRequested = false;

    LOG_I(POWER_TAG, "Power service initialized");
    return true;
}

bool powerStart(void)
{
    if (!powerInit()) {
        return false;
    }

    if (!managerLifecycleStart(&gPowerStatus.lifecycle)) {
        gPowerStatus.state = ePOWER_STATE_FAULT;
        return false;
    }

    gPowerStatus.state = gPowerStatus.isLowPowerRequested ? ePOWER_STATE_LOW_POWER : ePOWER_STATE_ACTIVE;
    return true;
}

void powerStop(void)
{
    if (!powerInit()) {
        return;
    }

    if (!managerLifecycleStop(&gPowerStatus.lifecycle)) {
        gPowerStatus.state = ePOWER_STATE_FAULT;
        return;
    }

    gPowerStatus.state = ePOWER_STATE_STOPPED;
}

void powerProcess(void)
{
    if (!managerLifecycleNoteProcess(&gPowerStatus.lifecycle)) {
        if (gPowerStatus.lifecycle.hasFault) {
            gPowerStatus.state = ePOWER_STATE_FAULT;
        }
        return;
    }

    gPowerStatus.state = gPowerStatus.isLowPowerRequested ? ePOWER_STATE_LOW_POWER : ePOWER_STATE_ACTIVE;
}

bool powerRequestLowPower(bool isEnabled)
{
    if (!gPowerStatus.lifecycle.isReady || gPowerStatus.lifecycle.hasFault) {
        return false;
    }

    gPowerStatus.isLowPowerRequested = isEnabled;
    if (gPowerStatus.lifecycle.isStarted) {
        gPowerStatus.state = isEnabled ? ePOWER_STATE_LOW_POWER : ePOWER_STATE_ACTIVE;
    }
    return true;
}

ePowerState powerGetState(void)
{
    return gPowerStatus.state;
}

eManagerLifecycleError powerGetLastError(void)
{
    return gPowerStatus.lifecycle.lastError;
}

const stPowerStatus *powerGetStatus(void)
{
    return &gPowerStatus;
}

/**************************End of file********************************/
