/***********************************************************************************
* @file     : flowparser_stream.c
* @brief    : AT command transaction parser implementation.
* @details  : Executes single-command flows with prompt wait and URC splitting.
* @author   : GitHub Copilot
* @date     : 2026-04-02
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "flowparser_stream.h"

#include <string.h>

static uint32_t flowparserStreamGetTick(const stFlowParserStream *stream);
static bool flowparserStreamMatchPattern(const uint8_t *lineBuf, uint16_t lineLen, const char *pattern);
static bool flowparserStreamMatchPatterns(const uint8_t *lineBuf, uint16_t lineLen, const char *const *patterns, uint8_t patternCnt);
static bool flowparserStreamIsUrc(const stFlowParserStream *stream, const uint8_t *lineBuf, uint16_t lineLen);
static bool flowparserStreamIsStageTout(const stFlowParserStream *stream, uint32_t toutMs);
static void flowparserStreamFinish(stFlowParserStream *stream, eFlowParserResult result);
static void flowparserStreamHandleLine(stFlowParserStream *stream, const uint8_t *lineBuf, uint16_t lineLen);
static void flowparserStreamHandlePrompt(stFlowParserStream *stream);
static void flowparserStreamCheckTout(stFlowParserStream *stream);

static uint32_t flowparserStreamGetTick(const stFlowParserStream *stream)
{
    if ((stream == NULL) || (stream->cfg.getTick == NULL)) {
        return 0U;
    }

    return stream->cfg.getTick();
}

static bool flowparserStreamMatchPattern(const uint8_t *lineBuf, uint16_t lineLen, const char *pattern)
{
    uint32_t lPatLen;

    if ((lineBuf == NULL) || (pattern == NULL)) {
        return false;
    }

    lPatLen = (uint32_t)strlen(pattern);
    if (lPatLen == 0U) {
        return false;
    }

    if (pattern[lPatLen - 1U] == '*') {
        lPatLen--;
        if ((lPatLen == 0U) || (lineLen < lPatLen)) {
            return false;
        }

        return memcmp(lineBuf, pattern, lPatLen) == 0;
    }

    if (lineLen != lPatLen) {
        return false;
    }

    return memcmp(lineBuf, pattern, lPatLen) == 0;
}

static bool flowparserStreamMatchPatterns(const uint8_t *lineBuf, uint16_t lineLen, const char *const *patterns, uint8_t patternCnt)
{
    uint8_t lIdx;

    if ((patterns == NULL) || (patternCnt == 0U)) {
        return false;
    }

    for (lIdx = 0U; lIdx < patternCnt; lIdx++) {
        if (flowparserStreamMatchPattern(lineBuf, lineLen, patterns[lIdx])) {
            return true;
        }
    }

    return false;
}

static bool flowparserStreamIsUrc(const stFlowParserStream *stream, const uint8_t *lineBuf, uint16_t lineLen)
{
    if ((stream == NULL) || (lineBuf == NULL)) {
        return false;
    }

    if ((stream->cfg.isUrc != NULL) && (stream->cfg.isUrc(lineBuf, lineLen, stream->cfg.urcMatchUserCtx))) {
        return true;
    }

    return flowparserStreamMatchPatterns(lineBuf, lineLen, stream->cfg.urcPatterns, stream->cfg.urcPatternCnt);
}

static bool flowparserStreamIsStageTout(const stFlowParserStream *stream, uint32_t toutMs)
{
    if ((stream == NULL) || (stream->txn.isActive == false) || (toutMs == 0U)) {
        return false;
    }

    return (uint32_t)(flowparserStreamGetTick(stream) - stream->txn.stageStartTick) >= toutMs;
}

static void flowparserStreamFinish(stFlowParserStream *stream, eFlowParserResult result)
{
    flowparserStreamDoneFunc lDoneHandler;
    void *lUserData;

    if (stream == NULL) {
        return;
    }

    lDoneHandler = stream->txn.doneHandler;
    lUserData = stream->txn.userData;

    stream->txn.result = result;
    stream->txn.stage = FLOWPARSER_STAGE_IDLE;
    stream->txn.isActive = false;
    stream->txn.spec = NULL;
    stream->txn.lineHandler = NULL;
    stream->txn.doneHandler = NULL;
    stream->txn.userData = NULL;
    stream->txn.cmdLen = 0U;
    stream->txn.payloadLen = 0U;
    stream->txn.totalStartTick = 0U;
    stream->txn.stageStartTick = 0U;

    if (lDoneHandler != NULL) {
        lDoneHandler(lUserData, result);
    }
}

static void flowparserStreamHandleLine(stFlowParserStream *stream, const uint8_t *lineBuf, uint16_t lineLen)
{
    const stFlowParserSpec *lSpec;

    if ((stream == NULL) || (lineBuf == NULL)) {
        return;
    }

    lSpec = stream->txn.spec;
    if ((stream->txn.isActive == true) && (lSpec != NULL)) {
        if (flowparserStreamMatchPatterns(lineBuf, lineLen, lSpec->errorPatterns, lSpec->errorPatternCnt)) {
            flowparserStreamFinish(stream, FLOWPARSER_RESULT_ERROR);
            return;
        }

        if ((stream->txn.stage == FLOWPARSER_STAGE_WAIT_RESPONSE) &&
            flowparserStreamMatchPatterns(lineBuf, lineLen, lSpec->responseDonePatterns, lSpec->responseDonePatternCnt)) {
            flowparserStreamFinish(stream, FLOWPARSER_RESULT_OK);
            return;
        }

        if ((stream->txn.stage == FLOWPARSER_STAGE_WAIT_FINAL) &&
            flowparserStreamMatchPatterns(lineBuf, lineLen, lSpec->finalDonePatterns, lSpec->finalDonePatternCnt)) {
            flowparserStreamFinish(stream, FLOWPARSER_RESULT_OK);
            return;
        }
    }

    if (flowparserStreamIsUrc(stream, lineBuf, lineLen)) {
        if (stream->cfg.urcHandler != NULL) {
            stream->cfg.urcHandler(stream->cfg.urcUserCtx, lineBuf, lineLen);
        }
        return;
    }

    if ((stream->txn.isActive == true) && (stream->txn.lineHandler != NULL)) {
        stream->txn.lineHandler(stream->txn.userData, lineBuf, lineLen);
    }
}

static void flowparserStreamHandlePrompt(stFlowParserStream *stream)
{
    eDrvStatus lDrvStatus;

    if ((stream == NULL) || (stream->txn.isActive == false) || (stream->txn.stage != FLOWPARSER_STAGE_WAIT_PROMPT)) {
        return;
    }

    lDrvStatus = DRV_STATUS_OK;
    if (stream->txn.payloadLen > 0U) {
        lDrvStatus = stream->cfg.send(stream->cfg.payloadBuf, stream->txn.payloadLen, stream->cfg.portUserCtx);
    }

    if (lDrvStatus != DRV_STATUS_OK) {
        flowparserStreamFinish(stream, FLOWPARSER_RESULT_SEND_FAIL);
        return;
    }

    stream->txn.stage = FLOWPARSER_STAGE_WAIT_FINAL;
    stream->txn.stageStartTick = flowparserStreamGetTick(stream);
}

static void flowparserStreamCheckTout(stFlowParserStream *stream)
{
    const stFlowParserSpec *lSpec;
    uint32_t lTotalToutMs;
    uint32_t lStageToutMs;

    if ((stream == NULL) || (stream->txn.isActive == false) || (stream->txn.spec == NULL)) {
        return;
    }

    lSpec = stream->txn.spec;
    lTotalToutMs = lSpec->totalToutMs;
    if ((lTotalToutMs != 0U) && ((uint32_t)(flowparserStreamGetTick(stream) - stream->txn.totalStartTick) >= lTotalToutMs)) {
        flowparserStreamFinish(stream, FLOWPARSER_RESULT_TIMEOUT);
        return;
    }

    lStageToutMs = 0U;
    if (stream->txn.stage == FLOWPARSER_STAGE_WAIT_RESPONSE) {
        lStageToutMs = lSpec->responseToutMs;
    } else if (stream->txn.stage == FLOWPARSER_STAGE_WAIT_PROMPT) {
        lStageToutMs = lSpec->promptToutMs;
    } else if (stream->txn.stage == FLOWPARSER_STAGE_WAIT_FINAL) {
        lStageToutMs = lSpec->finalToutMs;
    }

    if ((lStageToutMs == 0U) && (lTotalToutMs != 0U)) {
        lStageToutMs = lTotalToutMs;
    }

    if (flowparserStreamIsStageTout(stream, lStageToutMs)) {
        flowparserStreamFinish(stream, FLOWPARSER_RESULT_TIMEOUT);
    }
}

bool flowparserStreamIsCfgValid(const stFlowParserStreamCfg *cfg)
{
    if ((cfg == NULL) || (!flowparserTokIsCfgValid(&cfg->tokCfg)) || (cfg->cmdBuf == NULL) || (cfg->cmdBufSize == 0U) ||
        (cfg->send == NULL) || (cfg->getTick == NULL)) {
        return false;
    }

    if ((cfg->payloadBufSize > 0U) && (cfg->payloadBuf == NULL)) {
        return false;
    }

    return true;
}

eFlowParserStrmSta flowparserStreamInit(stFlowParserStream *stream, const stFlowParserStreamCfg *cfg)
{
    if ((stream == NULL) || (!flowparserStreamIsCfgValid(cfg))) {
        return FLOWPARSER_STREAM_INVALID_ARG;
    }

    (void)memset(stream, 0, sizeof(*stream));
    stream->cfg = *cfg;
    if (stream->cfg.procBudget == 0U) {
        stream->cfg.procBudget = 8U;
    }

    if (flowparserTokInit(&stream->tokenizer, &cfg->tokCfg) != FLOWPARSER_TOK_OK) {
        return FLOWPARSER_STREAM_INVALID_ARG;
    }

    stream->txn.stage = FLOWPARSER_STAGE_IDLE;
    stream->isInit = true;
    return FLOWPARSER_STREAM_OK;
}

void flowparserStreamReset(stFlowParserStream *stream)
{
    if ((stream == NULL) || (stream->isInit == false)) {
        return;
    }

    flowparserTokReset(&stream->tokenizer);
    (void)memset(&stream->txn, 0, sizeof(stream->txn));
    stream->txn.stage = FLOWPARSER_STAGE_IDLE;
}

eFlowParserStrmSta flowparserStreamSubmit(stFlowParserStream *stream, const stFlowParserReq *req)
{
    eDrvStatus lDrvStatus;

    if ((stream == NULL) || (req == NULL) || (req->spec == NULL) || (req->cmdBuf == NULL) || (req->cmdLen == 0U)) {
        return FLOWPARSER_STREAM_INVALID_ARG;
    }

    if (stream->isInit == false) {
        return FLOWPARSER_STREAM_NOT_INIT;
    }

    if (stream->txn.isActive == true) {
        return FLOWPARSER_STREAM_BUSY;
    }

    if ((req->cmdLen > stream->cfg.cmdBufSize) || (req->payloadLen > stream->cfg.payloadBufSize) ||
        ((req->payloadLen > 0U) && (req->payloadBuf == NULL))) {
        return FLOWPARSER_STREAM_INVALID_ARG;
    }

    (void)memcpy(stream->cfg.cmdBuf, req->cmdBuf, req->cmdLen);
    if (req->payloadLen > 0U) {
        (void)memcpy(stream->cfg.payloadBuf, req->payloadBuf, req->payloadLen);
    }

    lDrvStatus = stream->cfg.send(stream->cfg.cmdBuf, req->cmdLen, stream->cfg.portUserCtx);
    if (lDrvStatus != DRV_STATUS_OK) {
        return FLOWPARSER_STREAM_PORT_FAIL;
    }

    stream->txn.spec = req->spec;
    stream->txn.lineHandler = req->lineHandler;
    stream->txn.doneHandler = req->doneHandler;
    stream->txn.userData = req->userData;
    stream->txn.cmdLen = req->cmdLen;
    stream->txn.payloadLen = req->payloadLen;
    stream->txn.totalStartTick = flowparserStreamGetTick(stream);
    stream->txn.stageStartTick = stream->txn.totalStartTick;
    stream->txn.result = FLOWPARSER_RESULT_NONE;
    stream->txn.stage = req->spec->needPrompt ? FLOWPARSER_STAGE_WAIT_PROMPT : FLOWPARSER_STAGE_WAIT_RESPONSE;
    stream->txn.isActive = true;

    return FLOWPARSER_STREAM_OK;
}

eFlowParserStrmSta flowparserStreamProc(stFlowParserStream *stream)
{
    stFlowParserTok lToken;
    eFlowParserTokSta lTokSta;
    uint8_t lProcCnt;
    bool lDidWork = false;

    if (stream == NULL) {
        return FLOWPARSER_STREAM_INVALID_ARG;
    }

    if (stream->isInit == false) {
        return FLOWPARSER_STREAM_NOT_INIT;
    }

    lProcCnt = 0U;
    while (lProcCnt < stream->cfg.procBudget) {
        lTokSta = flowparserTokGet(&stream->tokenizer, &lToken);
        if (lTokSta == FLOWPARSER_TOK_EMPTY) {
            break;
        }

        if (lTokSta != FLOWPARSER_TOK_OK) {
            return FLOWPARSER_STREAM_INVALID_ARG;
        }

        lDidWork = true;
        lProcCnt++;

        if (lToken.type == FLOWPARSER_TOK_TYPE_LINE) {
            flowparserStreamHandleLine(stream, lToken.buf, lToken.len);
        } else if (lToken.type == FLOWPARSER_TOK_TYPE_PROMPT) {
            flowparserStreamHandlePrompt(stream);
        } else if (lToken.type == FLOWPARSER_TOK_TYPE_OVERFLOW) {
            if (stream->txn.isActive == true) {
                flowparserStreamFinish(stream, FLOWPARSER_RESULT_OVERFLOW);
            }
        }

        if (stream->txn.isActive == false) {
            continue;
        }

        flowparserStreamCheckTout(stream);
    }

    flowparserStreamCheckTout(stream);
    return lDidWork ? FLOWPARSER_STREAM_OK : FLOWPARSER_STREAM_EMPTY;
}

bool flowparserStreamIsBusy(const stFlowParserStream *stream)
{
    if ((stream == NULL) || (stream->isInit == false)) {
        return false;
    }

    return stream->txn.isActive;
}

eFlowParserStage flowparserStreamGetStage(const stFlowParserStream *stream)
{
    if ((stream == NULL) || (stream->isInit == false)) {
        return FLOWPARSER_STAGE_IDLE;
    }

    return stream->txn.stage;
}

/**************************End of file********************************/
