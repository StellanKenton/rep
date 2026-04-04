/************************************************************************************
* @file     : selfcheck.c
* @brief    : Startup self-check state aggregator.
* @details  : Stores service check results for startup evaluation and later query.
* @author   : GitHub Copilot
* @date     : 2026-04-04
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "selfcheck.h"

#include "log.h"

static stSelfCheckStatus gSelfCheckStatus = {
    .lifecycle = {
        .classType = eMANAGER_SERVICE_CLASS_RECOVERABLE_SERVICE,
        .state = eMANAGER_LIFECYCLE_STATE_UNINIT,
        .lastError = eMANAGER_LIFECYCLE_ERROR_NONE,
        .initCount = 0U,
        .startCount = 0U,
        .stopCount = 0U,
        .processCount = 0U,
        .recoverCount = 0U,
        .isReady = false,
        .isStarted = false,
        .hasFault = false,
    },
    .summary = {
        .state = eSELFCHECK_STATE_IDLE,
        .console = eSELFCHECK_RESULT_UNKNOWN,
        .appComm = eSELFCHECK_RESULT_UNKNOWN,
        .power = eSELFCHECK_RESULT_UNKNOWN,
        .update = eSELFCHECK_RESULT_UNKNOWN,
        .hasRun = false,
        .isPassed = false,
    },
};

static eSelfCheckResult selfCheckGetResult(bool isPassed)
{
    return isPassed ? eSELFCHECK_RESULT_PASS : eSELFCHECK_RESULT_FAIL;
}

static bool selfCheckIsItemPassed(eSelfCheckResult result)
{
    return result == eSELFCHECK_RESULT_PASS;
}

bool selfCheckInit(void)
{
    if (!managerLifecycleInit(&gSelfCheckStatus.lifecycle)) {
        gSelfCheckStatus.summary.state = eSELFCHECK_STATE_FAIL;
        return false;
    }

    if (gSelfCheckStatus.summary.state == eSELFCHECK_STATE_IDLE) {
        return true;
    }

    gSelfCheckStatus.summary.state = eSELFCHECK_STATE_IDLE;
    return true;
}

bool selfCheckStart(void)
{
    if (!selfCheckInit()) {
        return false;
    }

    if (!managerLifecycleStart(&gSelfCheckStatus.lifecycle)) {
        gSelfCheckStatus.summary.state = eSELFCHECK_STATE_FAIL;
        return false;
    }

    selfCheckReset();
    return true;
}

void selfCheckReset(void)
{
    gSelfCheckStatus.summary.state = eSELFCHECK_STATE_RUNNING;
    gSelfCheckStatus.summary.console = eSELFCHECK_RESULT_UNKNOWN;
    gSelfCheckStatus.summary.appComm = eSELFCHECK_RESULT_UNKNOWN;
    gSelfCheckStatus.summary.power = eSELFCHECK_RESULT_UNKNOWN;
    gSelfCheckStatus.summary.update = eSELFCHECK_RESULT_UNKNOWN;
    gSelfCheckStatus.summary.hasRun = false;
    gSelfCheckStatus.summary.isPassed = false;
}

void selfCheckSetConsoleResult(bool isPassed)
{
    gSelfCheckStatus.summary.console = selfCheckGetResult(isPassed);
}

void selfCheckSetAppCommResult(bool isPassed)
{
    gSelfCheckStatus.summary.appComm = selfCheckGetResult(isPassed);
}

void selfCheckSetPowerResult(bool isPassed)
{
    gSelfCheckStatus.summary.power = selfCheckGetResult(isPassed);
}

void selfCheckSetUpdateResult(bool isPassed)
{
    gSelfCheckStatus.summary.update = selfCheckGetResult(isPassed);
}

bool selfCheckCommit(void)
{
    bool lPassed;

    if (!managerLifecycleNoteProcess(&gSelfCheckStatus.lifecycle)) {
        gSelfCheckStatus.summary.state = eSELFCHECK_STATE_FAIL;
        return false;
    }

    lPassed = selfCheckIsItemPassed(gSelfCheckStatus.summary.console) &&
              selfCheckIsItemPassed(gSelfCheckStatus.summary.appComm) &&
              selfCheckIsItemPassed(gSelfCheckStatus.summary.power) &&
              selfCheckIsItemPassed(gSelfCheckStatus.summary.update);

    gSelfCheckStatus.summary.hasRun = true;
    gSelfCheckStatus.summary.isPassed = lPassed;
    gSelfCheckStatus.summary.state = lPassed ? eSELFCHECK_STATE_PASS : eSELFCHECK_STATE_FAIL;

    if (lPassed) {
        (void)managerLifecycleStop(&gSelfCheckStatus.lifecycle);
    } else {
        managerLifecycleReportFault(&gSelfCheckStatus.lifecycle, eMANAGER_LIFECYCLE_ERROR_CHECK_FAILED);
    }

    LOG_I(SELFCHECK_TAG,
          "Startup self-check result: console=%d appComm=%d power=%d update=%d overall=%d",
          (int)gSelfCheckStatus.summary.console,
          (int)gSelfCheckStatus.summary.appComm,
          (int)gSelfCheckStatus.summary.power,
          (int)gSelfCheckStatus.summary.update,
          (int)gSelfCheckStatus.summary.isPassed);
    return lPassed;
}

bool selfCheckRecover(void)
{
    if (!selfCheckInit()) {
        return false;
    }

    if (!managerLifecycleRecover(&gSelfCheckStatus.lifecycle)) {
        return false;
    }

    gSelfCheckStatus.summary.state = eSELFCHECK_STATE_IDLE;
    gSelfCheckStatus.summary.console = eSELFCHECK_RESULT_UNKNOWN;
    gSelfCheckStatus.summary.appComm = eSELFCHECK_RESULT_UNKNOWN;
    gSelfCheckStatus.summary.power = eSELFCHECK_RESULT_UNKNOWN;
    gSelfCheckStatus.summary.update = eSELFCHECK_RESULT_UNKNOWN;
    gSelfCheckStatus.summary.hasRun = false;
    gSelfCheckStatus.summary.isPassed = false;
    return true;
}

eManagerLifecycleState selfCheckGetState(void)
{
    return gSelfCheckStatus.lifecycle.state;
}

eManagerLifecycleError selfCheckGetLastError(void)
{
    return gSelfCheckStatus.lifecycle.lastError;
}

const stSelfCheckSummary *selfCheckGetSummary(void)
{
    return &gSelfCheckStatus.summary;
}

const stSelfCheckStatus *selfCheckGetStatus(void)
{
    return &gSelfCheckStatus;
}

/**************************End of file********************************/
