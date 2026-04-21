/************************************************************************************
* @file     : flowparser.h
* @brief    : Public types and helpers for line-oriented command transactions.
* @details  : This module defines reusable transaction/result descriptors for AT-like streams.
* @author   : GitHub Copilot
* @date     : 2026-04-21
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef FLOWPARSER_H
#define FLOWPARSER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eFlowParserResult {
    FLOWPARSER_RESULT_OK = 0,
    FLOWPARSER_RESULT_ERROR,
    FLOWPARSER_RESULT_TIMEOUT,
    FLOWPARSER_RESULT_OVERFLOW,
    FLOWPARSER_RESULT_SEND_FAIL,
} eFlowParserResult;

typedef enum eFlowParserStage {
    FLOWPARSER_STAGE_IDLE = 0,
    FLOWPARSER_STAGE_WAIT_RESPONSE,
    FLOWPARSER_STAGE_WAIT_PROMPT,
    FLOWPARSER_STAGE_WAIT_FINAL,
} eFlowParserStage;

typedef void (*flowparserLineFunc)(void *userData, const uint8_t *lineBuf, uint16_t lineLen);
typedef void (*flowparserDoneFunc)(void *userData, eFlowParserResult result);

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
    flowparserLineFunc lineHandler;
    flowparserDoneFunc doneHandler;
    void *userData;
} stFlowParserReq;

bool flowparserMatchPattern(const uint8_t *lineBuf, uint16_t lineLen, const char *pattern);
bool flowparserMatchPatterns(const uint8_t *lineBuf, uint16_t lineLen, const char *const *patterns, uint8_t patternCnt);

#ifdef __cplusplus
}
#endif

#endif  // FLOWPARSER_H
/**************************End of file********************************/
