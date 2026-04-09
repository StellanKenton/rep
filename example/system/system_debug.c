/***********************************************************************************
* @file     : system_debug.c
* @brief    : System debug and console command implementation.
* @details  : This file hosts optional console bindings for system debug operations.
* @author   : GitHub Copilot
* @date     : 2026-04-01
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "system_debug.h"

#include <stdint.h>
#include <string.h>

#include "console.h"
#include "manager/manager.h"
#include "rep_config.h"
#include "system.h"

#if (SYSTEM_DEBUG_CONSOLE_SUPPORT == 1) && (REP_RTOS_SYSTEM == REP_RTOS_FREERTOS)
#include "FreeRTOS.h"
#include "task.h"

#define SYSTEM_DEBUG_TASK_USAGE_MAX_TASKS            16U
#define SYSTEM_DEBUG_TASK_USAGE_SAMPLE_PERIOD_MS     50U
#define SYSTEM_DEBUG_TASK_USAGE_SAMPLE_COUNT         20U
#define SYSTEM_DEBUG_TASK_USAGE_TASK_STACK_SIZE      (configMINIMAL_STACK_SIZE * 4U)
#define SYSTEM_DEBUG_TASK_USAGE_TASK_PRIORITY        (tskIDLE_PRIORITY + 1U)

static const TaskStatus_t *systemDebugFindTaskStatusByHandle(const TaskStatus_t *taskStatusArray, UBaseType_t taskCount, TaskHandle_t taskHandle);
static bool systemDebugCaptureTaskUsageSnapshot(TaskStatus_t *taskStatusArray, UBaseType_t capacity, UBaseType_t *taskCount, uint32_t *totalRunTime);
static eConsoleCommandResult systemDebugReplyTaskUsageSample(uint32_t transport, uint32_t sampleIndex, const TaskStatus_t *currentTaskStats, UBaseType_t currentTaskCount, const TaskStatus_t *previousTaskStats, UBaseType_t previousTaskCount, uint32_t totalRunTimeDelta);
static void systemDebugTaskUsageSampler(void *parameter);
#endif

static eConsoleCommandResult systemDebugConsoleVersionHandler(uint32_t transport, int argc, char *argv[]);
static eConsoleCommandResult systemDebugConsoleStatusHandler(uint32_t transport, int argc, char *argv[]);
static eConsoleCommandResult systemDebugConsoleHealthHandler(uint32_t transport, int argc, char *argv[]);
static const char *systemDebugGetManagerHealthLevelString(eManagerHealthLevel level);
static const char *systemDebugGetLifecycleStateString(eManagerLifecycleState state);
static const char *systemDebugGetLifecycleErrorString(eManagerLifecycleError error);
static const char *systemDebugGetPowerStateString(ePowerState state);
static const char *systemDebugGetUpdateStateString(eUpdateState state);
static const char *systemDebugGetSelfCheckStateString(eSelfCheckState state);
static const char *systemDebugGetSelfCheckResultString(eSelfCheckResult result);
#if (SYSTEM_DEBUG_CONSOLE_SUPPORT == 1) && (REP_RTOS_SYSTEM == REP_RTOS_FREERTOS)
static eConsoleCommandResult systemDebugConsoleTaskUsageHandler(uint32_t transport, int argc, char *argv[]);

static TaskHandle_t gSystemDebugTaskUsageHandle = NULL;
#endif

static const stConsoleCommand gSystemVersionConsoleCommand = {
    .commandName = "ver",
    .helpText = "ver - show firmware and hardware version",
    .ownerTag = "system",
    .handler = systemDebugConsoleVersionHandler,
};

static const stConsoleCommand gSystemStatusConsoleCommand = {
    .commandName = "sys",
    .helpText = "sys - show system status",
    .ownerTag = "system",
    .handler = systemDebugConsoleStatusHandler,
};

static const stConsoleCommand gSystemHealthConsoleCommand = {
    .commandName = "health",
    .helpText = "health - show manager health summary",
    .ownerTag = "system",
    .handler = systemDebugConsoleHealthHandler,
};

#if (SYSTEM_DEBUG_CONSOLE_SUPPORT == 1) && (REP_RTOS_SYSTEM == REP_RTOS_FREERTOS)
static const stConsoleCommand gSystemTaskUsageConsoleCommand = {
    .commandName = "top",
    .helpText = "top - sample task cpu usage every 50 ms for 1 s",
    .ownerTag = "system",
    .handler = systemDebugConsoleTaskUsageHandler,
};

static const TaskStatus_t *systemDebugFindTaskStatusByHandle(const TaskStatus_t *taskStatusArray, UBaseType_t taskCount, TaskHandle_t taskHandle)
{
    UBaseType_t lIndex;

    if ((taskStatusArray == NULL) || (taskHandle == NULL)) {
        return NULL;
    }

    for (lIndex = 0U; lIndex < taskCount; lIndex++) {
        if (taskStatusArray[lIndex].xHandle == taskHandle) {
            return &taskStatusArray[lIndex];
        }
    }

    return NULL;
}

static bool systemDebugCaptureTaskUsageSnapshot(TaskStatus_t *taskStatusArray, UBaseType_t capacity, UBaseType_t *taskCount, uint32_t *totalRunTime)
{
    UBaseType_t lTaskCount;

    if ((taskStatusArray == NULL) || (taskCount == NULL) || (totalRunTime == NULL) || (capacity == 0U)) {
        return false;
    }

    lTaskCount = uxTaskGetNumberOfTasks();
    if ((lTaskCount == 0U) || (lTaskCount > capacity)) {
        return false;
    }

    *totalRunTime = 0U;
    lTaskCount = uxTaskGetSystemState(taskStatusArray, capacity, totalRunTime);
    if ((lTaskCount == 0U) || (*totalRunTime == 0U)) {
        return false;
    }

    *taskCount = lTaskCount;
    return true;
}

static eConsoleCommandResult systemDebugReplyTaskUsageSample(uint32_t transport, uint32_t sampleIndex, const TaskStatus_t *currentTaskStats, UBaseType_t currentTaskCount, const TaskStatus_t *previousTaskStats, UBaseType_t previousTaskCount, uint32_t totalRunTimeDelta)
{
    UBaseType_t lIndex;

    if ((currentTaskStats == NULL) || (previousTaskStats == NULL) || (totalRunTimeDelta == 0U)) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (consoleReply(transport,
        "sample %lu/%lu window=%lums",
        (unsigned long)(sampleIndex + 1U),
        (unsigned long)SYSTEM_DEBUG_TASK_USAGE_SAMPLE_COUNT,
        (unsigned long)SYSTEM_DEBUG_TASK_USAGE_SAMPLE_PERIOD_MS) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    for (lIndex = 0U; lIndex < currentTaskCount; lIndex++) {
        const TaskStatus_t *lPreviousTaskStatus;
        const char *lTaskName;
        uint32_t lTaskRunTimeDelta = 0U;
        uint32_t lUsagePercentX10;

        lPreviousTaskStatus = systemDebugFindTaskStatusByHandle(previousTaskStats,
            previousTaskCount,
            currentTaskStats[lIndex].xHandle);
        if ((lPreviousTaskStatus != NULL) &&
            (currentTaskStats[lIndex].ulRunTimeCounter >= lPreviousTaskStatus->ulRunTimeCounter)) {
            lTaskRunTimeDelta = currentTaskStats[lIndex].ulRunTimeCounter - lPreviousTaskStatus->ulRunTimeCounter;
        }

        lUsagePercentX10 = (uint32_t)((((uint64_t)lTaskRunTimeDelta * 1000ULL) +
            ((uint64_t)totalRunTimeDelta / 2ULL)) /
            (uint64_t)totalRunTimeDelta);
        lTaskName = (currentTaskStats[lIndex].pcTaskName != NULL) ? currentTaskStats[lIndex].pcTaskName : "unknown";
        if (consoleReply(transport,
            "%s: %lu.%lu%%",
            lTaskName,
            (unsigned long)(lUsagePercentX10 / 10U),
            (unsigned long)(lUsagePercentX10 % 10U)) <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static void systemDebugTaskUsageSampler(void *parameter)
{
    TaskStatus_t lPreviousTaskStats[SYSTEM_DEBUG_TASK_USAGE_MAX_TASKS];
    TaskStatus_t lCurrentTaskStats[SYSTEM_DEBUG_TASK_USAGE_MAX_TASKS];
    UBaseType_t lPreviousTaskCount = 0U;
    UBaseType_t lCurrentTaskCount = 0U;
    TickType_t lLastWakeTime;
    uint32_t lPreviousTotalRunTime = 0U;
    uint32_t lCurrentTotalRunTime = 0U;
    uint32_t lSampleIndex;
    uint32_t lTransport = (uint32_t)parameter;

    if (!systemDebugCaptureTaskUsageSnapshot(lPreviousTaskStats,
        SYSTEM_DEBUG_TASK_USAGE_MAX_TASKS,
        &lPreviousTaskCount,
        &lPreviousTotalRunTime)) {
        (void)consoleReply(lTransport, "ERROR: task runtime stats unavailable");
        gSystemDebugTaskUsageHandle = NULL;
        vTaskDelete(NULL);
        return;
    }

    lLastWakeTime = xTaskGetTickCount();
    for (lSampleIndex = 0U; lSampleIndex < SYSTEM_DEBUG_TASK_USAGE_SAMPLE_COUNT; lSampleIndex++) {
        vTaskDelayUntil(&lLastWakeTime, pdMS_TO_TICKS(SYSTEM_DEBUG_TASK_USAGE_SAMPLE_PERIOD_MS));

        if (!systemDebugCaptureTaskUsageSnapshot(lCurrentTaskStats,
            SYSTEM_DEBUG_TASK_USAGE_MAX_TASKS,
            &lCurrentTaskCount,
            &lCurrentTotalRunTime)) {
            (void)consoleReply(lTransport, "ERROR: task runtime stats unavailable");
            break;
        }

        if (lCurrentTotalRunTime <= lPreviousTotalRunTime) {
            (void)consoleReply(lTransport, "ERROR: invalid runtime stats window");
            break;
        }

        if (systemDebugReplyTaskUsageSample(lTransport,
            lSampleIndex,
            lCurrentTaskStats,
            lCurrentTaskCount,
            lPreviousTaskStats,
            lPreviousTaskCount,
            lCurrentTotalRunTime - lPreviousTotalRunTime) != CONSOLE_COMMAND_RESULT_OK) {
            break;
        }

        (void)memcpy(lPreviousTaskStats,
            lCurrentTaskStats,
            (size_t)lCurrentTaskCount * sizeof(TaskStatus_t));
        lPreviousTaskCount = lCurrentTaskCount;
        lPreviousTotalRunTime = lCurrentTotalRunTime;
    }

    (void)consoleReply(lTransport, "taskcpu done");
    gSystemDebugTaskUsageHandle = NULL;
    vTaskDelete(NULL);
}

static eConsoleCommandResult systemDebugConsoleTaskUsageHandler(uint32_t transport, int argc, char *argv[])
{
    (void)argv;

    if (argc != 1) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (gSystemDebugTaskUsageHandle != NULL) {
        if (consoleReply(transport, "ERROR: taskcpu sampler busy") <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }
        return CONSOLE_COMMAND_RESULT_OK;
    }

    if (xTaskCreate(systemDebugTaskUsageSampler,
        "TaskCpuMon",
        SYSTEM_DEBUG_TASK_USAGE_TASK_STACK_SIZE,
        (void *)transport,
        SYSTEM_DEBUG_TASK_USAGE_TASK_PRIORITY,
        &gSystemDebugTaskUsageHandle) != pdPASS) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (consoleReply(transport,
        "taskcpu started: %lums x %lu",
        (unsigned long)SYSTEM_DEBUG_TASK_USAGE_SAMPLE_PERIOD_MS,
        (unsigned long)SYSTEM_DEBUG_TASK_USAGE_SAMPLE_COUNT) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}
#endif

/**
* @brief : Reply with firmware and hardware version strings.
* @param : transport - console reply transport.
* @param : argc - console argument count.
* @param : argv - console argument vector.
* @return: Console command execution result.
**/
static eConsoleCommandResult systemDebugConsoleVersionHandler(uint32_t transport, int argc, char *argv[])
{
    (void)argv;

    if (argc != 1) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (consoleReply(transport,
        "Firmware: %s\nHardware: %s\nOK",
        systemGetFirmwareVersion(),
        systemGetHardwareVersion()) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

/**
* @brief : Reply with current system runtime status.
* @param : transport - console reply transport.
* @param : argc - console argument count.
* @param : argv - console argument vector.
* @return: Console command execution result.
**/
static eConsoleCommandResult systemDebugConsoleStatusHandler(uint32_t transport, int argc, char *argv[])
{
    (void)argv;

    if (argc != 1) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (consoleReply(transport,
        "Mode: %s\nOK",
        systemGetModeString(systemGetMode())) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static const char *systemDebugGetManagerHealthLevelString(eManagerHealthLevel level)
{
    switch (level) {
        case eMANAGER_HEALTH_LEVEL_OK:
            return "ok";
        case eMANAGER_HEALTH_LEVEL_WARN:
            return "warn";
        case eMANAGER_HEALTH_LEVEL_ERROR:
            return "error";
        default:
            return "unknown";
    }
}

static const char *systemDebugGetLifecycleStateString(eManagerLifecycleState state)
{
    switch (state) {
        case eMANAGER_LIFECYCLE_STATE_UNINIT:
            return "uninit";
        case eMANAGER_LIFECYCLE_STATE_READY:
            return "ready";
        case eMANAGER_LIFECYCLE_STATE_RUNNING:
            return "running";
        case eMANAGER_LIFECYCLE_STATE_STOPPED:
            return "stopped";
        case eMANAGER_LIFECYCLE_STATE_FAULT:
            return "fault";
        default:
            return "unknown";
    }
}

static const char *systemDebugGetLifecycleErrorString(eManagerLifecycleError error)
{
    switch (error) {
        case eMANAGER_LIFECYCLE_ERROR_NONE:
            return "none";
        case eMANAGER_LIFECYCLE_ERROR_INVALID_PARAM:
            return "invalid_param";
        case eMANAGER_LIFECYCLE_ERROR_NOT_READY:
            return "not_ready";
        case eMANAGER_LIFECYCLE_ERROR_NOT_STARTED:
            return "not_started";
        case eMANAGER_LIFECYCLE_ERROR_FAULTED:
            return "faulted";
        case eMANAGER_LIFECYCLE_ERROR_CHECK_FAILED:
            return "check_failed";
        case eMANAGER_LIFECYCLE_ERROR_INTERNAL:
            return "internal";
        default:
            return "unknown";
    }
}

static const char *systemDebugGetPowerStateString(ePowerState state)
{
    switch (state) {
        case ePOWER_STATE_UNINIT:
            return "uninit";
        case ePOWER_STATE_READY:
            return "ready";
        case ePOWER_STATE_ACTIVE:
            return "active";
        case ePOWER_STATE_LOW_POWER:
            return "low_power";
        case ePOWER_STATE_STOPPED:
            return "stopped";
        case ePOWER_STATE_FAULT:
            return "fault";
        default:
            return "unknown";
    }
}

static const char *systemDebugGetUpdateStateString(eUpdateState state)
{
    switch (state) {
        case eUPDATE_STATE_UNINIT:
            return "uninit";
        case eUPDATE_STATE_IDLE:
            return "idle";
        case eUPDATE_STATE_PENDING:
            return "pending";
        case eUPDATE_STATE_ACTIVE:
            return "active";
        case eUPDATE_STATE_DONE:
            return "done";
        case eUPDATE_STATE_STOPPED:
            return "stopped";
        case eUPDATE_STATE_FAULT:
            return "fault";
        default:
            return "unknown";
    }
}

static const char *systemDebugGetSelfCheckStateString(eSelfCheckState state)
{
    switch (state) {
        case eSELFCHECK_STATE_IDLE:
            return "idle";
        case eSELFCHECK_STATE_RUNNING:
            return "running";
        case eSELFCHECK_STATE_PASS:
            return "pass";
        case eSELFCHECK_STATE_FAIL:
            return "fail";
        default:
            return "unknown";
    }
}

static const char *systemDebugGetSelfCheckResultString(eSelfCheckResult result)
{
    switch (result) {
        case eSELFCHECK_RESULT_UNKNOWN:
            return "unknown";
        case eSELFCHECK_RESULT_PASS:
            return "pass";
        case eSELFCHECK_RESULT_FAIL:
            return "fail";
        default:
            return "unknown";
    }
}

static eConsoleCommandResult systemDebugConsoleHealthHandler(uint32_t transport, int argc, char *argv[])
{
    const stManagerHealthSummary *lHealth;

    (void)argv;

    if (argc != 1) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    lHealth = managerGetHealthSummary();
    if (lHealth == NULL) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (consoleReply(transport,
        "Manager: level=%s init=%d ready=%lu/%lu running=%lu fault=%lu\n"
        "Power: state=%s lifecycle=%s err=%s low_power=%d stats=%lu/%lu/%lu/%lu/%lu\n"
        "Update: state=%s lifecycle=%s err=%s requested=%d stats=%lu/%lu/%lu/%lu/%lu\n"
        "SelfCheck: state=%s lifecycle=%s err=%s passed=%d items=%s/%s/%s/%s stats=%lu/%lu/%lu/%lu/%lu\n"
        "OK",
        systemDebugGetManagerHealthLevelString(lHealth->level),
        (int)lHealth->isManagerInitialized,
        (unsigned long)lHealth->readyServiceCount,
        (unsigned long)lHealth->totalServiceCount,
        (unsigned long)lHealth->runningServiceCount,
        (unsigned long)lHealth->faultServiceCount,
        systemDebugGetPowerStateString(lHealth->powerState),
        systemDebugGetLifecycleStateString(lHealth->power.lifecycleState),
        systemDebugGetLifecycleErrorString(lHealth->power.lastError),
        (int)lHealth->isLowPowerRequested,
        (unsigned long)lHealth->power.initCount,
        (unsigned long)lHealth->power.startCount,
        (unsigned long)lHealth->power.stopCount,
        (unsigned long)lHealth->power.processCount,
        (unsigned long)lHealth->power.recoverCount,
        systemDebugGetUpdateStateString(lHealth->updateState),
        systemDebugGetLifecycleStateString(lHealth->update.lifecycleState),
        systemDebugGetLifecycleErrorString(lHealth->update.lastError),
        (int)lHealth->isUpdateRequested,
        (unsigned long)lHealth->update.initCount,
        (unsigned long)lHealth->update.startCount,
        (unsigned long)lHealth->update.stopCount,
        (unsigned long)lHealth->update.processCount,
        (unsigned long)lHealth->update.recoverCount,
        systemDebugGetSelfCheckStateString(lHealth->selfCheckSummary.state),
        systemDebugGetLifecycleStateString(lHealth->selfCheck.lifecycleState),
        systemDebugGetLifecycleErrorString(lHealth->selfCheck.lastError),
        (int)lHealth->selfCheckSummary.isPassed,
        systemDebugGetSelfCheckResultString(lHealth->selfCheckSummary.console),
        systemDebugGetSelfCheckResultString(lHealth->selfCheckSummary.appComm),
        systemDebugGetSelfCheckResultString(lHealth->selfCheckSummary.power),
        systemDebugGetSelfCheckResultString(lHealth->selfCheckSummary.update),
        (unsigned long)lHealth->selfCheck.initCount,
        (unsigned long)lHealth->selfCheck.startCount,
        (unsigned long)lHealth->selfCheck.stopCount,
        (unsigned long)lHealth->selfCheck.processCount,
        (unsigned long)lHealth->selfCheck.recoverCount) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

bool systemDebugConsoleRegister(void)
{
#if (SYSTEM_DEBUG_CONSOLE_SUPPORT == 1) && (REP_RTOS_SYSTEM == REP_RTOS_FREERTOS)
    if (!consoleRegisterCommand(&gSystemVersionConsoleCommand)) {
        return false;
    }

    if (!consoleRegisterCommand(&gSystemStatusConsoleCommand)) {
        return false;
    }

    if (!consoleRegisterCommand(&gSystemHealthConsoleCommand)) {
        return false;
    }

    if (!consoleRegisterCommand(&gSystemTaskUsageConsoleCommand)) {
        return false;
    }

    return true;
#elif (SYSTEM_DEBUG_CONSOLE_SUPPORT == 1)
    if (!consoleRegisterCommand(&gSystemVersionConsoleCommand)) {
        return false;
    }

    if (!consoleRegisterCommand(&gSystemStatusConsoleCommand)) {
        return false;
    }

    if (!consoleRegisterCommand(&gSystemHealthConsoleCommand)) {
        return false;
    }

    return true;
#else
    return true;
#endif
}
/**************************End of file********************************/

