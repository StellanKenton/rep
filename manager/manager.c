/************************************************************************************
* @file     : manager.c
* @brief    : Service orchestration entry for the manager layer.
* @details  : Centralizes startup checks and delegates runtime service processing.
* @author   : GitHub Copilot
* @date     : 2026-04-04
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "manager.h"

#include <stddef.h>

#include "log.h"

static bool gManagerIsInitialized = false;
static stManagerHealthSummary gManagerHealthSummary = {
    .level = eMANAGER_HEALTH_LEVEL_WARN,
    .totalServiceCount = 4U,
    .readyServiceCount = 0U,
    .runningServiceCount = 0U,
    .faultServiceCount = 0U,
    .isManagerInitialized = false,
};

static void managerCopyLifecycleHealth(stManagerServiceHealthSummary *summary, const stManagerServiceLifecycle *lifecycle)
{
    if ((summary == NULL) || (lifecycle == NULL)) {
        return;
    }

    summary->lifecycleState = lifecycle->state;
    summary->lastError = lifecycle->lastError;
    summary->initCount = lifecycle->initCount;
    summary->startCount = lifecycle->startCount;
    summary->stopCount = lifecycle->stopCount;
    summary->processCount = lifecycle->processCount;
    summary->recoverCount = lifecycle->recoverCount;
    summary->isReady = lifecycle->isReady;
    summary->isStarted = lifecycle->isStarted;
    summary->hasFault = lifecycle->hasFault;
}

static void managerSummarizeServiceCounts(const stManagerServiceHealthSummary *summary, uint32_t *readyCount, uint32_t *runningCount, uint32_t *faultCount)
{
    if ((summary == NULL) || (readyCount == NULL) || (runningCount == NULL) || (faultCount == NULL)) {
        return;
    }

    if (summary->isReady) {
        (*readyCount)++;
    }

    if (summary->isStarted) {
        (*runningCount)++;
    }

    if (summary->hasFault) {
        (*faultCount)++;
    }
}

static void managerRefreshHealthSummary(void)
{
    const stPowerStatus *lPowerStatus;
    const stUpdateStatus *lUpdateStatus;
    const stSelfCheckStatus *lSelfCheckStatus;
    const stBleMgrStatus *lBleStatus;

    lBleStatus = blemgrGetStatus();
    lPowerStatus = powerGetStatus();
    lUpdateStatus = updateGetStatus();
    lSelfCheckStatus = selfCheckGetStatus();

    gManagerHealthSummary.level = eMANAGER_HEALTH_LEVEL_WARN;
    gManagerHealthSummary.totalServiceCount = 4U;
    gManagerHealthSummary.readyServiceCount = 0U;
    gManagerHealthSummary.runningServiceCount = 0U;
    gManagerHealthSummary.faultServiceCount = 0U;
    gManagerHealthSummary.isManagerInitialized = gManagerIsInitialized;

    if (lBleStatus != NULL) {
        managerCopyLifecycleHealth(&gManagerHealthSummary.ble, &lBleStatus->lifecycle);
        gManagerHealthSummary.bleState = lBleStatus->state;
        gManagerHealthSummary.isBleConfigured = lBleStatus->isConfigured;
        gManagerHealthSummary.isBleHandshakeDone = lBleStatus->isHandshakeDone;
        gManagerHealthSummary.bleRxPacketCount = lBleStatus->rxPacketCount;
        gManagerHealthSummary.bleRxInvalidPacketCount = lBleStatus->rxInvalidPacketCount;
        gManagerHealthSummary.bleLastCmd = lBleStatus->lastCmd;
        managerSummarizeServiceCounts(&gManagerHealthSummary.ble,
                                      &gManagerHealthSummary.readyServiceCount,
                                      &gManagerHealthSummary.runningServiceCount,
                                      &gManagerHealthSummary.faultServiceCount);
    }

    if (lPowerStatus != NULL) {
        managerCopyLifecycleHealth(&gManagerHealthSummary.power, &lPowerStatus->lifecycle);
        gManagerHealthSummary.powerState = lPowerStatus->state;
        gManagerHealthSummary.isLowPowerRequested = lPowerStatus->isLowPowerRequested;
        managerSummarizeServiceCounts(&gManagerHealthSummary.power,
                                      &gManagerHealthSummary.readyServiceCount,
                                      &gManagerHealthSummary.runningServiceCount,
                                      &gManagerHealthSummary.faultServiceCount);
    }

    if (lUpdateStatus != NULL) {
        managerCopyLifecycleHealth(&gManagerHealthSummary.update, &lUpdateStatus->lifecycle);
        gManagerHealthSummary.updateState = lUpdateStatus->state;
        gManagerHealthSummary.isUpdateRequested = lUpdateStatus->isUpdateRequested;
        managerSummarizeServiceCounts(&gManagerHealthSummary.update,
                                      &gManagerHealthSummary.readyServiceCount,
                                      &gManagerHealthSummary.runningServiceCount,
                                      &gManagerHealthSummary.faultServiceCount);
    }

    if (lSelfCheckStatus != NULL) {
        managerCopyLifecycleHealth(&gManagerHealthSummary.selfCheck, &lSelfCheckStatus->lifecycle);
        gManagerHealthSummary.selfCheckSummary = lSelfCheckStatus->summary;
        managerSummarizeServiceCounts(&gManagerHealthSummary.selfCheck,
                                      &gManagerHealthSummary.readyServiceCount,
                                      &gManagerHealthSummary.runningServiceCount,
                                      &gManagerHealthSummary.faultServiceCount);
    }

    if ((gManagerHealthSummary.faultServiceCount > 0U) ||
        (gManagerHealthSummary.selfCheckSummary.hasRun && !gManagerHealthSummary.selfCheckSummary.isPassed)) {
        gManagerHealthSummary.level = eMANAGER_HEALTH_LEVEL_ERROR;
        return;
    }

    if ((!gManagerHealthSummary.isManagerInitialized) ||
        (gManagerHealthSummary.readyServiceCount < gManagerHealthSummary.totalServiceCount) ||
        (!gManagerHealthSummary.selfCheckSummary.hasRun)) {
        gManagerHealthSummary.level = eMANAGER_HEALTH_LEVEL_WARN;
        return;
    }

    gManagerHealthSummary.level = eMANAGER_HEALTH_LEVEL_OK;
}

bool managerInit(void)
{
    if (gManagerIsInitialized) {
        managerRefreshHealthSummary();
        return true;
    }

    if (!selfCheckInit()) {
        LOG_E(MANAGER_TAG, "Self-check manager init failed");
        return false;
    }

    if (!blemgrInit()) {
        LOG_E(MANAGER_TAG, "BLE manager init failed");
        return false;
    }

    if (!powerInit()) {
        LOG_E(MANAGER_TAG, "Power manager init failed");
        return false;
    }

    if (!updateInit()) {
        LOG_E(MANAGER_TAG, "Update manager init failed");
        return false;
    }

    gManagerIsInitialized = true;
    managerRefreshHealthSummary();
    LOG_I(MANAGER_TAG, "Manager initialized");
    return true;
}

bool managerRunStartupSelfCheck(bool isConsoleReady, bool isAppCommReady)
{
    bool lPowerReady;
    bool lUpdateReady;

    if (!managerInit()) {
        return false;
    }

    if (!selfCheckStart()) {
        LOG_E(MANAGER_TAG, "Self-check service start failed");
        return false;
    }

    selfCheckReset();
    selfCheckSetConsoleResult(isConsoleReady);
    selfCheckSetAppCommResult(isAppCommReady);

    lPowerReady = powerInit();
    selfCheckSetPowerResult(lPowerReady);

    lUpdateReady = updateInit();
    selfCheckSetUpdateResult(lUpdateReady);

    lUpdateReady = selfCheckCommit();
    managerRefreshHealthSummary();
    return lUpdateReady;
}

bool managerBleStart(void)
{
    bool lResult;

    lResult = managerInit() && blemgrStart();
    managerRefreshHealthSummary();
    return lResult;
}

void managerBleStop(void)
{
    if (!gManagerIsInitialized) {
        return;
    }

    blemgrStop();
    managerRefreshHealthSummary();
}

void managerBleProcess(void)
{
    if (!managerBleStart()) {
        return;
    }

    blemgrProcess();
    managerRefreshHealthSummary();
}

const stBleMgrStatus *managerGetBleStatus(void)
{
    return blemgrGetStatus();
}

bool managerPowerStart(void)
{
    bool lResult;

    lResult = managerInit() && powerStart();
    managerRefreshHealthSummary();
    return lResult;
}

void managerPowerStop(void)
{
    if (!gManagerIsInitialized) {
        return;
    }

    powerStop();
    managerRefreshHealthSummary();
}

void managerPowerProcess(void)
{
    if (!managerPowerStart()) {
        return;
    }

    powerProcess();
    managerRefreshHealthSummary();
}

const stPowerStatus *managerGetPowerStatus(void)
{
    return powerGetStatus();
}

bool managerUpdateStart(void)
{
    bool lResult;

    lResult = managerInit() && updateStart();
    managerRefreshHealthSummary();
    return lResult;
}

void managerUpdateStop(void)
{
    if (!gManagerIsInitialized) {
        return;
    }

    updateStop();
    managerRefreshHealthSummary();
}

void managerUpdateProcess(void)
{
    if (!managerUpdateStart()) {
        return;
    }

    updateProcess();
    managerRefreshHealthSummary();
}

const stUpdateStatus *managerGetUpdateStatus(void)
{
    return updateGetStatus();
}

const stManagerHealthSummary *managerGetHealthSummary(void)
{
    managerRefreshHealthSummary();
    return &gManagerHealthSummary;
}

const stSelfCheckSummary *managerGetSelfCheckSummary(void)
{
    return selfCheckGetSummary();
}

const stSelfCheckStatus *managerGetSelfCheckStatus(void)
{
    return selfCheckGetStatus();
}

/**************************End of file********************************/
