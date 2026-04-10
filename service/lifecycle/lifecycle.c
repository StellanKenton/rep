/***********************************************************************************
* @file     : lifecycle.c
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "lifecycle.h"

#include <stddef.h>

static eServiceLifecycleError lifecycleNormalizeFaultError(eServiceLifecycleError error)
{
    return (error == eSERVICE_LIFECYCLE_ERROR_NONE) ? eSERVICE_LIFECYCLE_ERROR_INTERNAL : error;
}

void lifecycleReset(stServiceLifecycle *lifecycle, eServiceLifecycleClass classType)
{
    if (lifecycle == NULL) {
        return;
    }

    lifecycle->classType = classType;
    lifecycle->state = eSERVICE_LIFECYCLE_STATE_UNINIT;
    lifecycle->lastError = eSERVICE_LIFECYCLE_ERROR_NONE;
    lifecycle->initCount = 0U;
    lifecycle->startCount = 0U;
    lifecycle->stopCount = 0U;
    lifecycle->processCount = 0U;
    lifecycle->recoverCount = 0U;
    lifecycle->isReady = false;
    lifecycle->isStarted = false;
    lifecycle->hasFault = false;
}

bool lifecycleInit(stServiceLifecycle *lifecycle)
{
    if (lifecycle == NULL) {
        return false;
    }

    if (lifecycle->isReady) {
        lifecycle->lastError = eSERVICE_LIFECYCLE_ERROR_NONE;
        return true;
    }

    lifecycle->isReady = true;
    lifecycle->isStarted = false;
    lifecycle->hasFault = false;
    lifecycle->state = eSERVICE_LIFECYCLE_STATE_READY;
    lifecycle->lastError = eSERVICE_LIFECYCLE_ERROR_NONE;
    lifecycle->initCount++;
    return true;
}

bool lifecycleStart(stServiceLifecycle *lifecycle)
{
    if (lifecycle == NULL) {
        return false;
    }

    if (!lifecycle->isReady) {
        lifecycle->lastError = eSERVICE_LIFECYCLE_ERROR_NOT_READY;
        return false;
    }

    if (lifecycle->hasFault) {
        lifecycle->lastError = eSERVICE_LIFECYCLE_ERROR_FAULTED;
        return false;
    }

    if (lifecycle->isStarted) {
        lifecycle->lastError = eSERVICE_LIFECYCLE_ERROR_NONE;
        return true;
    }

    lifecycle->isStarted = true;
    lifecycle->state = eSERVICE_LIFECYCLE_STATE_RUNNING;
    lifecycle->lastError = eSERVICE_LIFECYCLE_ERROR_NONE;
    lifecycle->startCount++;
    return true;
}

bool lifecycleStop(stServiceLifecycle *lifecycle)
{
    if (lifecycle == NULL) {
        return false;
    }

    if (!lifecycle->isReady) {
        lifecycle->lastError = eSERVICE_LIFECYCLE_ERROR_NOT_READY;
        return false;
    }

    if (lifecycle->hasFault) {
        lifecycle->lastError = eSERVICE_LIFECYCLE_ERROR_FAULTED;
        return false;
    }

    lifecycle->isStarted = false;
    lifecycle->state = eSERVICE_LIFECYCLE_STATE_STOPPED;
    lifecycle->lastError = eSERVICE_LIFECYCLE_ERROR_NONE;
    lifecycle->stopCount++;
    return true;
}

bool lifecycleNoteProcess(stServiceLifecycle *lifecycle)
{
    if (lifecycle == NULL) {
        return false;
    }

    if (!lifecycle->isReady) {
        lifecycle->lastError = eSERVICE_LIFECYCLE_ERROR_NOT_READY;
        return false;
    }

    if (lifecycle->hasFault) {
        lifecycle->lastError = eSERVICE_LIFECYCLE_ERROR_FAULTED;
        return false;
    }

    if ((lifecycle->classType != eSERVICE_LIFECYCLE_CLASS_PASSIVE_MODULE) && !lifecycle->isStarted) {
        lifecycle->lastError = eSERVICE_LIFECYCLE_ERROR_NOT_STARTED;
        return false;
    }

    lifecycle->processCount++;
    lifecycle->lastError = eSERVICE_LIFECYCLE_ERROR_NONE;
    return true;
}

void lifecycleReportFault(stServiceLifecycle *lifecycle, eServiceLifecycleError error)
{
    if (lifecycle == NULL) {
        return;
    }

    lifecycle->hasFault = true;
    lifecycle->isStarted = false;
    lifecycle->isReady = true;
    lifecycle->state = eSERVICE_LIFECYCLE_STATE_FAULT;
    lifecycle->lastError = lifecycleNormalizeFaultError(error);
}

bool lifecycleRecover(stServiceLifecycle *lifecycle)
{
    if ((lifecycle == NULL) || (lifecycle->classType != eSERVICE_LIFECYCLE_CLASS_RECOVERABLE_SERVICE)) {
        return false;
    }

    if (!lifecycle->isReady) {
        lifecycle->lastError = eSERVICE_LIFECYCLE_ERROR_NOT_READY;
        return false;
    }

    lifecycle->hasFault = false;
    lifecycle->isStarted = false;
    lifecycle->state = eSERVICE_LIFECYCLE_STATE_READY;
    lifecycle->lastError = eSERVICE_LIFECYCLE_ERROR_NONE;
    lifecycle->recoverCount++;
    return true;
}

/**************************End of file********************************/
