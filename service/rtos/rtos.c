/***********************************************************************************
* @file     : rtos.c
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "rtos.h"

#include <stddef.h>

#include "rtos_port.h"

static const stRepRtosOps *repRtosGetOps(void)
{
    return rtosPortGetOps();
}

eRepRtosSchedulerState repRtosGetSchedulerState(void)
{
    const stRepRtosOps *ops = repRtosGetOps();

    if ((ops == NULL) || (ops->getSchedulerState == NULL)) {
        return REP_RTOS_SCHEDULER_UNKNOWN;
    }

    return ops->getSchedulerState();
}

bool repRtosIsSchedulerRunning(void)
{
    return repRtosGetSchedulerState() == REP_RTOS_SCHEDULER_RUNNING;
}

eRepRtosStatus repRtosDelayMs(uint32_t delayMs)
{
    const stRepRtosOps *ops = repRtosGetOps();

    if ((ops == NULL) || (ops->delayMs == NULL)) {
        return REP_RTOS_STATUS_NOT_READY;
    }

    return ops->delayMs(delayMs);
}

uint32_t repRtosGetTickMs(void)
{
    const stRepRtosOps *ops = repRtosGetOps();

    if ((ops == NULL) || (ops->getTickMs == NULL)) {
        return 0U;
    }

    return ops->getTickMs();
}

void repRtosYield(void)
{
    const stRepRtosOps *ops = repRtosGetOps();

    if ((ops == NULL) || (ops->yield == NULL)) {
        return;
    }

    ops->yield();
}

void repRtosEnterCritical(void)
{
    const stRepRtosOps *ops = repRtosGetOps();

    if ((ops == NULL) || (ops->enterCritical == NULL)) {
        return;
    }

    ops->enterCritical();
}

void repRtosExitCritical(void)
{
    const stRepRtosOps *ops = repRtosGetOps();

    if ((ops == NULL) || (ops->exitCritical == NULL)) {
        return;
    }

    ops->exitCritical();
}

eRepRtosStatus repRtosMutexCreate(stRepRtosMutex *mutex)
{
    const stRepRtosOps *ops = repRtosGetOps();

    if ((ops == NULL) || (ops->mutexCreate == NULL)) {
        return REP_RTOS_STATUS_NOT_READY;
    }

    return ops->mutexCreate(mutex);
}

eRepRtosStatus repRtosMutexTake(stRepRtosMutex *mutex, uint32_t timeoutMs)
{
    const stRepRtosOps *ops = repRtosGetOps();

    if ((ops == NULL) || (ops->mutexTake == NULL)) {
        return REP_RTOS_STATUS_NOT_READY;
    }

    return ops->mutexTake(mutex, timeoutMs);
}

eRepRtosStatus repRtosMutexGive(stRepRtosMutex *mutex)
{
    const stRepRtosOps *ops = repRtosGetOps();

    if ((ops == NULL) || (ops->mutexGive == NULL)) {
        return REP_RTOS_STATUS_NOT_READY;
    }

    return ops->mutexGive(mutex);
}

eRepRtosStatus repRtosQueueCreate(stRepRtosQueue *queue, uint32_t itemSize, uint32_t capacity)
{
    const stRepRtosOps *ops = repRtosGetOps();

    if ((ops == NULL) || (ops->queueCreate == NULL)) {
        return REP_RTOS_STATUS_NOT_READY;
    }

    return ops->queueCreate(queue, itemSize, capacity);
}

eRepRtosStatus repRtosQueueSend(stRepRtosQueue *queue, const void *item, uint32_t timeoutMs)
{
    const stRepRtosOps *ops = repRtosGetOps();

    if ((ops == NULL) || (ops->queueSend == NULL)) {
        return REP_RTOS_STATUS_NOT_READY;
    }

    return ops->queueSend(queue, item, timeoutMs);
}

eRepRtosStatus repRtosQueueReceive(stRepRtosQueue *queue, void *item, uint32_t timeoutMs)
{
    const stRepRtosOps *ops = repRtosGetOps();

    if ((ops == NULL) || (ops->queueReceive == NULL)) {
        return REP_RTOS_STATUS_NOT_READY;
    }

    return ops->queueReceive(queue, item, timeoutMs);
}

eRepRtosStatus repRtosQueueReset(stRepRtosQueue *queue)
{
    const stRepRtosOps *ops = repRtosGetOps();

    if ((ops == NULL) || (ops->queueReset == NULL)) {
        return REP_RTOS_STATUS_NOT_READY;
    }

    return ops->queueReset(queue);
}

eRepRtosStatus repRtosTaskCreate(const stRepRtosTaskConfig *config)
{
    const stRepRtosOps *ops = repRtosGetOps();

    if ((ops == NULL) || (ops->taskCreate == NULL)) {
        return REP_RTOS_STATUS_NOT_READY;
    }

    return ops->taskCreate(config);
}

eRepRtosStatus repRtosTaskDelayUntilMs(uint32_t *lastWakeTimeMs, uint32_t periodMs)
{
    const stRepRtosOps *ops = repRtosGetOps();

    if ((ops == NULL) || (ops->taskDelayUntilMs == NULL)) {
        return REP_RTOS_STATUS_NOT_READY;
    }

    return ops->taskDelayUntilMs(lastWakeTimeMs, periodMs);
}

eRepRtosStatus repRtosStatsInit(void)
{
    const stRepRtosOps *ops = repRtosGetOps();

    if ((ops == NULL) || (ops->statsInit == NULL)) {
        return REP_RTOS_STATUS_NOT_READY;
    }

    return ops->statsInit();
}

const char *repRtosGetName(void)
{
    const char *name = rtosPortGetName();

    return (name == NULL) ? "unknown" : name;
}

uint32_t repRtosGetSystem(void)
{
    return rtosPortGetSystem();
}

/**************************End of file********************************/
