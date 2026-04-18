/************************************************************************************
* @file     : log.h
* @brief    : Lightweight logging interface.
* @details  : Provides unified LOG_I/LOG_E/LOG_W/LOG_D macros and fixed hook arrays.
* @author   : \.rumi
* @date     : 2026-03-31
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef LOG_H
#define LOG_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

#include "rep_config.h"
#include "ringbuffer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eLogLevel {
    LOG_LEVEL_NONE = 0,
    LOG_LEVEL_ERROR = 1,
    LOG_LEVEL_WARN = 2,
    LOG_LEVEL_INFO = 3,
    LOG_LEVEL_DEBUG = 4,
} eLogLevel;

#ifndef LOG_COMPILED_LEVEL
#define LOG_COMPILED_LEVEL REP_LOG_LEVEL
#endif

#ifndef LOG_LINE_BUFFER_SIZE
#define LOG_LINE_BUFFER_SIZE 256U
#endif

#ifndef LOG_OUTPUT_QUEUE_SIZE
#define LOG_OUTPUT_QUEUE_SIZE 2048U
#endif

#ifndef LOG_OUTPUT_PROCESS_BUDGET
#define LOG_OUTPUT_PROCESS_BUDGET 128U
#endif

#ifndef LOG_OUTPUT_MAX_FRAME_SIZE
#define LOG_OUTPUT_MAX_FRAME_SIZE LOG_LINE_BUFFER_SIZE
#endif

#define LOG_TRANSPORT_NONE      0x00U
#define LOG_TRANSPORT_RTT       0x01U
#define LOG_TRANSPORT_UART      0x02U
#define LOG_TRANSPORT_CAN       0x03U
#define LOG_TRANSPORT_ESP32     0x04U

typedef uint32_t (*logTimestampProvider)(void);
typedef void (*logInitFunc)(void);
typedef int32_t (*logOutputWriteFunc)(const uint8_t *buffer, uint16_t length);
typedef stRingBuffer *(*logInputGetBufferFunc)(void);

typedef struct stLogInterface {
    uint32_t transport;
    logInitFunc init;
    logOutputWriteFunc write;
    logInputGetBufferFunc getBuffer;
    bool isOutputEnabled;
    bool isInputEnabled;
} stLogInterface;

typedef struct stLogOutputStats {
    uint32_t transport;
    uint32_t pendingBytes;
    uint32_t droppedLines;
    uint32_t droppedBytes;
    uint32_t sentBytes;
    uint32_t busyCount;
    bool hasPendingFrame;
} stLogOutputStats;

bool logInit(void);
uint32_t logGetInputCount(void);
uint32_t logGetInputTransport(uint32_t index);
stRingBuffer *logGetInputBuffer(uint32_t transport);
int32_t logWriteToTransport(uint32_t transport, const uint8_t *buffer, uint16_t length);
int32_t logDirectWriteToTransport(uint32_t transport, const uint8_t *buffer, uint16_t length);
void logProcessOutput(void);
bool logGetStats(uint32_t transport, stLogOutputStats *stats);
void logSetTimestampProvider(logTimestampProvider provider);

void logWrite(eLogLevel level, const char *tag, const char *format, ...) __attribute__((format(printf, 3, 4)));
void logVWrite(eLogLevel level, const char *tag, const char *format, va_list args);

static inline uint16_t logGetTextLength(const char *buffer)
{
    uint16_t lLength = 0U;

    if (buffer == NULL) {
        return 0U;
    }

    while ((buffer[lLength] != '\0') && (lLength < UINT16_MAX)) {
        lLength++;
    }

    return lLength;
}

static inline int32_t logDirectWriteText(uint32_t transport, const char *buffer)
{
    uint16_t lLength = logGetTextLength(buffer);

    if ((buffer == NULL) || (lLength == 0U)) {
        return 0;
    }

    return logDirectWriteToTransport(transport, (const uint8_t *)buffer, lLength);
}

#define LOG_T_1(buffer) logDirectWriteText(LOG_TRANSPORT_RTT, (buffer))
#define LOG_T_2(transport, buffer) logDirectWriteText((transport), (buffer))
#define LOG_T_3(transport, buffer, length) logDirectWriteToTransport((transport), (const uint8_t *)(buffer), (length))
#define LOG_T_SELECT(_1, _2, _3, NAME, ...) NAME
#define LOG_T(...) LOG_T_SELECT(__VA_ARGS__, LOG_T_3, LOG_T_2, LOG_T_1)(__VA_ARGS__)

#if LOG_COMPILED_LEVEL >= LOG_LEVEL_ERROR
#define LOG_E(tag, format, ...) logWrite(LOG_LEVEL_ERROR, tag, format, ##__VA_ARGS__)
#else
#define LOG_E(tag, format, ...) ((void)0)
#endif

#if LOG_COMPILED_LEVEL >= LOG_LEVEL_WARN
#define LOG_W(tag, format, ...) logWrite(LOG_LEVEL_WARN, tag, format, ##__VA_ARGS__)
#else
#define LOG_W(tag, format, ...) ((void)0)
#endif

#if LOG_COMPILED_LEVEL >= LOG_LEVEL_INFO
#define LOG_I(tag, format, ...) logWrite(LOG_LEVEL_INFO, tag, format, ##__VA_ARGS__)
#else
#define LOG_I(tag, format, ...) ((void)0)
#endif

#if LOG_COMPILED_LEVEL >= LOG_LEVEL_DEBUG
#define LOG_D(tag, format, ...) logWrite(LOG_LEVEL_DEBUG, tag, format, ##__VA_ARGS__)
#else
#define LOG_D(tag, format, ...) ((void)0)
#endif

#ifdef __cplusplus
}
#endif
#endif
/**************************End of file********************************/
