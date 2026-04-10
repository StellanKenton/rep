#ifndef REP_SERVICE_LIFECYCLE_H
#define REP_SERVICE_LIFECYCLE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eServiceLifecycleClass {
    eSERVICE_LIFECYCLE_CLASS_PASSIVE_MODULE = 0,
    eSERVICE_LIFECYCLE_CLASS_ACTIVE_SERVICE,
    eSERVICE_LIFECYCLE_CLASS_RECOVERABLE_SERVICE,
} eServiceLifecycleClass;

typedef enum eServiceLifecycleState {
    eSERVICE_LIFECYCLE_STATE_UNINIT = 0,
    eSERVICE_LIFECYCLE_STATE_READY,
    eSERVICE_LIFECYCLE_STATE_RUNNING,
    eSERVICE_LIFECYCLE_STATE_STOPPED,
    eSERVICE_LIFECYCLE_STATE_FAULT,
} eServiceLifecycleState;

typedef enum eServiceLifecycleError {
    eSERVICE_LIFECYCLE_ERROR_NONE = 0,
    eSERVICE_LIFECYCLE_ERROR_INVALID_PARAM,
    eSERVICE_LIFECYCLE_ERROR_NOT_READY,
    eSERVICE_LIFECYCLE_ERROR_NOT_STARTED,
    eSERVICE_LIFECYCLE_ERROR_FAULTED,
    eSERVICE_LIFECYCLE_ERROR_CHECK_FAILED,
    eSERVICE_LIFECYCLE_ERROR_INTERNAL,
} eServiceLifecycleError;

typedef struct stServiceLifecycle {
    eServiceLifecycleClass classType;
    eServiceLifecycleState state;
    eServiceLifecycleError lastError;
    uint32_t initCount;
    uint32_t startCount;
    uint32_t stopCount;
    uint32_t processCount;
    uint32_t recoverCount;
    bool isReady;
    bool isStarted;
    bool hasFault;
} stServiceLifecycle;

void lifecycleReset(stServiceLifecycle *lifecycle, eServiceLifecycleClass classType);
bool lifecycleInit(stServiceLifecycle *lifecycle);
bool lifecycleStart(stServiceLifecycle *lifecycle);
bool lifecycleStop(stServiceLifecycle *lifecycle);
bool lifecycleNoteProcess(stServiceLifecycle *lifecycle);
void lifecycleReportFault(stServiceLifecycle *lifecycle, eServiceLifecycleError error);
bool lifecycleRecover(stServiceLifecycle *lifecycle);

#ifdef __cplusplus
}
#endif

#endif