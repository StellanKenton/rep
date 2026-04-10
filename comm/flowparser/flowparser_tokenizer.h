/************************************************************************************
* @file     : flowparser_tokenizer.h
* @brief    : AT command stream tokenizer public API.
* @details  : Splits a UART byte stream into LINE, PROMPT and OVERFLOW tokens.
* @author   : GitHub Copilot
* @date     : 2026-04-02
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef FLOWPARSER_TOKENIZER_H
#define FLOWPARSER_TOKENIZER_H

#include <stdbool.h>
#include <stdint.h>

#include "ringbuffer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eFlowParserTokSta {
    FLOWPARSER_TOK_OK = 0,
    FLOWPARSER_TOK_EMPTY,
    FLOWPARSER_TOK_INVALID_ARG,
    FLOWPARSER_TOK_NOT_INIT,
} eFlowParserTokSta;

typedef enum eFlowParserTokType {
    FLOWPARSER_TOK_TYPE_NONE = 0,
    FLOWPARSER_TOK_TYPE_LINE,
    FLOWPARSER_TOK_TYPE_PROMPT,
    FLOWPARSER_TOK_TYPE_OVERFLOW,
} eFlowParserTokType;

typedef struct stFlowParserTok {
    eFlowParserTokType type;
    const uint8_t *buf;
    uint16_t len;
} stFlowParserTok;

typedef struct stFlowParserTokCfg {
    stRingBuffer *ringBuf;
    uint8_t *lineBuf;
    uint16_t lineBufSize;
} stFlowParserTokCfg;

typedef struct stFlowParserTokenizer {
    stRingBuffer *ringBuf;
    uint8_t *lineBuf;
    uint16_t lineBufSize;
    uint16_t lineLen;
    bool isOverflow;
    bool hasPendCr;
    bool isInit;
} stFlowParserTokenizer;

bool flowparserTokIsCfgValid(const stFlowParserTokCfg *cfg);
eFlowParserTokSta flowparserTokInit(stFlowParserTokenizer *tokenizer, const stFlowParserTokCfg *cfg);
void flowparserTokReset(stFlowParserTokenizer *tokenizer);
eFlowParserTokSta flowparserTokGet(stFlowParserTokenizer *tokenizer, stFlowParserTok *token);

#ifdef __cplusplus
}
#endif

#endif  // FLOWPARSER_TOKENIZER_H
/**************************End of file********************************/
