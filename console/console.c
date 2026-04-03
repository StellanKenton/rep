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

#if (REP_RTOS_SYSTEM == REP_RTOS_FREERTOS)
#include "FreeRTOS.h"
#include "task.h"
#endif

static bool gConsoleIsInitialized = false;
static stConsoleSession gConsoleSessions[CONSOLE_MAX_SESSIONS];
static uint32_t gConsoleSessionCount = 0U;
static stConsoleCommand gConsoleCommands[CONSOLE_MAX_COMMANDS];
static uint32_t gConsoleCommandCount = 0U;

static uint32_t consoleGetTick(void);
static bool consoleIsSpace(char value);
static bool consoleIsSameString(const char *left, const char *right);
static bool consoleIsSameCommand(const stConsoleCommand *left, const stConsoleCommand *right);
static const stConsoleCommand *consoleFindCommand(const char *commandName);
static void consoleResetSession(stConsoleSession *session);
static int consoleTokenize(char *lineBuffer, char *argv[], int maxArgs);
static void consoleHandleLine(stConsoleSession *session);
static void consoleProcessByte(stConsoleSession *session, uint8_t data);
static void consoleProcessSession(stConsoleSession *session);

static uint32_t consoleGetTick(void)
{
#if (REP_RTOS_SYSTEM == REP_RTOS_FREERTOS)
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
#else
    return 0U;
#endif
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
        (void)consoleReply(session->transport, "ERROR: command buffer overflow");
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
        (void)consoleReply(session->transport, "ERROR: invalid argument");
        consoleResetSession(session);
        return;
    }

    lCommand = consoleFindCommand(lArgv[0]);
    if (lCommand == NULL) {
        (void)consoleReply(session->transport, "ERROR: unknown command");
        consoleResetSession(session);
        return;
    }

    lResult = lCommand->handler(session->transport, lArgc, lArgv);
    if (lResult == CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT) {
        if ((lCommand->helpText != NULL) && (lCommand->helpText[0] != '\0')) {
            (void)consoleReply(session->transport, "ERROR: invalid argument\nUsage: %s", lCommand->helpText);
        } else {
            (void)consoleReply(session->transport, "ERROR: invalid argument");
        }
    } else if (lResult != CONSOLE_COMMAND_RESULT_OK) {
        (void)consoleReply(session->transport, "ERROR: command failed");
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

bool consoleInit(void)
{
    uint32_t lInputCount = 0U;
    uint32_t lTransportIndex = 0U;

    if (gConsoleIsInitialized) {
        return true;
    }

    if (!logInit()) {
        return false;
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
        (command->handler == NULL)) {
        return false;
    }

    if (!gConsoleIsInitialized && !consoleInit()) {
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

void consoleProcess(void)
{
    uint32_t lIndex = 0U;

    logProcessOutput();

    if (!gConsoleIsInitialized && !consoleInit()) {
        return;
    }
    
    for (lIndex = 0U; lIndex < gConsoleSessionCount; lIndex++) {
        consoleProcessSession(&gConsoleSessions[lIndex]);
    }
}

int32_t consoleReply(uint32_t transport, const char *format, ...)
{
    char lBuffer[CONSOLE_REPLY_BUFFER_SIZE];
    int lLength = 0;
    va_list lArgs;

    if (format == NULL) {
        return 0;
    }

    va_start(lArgs, format);
    lLength = vsnprintf(lBuffer, sizeof(lBuffer), format, lArgs);
    va_end(lArgs);

    if (lLength < 0) {
        return 0;
    }

    if (lLength >= (int)sizeof(lBuffer)) {
        lLength = (int)sizeof(lBuffer) - 1;
        lBuffer[lLength] = '\0';
    }

    if ((lLength == 0) || (lBuffer[lLength - 1] != '\n')) {
        if (lLength >= ((int)sizeof(lBuffer) - 1)) {
            lLength = (int)sizeof(lBuffer) - 2;
        }
        lBuffer[lLength] = '\n';
        lLength++;
        lBuffer[lLength] = '\0';
    }

    return logWriteToTransport(transport, (const uint8_t *)lBuffer, (uint16_t)lLength);
}
/**************************End of file********************************/
