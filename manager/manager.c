/************************************************************************************
* @file     : manager.c
* @brief    : Service orchestration entry for the manager layer.
* @details  : Centralizes startup checks and delegates runtime service processing.
* @author   : GitHub Copilot
* @date     : 2026-04-04
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "manager.h"

#include "log.h"
#include "power/power.h"
#include "update/update.h"

static bool gManagerIsInitialized = false;

bool managerInit(void)
{
    if (gManagerIsInitialized) {
        return true;
    }

    if (!selfCheckInit()) {
        LOG_E(MANAGER_TAG, "Self-check manager init failed");
        return false;
    }

    gManagerIsInitialized = true;
    LOG_I(MANAGER_TAG, "Manager initialized");
    return true;
}

bool managerRunStartupSelfCheck(bool isConsoleReady, bool isAppCommReady)
{
    bool lPowerReady;
    bool lUpdateReady;

    if (!managerInit()) {
        return false;
    }

    selfCheckReset();
    selfCheckSetConsoleResult(isConsoleReady);
    selfCheckSetAppCommResult(isAppCommReady);

    lPowerReady = powerInit();
    selfCheckSetPowerResult(lPowerReady);

    lUpdateReady = updateInit();
    selfCheckSetUpdateResult(lUpdateReady);

    return selfCheckCommit();
}

void managerPowerProcess(void)
{
    if (!powerInit()) {
        return;
    }

    powerProcess();
}

void managerUpdateProcess(void)
{
    if (!updateInit()) {
        return;
    }

    updateProcess();
}

const stSelfCheckSummary *managerGetSelfCheckSummary(void)
{
    return selfCheckGetSummary();
}

/**************************End of file********************************/
