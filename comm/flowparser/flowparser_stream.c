/***********************************************************************************
* @file     : flowparser_stream.c
* @brief    : Transport-bound executor for line-oriented command transactions.
* @details  : This file manages byte buffering, line splitting, prompt handling, and timeout checks.
* @author   : GitHub Copilot
* @date     : 2026-04-21
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "flowparser_stream.h"

#include <stddef.h>
#include <string.h>

static void flowparserStreamClearTxn(stFlowParserStream *stream);
static void flowparserStreamFinish(stFlowParserStream *stream, eFlowParserResult result);
static void flowparserStreamSetStage(stFlowParserStream *stream, eFlowParserStage stage);
static bool flowparserStreamHasTimedOut(const stFlowParserStream *stream, uint32_t limitMs);
static eFlowParserStrmSta flowparserStreamCheckTimeout(stFlowParserStream *stream);
static eFlowParserStrmSta flowparserStreamConsumeByte(stFlowParserStream *stream, uint8_t data, bool *hasLine, bool *isPrompt);
static eFlowParserStrmSta flowparserStreamDispatchLine(stFlowParserStream *stream, const uint8_t *lineBuf, uint16_t lineLen, bool isPrompt);
static bool flowparserStreamIsConfiguredUrc(const stFlowParserStream *stream, const uint8_t *lineBuf, uint16_t lineLen);
static bool flowparserStreamMatchError(const stFlowParserStream *stream, const uint8_t *lineBuf, uint16_t lineLen);
static bool flowparserStreamMatchResponseDone(const stFlowParserStream *stream, const uint8_t *lineBuf, uint16_t lineLen);
static bool flowparserStreamMatchFinalDone(const stFlowParserStream *stream, const uint8_t *lineBuf, uint16_t lineLen);

eFlowParserStrmSta flowparserStreamInit(stFlowParserStream *stream, const stFlowParserStreamCfg *cfg)
{
    if ((stream == NULL) || (cfg == NULL) || (cfg->rxStorage == NULL) || (cfg->rxStorageSize == 0U) ||
        (cfg->lineBuf == NULL) || (cfg->lineBufSize < 2U) || (cfg->pfSend == NULL)) {
        return FLOWPARSER_STREAM_INVALID_PARAM;
    }

    (void)memset(stream, 0, sizeof(*stream));
    if (ringBufferInit(&stream->rxRb, cfg->rxStorage, cfg->rxStorageSize) != RINGBUFFER_OK) {
        return FLOWPARSER_STREAM_INVALID_PARAM;
    }

    stream->lineBuf = cfg->lineBuf;
    stream->lineBufSize = cfg->lineBufSize;
    stream->pfSend = cfg->pfSend;
    stream->sendUserData = cfg->sendUserData;
    stream->pfGetTickMs = cfg->pfGetTickMs;
    stream->tickUserData = cfg->tickUserData;
    stream->pfUrcHandler = cfg->pfUrcHandler;
    stream->urcUserData = cfg->urcUserData;
    stream->pfIsUrc = cfg->pfIsUrc;
    stream->isUrcUserData = cfg->isUrcUserData;
    stream->stage = FLOWPARSER_STAGE_IDLE;
    stream->isInit = true;
    return FLOWPARSER_STREAM_OK;
}

void flowparserStreamReset(stFlowParserStream *stream)
{
    if ((stream == NULL) || !stream->isInit) {
        return;
    }

    (void)ringBufferReset(&stream->rxRb);
    flowparserStreamClearTxn(stream);
}

eFlowParserStrmSta flowparserStreamFeed(stFlowParserStream *stream, const uint8_t *data, uint16_t len)
{
    uint32_t written;

    if ((stream == NULL) || !stream->isInit) {
        return FLOWPARSER_STREAM_NOT_INIT;
    }

    if ((data == NULL) || (len == 0U)) {
        return FLOWPARSER_STREAM_INVALID_PARAM;
    }

    written = ringBufferWrite(&stream->rxRb, data, len);
    if (written != len) {
        return FLOWPARSER_STREAM_OVERFLOW;
    }

    return FLOWPARSER_STREAM_OK;
}

eFlowParserStrmSta flowparserStreamSubmit(stFlowParserStream *stream, const stFlowParserReq *req)
{
    eFlowParserStrmSta status;

    if ((stream == NULL) || !stream->isInit) {
        return FLOWPARSER_STREAM_NOT_INIT;
    }

    if ((req == NULL) || (req->spec == NULL) || (req->cmdBuf == NULL) || (req->cmdLen == 0U)) {
        return FLOWPARSER_STREAM_INVALID_PARAM;
    }

    if (flowparserStreamIsBusy(stream)) {
        return FLOWPARSER_STREAM_BUSY;
    }

    status = stream->pfSend(stream->sendUserData, req->cmdBuf, req->cmdLen);
    if (status != FLOWPARSER_STREAM_OK) {
        return (status == FLOWPARSER_STREAM_PORT_FAIL) ? FLOWPARSER_STREAM_PORT_FAIL : FLOWPARSER_STREAM_ERROR;
    }

    stream->spec = req->spec;
    stream->lineHandler = req->lineHandler;
    stream->doneHandler = req->doneHandler;
    stream->txnUserData = req->userData;
    stream->payloadBuf = req->payloadBuf;
    stream->payloadLen = req->payloadLen;
    stream->lineLen = 0U;

    if (stream->pfGetTickMs != NULL) {
        stream->totalStartTick = stream->pfGetTickMs(stream->tickUserData);
        stream->stageStartTick = stream->totalStartTick;
    } else {
        stream->totalStartTick = 0U;
        stream->stageStartTick = 0U;
    }

    flowparserStreamSetStage(stream, req->spec->needPrompt ? FLOWPARSER_STAGE_WAIT_PROMPT : FLOWPARSER_STAGE_WAIT_RESPONSE);
    return FLOWPARSER_STREAM_OK;
}

eFlowParserStrmSta flowparserStreamProc(stFlowParserStream *stream)
{
    uint8_t data;
    bool hasLine;
    bool isPrompt;
    bool consumed;
    eFlowParserStrmSta status;

    if ((stream == NULL) || !stream->isInit) {
        return FLOWPARSER_STREAM_NOT_INIT;
    }

    status = flowparserStreamCheckTimeout(stream);
    if (status != FLOWPARSER_STREAM_OK) {
        return status;
    }

    consumed = false;
    while (ringBufferPopByte(&stream->rxRb, &data) == RINGBUFFER_OK) {
        consumed = true;
        hasLine = false;
        isPrompt = false;
        status = flowparserStreamConsumeByte(stream, data, &hasLine, &isPrompt);
        if (status != FLOWPARSER_STREAM_OK) {
            return status;
        }
        if (!hasLine) {
            continue;
        }

        status = flowparserStreamDispatchLine(stream, stream->lineBuf, stream->lineLen, isPrompt);
        stream->lineLen = 0U;
        if (status != FLOWPARSER_STREAM_OK) {
            return status;
        }
    }

    if (!consumed) {
        return flowparserStreamIsBusy(stream) ? FLOWPARSER_STREAM_BUSY : FLOWPARSER_STREAM_EMPTY;
    }

    return FLOWPARSER_STREAM_OK;
}

bool flowparserStreamIsBusy(const stFlowParserStream *stream)
{
    return (stream != NULL) && stream->isInit && (stream->stage != FLOWPARSER_STAGE_IDLE);
}

eFlowParserStage flowparserStreamGetStage(const stFlowParserStream *stream)
{
    if (stream == NULL) {
        return FLOWPARSER_STAGE_IDLE;
    }

    return stream->stage;
}

void flowparserStreamSetUrcHandler(stFlowParserStream *stream, flowparserStreamLineFunc urcHandler, void *userData)
{
    if (stream == NULL) {
        return;
    }

    stream->pfUrcHandler = urcHandler;
    stream->urcUserData = userData;
}

void flowparserStreamSetUrcMatcher(stFlowParserStream *stream, flowparserStreamIsUrcFunc isUrc, void *userData)
{
    if (stream == NULL) {
        return;
    }

    stream->pfIsUrc = isUrc;
    stream->isUrcUserData = userData;
}

static void flowparserStreamClearTxn(stFlowParserStream *stream)
{
    stream->spec = NULL;
    stream->lineHandler = NULL;
    stream->doneHandler = NULL;
    stream->txnUserData = NULL;
    stream->payloadBuf = NULL;
    stream->payloadLen = 0U;
    stream->lineLen = 0U;
    stream->totalStartTick = 0U;
    stream->stageStartTick = 0U;
    stream->stage = FLOWPARSER_STAGE_IDLE;
}

static void flowparserStreamFinish(stFlowParserStream *stream, eFlowParserResult result)
{
    flowparserDoneFunc doneHandler;
    void *userData;

    doneHandler = stream->doneHandler;
    userData = stream->txnUserData;
    flowparserStreamClearTxn(stream);

    if (doneHandler != NULL) {
        doneHandler(userData, result);
    }
}

static void flowparserStreamSetStage(stFlowParserStream *stream, eFlowParserStage stage)
{
    stream->stage = stage;
    if (stream->pfGetTickMs != NULL) {
        stream->stageStartTick = stream->pfGetTickMs(stream->tickUserData);
    }
}

static bool flowparserStreamHasTimedOut(const stFlowParserStream *stream, uint32_t limitMs)
{
    uint32_t nowTick;
    uint32_t startTick;

    if ((stream->pfGetTickMs == NULL) || (limitMs == 0U)) {
        return false;
    }

    nowTick = stream->pfGetTickMs(stream->tickUserData);
    startTick = stream->stageStartTick;
    return (uint32_t)(nowTick - startTick) >= limitMs;
}

static eFlowParserStrmSta flowparserStreamCheckTimeout(stFlowParserStream *stream)
{
    uint32_t totalElapsed;

    if (!flowparserStreamIsBusy(stream) || (stream->spec == NULL) || (stream->pfGetTickMs == NULL)) {
        return FLOWPARSER_STREAM_OK;
    }

    if (stream->spec->totalToutMs != 0U) {
        totalElapsed = (uint32_t)(stream->pfGetTickMs(stream->tickUserData) - stream->totalStartTick);
        if (totalElapsed >= stream->spec->totalToutMs) {
            flowparserStreamFinish(stream, FLOWPARSER_RESULT_TIMEOUT);
            return FLOWPARSER_STREAM_TIMEOUT;
        }
    }

    switch (stream->stage) {
        case FLOWPARSER_STAGE_WAIT_RESPONSE:
            if (flowparserStreamHasTimedOut(stream, stream->spec->responseToutMs)) {
                flowparserStreamFinish(stream, FLOWPARSER_RESULT_TIMEOUT);
                return FLOWPARSER_STREAM_TIMEOUT;
            }
            break;
        case FLOWPARSER_STAGE_WAIT_PROMPT:
            if (flowparserStreamHasTimedOut(stream, stream->spec->promptToutMs)) {
                flowparserStreamFinish(stream, FLOWPARSER_RESULT_TIMEOUT);
                return FLOWPARSER_STREAM_TIMEOUT;
            }
            break;
        case FLOWPARSER_STAGE_WAIT_FINAL:
            if (flowparserStreamHasTimedOut(stream, stream->spec->finalToutMs)) {
                flowparserStreamFinish(stream, FLOWPARSER_RESULT_TIMEOUT);
                return FLOWPARSER_STREAM_TIMEOUT;
            }
            break;
        case FLOWPARSER_STAGE_IDLE:
        default:
            break;
    }

    return FLOWPARSER_STREAM_OK;
}

static eFlowParserStrmSta flowparserStreamConsumeByte(stFlowParserStream *stream, uint8_t data, bool *hasLine, bool *isPrompt)
{
    if ((stream == NULL) || (hasLine == NULL) || (isPrompt == NULL)) {
        return FLOWPARSER_STREAM_INVALID_PARAM;
    }

    *hasLine = false;
    *isPrompt = false;

    if ((stream->lineLen == 0U) && ((data == '\r') || (data == '\n'))) {
        return FLOWPARSER_STREAM_OK;
    }

    if ((stream->lineLen == 0U) && (data == '>')) {
        stream->lineBuf[0] = data;
        stream->lineLen = 1U;
        *hasLine = true;
        *isPrompt = true;
        return FLOWPARSER_STREAM_OK;
    }

    if ((data == '\r') || (data == '\n')) {
        if (stream->lineLen > 0U) {
            *hasLine = true;
        }
        return FLOWPARSER_STREAM_OK;
    }

    if ((uint32_t)stream->lineLen >= (uint32_t)(stream->lineBufSize - 1U)) {
        flowparserStreamFinish(stream, FLOWPARSER_RESULT_OVERFLOW);
        return FLOWPARSER_STREAM_OVERFLOW;
    }

    stream->lineBuf[stream->lineLen++] = data;
    stream->lineBuf[stream->lineLen] = '\0';
    return FLOWPARSER_STREAM_OK;
}

static eFlowParserStrmSta flowparserStreamDispatchLine(stFlowParserStream *stream, const uint8_t *lineBuf, uint16_t lineLen, bool isPrompt)
{
    if ((stream == NULL) || (lineBuf == NULL) || (lineLen == 0U)) {
        return FLOWPARSER_STREAM_OK;
    }

    if (flowparserStreamIsConfiguredUrc(stream, lineBuf, lineLen)) {
        if (stream->pfUrcHandler != NULL) {
            stream->pfUrcHandler(stream->urcUserData, lineBuf, lineLen);
        }
        return FLOWPARSER_STREAM_OK;
    }

    if (!flowparserStreamIsBusy(stream)) {
        if (stream->pfUrcHandler != NULL) {
            stream->pfUrcHandler(stream->urcUserData, lineBuf, lineLen);
        }
        return FLOWPARSER_STREAM_OK;
    }

    if (flowparserStreamMatchError(stream, lineBuf, lineLen)) {
        flowparserStreamFinish(stream, FLOWPARSER_RESULT_ERROR);
        return FLOWPARSER_STREAM_OK;
    }

    if ((stream->stage == FLOWPARSER_STAGE_WAIT_PROMPT) && isPrompt) {
        if ((stream->payloadBuf != NULL) && (stream->payloadLen > 0U)) {
            if (stream->pfSend(stream->sendUserData, stream->payloadBuf, stream->payloadLen) != FLOWPARSER_STREAM_OK) {
                flowparserStreamFinish(stream, FLOWPARSER_RESULT_SEND_FAIL);
                return FLOWPARSER_STREAM_PORT_FAIL;
            }
        }
        flowparserStreamSetStage(stream, FLOWPARSER_STAGE_WAIT_FINAL);
        return FLOWPARSER_STREAM_OK;
    }

    if (stream->lineHandler != NULL) {
        stream->lineHandler(stream->txnUserData, lineBuf, lineLen);
    }

    if (stream->stage == FLOWPARSER_STAGE_WAIT_RESPONSE) {
        if (flowparserStreamMatchFinalDone(stream, lineBuf, lineLen)) {
            flowparserStreamFinish(stream, FLOWPARSER_RESULT_OK);
            return FLOWPARSER_STREAM_OK;
        }

        if (flowparserStreamMatchResponseDone(stream, lineBuf, lineLen)) {
            if ((stream->spec != NULL) && (stream->spec->finalDonePatternCnt > 0U)) {
                flowparserStreamSetStage(stream, FLOWPARSER_STAGE_WAIT_FINAL);
            } else {
                flowparserStreamFinish(stream, FLOWPARSER_RESULT_OK);
            }
            return FLOWPARSER_STREAM_OK;
        }
    } else if (stream->stage == FLOWPARSER_STAGE_WAIT_FINAL) {
        if (flowparserStreamMatchFinalDone(stream, lineBuf, lineLen) || flowparserStreamMatchResponseDone(stream, lineBuf, lineLen)) {
            flowparserStreamFinish(stream, FLOWPARSER_RESULT_OK);
            return FLOWPARSER_STREAM_OK;
        }
    }

    return FLOWPARSER_STREAM_OK;
}

static bool flowparserStreamIsConfiguredUrc(const stFlowParserStream *stream, const uint8_t *lineBuf, uint16_t lineLen)
{
    if ((stream == NULL) || (stream->pfIsUrc == NULL)) {
        return false;
    }

    return stream->pfIsUrc(stream->isUrcUserData, lineBuf, lineLen);
}

static bool flowparserStreamMatchError(const stFlowParserStream *stream, const uint8_t *lineBuf, uint16_t lineLen)
{
    if ((stream == NULL) || (stream->spec == NULL)) {
        return false;
    }

    return flowparserMatchPatterns(lineBuf, lineLen, stream->spec->errorPatterns, stream->spec->errorPatternCnt);
}

static bool flowparserStreamMatchResponseDone(const stFlowParserStream *stream, const uint8_t *lineBuf, uint16_t lineLen)
{
    if ((stream == NULL) || (stream->spec == NULL)) {
        return false;
    }

    return flowparserMatchPatterns(lineBuf, lineLen,
                                   stream->spec->responseDonePatterns,
                                   stream->spec->responseDonePatternCnt);
}

static bool flowparserStreamMatchFinalDone(const stFlowParserStream *stream, const uint8_t *lineBuf, uint16_t lineLen)
{
    if ((stream == NULL) || (stream->spec == NULL)) {
        return false;
    }

    return flowparserMatchPatterns(lineBuf, lineLen,
                                   stream->spec->finalDonePatterns,
                                   stream->spec->finalDonePatternCnt);
}
/**************************End of file********************************/
