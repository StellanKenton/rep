/************************************************************************************
* @file     : selfcheck.h
* @brief    : Startup self-check state aggregator.
* @details  : Collects service-level check results and exposes a stable summary.
* @author   : GitHub Copilot
* @date     : 2026-04-04
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef SELFCHECK_H
#define SELFCHECK_H

#include <stdbool.h>

#include "../service_lifecycle.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SELFCHECK_TAG "SelfCheck"

typedef enum eSelfCheckResult {
    eSELFCHECK_RESULT_UNKNOWN = 0,
    eSELFCHECK_RESULT_PASS,
    eSELFCHECK_RESULT_FAIL,
} eSelfCheckResult;

typedef enum eSelfCheckState {
    eSELFCHECK_STATE_IDLE = 0,
    eSELFCHECK_STATE_RUNNING,
    eSELFCHECK_STATE_PASS,
    eSELFCHECK_STATE_FAIL,
} eSelfCheckState;

typedef struct stSelfCheckSummary {
    eSelfCheckState state;
    eSelfCheckResult console;
    eSelfCheckResult appComm;
    eSelfCheckResult power;
    eSelfCheckResult update;
    bool hasRun;
    bool isPassed;
} stSelfCheckSummary;

typedef struct stSelfCheckStatus {
    stManagerServiceLifecycle lifecycle;
    stSelfCheckSummary summary;
} stSelfCheckStatus;

bool selfCheckInit(void);
bool selfCheckStart(void);
void selfCheckReset(void);
void selfCheckSetConsoleResult(bool isPassed);
void selfCheckSetAppCommResult(bool isPassed);
void selfCheckSetPowerResult(bool isPassed);
void selfCheckSetUpdateResult(bool isPassed);
bool selfCheckCommit(void);
bool selfCheckRecover(void);
eManagerLifecycleState selfCheckGetState(void);
eManagerLifecycleError selfCheckGetLastError(void);
const stSelfCheckSummary *selfCheckGetSummary(void);
const stSelfCheckStatus *selfCheckGetStatus(void);

#ifdef __cplusplus
}
#endif

#endif  // SELFCHECK_H
/**************************End of file********************************/
