 /************************************************************************************
* @file     : flowparser_stream.h
* @brief    : AT command transaction parser public API.
* @details  : Drives a single-command state machine on top of tokenizer tokens.
* @author   : GitHub Copilot
* @date     : 2026-04-02
* @version  : V1.0.0
***********************************************************************************/
#ifndef FLOWPARSER_STREAM_H
#define FLOWPARSER_STREAM_H

#include <stdbool.h>
#include <stdint.h>

#include "rep_config.h"
#include "flowparser_tokenizer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eFlowParserStrmSta {
    FLOWPARSER_STREAM_OK = 0,
    FLOWPARSER_STREAM_EMPTY,
    FLOWPARSER_STREAM_INVALID_ARG,
    FLOWPARSER_STREAM_NOT_INIT,
    FLOWPARSER_STREAM_BUSY,
    FLOWPARSER_STREAM_PORT_FAIL,
} eFlowParserStrmSta;

typedef enum eFlowParserStage {
    FLOWPARSER_STAGE_IDLE = 0,
    FLOWPARSER_STAGE_WAIT_RESPONSE,
    FLOWPARSER_STAGE_WAIT_PROMPT,
    FLOWPARSER_STAGE_WAIT_FINAL,
} eFlowParserStage;

typedef enum eFlowParserResult {
    FLOWPARSER_RESULT_NONE = 0,
    FLOWPARSER_RESULT_OK,
    FLOWPARSER_RESULT_ERROR,
    FLOWPARSER_RESULT_TIMEOUT,
    FLOWPARSER_RESULT_OVERFLOW,
    FLOWPARSER_RESULT_SEND_FAIL,
} eFlowParserResult;

typedef eDrvStatus (*flowparserStreamSendFunc)(const uint8_t *buf, uint16_t len, void *userCtx);
typedef uint32_t (*flowparserStreamGetTickFunc)(void);
typedef bool (*flowparserStreamMatchFunc)(const uint8_t *lineBuf, uint16_t lineLen, void *userCtx);
typedef void (*flowparserStreamLineFunc)(void *userData, const uint8_t *lineBuf, uint16_t lineLen);
typedef void (*flowparserStreamDoneFunc)(void *userData, eFlowParserResult result);

typedef struct stFlowParserSpec {
    const char *const *responseDonePatterns;
    uint8_t responseDonePatternCnt;
    const char *const *finalDonePatterns;
    uint8_t finalDonePatternCnt;
    const char *const *errorPatterns;
    uint8_t errorPatternCnt;
    uint32_t totalToutMs;
    uint32_t responseToutMs;
    uint32_t promptToutMs;
    uint32_t finalToutMs;
    bool needPrompt;
} stFlowParserSpec;

typedef struct stFlowParserReq {
    const stFlowParserSpec *spec;
    const uint8_t *cmdBuf;
    uint16_t cmdLen;
    const uint8_t *payloadBuf;
    uint16_t payloadLen;
    flowparserStreamLineFunc lineHandler;
    flowparserStreamDoneFunc doneHandler;
    void *userData;
} stFlowParserReq;

typedef struct stFlowParserStreamCfg {
    stFlowParserTokCfg tokCfg;
    uint8_t *cmdBuf;
    uint16_t cmdBufSize;
    uint8_t *payloadBuf;
    uint16_t payloadBufSize;
    flowparserStreamSendFunc send;
    void *portUserCtx;
    flowparserStreamGetTickFunc getTick;
    const char *const *urcPatterns;
    uint8_t urcPatternCnt;
    flowparserStreamMatchFunc isUrc;
    void *urcMatchUserCtx;
    flowparserStreamLineFunc urcHandler;
    void *urcUserCtx;
    uint8_t procBudget;
} stFlowParserStreamCfg;

typedef struct stFlowParserTxn {
    const stFlowParserSpec *spec;
    flowparserStreamLineFunc lineHandler;
    flowparserStreamDoneFunc doneHandler;
    void *userData;
    uint16_t cmdLen;
    uint16_t payloadLen;
    uint32_t totalStartTick;
    uint32_t stageStartTick;
    eFlowParserStage stage;
    eFlowParserResult result;
    bool isActive;
} stFlowParserTxn;

typedef struct stFlowParserStream {
    stFlowParserTokenizer tokenizer;
    stFlowParserStreamCfg cfg;
    stFlowParserTxn txn;
    bool isInit;
} stFlowParserStream;

bool flowparserStreamIsCfgValid(const stFlowParserStreamCfg *cfg);
eFlowParserStrmSta flowparserStreamInit(stFlowParserStream *stream, const stFlowParserStreamCfg *cfg);
void flowparserStreamReset(stFlowParserStream *stream);
eFlowParserStrmSta flowparserStreamSubmit(stFlowParserStream *stream, const stFlowParserReq *req);
eFlowParserStrmSta flowparserStreamProc(stFlowParserStream *stream);
bool flowparserStreamIsBusy(const stFlowParserStream *stream);
eFlowParserStage flowparserStreamGetStage(const stFlowParserStream *stream);

#ifdef __cplusplus
}
#endif

#endif  // FLOWPARSER_STREAM_H
/**************************End of file********************************/
