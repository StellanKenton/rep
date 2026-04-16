/************************************************************************************
* @file     : update_debug.h
* @brief    : Optional update service debug hooks.
* @details  : Provides state-name helpers and an overridable state transition
*             notification bridge for platform-specific diagnostics.
* @author   : GitHub Copilot
* @date     : 2026-04-16
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REP_SERVICE_UPDATE_DEBUG_H
#define REP_SERVICE_UPDATE_DEBUG_H

#include "update.h"

#ifdef __cplusplus
extern "C" {
#endif

const char *updateDbgGetStateName(eUpdateState state);
void updateDbgNotifyStateChanged(eUpdateState from, eUpdateState to);
void updateDbgOnStateChanged(eUpdateState from, eUpdateState to);

#ifdef __cplusplus
}
#endif

#endif  // REP_SERVICE_UPDATE_DEBUG_H
/**************************End of file********************************/