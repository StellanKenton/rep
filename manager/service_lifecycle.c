/************************************************************************************
* @file     : service_lifecycle.c
* @brief    : Shared lifecycle helpers for manager-layer services.
* @details  : Keeps Init/Start/Process/Stop/Recover bookkeeping consistent across
*             manager services without introducing a registration framework.
* @author   : GitHub Copilot
* @date     : 2026-04-04
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "service_lifecycle.h"

static eManagerLifecycleError managerLifecycleNormalizeFaultError(eManagerLifecycleError error)
{
    return (error == eMANAGER_LIFECYCLE_ERROR_NONE) ? eMANAGER_LIFECYCLE_ERROR_INTERNAL : error;
}

void managerLifecycleReset(stManagerServiceLifecycle *lifecycle, eManagerServiceClass classType)
{
    if (lifecycle == NULL) {
        return;
    }

    lifecycle->classType = classType;
    lifecycle->state = eMANAGER_LIFECYCLE_STATE_UNINIT;
    lifecycle->lastError = eMANAGER_LIFECYCLE_ERROR_NONE;
    lifecycle->initCount = 0U;
    lifecycle->startCount = 0U;
    lifecycle->stopCount = 0U;
    lifecycle->processCount = 0U;
    lifecycle->recoverCount = 0U;
    lifecycle->isReady = false;
    lifecycle->isStarted = false;
    lifecycle->hasFault = false;
}

bool managerLifecycleInit(stManagerServiceLifecycle *lifecycle)
{
    if (lifecycle == NULL) {
        return false;
    }

    if (lifecycle->isReady) {
        lifecycle->lastError = eMANAGER_LIFECYCLE_ERROR_NONE;
        return true;
    }

    lifecycle->isReady = true;
    lifecycle->isStarted = false;
    lifecycle->hasFault = false;
    lifecycle->state = eMANAGER_LIFECYCLE_STATE_READY;
    lifecycle->lastError = eMANAGER_LIFECYCLE_ERROR_NONE;
    lifecycle->initCount++;
    return true;
}

bool managerLifecycleStart(stManagerServiceLifecycle *lifecycle)
{
    if (lifecycle == NULL) {
        return false;
    }

    if (!lifecycle->isReady) {
        lifecycle->lastError = eMANAGER_LIFECYCLE_ERROR_NOT_READY;
        return false;
    }

    if (lifecycle->hasFault) {
        lifecycle->lastError = eMANAGER_LIFECYCLE_ERROR_FAULTED;
        return false;
    }

    if (lifecycle->isStarted) {
        lifecycle->lastError = eMANAGER_LIFECYCLE_ERROR_NONE;
        return true;
    }

    lifecycle->isStarted = true;
    lifecycle->state = eMANAGER_LIFECYCLE_STATE_RUNNING;
    lifecycle->lastError = eMANAGER_LIFECYCLE_ERROR_NONE;
    lifecycle->startCount++;
    return true;
}

bool managerLifecycleStop(stManagerServiceLifecycle *lifecycle)
{
    if (lifecycle == NULL) {
        return false;
    }

    if (!lifecycle->isReady) {
        lifecycle->lastError = eMANAGER_LIFECYCLE_ERROR_NOT_READY;
        return false;
    }

    if (lifecycle->hasFault) {
        lifecycle->lastError = eMANAGER_LIFECYCLE_ERROR_FAULTED;
        return false;
    }

    if (!lifecycle->isStarted) {
        lifecycle->state = eMANAGER_LIFECYCLE_STATE_STOPPED;
        lifecycle->lastError = eMANAGER_LIFECYCLE_ERROR_NONE;
        return true;
    }

    lifecycle->isStarted = false;
    lifecycle->state = eMANAGER_LIFECYCLE_STATE_STOPPED;
    lifecycle->lastError = eMANAGER_LIFECYCLE_ERROR_NONE;
    lifecycle->stopCount++;
    return true;
}

bool managerLifecycleNoteProcess(stManagerServiceLifecycle *lifecycle)
{
    if (lifecycle == NULL) {
        return false;
    }

    if (!lifecycle->isReady) {
        lifecycle->lastError = eMANAGER_LIFECYCLE_ERROR_NOT_READY;
        return false;
    }

    if (lifecycle->hasFault) {
        lifecycle->lastError = eMANAGER_LIFECYCLE_ERROR_FAULTED;
        return false;
    }

    if ((lifecycle->classType != eMANAGER_SERVICE_CLASS_PASSIVE_MODULE) &&
        (lifecycle->isStarted == false)) {
        lifecycle->lastError = eMANAGER_LIFECYCLE_ERROR_NOT_STARTED;
        return false;
    }

    lifecycle->processCount++;
    lifecycle->lastError = eMANAGER_LIFECYCLE_ERROR_NONE;
    return true;
}

void managerLifecycleReportFault(stManagerServiceLifecycle *lifecycle, eManagerLifecycleError error)
{
    if (lifecycle == NULL) {
        return;
    }

    lifecycle->hasFault = true;
    lifecycle->isStarted = false;
    lifecycle->isReady = true;
    lifecycle->state = eMANAGER_LIFECYCLE_STATE_FAULT;
    lifecycle->lastError = managerLifecycleNormalizeFaultError(error);
}

bool managerLifecycleRecover(stManagerServiceLifecycle *lifecycle)
{
    if ((lifecycle == NULL) || (lifecycle->classType != eMANAGER_SERVICE_CLASS_RECOVERABLE_SERVICE)) {
        return false;
    }

    if (!lifecycle->isReady) {
        lifecycle->lastError = eMANAGER_LIFECYCLE_ERROR_NOT_READY;
        return false;
    }

    lifecycle->hasFault = false;
    lifecycle->isStarted = false;
    lifecycle->state = eMANAGER_LIFECYCLE_STATE_READY;
    lifecycle->lastError = eMANAGER_LIFECYCLE_ERROR_NONE;
    lifecycle->recoverCount++;
    return true;
}
/**************************End of file********************************/