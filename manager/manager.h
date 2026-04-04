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

#include "selfcheck/selfcheck.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MANAGER_TAG "Manager"

bool managerInit(void);
bool managerRunStartupSelfCheck(bool isConsoleReady, bool isAppCommReady);
void managerPowerProcess(void);
void managerUpdateProcess(void);
const stSelfCheckSummary *managerGetSelfCheckSummary(void);

#ifdef __cplusplus
}
#endif

#endif  // MANAGER_H
/**************************End of file********************************/
