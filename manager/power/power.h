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
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define POWER_TAG "PowerMgr"

typedef enum ePowerState {
    ePOWER_STATE_UNINIT = 0,
    ePOWER_STATE_READY,
    ePOWER_STATE_ACTIVE,
    ePOWER_STATE_LOW_POWER,
    ePOWER_STATE_FAULT,
} ePowerState;

typedef struct stPowerStatus {
    ePowerState state;
    uint32_t processCount;
    bool isReady;
    bool isLowPowerRequested;
} stPowerStatus;

bool powerInit(void);
void powerProcess(void);
bool powerRequestLowPower(bool isEnabled);
const stPowerStatus *powerGetStatus(void);

#ifdef __cplusplus
}
#endif

#endif  // POWER_H
/**************************End of file********************************/
