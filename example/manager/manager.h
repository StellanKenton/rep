/************************************************************************************
* @file     : manager.h
* @brief    : Service orchestration entry for the manager layer.
* @details  : Provides startup self-check orchestration and runtime dispatch hooks.
* @author   : GitHub Copilot
* @date     : 2026-04-04
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef MANAGER_H
#define MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#include "blemgr/blemgr.h"
#include "power/power.h"
#include "selfcheck/selfcheck.h"
#include "update/update.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MANAGER_TAG "Manager"

typedef enum eManagerHealthLevel {
	eMANAGER_HEALTH_LEVEL_OK = 0,
	eMANAGER_HEALTH_LEVEL_WARN,
	eMANAGER_HEALTH_LEVEL_ERROR,
} eManagerHealthLevel;

typedef struct stManagerServiceHealthSummary {
	eManagerLifecycleState lifecycleState;
	eManagerLifecycleError lastError;
	uint32_t initCount;
	uint32_t startCount;
	uint32_t stopCount;
	uint32_t processCount;
	uint32_t recoverCount;
	bool isReady;
	bool isStarted;
	bool hasFault;
} stManagerServiceHealthSummary;

typedef struct stManagerHealthSummary {
	eManagerHealthLevel level;
	uint32_t totalServiceCount;
	uint32_t readyServiceCount;
	uint32_t runningServiceCount;
	uint32_t faultServiceCount;
	bool isManagerInitialized;
	stManagerServiceHealthSummary ble;
	eBleMgrState bleState;
	bool isBleConfigured;
	bool isBleHandshakeDone;
	uint32_t bleRxPacketCount;
	uint32_t bleRxInvalidPacketCount;
	uint8_t bleLastCmd;
	stManagerServiceHealthSummary power;
	ePowerState powerState;
	bool isLowPowerRequested;
	stManagerServiceHealthSummary update;
	eUpdateState updateState;
	bool isUpdateRequested;
	stManagerServiceHealthSummary selfCheck;
	stSelfCheckSummary selfCheckSummary;
} stManagerHealthSummary;

bool managerInit(void);
bool managerRunStartupSelfCheck(bool isConsoleReady, bool isAppCommReady);
bool managerBleStart(void);
void managerBleStop(void);
void managerBleProcess(void);
const stBleMgrStatus *managerGetBleStatus(void);
bool managerPowerStart(void);
void managerPowerStop(void);
void managerPowerProcess(void);
const stPowerStatus *managerGetPowerStatus(void);
bool managerUpdateStart(void);
void managerUpdateStop(void);
void managerUpdateProcess(void);
const stUpdateStatus *managerGetUpdateStatus(void);
const stManagerHealthSummary *managerGetHealthSummary(void);
const stSelfCheckSummary *managerGetSelfCheckSummary(void);
const stSelfCheckStatus *managerGetSelfCheckStatus(void);

#ifdef __cplusplus
}
#endif

#endif  // MANAGER_H
/**************************End of file********************************/
