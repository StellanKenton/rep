/************************************************************************************
* @file     : log_internal.h
* @brief    : Internal declarations shared by log core and console core.
* @details  : This header is private to rep/sys/log and must not be included by modules.
* @author   : GitHub Copilot
* @date     : 2026-05-12
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef LOG_INTERNAL_H
#define LOG_INTERNAL_H

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

bool consoleCoreInit(void);
bool consoleCoreRegisterCommand(const stConsoleCommand *command);
void consoleCoreProcess(void);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/