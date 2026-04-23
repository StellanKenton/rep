/************************************************************************************
* @file     : log.c
* @brief    : Lightweight logging implementation.
* @details  : Formats log lines once, queues them per transport, and flushes them asynchronously.
* @author   : \.rumi
* @date     : 2026-03-31
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "log.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#define LOG_CONSOLE_INTERNAL_BUILD 1
#include "console.h"
#include "../rtos/rtos.h"

#if (REP_RTOS_SYSTEM == REP_RTOS_NONE) && (REP_MCU_PLATFORM == REP_MCU_PLATFORM_GD32)
#define LOG_HAS_BARE_METAL_IRQ_LOCK 1
#include "gd32f4xx.h"
#else
#define LOG_HAS_BARE_METAL_IRQ_LOCK 0
#endif

#if (REP_MCU_PLATFORM == REP_MCU_PLATFORM_ESP32)
#include "esp_log.h"
#endif

static logTimestampProvider gLogTimestampProvider = NULL;
static bool gLogIsInitialized = false;
static char gLogScratchBuffer[LOG_LINE_BUFFER_SIZE];
static stRepRtosMutex gLogScratchMutex;

static const char *logGetLevelLabel(eLogLevel level);
static const stLogInterface *logGetInterfaces(void);
static bool logIsValidOutputInterface(const stLogInterface *interface);
static bool logIsValidInputInterface(const stLogInterface *interface);
static int32_t logGetInterfaceIndexByTransport(uint32_t transport);
static const stLogInterface *logGetInterfaceByTransport(uint32_t transport);
static stLogOutputState *logGetOutputStateByTransport(uint32_t transport);
static bool logGetOutputBinding(uint32_t transport, const stLogInterface **interface, stLogOutputState **state);
static uint32_t logGetAvailableInterfaceCount(void);
static uint32_t logGetInterfaceCount(void);
static void logEnterCritical(void);
static void logExitCritical(void);
static bool logInitScratchLock(void);
static bool logLockScratch(void);
static void logUnlockScratch(void);
static bool logInitOutputState(stLogOutputState *state);
static void logDropFrame(stLogOutputState *state, uint32_t length);
static void logResetCorruptedQueue(stLogOutputState *state);
static void logCommitWrite(stLogOutputState *state, uint16_t length);
static void logEncodeFrameLength(uint8_t header[LOG_OUTPUT_FRAME_HEADER_SIZE], uint16_t length);
static uint16_t logDecodeFrameLength(const uint8_t header[LOG_OUTPUT_FRAME_HEADER_SIZE]);
static int32_t logQueueOutput(stLogOutputState *state, const uint8_t *buffer, uint16_t length);
static bool logLoadNextFrame(stLogOutputState *state);
static void logProcessInterface(const stLogInterface *interface, stLogOutputState *state);
static void logProcessOutputCore(void);

__attribute__((weak)) const stLogInterface *logGetPlatformInterfaces(void)
{
    return NULL;
}

__attribute__((weak)) uint32_t logGetPlatformInterfaceCount(void)
{
    return 0U;
}

static uint32_t logGetDefaultTimestamp(void)
{
#if (REP_MCU_PLATFORM == REP_MCU_PLATFORM_ESP32)
    return (uint32_t)esp_log_timestamp();
#else
    return repRtosGetTickMs();
#endif
}

static stLogOutputState gLogOutputStates[REP_LOG_OUTPUT_PORT];

static const stLogInterface *logGetInterfaces(void)
{
    return logGetPlatformInterfaces();
}

static bool logIsValidOutputInterface(const stLogInterface *interface)
{
    return (interface != NULL) &&
           (interface->transport != LOG_TRANSPORT_NONE) &&
           (interface->write != NULL) &&
           (interface->isOutputEnabled == true);
}

static bool logIsValidInputInterface(const stLogInterface *interface)
{
    return (interface != NULL) &&
           (interface->transport != LOG_TRANSPORT_NONE) &&
           (interface->getBuffer != NULL) &&
           (interface->isInputEnabled == true);
}

static int32_t logGetInterfaceIndexByTransport(uint32_t transport)
{
    const stLogInterface *lInterfaces = logGetInterfaces();
    uint32_t lIndex = 0U;

    if (lInterfaces == NULL) {
        return -1;
    }

    for (lIndex = 0U; lIndex < logGetInterfaceCount(); lIndex++) {
        if (lInterfaces[lIndex].transport == transport) {
            return (int32_t)lIndex;
        }
    }

    return -1;
}

static const stLogInterface *logGetInterfaceByTransport(uint32_t transport)
{
    int32_t lIndex = logGetInterfaceIndexByTransport(transport);
    const stLogInterface *lInterfaces = logGetInterfaces();

    if ((lIndex < 0) || (lInterfaces == NULL)) {
        return NULL;
    }

    return &lInterfaces[(uint32_t)lIndex];
}

static stLogOutputState *logGetOutputStateByTransport(uint32_t transport)
{
    int32_t lIndex = logGetInterfaceIndexByTransport(transport);

    if (lIndex < 0) {
        return NULL;
    }

    return &gLogOutputStates[(uint32_t)lIndex];
}

static bool logGetOutputBinding(uint32_t transport, const stLogInterface **interface, stLogOutputState **state)
{
    const stLogInterface *lInterface = logGetInterfaceByTransport(transport);
    stLogOutputState *lState = logGetOutputStateByTransport(transport);

    if (!logIsValidOutputInterface(lInterface) || (lState == NULL)) {
        return false;
    }

    if (interface != NULL) {
        *interface = lInterface;
    }

    if (state != NULL) {
        *state = lState;
    }

    return true;
}

static void logEnterCritical(void)
{
    repRtosEnterCritical();
}

static void logExitCritical(void)
{
    repRtosExitCritical();
}

static bool logInitScratchLock(void)
{
    if (!gLogScratchMutex.isCreated) {
        if (repRtosMutexCreate(&gLogScratchMutex) != REP_RTOS_STATUS_OK) {
            return false;
        }
    }

    return true;
}

static bool logLockScratch(void)
{
    uint32_t lWaitMs = 0U;

    if (!logInitScratchLock()) {
        return false;
    }

    if (repRtosIsSchedulerRunning()) {
        lWaitMs = REP_RTOS_WAIT_FOREVER;
    }

    return repRtosMutexTake(&gLogScratchMutex, lWaitMs) == REP_RTOS_STATUS_OK;
}

static void logUnlockScratch(void)
{
    (void)repRtosMutexGive(&gLogScratchMutex);
}

static bool logInitOutputState(stLogOutputState *state)
{
    if (state == NULL) {
        return false;
    }

    (void)memset(state, 0, sizeof(*state));
    if (ringBufferInit(&state->queue, state->queueStorage, LOG_OUTPUT_QUEUE_SIZE) != RINGBUFFER_OK) {
        return false;
    }

    state->isQueueInitialized = true;
    return true;
}

static void logDropFrame(stLogOutputState *state, uint32_t length)
{
    if (state == NULL) {
        return;
    }

    logEnterCritical();
    state->droppedLines++;
    state->droppedBytes += length;
    logExitCritical();
}

static void logResetCorruptedQueue(stLogOutputState *state)
{
    if (state == NULL) {
        return;
    }

    state->queue.tail = state->queue.head;
    state->activeFrameLength = 0U;
    state->activeFrameOffset = 0U;
    if (state->pendingBytes != 0U) {
        state->droppedLines++;
        state->droppedBytes += state->pendingBytes;
        state->pendingBytes = 0U;
    }
}

static void logCommitWrite(stLogOutputState *state, uint16_t length)
{
    if ((state == NULL) || (length == 0U)) {
        return;
    }

    logEnterCritical();
    state->sentBytes += (uint32_t)length;
    if (state->pendingBytes >= (uint32_t)length) {
        state->pendingBytes -= (uint32_t)length;
    } else {
        state->pendingBytes = 0U;
    }
    logExitCritical();
}

static void logEncodeFrameLength(uint8_t header[LOG_OUTPUT_FRAME_HEADER_SIZE], uint16_t length)
{
    header[0] = (uint8_t)(length & 0xFFU);
    header[1] = (uint8_t)((length >> 8U) & 0xFFU);
}

static uint16_t logDecodeFrameLength(const uint8_t header[LOG_OUTPUT_FRAME_HEADER_SIZE])
{
    return (uint16_t)((uint16_t)header[0] | ((uint16_t)header[1] << 8U));
}

static int32_t logQueueOutput(stLogOutputState *state, const uint8_t *buffer, uint16_t length)
{
    uint8_t lHeader[LOG_OUTPUT_FRAME_HEADER_SIZE];
    uint32_t lHeaderWritten = 0U;
    uint32_t lPayloadWritten = 0U;
    uint32_t lRequiredLength = (uint32_t)length + LOG_OUTPUT_FRAME_HEADER_SIZE;

    if ((state == NULL) || (buffer == NULL) || (length == 0U) || (state->isQueueInitialized == false)) {
        return 0;
    }

    if (length > LOG_OUTPUT_MAX_FRAME_SIZE) {
        logDropFrame(state, (uint32_t)length);
        return 0;
    }

    logEncodeFrameLength(lHeader, length);

    logEnterCritical();
    if (ringBufferGetFree(&state->queue) < lRequiredLength) {
        logExitCritical();
        logDropFrame(state, (uint32_t)length);
        return 0;
    }

    lHeaderWritten = ringBufferWrite(&state->queue, lHeader, LOG_OUTPUT_FRAME_HEADER_SIZE);
    if (lHeaderWritten != LOG_OUTPUT_FRAME_HEADER_SIZE) {
        if (lHeaderWritten > 0U) {
            state->queue.head -= lHeaderWritten;
        }
        logExitCritical();
        logDropFrame(state, (uint32_t)length);
        return 0;
    }

    lPayloadWritten = ringBufferWrite(&state->queue, buffer, length);
    if (lPayloadWritten != length) {
        state->queue.head -= (LOG_OUTPUT_FRAME_HEADER_SIZE + lPayloadWritten);
        logExitCritical();
        logDropFrame(state, (uint32_t)length);
        return 0;
    }

    state->pendingBytes += (uint32_t)length;
    logExitCritical();

    return (int32_t)length;
}

static bool logLoadNextFrame(stLogOutputState *state)
{
    uint8_t lHeader[LOG_OUTPUT_FRAME_HEADER_SIZE];
    uint32_t lHeaderRead = 0U;
    uint32_t lPayloadRead = 0U;
    uint16_t lFrameLength = 0U;

    if ((state == NULL) || (state->isQueueInitialized == false) || (state->activeFrameLength != 0U)) {
        return false;
    }

    logEnterCritical();
    if (ringBufferGetUsed(&state->queue) < LOG_OUTPUT_FRAME_HEADER_SIZE) {
        logExitCritical();
        return false;
    }

    lHeaderRead = ringBufferRead(&state->queue, lHeader, LOG_OUTPUT_FRAME_HEADER_SIZE);
    if (lHeaderRead != LOG_OUTPUT_FRAME_HEADER_SIZE) {
        if (lHeaderRead > 0U) {
            state->queue.tail -= lHeaderRead;
        }
        logExitCritical();
        return false;
    }

    lFrameLength = logDecodeFrameLength(lHeader);
    if ((lFrameLength == 0U) ||
        (lFrameLength > LOG_OUTPUT_MAX_FRAME_SIZE) ||
        (ringBufferGetUsed(&state->queue) < lFrameLength)) {
        logResetCorruptedQueue(state);
        logExitCritical();
        return false;
    }

    lPayloadRead = ringBufferRead(&state->queue, state->activeFrame, lFrameLength);
    if (lPayloadRead != lFrameLength) {
        logResetCorruptedQueue(state);
        logExitCritical();
        return false;
    }

    state->activeFrameLength = lFrameLength;
    state->activeFrameOffset = 0U;
    logExitCritical();

    return true;
}

static uint32_t logGetAvailableInterfaceCount(void)
{
    return logGetPlatformInterfaceCount();
}

static uint32_t logGetInterfaceCount(void)
{
    uint32_t lAvailableCount = logGetAvailableInterfaceCount();

    if (REP_LOG_OUTPUT_PORT < lAvailableCount) {
        return REP_LOG_OUTPUT_PORT;
    }

    return lAvailableCount;
}

static bool logIsValidLevel(eLogLevel level)
{
    return (level > LOG_LEVEL_NONE) && (level <= LOG_LEVEL_DEBUG);
}

static uint16_t logFormatLine(char *buffer, uint16_t capacity, eLogLevel level, const char *tag, const char *format, va_list args)
{
    const char *lTag = (tag != NULL) ? tag : "app";
    uint32_t lTimestamp = 0U;
    int lPrefixLength = 0;
    int lMessageLength = 0;
    uint16_t lUsed = 0U;
    va_list lArgsCopy;

    if (buffer == NULL || capacity == 0U || format == NULL || !logIsValidLevel(level)) {
        return 0U;
    }

    if (gLogTimestampProvider == NULL) {
        gLogTimestampProvider = logGetDefaultTimestamp;
    }

    lTimestamp = gLogTimestampProvider();
    lPrefixLength = snprintf(buffer, capacity, "%s (%" PRIu32 ") %s: ", logGetLevelLabel(level), lTimestamp, lTag);
    if (lPrefixLength < 0) {
        return 0U;
    }

    if ((uint16_t)lPrefixLength >= capacity) {
        buffer[capacity - 1U] = '\n';
        return capacity;
    }

    va_copy(lArgsCopy, args);
    lMessageLength = vsnprintf(&buffer[lPrefixLength], capacity - (uint16_t)lPrefixLength, format, lArgsCopy);
    va_end(lArgsCopy);

    if (lMessageLength < 0) {
        return 0U;
    }

    lUsed = (uint16_t)lPrefixLength;
    if ((uint16_t)lMessageLength >= (capacity - lUsed)) {
        lUsed = capacity - 1U;
    } else {
        lUsed = lUsed + (uint16_t)lMessageLength;
    }

    if (lUsed >= capacity) {
        lUsed = capacity - 1U;
    }

    if (lUsed < (capacity - 1U)) {
        buffer[lUsed] = '\n';
        lUsed++;
        buffer[lUsed] = '\0';
    } else {
        buffer[capacity - 2U] = '\n';
        buffer[capacity - 1U] = '\0';
        lUsed = capacity - 1U;
    }

    return lUsed;
}

bool logInit(void)
{
    const stLogInterface *lInterfaces = logGetInterfaces();
    uint32_t lIndex = 0U;

    if (gLogIsInitialized) {
        return true;
    }

    gLogTimestampProvider = logGetDefaultTimestamp;
    (void)memset(gLogOutputStates, 0, sizeof(gLogOutputStates));
    if (!logInitScratchLock()) {
        return false;
    }

    if (lInterfaces == NULL) {
        return false;
    }

    for (lIndex = 0U; lIndex < logGetInterfaceCount(); lIndex++) {
        if (logIsValidOutputInterface(&lInterfaces[lIndex]) &&
            !logInitOutputState(&gLogOutputStates[lIndex])) {
            return false;
        }

        if ((logIsValidOutputInterface(&lInterfaces[lIndex]) || logIsValidInputInterface(&lInterfaces[lIndex])) &&
            (lInterfaces[lIndex].init != NULL)) {
            lInterfaces[lIndex].init();
        }
    }

    gLogIsInitialized = true;

    if (!consoleCoreInit()) {
        gLogIsInitialized = false;
        return false;
    }

    return true;
}

bool logRegisterConsole(const stConsoleCommand *command)
{
    if (!logInit()) {
        return false;
    }

    return consoleCoreRegisterCommand(command);
}

uint32_t logGetInputCount(void)
{
    const stLogInterface *lInterfaces = logGetInterfaces();
    uint32_t lIndex = 0U;
    uint32_t lCount = 0U;

    if (lInterfaces == NULL) {
        return 0U;
    }

    for (lIndex = 0U; lIndex < logGetInterfaceCount(); lIndex++) {
        if (logIsValidInputInterface(&lInterfaces[lIndex])) {
            lCount++;
        }
    }

    return lCount;
}

uint32_t logGetInputTransport(uint32_t index)
{
    const stLogInterface *lInterfaces = logGetInterfaces();
    uint32_t lIndex = 0U;
    uint32_t lCount = 0U;

    if (lInterfaces == NULL) {
        return LOG_TRANSPORT_NONE;
    }

    for (lIndex = 0U; lIndex < logGetInterfaceCount(); lIndex++) {
        if (!logIsValidInputInterface(&lInterfaces[lIndex])) {
            continue;
        }

        if (lCount == index) {
            return lInterfaces[lIndex].transport;
        }

        lCount++;
    }

    return LOG_TRANSPORT_NONE;
}

stRingBuffer *logGetInputBuffer(uint32_t transport)
{
    const stLogInterface *lInterface = NULL;

    lInterface = logGetInterfaceByTransport(transport);
    if (!logIsValidInputInterface(lInterface)) {
        return NULL;
    }

    return lInterface->getBuffer();
}

int32_t logWriteToTransport(uint32_t transport, const uint8_t *buffer, uint16_t length)
{
    stLogOutputState *lOutputState = NULL;

    if ((buffer == NULL) || (length == 0U)) {
        return 0;
    }

    if (!logInit()) {
        return 0;
    }

    if (!logGetOutputBinding(transport, NULL, &lOutputState)) {
        return 0;
    }

    return logQueueOutput(lOutputState, buffer, length);
}

int32_t logDirectWriteToTransport(uint32_t transport, const uint8_t *buffer, uint16_t length)
{
    const stLogInterface *lInterface = NULL;
    stLogOutputState *lOutputState = NULL;
    int32_t lWriteLength = 0;

    if ((buffer == NULL) || (length == 0U)) {
        return 0;
    }

    if (!logInit()) {
        return 0;
    }

    if (!logGetOutputBinding(transport, &lInterface, &lOutputState)) {
        return 0;
    }

    lWriteLength = lInterface->write(buffer, length);
    if (lWriteLength <= 0) {
        logEnterCritical();
        lOutputState->busyCount++;
        logExitCritical();
        return 0;
    }

    if ((uint16_t)lWriteLength > length) {
        lWriteLength = (int32_t)length;
    }

    logCommitWrite(lOutputState, (uint16_t)lWriteLength);

    return lWriteLength;
}

static void logProcessInterface(const stLogInterface *interface, stLogOutputState *state)
{
    uint16_t lBudget = LOG_OUTPUT_PROCESS_BUDGET;
    uint16_t lRemainingLength = 0U;
    uint16_t lRequestLength = 0U;
    int32_t lWriteLength = 0;

    if (!logIsValidOutputInterface(interface) || (state == NULL) || (state->isQueueInitialized == false)) {
        return;
    }

    while (lBudget > 0U) {
        if ((state->activeFrameLength == 0U) && !logLoadNextFrame(state)) {
            break;
        }

        lRemainingLength = (uint16_t)(state->activeFrameLength - state->activeFrameOffset);
        if (lRemainingLength == 0U) {
            state->activeFrameLength = 0U;
            state->activeFrameOffset = 0U;
            continue;
        }

        lRequestLength = (lRemainingLength < lBudget) ? lRemainingLength : lBudget;
        lWriteLength = interface->write(&state->activeFrame[state->activeFrameOffset], lRequestLength);
        if (lWriteLength <= 0) {
            logEnterCritical();
            state->busyCount++;
            logExitCritical();
            break;
        }

        if ((uint16_t)lWriteLength > lRequestLength) {
            lWriteLength = (int32_t)lRequestLength;
        }

        logEnterCritical();
        state->activeFrameOffset = (uint16_t)(state->activeFrameOffset + (uint16_t)lWriteLength);
        if (state->activeFrameOffset >= state->activeFrameLength) {
            state->activeFrameLength = 0U;
            state->activeFrameOffset = 0U;
        }
        logExitCritical();

        logCommitWrite(state, (uint16_t)lWriteLength);

        lBudget = (uint16_t)(lBudget - (uint16_t)lWriteLength);
        if ((uint16_t)lWriteLength < lRequestLength) {
            break;
        }
    }
}

static void logProcessOutputCore(void)
{
    const stLogInterface *lInterfaces = logGetInterfaces();
    uint32_t lIndex = 0U;

    if (!logInit()) {
        return;
    }

    if (lInterfaces == NULL) {
        return;
    }

    for (lIndex = 0U; lIndex < logGetInterfaceCount(); lIndex++) {
        if (!logIsValidOutputInterface(&lInterfaces[lIndex])) {
            continue;
        }

        logProcessInterface(&lInterfaces[lIndex], &gLogOutputStates[lIndex]);
    }
}

void logProcessOutput(void)
{
    if (!logInit()) {
        return;
    }

    logProcessOutputCore();
}

void ConsoleBackGournd(void)
{
    if (!logInit()) {
        return;
    }

    logProcessOutputCore();
    logPlatformConsolePoll();
    consoleCoreProcess();
}

bool logGetStats(uint32_t transport, stLogOutputStats *stats)
{
    stLogOutputState *lOutputState = NULL;

    if (stats == NULL) {
        return false;
    }

    (void)memset(stats, 0, sizeof(*stats));
    if (!logInit()) {
        return false;
    }

    if (!logGetOutputBinding(transport, NULL, &lOutputState) ||
        (lOutputState->isQueueInitialized == false)) {
        return false;
    }

    logEnterCritical();
    stats->transport = transport;
    stats->pendingBytes = lOutputState->pendingBytes;
    stats->droppedLines = lOutputState->droppedLines;
    stats->droppedBytes = lOutputState->droppedBytes;
    stats->sentBytes = lOutputState->sentBytes;
    stats->busyCount = lOutputState->busyCount;
    stats->hasPendingFrame = (lOutputState->activeFrameLength != 0U);
    logExitCritical();

    return true;
}


void logSetTimestampProvider(logTimestampProvider provider)
{
    gLogTimestampProvider = (provider != NULL) ? provider : logGetDefaultTimestamp;
}

static const char *logGetLevelLabel(eLogLevel level)
{
    switch (level) {
        case LOG_LEVEL_ERROR:
            return "E";
        case LOG_LEVEL_WARN:
            return "W";
        case LOG_LEVEL_INFO:
            return "I";
        case LOG_LEVEL_DEBUG:
            return "D";
        default:
            return "?";
    }
}

void logVWrite(eLogLevel level, const char *tag, const char *format, va_list args)
{
    const stLogInterface *lInterfaces = logGetInterfaces();
    uint16_t lLength = 0U;
    uint32_t lIndex = 0U;

    if (!logInit()) {
        return;
    }

    if (lInterfaces == NULL) {
        return;
    }

    if (!logLockScratch()) {
        return;
    }

    lLength = logFormatLine(gLogScratchBuffer, (uint16_t)sizeof(gLogScratchBuffer), level, tag, format, args);
    if (lLength == 0U) {
        logUnlockScratch();
        return;
    }

    for (lIndex = 0U; lIndex < logGetInterfaceCount(); lIndex++) {
        if (!logIsValidOutputInterface(&lInterfaces[lIndex])) {
            continue;
        }

        (void)logQueueOutput(&gLogOutputStates[lIndex], (const uint8_t *)gLogScratchBuffer, lLength);
    }

    logUnlockScratch();
}

void logWrite(eLogLevel level, const char *tag, const char *format, ...)
{
    va_list lArgs;

    va_start(lArgs, format);
    logVWrite(level, tag, format, lArgs);
    va_end(lArgs);
}

/**************************End of file********************************/
