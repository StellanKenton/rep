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

#include "log.h"
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

uint32_t logGetInputCount(void);
uint32_t logGetInputTransport(uint32_t index);
stRingBuffer *logGetInputBuffer(uint32_t transport);
int32_t logWriteToTransport(uint32_t transport, const uint8_t *buffer, uint16_t length);
bool logGetStats(uint32_t transport, stLogOutputStats *stats);
void logSetTimestampProvider(logTimestampProvider provider);

bool consoleCoreInit(void);
bool consoleCoreRegisterCommand(const stConsoleCommand *command);
void consoleCoreProcess(void);
int32_t logConsoleReply(uint32_t transport, const char *format, ...) __attribute__((format(printf, 2, 3)));
void logPlatformConsolePoll(void);

#ifndef LOG_CONSOLE_INTERNAL_BUILD
#define consoleInit() logInit()
#define consoleRegisterCommand(command) logRegisterConsole(command)
#define consoleProcess() ConsoleBackGournd()
#define consoleReply logConsoleReply
#endif

#ifdef __cplusplus
}
#endif

#endif  // CONSOLE_H
/**************************End of file********************************/
