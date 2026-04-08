/************************************************************************************
* @file     : console.h
* @brief    : Text command console built on top of the log transport layer.
* @details  : Provides command registration, transport-aware reply, and polling APIs.
* @author   : GitHub Copilot
* @date     : 2026-03-31
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdbool.h>
#include <stdint.h>

#include "ringbuffer.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CONSOLE_MAX_SESSIONS
#define CONSOLE_MAX_SESSIONS                4U
#endif

#ifndef CONSOLE_MAX_COMMANDS
#define CONSOLE_MAX_COMMANDS                16U
#endif

#ifndef CONSOLE_MAX_LINE_LENGTH
#define CONSOLE_MAX_LINE_LENGTH             96U
#endif

#ifndef CONSOLE_MAX_ARGS
#define CONSOLE_MAX_ARGS                    8
#endif

#ifndef CONSOLE_PROCESS_BYTES_PER_SESSION
#define CONSOLE_PROCESS_BYTES_PER_SESSION   64U
#endif

#ifndef CONSOLE_REPLY_BUFFER_SIZE
#define CONSOLE_REPLY_BUFFER_SIZE           512U
#endif

typedef struct stConsoleSession {
    uint32_t transport;
    stRingBuffer *inputBuffer;
    char lineBuffer[CONSOLE_MAX_LINE_LENGTH];
    uint16_t lineLength;
    bool isLineOverflow;
    uint32_t lastActivityTick;
} stConsoleSession;

typedef enum eConsoleCommandResult {
    CONSOLE_COMMAND_RESULT_OK = 0,
    CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT,
    CONSOLE_COMMAND_RESULT_ERROR,
} eConsoleCommandResult;

typedef eConsoleCommandResult (*consoleCommandHandler)(uint32_t transport, int argc, char *argv[]);

typedef struct stConsoleCommand {
    const char *commandName;
    const char *helpText;
    const char *ownerTag;
    consoleCommandHandler handler;
} stConsoleCommand;

bool consoleInit(void);
bool consoleInitDefault(void);
bool consoleRegisterCommand(const stConsoleCommand *command);
void consoleProcess(void);
int32_t consoleReply(uint32_t transport, const char *format, ...) __attribute__((format(printf, 2, 3)));

#ifdef __cplusplus
}
#endif

#endif  // CONSOLE_H
/**************************End of file********************************/
