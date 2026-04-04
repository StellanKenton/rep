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

static stSelfCheckSummary gSelfCheckSummary = {
    .state = eSELFCHECK_STATE_IDLE,
    .console = eSELFCHECK_RESULT_UNKNOWN,
    .appComm = eSELFCHECK_RESULT_UNKNOWN,
    .power = eSELFCHECK_RESULT_UNKNOWN,
    .update = eSELFCHECK_RESULT_UNKNOWN,
    .hasRun = false,
    .isPassed = false,
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
    selfCheckReset();
    gSelfCheckSummary.state = eSELFCHECK_STATE_IDLE;
    return true;
}

void selfCheckReset(void)
{
    gSelfCheckSummary.state = eSELFCHECK_STATE_RUNNING;
    gSelfCheckSummary.console = eSELFCHECK_RESULT_UNKNOWN;
    gSelfCheckSummary.appComm = eSELFCHECK_RESULT_UNKNOWN;
    gSelfCheckSummary.power = eSELFCHECK_RESULT_UNKNOWN;
    gSelfCheckSummary.update = eSELFCHECK_RESULT_UNKNOWN;
    gSelfCheckSummary.hasRun = false;
    gSelfCheckSummary.isPassed = false;
}

void selfCheckSetConsoleResult(bool isPassed)
{
    gSelfCheckSummary.console = selfCheckGetResult(isPassed);
}

void selfCheckSetAppCommResult(bool isPassed)
{
    gSelfCheckSummary.appComm = selfCheckGetResult(isPassed);
}

void selfCheckSetPowerResult(bool isPassed)
{
    gSelfCheckSummary.power = selfCheckGetResult(isPassed);
}

void selfCheckSetUpdateResult(bool isPassed)
{
    gSelfCheckSummary.update = selfCheckGetResult(isPassed);
}

bool selfCheckCommit(void)
{
    bool lPassed;

    lPassed = selfCheckIsItemPassed(gSelfCheckSummary.console) &&
              selfCheckIsItemPassed(gSelfCheckSummary.appComm) &&
              selfCheckIsItemPassed(gSelfCheckSummary.power) &&
              selfCheckIsItemPassed(gSelfCheckSummary.update);

    gSelfCheckSummary.hasRun = true;
    gSelfCheckSummary.isPassed = lPassed;
    gSelfCheckSummary.state = lPassed ? eSELFCHECK_STATE_PASS : eSELFCHECK_STATE_FAIL;

    LOG_I(SELFCHECK_TAG,
          "Startup self-check result: console=%d appComm=%d power=%d update=%d overall=%d",
          (int)gSelfCheckSummary.console,
          (int)gSelfCheckSummary.appComm,
          (int)gSelfCheckSummary.power,
          (int)gSelfCheckSummary.update,
          (int)gSelfCheckSummary.isPassed);
    return lPassed;
}

const stSelfCheckSummary *selfCheckGetSummary(void)
{
    return &gSelfCheckSummary;
}

/**************************End of file********************************/
