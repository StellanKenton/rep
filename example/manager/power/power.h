/************************************************************************************
* @file     : power.h
* @brief    : Power service manager.
* @details  : Tracks power-service lifecycle state for the manager layer.
* @author   : GitHub Copilot
* @date     : 2026-04-04
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef POWER_H
#define POWER_H

#include <stdbool.h>

#include "../service_lifecycle.h"

#ifdef __cplusplus
extern "C" {
#endif

#define POWER_TAG "PowerMgr"

typedef enum ePowerState {
    ePOWER_STATE_UNINIT = 0,
    ePOWER_STATE_READY,
    ePOWER_STATE_ACTIVE,
    ePOWER_STATE_LOW_POWER,
    ePOWER_STATE_STOPPED,
    ePOWER_STATE_FAULT,
} ePowerState;

typedef struct stPowerStatus {
    stManagerServiceLifecycle lifecycle;
    ePowerState state;
    bool isLowPowerRequested;
} stPowerStatus;

bool powerInit(void);
bool powerStart(void);
void powerStop(void);
void powerProcess(void);
bool powerRequestLowPower(bool isEnabled);
ePowerState powerGetState(void);
eManagerLifecycleError powerGetLastError(void);
const stPowerStatus *powerGetStatus(void);

#ifdef __cplusplus
}
#endif

#endif  // POWER_H
/**************************End of file********************************/
