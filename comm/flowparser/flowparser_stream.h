/************************************************************************************
* @file     : flowparser_stream.h
* @brief    : Transport-bound stream executor for line-oriented command transactions.
* @details  : This module binds byte input, command sending, timeout, and line callbacks.
* @author   : GitHub Copilot
* @date     : 2026-04-21
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef FLOWPARSER_STREAM_H
#define FLOWPARSER_STREAM_H

#include <stdbool.h>
#include <stdint.h>

#include "flowparser.h"
#include "../../tools/ringbuffer/ringbuffer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eFlowParserStrmSta {
    FLOWPARSER_STREAM_OK = 0,
    FLOWPARSER_STREAM_EMPTY,
    FLOWPARSER_STREAM_BUSY,
    FLOWPARSER_STREAM_INVALID_PARAM,
    FLOWPARSER_STREAM_NOT_INIT,
    FLOWPARSER_STREAM_OVERFLOW,
    FLOWPARSER_STREAM_TIMEOUT,
    FLOWPARSER_STREAM_PORT_FAIL,
    FLOWPARSER_STREAM_ERROR,
} eFlowParserStrmSta;

typedef enum eFlowParserRawMatchSta {
    FLOWPARSER_RAW_MATCH_NONE = 0,
    FLOWPARSER_RAW_MATCH_NEED_MORE,
    FLOWPARSER_RAW_MATCH_OK,
} eFlowParserRawMatchSta;

typedef void (*flowparserStreamLineFunc)(void *userData, const uint8_t *lineBuf, uint16_t lineLen);
typedef uint32_t (*flowparserStreamGetTickFunc)(void *userData);
typedef bool (*flowparserStreamIsUrcFunc)(void *userData, const uint8_t *lineBuf, uint16_t lineLen);
typedef eFlowParserStrmSta (*flowparserStreamSendFunc)(void *userData, const uint8_t *buf, uint16_t len);
typedef eFlowParserRawMatchSta (*flowparserStreamRawMatchFunc)(void *userData,
                                                               const uint8_t *buf,
                                                               uint16_t availLen,
                                                               uint16_t *frameLen);
typedef void (*flowparserStreamRawFrameFunc)(void *userData, const uint8_t *frameBuf, uint16_t frameLen);

typedef struct stFlowParserStreamCfg {
    uint8_t *rxStorage;
    uint16_t rxStorageSize;
    uint8_t *lineBuf;
    uint16_t lineBufSize;
    flowparserStreamSendFunc pfSend;
    void *sendUserData;
    flowparserStreamGetTickFunc pfGetTickMs;
    void *tickUserData;
    flowparserStreamLineFunc pfUrcHandler;
    void *urcUserData;
    flowparserStreamIsUrcFunc pfIsUrc;
    void *isUrcUserData;
    flowparserStreamRawMatchFunc pfRawMatcher;
    void *rawMatchUserData;
    flowparserStreamRawFrameFunc pfRawHandler;
    void *rawHandlerUserData;
} stFlowParserStreamCfg;

typedef struct stFlowParserStream {
    stRingBuffer rxRb;
    const stFlowParserSpec *spec;
    flowparserLineFunc lineHandler;
    flowparserDoneFunc doneHandler;
    void *txnUserData;
    flowparserStreamSendFunc pfSend;
    void *sendUserData;
    flowparserStreamGetTickFunc pfGetTickMs;
    void *tickUserData;
    flowparserStreamLineFunc pfUrcHandler;
    void *urcUserData;
    flowparserStreamIsUrcFunc pfIsUrc;
    void *isUrcUserData;
    flowparserStreamRawMatchFunc pfRawMatcher;
    void *rawMatchUserData;
    flowparserStreamRawFrameFunc pfRawHandler;
    void *rawHandlerUserData;
    uint8_t *lineBuf;
    uint16_t lineBufSize;
    uint16_t lineLen;
    uint32_t totalStartTick;
    uint32_t stageStartTick;
    const uint8_t *payloadBuf;
    uint16_t payloadLen;
    eFlowParserStage stage;
    bool isInit;
} stFlowParserStream;

eFlowParserStrmSta flowparserStreamInit(stFlowParserStream *stream, const stFlowParserStreamCfg *cfg);
void flowparserStreamReset(stFlowParserStream *stream);
eFlowParserStrmSta flowparserStreamFeed(stFlowParserStream *stream, const uint8_t *data, uint16_t len);
eFlowParserStrmSta flowparserStreamSubmit(stFlowParserStream *stream, const stFlowParserReq *req);
eFlowParserStrmSta flowparserStreamProc(stFlowParserStream *stream);
bool flowparserStreamIsBusy(const stFlowParserStream *stream);
eFlowParserStage flowparserStreamGetStage(const stFlowParserStream *stream);
void flowparserStreamSetUrcHandler(stFlowParserStream *stream, flowparserStreamLineFunc urcHandler, void *userData);
void flowparserStreamSetUrcMatcher(stFlowParserStream *stream, flowparserStreamIsUrcFunc isUrc, void *userData);
void flowparserStreamSetRawHook(stFlowParserStream *stream,
                                flowparserStreamRawMatchFunc rawMatcher,
                                void *matchUserData,
                                flowparserStreamRawFrameFunc rawHandler,
                                void *handlerUserData);

#ifdef __cplusplus
}
#endif

#endif  // FLOWPARSER_STREAM_H
/**************************End of file********************************/
