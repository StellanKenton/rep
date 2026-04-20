#define LOG_CONSOLE_INTERNAL_BUILD 1
/************************************************************************************
* @file     : console.c
* @brief    : Text command console core implementation.
* @details  : Polls input transports, assembles lines, dispatches commands, and replies.
* @author   : GitHub Copilot
* @date     : 2026-03-31
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "console.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "log.h"
#include "ringbuffer.h"
#include "../rtos/rtos.h"

static bool gConsoleIsInitialized = false;
static stConsoleSession gConsoleSessions[CONSOLE_MAX_SESSIONS];
static uint32_t gConsoleSessionCount = 0U;
static stConsoleCommand gConsoleCommands[CONSOLE_MAX_COMMANDS];
static uint32_t gConsoleCommandCount = 0U;
static char gConsoleReplyBuffer[CONSOLE_REPLY_BUFFER_SIZE];
static stRepRtosMutex gConsoleReplyMutex;

static uint32_t consoleGetTick(void);
static bool consoleInitReplyLock(void);
static bool consoleLockReplyBuffer(void);
static void consoleUnlockReplyBuffer(void);
static bool consoleIsSpace(char value);
static bool consoleIsSameString(const char *left, const char *right);
static bool consoleIsSameCommand(const stConsoleCommand *left, const stConsoleCommand *right);
static bool consoleIsHelpCommand(const char *commandName);
static bool consoleIsReservedCommandName(const char *commandName);
static const char *consoleGetCommandGroupName(const stConsoleCommand *command);
static const stConsoleCommand *consoleFindCommand(const char *commandName);
static void consoleResetSession(stConsoleSession *session);
static int consoleTokenize(char *lineBuffer, char *argv[], int maxArgs);
static eConsoleCommandResult consoleReplyCommandGroups(uint32_t transport);
static void consoleHandleLine(stConsoleSession *session);
static void consoleProcessByte(stConsoleSession *session, uint8_t data);
static void consoleProcessSession(stConsoleSession *session);

static uint32_t consoleGetTick(void)
{
    return repRtosGetTickMs();
}

static bool consoleInitReplyLock(void)
{
    if (!gConsoleReplyMutex.isCreated) {
        if (repRtosMutexCreate(&gConsoleReplyMutex) != REP_RTOS_STATUS_OK) {
            return false;
        }
    }

    return true;
}

static bool consoleLockReplyBuffer(void)
{
    uint32_t lWaitMs = 0U;

    if (!consoleInitReplyLock()) {
        return false;
    }

    if (repRtosIsSchedulerRunning()) {
        lWaitMs = REP_RTOS_WAIT_FOREVER;
    }

    return repRtosMutexTake(&gConsoleReplyMutex, lWaitMs) == REP_RTOS_STATUS_OK;
}

static void consoleUnlockReplyBuffer(void)
{
    (void)repRtosMutexGive(&gConsoleReplyMutex);
}

static bool consoleIsSpace(char value)
{
    return (value == ' ') || (value == '\t');
}

static bool consoleIsSameString(const char *left, const char *right)
{
    if (left == right) {
        return true;
    }

    if ((left == NULL) || (right == NULL)) {
        return false;
    }

    return strcmp(left, right) == 0;
}

static bool consoleIsSameCommand(const stConsoleCommand *left, const stConsoleCommand *right)
{
    if ((left == NULL) || (right == NULL)) {
        return false;
    }

    return consoleIsSameString(left->commandName, right->commandName) &&
        consoleIsSameString(left->helpText, right->helpText) &&
        consoleIsSameString(left->ownerTag, right->ownerTag) &&
        (left->handler == right->handler);
}

static bool consoleIsHelpCommand(const char *commandName)
{
    return consoleIsSameString(commandName, "help");
}

static bool consoleIsReservedCommandName(const char *commandName)
{
    return consoleIsHelpCommand(commandName);
}

static const char *consoleGetCommandGroupName(const stConsoleCommand *command)
{
    if (command == NULL) {
        return NULL;
    }

    if ((command->ownerTag != NULL) && (command->ownerTag[0] != '\0')) {
        return command->ownerTag;
    }

    return command->commandName;
}

static const stConsoleCommand *consoleFindCommand(const char *commandName)
{
    uint32_t lIndex = 0U;

    if (commandName == NULL) {
        return NULL;
    }

    for (lIndex = 0U; lIndex < gConsoleCommandCount; lIndex++) {
        if (strcmp(gConsoleCommands[lIndex].commandName, commandName) == 0) {
            return &gConsoleCommands[lIndex];
        }
    }

    return NULL;
}

static void consoleResetSession(stConsoleSession *session)
{
    if (session == NULL) {
        return;
    }

    session->lineLength = 0U;
    session->isLineOverflow = false;
    session->lineBuffer[0] = '\0';
}

static int consoleTokenize(char *lineBuffer, char *argv[], int maxArgs)
{
    int lArgc = 0;
    char *lCursor = lineBuffer;

    if ((lineBuffer == NULL) || (argv == NULL) || (maxArgs <= 0)) {
        return -1;
    }

    while (*lCursor != '\0') {
        while (consoleIsSpace(*lCursor)) {
            lCursor++;
        }

        if (*lCursor == '\0') {
            break;
        }

        if (lArgc >= maxArgs) {
            return -1;
        }

        argv[lArgc] = lCursor;
        lArgc++;

        while ((*lCursor != '\0') && !consoleIsSpace(*lCursor)) {
            lCursor++;
        }

        if (*lCursor == '\0') {
            break;
        }

        *lCursor = '\0';
        lCursor++;
    }

    return lArgc;
}

static eConsoleCommandResult consoleReplyCommandGroups(uint32_t transport)
{
    uint32_t lIndex = 0U;

    if (logConsoleReply(transport, "Available commands:") <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (gConsoleCommandCount == 0U) {
        if (logConsoleReply(transport, "(none)") <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        if (logConsoleReply(transport, "OK") <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_OK;
    }

    for (lIndex = 0U; lIndex < gConsoleCommandCount; lIndex++) {
        const char *lGroupName = consoleGetCommandGroupName(&gConsoleCommands[lIndex]);
        const char *lHelpText = gConsoleCommands[lIndex].helpText;

        if ((lHelpText != NULL) && (lHelpText[0] != '\0')) {
            if ((lGroupName != NULL) && (lGroupName[0] != '\0')) {
                if (logConsoleReply(transport, "%s: %s", lGroupName, lHelpText) <= 0) {
                    return CONSOLE_COMMAND_RESULT_ERROR;
                }
            } else if (logConsoleReply(transport, "%s", lHelpText) <= 0) {
                return CONSOLE_COMMAND_RESULT_ERROR;
            }
        } else if ((lGroupName != NULL) && (lGroupName[0] != '\0')) {
            if (logConsoleReply(transport, "%s: %s", lGroupName, gConsoleCommands[lIndex].commandName) <= 0) {
                return CONSOLE_COMMAND_RESULT_ERROR;
            }
        } else if (logConsoleReply(transport, "%s", gConsoleCommands[lIndex].commandName) <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }
    }

    if (logConsoleReply(transport, "OK") <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static void consoleHandleLine(stConsoleSession *session)
{
    char *lArgv[CONSOLE_MAX_ARGS];
    int lArgc = 0;
    const stConsoleCommand *lCommand = NULL;
    eConsoleCommandResult lResult;

    if (session == NULL) {
        return;
    }

    if (session->isLineOverflow) {
        (void)logConsoleReply(session->transport, "ERROR: command buffer overflow");
        consoleResetSession(session);
        return;
    }

    if (session->lineLength == 0U) {
        consoleResetSession(session);
        return;
    }

    session->lineBuffer[session->lineLength] = '\0';
    lArgc = consoleTokenize(session->lineBuffer, lArgv, CONSOLE_MAX_ARGS);
    if (lArgc <= 0) {
        (void)logConsoleReply(session->transport, "ERROR: invalid argument");
        consoleResetSession(session);
        return;
    }

    if (consoleIsHelpCommand(lArgv[0])) {
        if (lArgc != 1) {
            (void)logConsoleReply(session->transport, "ERROR: invalid argument\nUsage: help");
        } else if (consoleReplyCommandGroups(session->transport) != CONSOLE_COMMAND_RESULT_OK) {
            (void)logConsoleReply(session->transport, "ERROR: command failed");
        }

        consoleResetSession(session);
        return;
    }

    lCommand = consoleFindCommand(lArgv[0]);
    if (lCommand == NULL) {
        (void)logConsoleReply(session->transport, "ERROR: unknown command");
        consoleResetSession(session);
        return;
    }

    lResult = lCommand->handler(session->transport, lArgc, lArgv);
    if (lResult == CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT) {
        if ((lCommand->helpText != NULL) && (lCommand->helpText[0] != '\0')) {
            (void)logConsoleReply(session->transport, "ERROR: invalid argument\nUsage: %s", lCommand->helpText);
        } else {
            (void)logConsoleReply(session->transport, "ERROR: invalid argument");
        }
    } else if (lResult != CONSOLE_COMMAND_RESULT_OK) {
        (void)logConsoleReply(session->transport, "ERROR: command failed");
    }

    consoleResetSession(session);
}

static void consoleProcessByte(stConsoleSession *session, uint8_t data)
{
    if (session == NULL) {
        return;
    }

    session->lastActivityTick = consoleGetTick();

    if ((data == '\r') || (data == '\n')) {
        consoleHandleLine(session);
        return;
    }

    if (session->isLineOverflow) {
        return;
    }

    if (session->lineLength >= (CONSOLE_MAX_LINE_LENGTH - 1U)) {
        session->isLineOverflow = true;
        return;
    }

    session->lineBuffer[session->lineLength] = (char)data;
    session->lineLength++;
    session->lineBuffer[session->lineLength] = '\0';
}

static void consoleProcessSession(stConsoleSession *session)
{
    uint32_t lProcessed = 0U;
    uint8_t lData = 0U;

    if (session == NULL) {
        return;
    }

    session->inputBuffer = logGetInputBuffer(session->transport);
    if (session->inputBuffer == NULL) {
        return;
    }

    while ((lProcessed < CONSOLE_PROCESS_BYTES_PER_SESSION) &&
           (ringBufferPopByte(session->inputBuffer, &lData) == RINGBUFFER_OK)) {
        consoleProcessByte(session, lData);
        lProcessed++;
    }
}

bool consoleCoreInit(void)
{
    uint32_t lInputCount = 0U;
    uint32_t lTransportIndex = 0U;

    if (gConsoleIsInitialized) {
        return true;
    }

    (void)memset(gConsoleSessions, 0, sizeof(gConsoleSessions));
    (void)memset(gConsoleCommands, 0, sizeof(gConsoleCommands));
    gConsoleSessionCount = 0U;
    gConsoleCommandCount = 0U;

    lInputCount = logGetInputCount();
    if (lInputCount > CONSOLE_MAX_SESSIONS) {
        lInputCount = CONSOLE_MAX_SESSIONS;
    }

    for (lTransportIndex = 0U; lTransportIndex < lInputCount; lTransportIndex++) {
        uint32_t lTransport = logGetInputTransport(lTransportIndex);
        stRingBuffer *lInputBuffer = logGetInputBuffer(lTransport);

        if ((lTransport == LOG_TRANSPORT_NONE) || (lInputBuffer == NULL)) {
            continue;
        }

        gConsoleSessions[gConsoleSessionCount].transport = lTransport;
        gConsoleSessions[gConsoleSessionCount].inputBuffer = lInputBuffer;
        gConsoleSessions[gConsoleSessionCount].lastActivityTick = 0U;
        consoleResetSession(&gConsoleSessions[gConsoleSessionCount]);
        gConsoleSessionCount++;
    }

    gConsoleIsInitialized = true;
    return true;
}

bool consoleRegisterCommand(const stConsoleCommand *command)
{
    const stConsoleCommand *lExistingCommand = NULL;

    if ((command == NULL) ||
        (command->commandName == NULL) ||
        (command->commandName[0] == '\0') ||
        consoleIsReservedCommandName(command->commandName) ||
        (command->handler == NULL)) {
        return false;
    }

    if (!gConsoleIsInitialized && !consoleCoreInit()) {
        return false;
    }

    lExistingCommand = consoleFindCommand(command->commandName);
    if (lExistingCommand != NULL) {
        return consoleIsSameCommand(command, lExistingCommand);
    }

    if (gConsoleCommandCount >= CONSOLE_MAX_COMMANDS) {
        return false;
    }

    gConsoleCommands[gConsoleCommandCount] = *command;
    gConsoleCommandCount++;
    return true;
}

bool consoleCoreRegisterCommand(const stConsoleCommand *command)
{
    return consoleRegisterCommand(command);
}

void consoleCoreProcess(void)
{
    uint32_t lIndex = 0U;

    if (!gConsoleIsInitialized) {
        return;
    }

    for (lIndex = 0U; lIndex < gConsoleSessionCount; lIndex++) {
        consoleProcessSession(&gConsoleSessions[lIndex]);
    }
}

__attribute__((weak)) void logPlatformConsolePoll(void)
{
}

int32_t logConsoleReply(uint32_t transport, const char *format, ...)
{
    int lLength = 0;
    int32_t lTotalWritten = 0;
    int32_t lWritten = 0;
    uint16_t lChunkLength = 0U;
    uint16_t lOffset = 0U;
    va_list lArgs;

    if (format == NULL) {
        return 0;
    }

    if (!consoleLockReplyBuffer()) {
        return 0;
    }

    va_start(lArgs, format);
    lLength = vsnprintf(gConsoleReplyBuffer, sizeof(gConsoleReplyBuffer), format, lArgs);
    va_end(lArgs);

    if (lLength < 0) {
        consoleUnlockReplyBuffer();
        return 0;
    }

    if (lLength >= (int)sizeof(gConsoleReplyBuffer)) {
        lLength = (int)sizeof(gConsoleReplyBuffer) - 1;
        gConsoleReplyBuffer[lLength] = '\0';
    }

    if ((lLength == 0) || (gConsoleReplyBuffer[lLength - 1] != '\n')) {
        if (lLength >= ((int)sizeof(gConsoleReplyBuffer) - 1)) {
            lLength = (int)sizeof(gConsoleReplyBuffer) - 2;
        }
        gConsoleReplyBuffer[lLength] = '\n';
        lLength++;
        gConsoleReplyBuffer[lLength] = '\0';
    }

    while (lOffset < (uint16_t)lLength) {
        lChunkLength = (uint16_t)lLength - lOffset;
        if (lChunkLength > LOG_OUTPUT_MAX_FRAME_SIZE) {
            lChunkLength = LOG_OUTPUT_MAX_FRAME_SIZE;
        }

        lWritten = logWriteToTransport(transport, (const uint8_t *)&gConsoleReplyBuffer[lOffset], lChunkLength);
        if (lWritten <= 0) {
            consoleUnlockReplyBuffer();
            return 0;
        }

        lTotalWritten += lWritten;
        lOffset = (uint16_t)(lOffset + lChunkLength);
    }

    consoleUnlockReplyBuffer();

    return lTotalWritten;
}

/**************************End of file********************************/
