/************************************************************************************
* @file     : rtos.h
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REP_SERVICE_RTOS_H
#define REP_SERVICE_RTOS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eRepRtosSchedulerState {
    REP_RTOS_SCHEDULER_UNKNOWN = 0,
    REP_RTOS_SCHEDULER_STOPPED,
    REP_RTOS_SCHEDULER_READY,
    REP_RTOS_SCHEDULER_RUNNING,
    REP_RTOS_SCHEDULER_LOCKED,
    REP_RTOS_SCHEDULER_SUSPENDED,
} eRepRtosSchedulerState;

typedef enum eRepRtosStatus {
    REP_RTOS_STATUS_OK = 0,
    REP_RTOS_STATUS_INVALID_PARAM,
    REP_RTOS_STATUS_NOT_READY,
    REP_RTOS_STATUS_BUSY,
    REP_RTOS_STATUS_UNSUPPORTED,
    REP_RTOS_STATUS_TIMEOUT,
    REP_RTOS_STATUS_ERROR,
} eRepRtosStatus;

#define REP_RTOS_WAIT_FOREVER 0xFFFFFFFFUL

typedef void (*repRtosTaskEntry)(void *argument);
typedef void *repRtosTaskHandle;
typedef uintptr_t repRtosStackType;

typedef struct stRepRtosMutex {
    void *nativeHandle;
    volatile bool isCreated;
    volatile bool isLocked;
} stRepRtosMutex;

typedef struct stRepRtosQueue {
    void *nativeHandle;
    uint32_t itemSize;
    uint32_t capacity;
    volatile bool isCreated;
} stRepRtosQueue;

typedef struct stRepRtosTaskConfig {
    const char *name;
    repRtosTaskEntry entry;
    void *argument;
    repRtosStackType *stackBuffer;
    uint32_t stackSize;
    uint32_t priority;
    repRtosTaskHandle *handle;
} stRepRtosTaskConfig;

typedef struct stRepRtosOps {
    eRepRtosSchedulerState (*getSchedulerState)(void);
    eRepRtosStatus (*delayMs)(uint32_t delayMs);
    uint32_t (*getTickMs)(void);
    void (*yield)(void);
    void (*enterCritical)(void);
    void (*exitCritical)(void);
    eRepRtosStatus (*mutexCreate)(stRepRtosMutex *mutex);
    eRepRtosStatus (*mutexTake)(stRepRtosMutex *mutex, uint32_t timeoutMs);
    eRepRtosStatus (*mutexGive)(stRepRtosMutex *mutex);
    eRepRtosStatus (*queueCreate)(stRepRtosQueue *queue, uint32_t itemSize, uint32_t capacity);
    eRepRtosStatus (*queueSend)(stRepRtosQueue *queue, const void *item, uint32_t timeoutMs);
    eRepRtosStatus (*queueReceive)(stRepRtosQueue *queue, void *item, uint32_t timeoutMs);
    eRepRtosStatus (*queueReset)(stRepRtosQueue *queue);
    eRepRtosStatus (*taskCreate)(const stRepRtosTaskConfig *config);
    eRepRtosStatus (*taskDelayUntilMs)(uint32_t *lastWakeTimeMs, uint32_t periodMs);
    eRepRtosStatus (*statsInit)(void);
} stRepRtosOps;

eRepRtosSchedulerState repRtosGetSchedulerState(void);
bool repRtosIsSchedulerRunning(void);
eRepRtosStatus repRtosDelayMs(uint32_t delayMs);
uint32_t repRtosGetTickMs(void);
void repRtosYield(void);
void repRtosEnterCritical(void);
void repRtosExitCritical(void);
eRepRtosStatus repRtosMutexCreate(stRepRtosMutex *mutex);
eRepRtosStatus repRtosMutexTake(stRepRtosMutex *mutex, uint32_t timeoutMs);
eRepRtosStatus repRtosMutexGive(stRepRtosMutex *mutex);
eRepRtosStatus repRtosQueueCreate(stRepRtosQueue *queue, uint32_t itemSize, uint32_t capacity);
eRepRtosStatus repRtosQueueSend(stRepRtosQueue *queue, const void *item, uint32_t timeoutMs);
eRepRtosStatus repRtosQueueReceive(stRepRtosQueue *queue, void *item, uint32_t timeoutMs);
eRepRtosStatus repRtosQueueReset(stRepRtosQueue *queue);
eRepRtosStatus repRtosTaskCreate(const stRepRtosTaskConfig *config);
eRepRtosStatus repRtosTaskDelayUntilMs(uint32_t *lastWakeTimeMs, uint32_t periodMs);
eRepRtosStatus repRtosStatsInit(void);
const char *repRtosGetName(void);
uint32_t repRtosGetSystem(void);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
