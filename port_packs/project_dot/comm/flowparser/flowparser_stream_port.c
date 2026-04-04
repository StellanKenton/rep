 /***********************************************************************************
* @file     : flowparser_stream_port.c
* @brief    : ESP-AT flow parser port helpers implementation.
* @details  : Supplies default timing values and a practical ESP-AT URC matcher.
* @author   : GitHub Copilot
* @date     : 2026-04-02
* @version  : V1.0.0
**********************************************************************************/
#include "flowparser_stream_port.h"

#include <string.h>

#include "rep_config.h"

#if (REP_RTOS_SYSTEM == REP_RTOS_FREERTOS)
#include "FreeRTOS.h"
#include "task.h"
#endif

static bool flowparserPortMatchPattern(const uint8_t *lineBuf, uint16_t lineLen, const char *pattern);
static bool flowparserPortMatchPatterns(const uint8_t *lineBuf, uint16_t lineLen, const char *const *patterns, uint8_t patternCnt);
static bool flowparserPortIsIdxUrc(const uint8_t *lineBuf, uint16_t lineLen, const char *suffix);

static const char *const gFlowparserPortEspAtRespDone[] = {
    "OK",
};

static const char *const gFlowparserPortEspAtSendDone[] = {
    "SEND OK",
};

static const char *const gFlowparserPortEspAtErr[] = {
    "ERROR",
    "SEND FAIL",
    "SEND Canceled",
    "SEND CANCELLED",
    "+CWJAP:*",
    "+ERRNO:*",
    "ERR CODE:*",
};

static const char *const gFlowparserPortEspAtUrcPatterns[] = {
    "ready",
    "busy p*",
    "WIFI *",
    "+ETH_*",
    "+IPD*",
    "+LINK_CONN*",
    "+STA_CONNECTED:*",
    "+STA_DISCONNECTED:*",
    "+DIST_STA_IP:*",
    "+TIME_UPDATED",
    "+SCRD:*",
    "Smart get wifi info",
    "smartconfig type:*",
    "smartconfig connected wifi",
    "Recv *",
    "Have *",
    "+QUITT",
    "Will force to restart!!!",
    "+MQTTCONNECTED*",
    "+MQTTDISCONNECTED*",
    "+MQTTSUBRECV*",
    "+MQTTPUB:FAIL",
    "+MQTTPUB:OK",
};

static bool flowparserPortMatchPattern(const uint8_t *lineBuf, uint16_t lineLen, const char *pattern)
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

static bool flowparserPortMatchPatterns(const uint8_t *lineBuf, uint16_t lineLen, const char *const *patterns, uint8_t patternCnt)
{
    uint8_t lIdx;

    if ((patterns == NULL) || (patternCnt == 0U)) {
        return false;
    }

    for (lIdx = 0U; lIdx < patternCnt; lIdx++) {
        if (flowparserPortMatchPattern(lineBuf, lineLen, patterns[lIdx])) {
            return true;
        }
    }

    return false;
}

static bool flowparserPortIsIdxUrc(const uint8_t *lineBuf, uint16_t lineLen, const char *suffix)
{
    uint32_t lSuffixLen;
    uint16_t lIdx = 0U;

    if ((lineBuf == NULL) || (suffix == NULL)) {
        return false;
    }

    while ((lIdx < lineLen) && (lineBuf[lIdx] >= '0') && (lineBuf[lIdx] <= '9')) {
        lIdx++;
    }

    if ((lIdx == 0U) || (lIdx >= lineLen) || (lineBuf[lIdx] != ',')) {
        return false;
    }

    lIdx++;
    lSuffixLen = (uint32_t)strlen(suffix);
    if ((lineLen - lIdx) != lSuffixLen) {
        return false;
    }

    return memcmp(&lineBuf[lIdx], suffix, lSuffixLen) == 0;
}

uint32_t flowparserPortGetTickMs(void)
{
#if (REP_RTOS_SYSTEM == REP_RTOS_FREERTOS)
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
#else
    return 0U;
#endif
}

void flowparserPortApplyDftCfg(stFlowParserStreamCfg *cfg)
{
    if (cfg == NULL) {
        return;
    }

    if (cfg->getTick == NULL) {
        cfg->getTick = flowparserPortGetTickMs;
    }

    if (cfg->procBudget == 0U) {
        cfg->procBudget = FLOWPARSER_PORT_PROC_BUDGET;
    }

    if (cfg->isUrc == NULL) {
        cfg->isUrc = flowparserPortIsEspAtUrc;
    }

    if ((cfg->urcPatterns == NULL) || (cfg->urcPatternCnt == 0U)) {
        cfg->urcPatterns = gFlowparserPortEspAtUrcPatterns;
        cfg->urcPatternCnt = (uint8_t)(sizeof(gFlowparserPortEspAtUrcPatterns) / sizeof(gFlowparserPortEspAtUrcPatterns[0]));
    }
}

void flowparserPortGetEspAtBaseSpec(stFlowParserSpec *spec)
{
    if (spec == NULL) {
        return;
    }

    (void)memset(spec, 0, sizeof(*spec));
    spec->responseDonePatterns = gFlowparserPortEspAtRespDone;
    spec->responseDonePatternCnt = (uint8_t)(sizeof(gFlowparserPortEspAtRespDone) / sizeof(gFlowparserPortEspAtRespDone[0]));
    spec->errorPatterns = gFlowparserPortEspAtErr;
    spec->errorPatternCnt = (uint8_t)(sizeof(gFlowparserPortEspAtErr) / sizeof(gFlowparserPortEspAtErr[0]));
    spec->totalToutMs = FLOWPARSER_PORT_TOTAL_TOUT_MS;
    spec->responseToutMs = FLOWPARSER_PORT_RESPONSE_TOUT_MS;
    spec->promptToutMs = FLOWPARSER_PORT_PROMPT_TOUT_MS;
    spec->finalToutMs = FLOWPARSER_PORT_FINAL_TOUT_MS;
    spec->needPrompt = false;
}

void flowparserPortGetEspAtSendSpec(stFlowParserSpec *spec)
{
    if (spec == NULL) {
        return;
    }

    (void)memset(spec, 0, sizeof(*spec));
    spec->finalDonePatterns = gFlowparserPortEspAtSendDone;
    spec->finalDonePatternCnt = (uint8_t)(sizeof(gFlowparserPortEspAtSendDone) / sizeof(gFlowparserPortEspAtSendDone[0]));
    spec->errorPatterns = gFlowparserPortEspAtErr;
    spec->errorPatternCnt = (uint8_t)(sizeof(gFlowparserPortEspAtErr) / sizeof(gFlowparserPortEspAtErr[0]));
    spec->totalToutMs = FLOWPARSER_PORT_TOTAL_TOUT_MS;
    spec->responseToutMs = FLOWPARSER_PORT_RESPONSE_TOUT_MS;
    spec->promptToutMs = FLOWPARSER_PORT_PROMPT_TOUT_MS;
    spec->finalToutMs = FLOWPARSER_PORT_FINAL_TOUT_MS;
    spec->needPrompt = true;
}

bool flowparserPortIsEspAtUrc(const uint8_t *lineBuf, uint16_t lineLen, void *userCtx)
{
    (void)userCtx;

    if ((lineBuf == NULL) || (lineLen == 0U)) {
        return false;
    }

    if (flowparserPortMatchPatterns(lineBuf, lineLen, gFlowparserPortEspAtUrcPatterns,
                                    (uint8_t)(sizeof(gFlowparserPortEspAtUrcPatterns) / sizeof(gFlowparserPortEspAtUrcPatterns[0])))) {
        return true;
    }

    if (flowparserPortMatchPattern(lineBuf, lineLen, "CONNECT") || flowparserPortMatchPattern(lineBuf, lineLen, "CLOSED")) {
        return true;
    }

    if (flowparserPortIsIdxUrc(lineBuf, lineLen, "CONNECT") || flowparserPortIsIdxUrc(lineBuf, lineLen, "CLOSED")) {
        return true;
    }

    return false;
}

eFlowParserStrmSta flowparserPortInitEspAt(stFlowParserStream *stream, stRingBuffer *ringBuf, uint8_t *lineBuf, uint16_t lineBufSize,
                                           uint8_t *cmdBuf, uint16_t cmdBufSize, uint8_t *payloadBuf, uint16_t payloadBufSize,
                                           flowparserStreamSendFunc sendFunc, void *portUserCtx, flowparserStreamLineFunc urcHandler, void *urcUserCtx)
{
    stFlowParserStreamCfg lCfg;

    if ((stream == NULL) || (ringBuf == NULL) || (lineBuf == NULL) || (cmdBuf == NULL) || (sendFunc == NULL)) {
        return FLOWPARSER_STREAM_INVALID_ARG;
    }

    (void)memset(&lCfg, 0, sizeof(lCfg));
    lCfg.tokCfg.ringBuf = ringBuf;
    lCfg.tokCfg.lineBuf = lineBuf;
    lCfg.tokCfg.lineBufSize = lineBufSize;
    lCfg.cmdBuf = cmdBuf;
    lCfg.cmdBufSize = cmdBufSize;
    lCfg.payloadBuf = payloadBuf;
    lCfg.payloadBufSize = payloadBufSize;
    lCfg.send = sendFunc;
    lCfg.portUserCtx = portUserCtx;
    lCfg.urcHandler = urcHandler;
    lCfg.urcUserCtx = urcUserCtx;

    flowparserPortApplyDftCfg(&lCfg);
    return flowparserStreamInit(stream, &lCfg);
}
/**************************End of file********************************/
