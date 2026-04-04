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
    .state = ePOWER_STATE_UNINIT,
    .processCount = 0U,
    .isReady = false,
    .isLowPowerRequested = false,
};

bool powerInit(void)
{
    if (gPowerStatus.isReady) {
        return true;
    }

    gPowerStatus.state = ePOWER_STATE_READY;
    gPowerStatus.processCount = 0U;
    gPowerStatus.isReady = true;
    gPowerStatus.isLowPowerRequested = false;

    LOG_I(POWER_TAG, "Power service initialized");
    return true;
}

void powerProcess(void)
{
    if (!gPowerStatus.isReady) {
        return;
    }

    gPowerStatus.processCount++;
    gPowerStatus.state = gPowerStatus.isLowPowerRequested ? ePOWER_STATE_LOW_POWER : ePOWER_STATE_ACTIVE;
}

bool powerRequestLowPower(bool isEnabled)
{
    if (!gPowerStatus.isReady) {
        return false;
    }

    gPowerStatus.isLowPowerRequested = isEnabled;
    return true;
}

const stPowerStatus *powerGetStatus(void)
{
    return &gPowerStatus;
}

/**************************End of file********************************/
