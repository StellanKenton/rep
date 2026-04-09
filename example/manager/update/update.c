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
    .lifecycle = {
        .classType = eMANAGER_SERVICE_CLASS_RECOVERABLE_SERVICE,
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
    .state = eUPDATE_STATE_UNINIT,
    .isUpdateRequested = false,
};

bool updateInit(void)
{
    if (managerLifecycleInit(&gUpdateStatus.lifecycle) == false) {
        gUpdateStatus.state = eUPDATE_STATE_FAULT;
        return false;
    }

    if (gUpdateStatus.state != eUPDATE_STATE_UNINIT) {
        return true;
    }

    gUpdateStatus.state = eUPDATE_STATE_IDLE;
    gUpdateStatus.isUpdateRequested = false;

    LOG_I(UPDATE_TAG, "Update service initialized");
    return true;
}

bool updateStart(void)
{
    if (!updateInit()) {
        return false;
    }

    if (!managerLifecycleStart(&gUpdateStatus.lifecycle)) {
        gUpdateStatus.state = eUPDATE_STATE_FAULT;
        return false;
    }

    gUpdateStatus.state = gUpdateStatus.isUpdateRequested ? eUPDATE_STATE_PENDING : eUPDATE_STATE_IDLE;
    return true;
}

void updateStop(void)
{
    if (!updateInit()) {
        return;
    }

    if (!managerLifecycleStop(&gUpdateStatus.lifecycle)) {
        gUpdateStatus.state = eUPDATE_STATE_FAULT;
        return;
    }

    gUpdateStatus.isUpdateRequested = false;
    gUpdateStatus.state = eUPDATE_STATE_STOPPED;
}

void updateProcess(void)
{
    if (!managerLifecycleNoteProcess(&gUpdateStatus.lifecycle)) {
        if (gUpdateStatus.lifecycle.hasFault) {
            gUpdateStatus.state = eUPDATE_STATE_FAULT;
        }
        return;
    }

    if (gUpdateStatus.isUpdateRequested) {
        gUpdateStatus.state = eUPDATE_STATE_ACTIVE;
    } else {
        gUpdateStatus.state = eUPDATE_STATE_IDLE;
    }
}

bool updateRequestStart(void)
{
    if (!gUpdateStatus.lifecycle.isReady || gUpdateStatus.lifecycle.hasFault) {
        return false;
    }

    gUpdateStatus.isUpdateRequested = true;
    gUpdateStatus.state = eUPDATE_STATE_PENDING;
    return true;
}

bool updateRequestCancel(void)
{
    if (!gUpdateStatus.lifecycle.isReady || gUpdateStatus.lifecycle.hasFault) {
        return false;
    }

    gUpdateStatus.isUpdateRequested = false;
    gUpdateStatus.state = eUPDATE_STATE_IDLE;
    return true;
}

void updateFault(eManagerLifecycleError error)
{
    managerLifecycleReportFault(&gUpdateStatus.lifecycle, error);
    gUpdateStatus.state = eUPDATE_STATE_FAULT;
}

bool updateRecover(void)
{
    if (!updateInit()) {
        return false;
    }

    if (!managerLifecycleRecover(&gUpdateStatus.lifecycle)) {
        return false;
    }

    gUpdateStatus.isUpdateRequested = false;
    gUpdateStatus.state = eUPDATE_STATE_IDLE;
    return true;
}

eUpdateState updateGetState(void)
{
    return gUpdateStatus.state;
}

eManagerLifecycleError updateGetLastError(void)
{
    return gUpdateStatus.lifecycle.lastError;
}

const stUpdateStatus *updateGetStatus(void)
{
    return &gUpdateStatus;
}

/**************************End of file********************************/
