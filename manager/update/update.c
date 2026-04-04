/************************************************************************************
* @file     : update.c
* @brief    : Update service manager.
* @details  : Maintains a minimal update-service state machine for system orchestration.
* @author   : GitHub Copilot
* @date     : 2026-04-04
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "update.h"

#include "log.h"

static stUpdateStatus gUpdateStatus = {
    .state = eUPDATE_STATE_UNINIT,
    .processCount = 0U,
    .isReady = false,
    .isUpdateRequested = false,
};

bool updateInit(void)
{
    if (gUpdateStatus.isReady) {
        return true;
    }

    gUpdateStatus.state = eUPDATE_STATE_IDLE;
    gUpdateStatus.processCount = 0U;
    gUpdateStatus.isReady = true;
    gUpdateStatus.isUpdateRequested = false;

    LOG_I(UPDATE_TAG, "Update service initialized");
    return true;
}

void updateProcess(void)
{
    if (!gUpdateStatus.isReady) {
        return;
    }

    gUpdateStatus.processCount++;
    if (gUpdateStatus.isUpdateRequested) {
        gUpdateStatus.state = eUPDATE_STATE_ACTIVE;
    } else {
        gUpdateStatus.state = eUPDATE_STATE_IDLE;
    }
}

bool updateRequestStart(void)
{
    if (!gUpdateStatus.isReady) {
        return false;
    }

    gUpdateStatus.isUpdateRequested = true;
    gUpdateStatus.state = eUPDATE_STATE_PENDING;
    return true;
}

bool updateRequestCancel(void)
{
    if (!gUpdateStatus.isReady) {
        return false;
    }

    gUpdateStatus.isUpdateRequested = false;
    gUpdateStatus.state = eUPDATE_STATE_IDLE;
    return true;
}

const stUpdateStatus *updateGetStatus(void)
{
    return &gUpdateStatus;
}

/**************************End of file********************************/
