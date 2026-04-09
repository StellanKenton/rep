/************************************************************************************
* @file     : service_lifecycle.h
* @brief    : Shared lifecycle contract for manager-layer services.
* @details  : Defines a lightweight, reusable lifecycle model used by active and
*             recoverable services under the manager layer.
* @author   : GitHub Copilot
* @date     : 2026-04-04
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef SERVICE_LIFECYCLE_H
#define SERVICE_LIFECYCLE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eManagerServiceClass {
    eMANAGER_SERVICE_CLASS_PASSIVE_MODULE = 0,
    eMANAGER_SERVICE_CLASS_ACTIVE_SERVICE,
    eMANAGER_SERVICE_CLASS_RECOVERABLE_SERVICE,
} eManagerServiceClass;

typedef enum eManagerLifecycleState {
    eMANAGER_LIFECYCLE_STATE_UNINIT = 0,
    eMANAGER_LIFECYCLE_STATE_READY,
    eMANAGER_LIFECYCLE_STATE_RUNNING,
    eMANAGER_LIFECYCLE_STATE_STOPPED,
    eMANAGER_LIFECYCLE_STATE_FAULT,
} eManagerLifecycleState;

typedef enum eManagerLifecycleError {
    eMANAGER_LIFECYCLE_ERROR_NONE = 0,
    eMANAGER_LIFECYCLE_ERROR_INVALID_PARAM,
    eMANAGER_LIFECYCLE_ERROR_NOT_READY,
    eMANAGER_LIFECYCLE_ERROR_NOT_STARTED,
    eMANAGER_LIFECYCLE_ERROR_FAULTED,
    eMANAGER_LIFECYCLE_ERROR_CHECK_FAILED,
    eMANAGER_LIFECYCLE_ERROR_INTERNAL,
} eManagerLifecycleError;

typedef struct stManagerServiceLifecycle {
    eManagerServiceClass classType;
    eManagerLifecycleState state;
    eManagerLifecycleError lastError;
    uint32_t initCount;
    uint32_t startCount;
    uint32_t stopCount;
    uint32_t processCount;
    uint32_t recoverCount;
    bool isReady;
    bool isStarted;
    bool hasFault;
} stManagerServiceLifecycle;

void managerLifecycleReset(stManagerServiceLifecycle *lifecycle, eManagerServiceClass classType);
bool managerLifecycleInit(stManagerServiceLifecycle *lifecycle);
bool managerLifecycleStart(stManagerServiceLifecycle *lifecycle);
bool managerLifecycleStop(stManagerServiceLifecycle *lifecycle);
bool managerLifecycleNoteProcess(stManagerServiceLifecycle *lifecycle);
void managerLifecycleReportFault(stManagerServiceLifecycle *lifecycle, eManagerLifecycleError error);
bool managerLifecycleRecover(stManagerServiceLifecycle *lifecycle);

#ifdef __cplusplus
}
#endif

#endif  // SERVICE_LIFECYCLE_H
/**************************End of file********************************/