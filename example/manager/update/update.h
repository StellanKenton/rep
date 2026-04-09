/************************************************************************************
* @file     : update.h
* @brief    : Update service manager.
* @details  : Tracks update-service lifecycle and pending requests.
* @author   : GitHub Copilot
* @date     : 2026-04-04
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef UPDATE_H
#define UPDATE_H

#include <stdbool.h>

#include "../service_lifecycle.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UPDATE_TAG "UpdateMgr"

typedef enum eUpdateState {
    eUPDATE_STATE_UNINIT = 0,
    eUPDATE_STATE_IDLE,
    eUPDATE_STATE_PENDING,
    eUPDATE_STATE_ACTIVE,
    eUPDATE_STATE_DONE,
    eUPDATE_STATE_STOPPED,
    eUPDATE_STATE_FAULT,
} eUpdateState;

typedef struct stUpdateStatus {
    stManagerServiceLifecycle lifecycle;
    eUpdateState state;
    bool isUpdateRequested;
} stUpdateStatus;

bool updateInit(void);
bool updateStart(void);
void updateStop(void);
void updateProcess(void);
bool updateRequestStart(void);
bool updateRequestCancel(void);
void updateFault(eManagerLifecycleError error);
bool updateRecover(void);
eUpdateState updateGetState(void);
eManagerLifecycleError updateGetLastError(void);
const stUpdateStatus *updateGetStatus(void);

#ifdef __cplusplus
}
#endif

#endif  // UPDATE_H
/**************************End of file********************************/
