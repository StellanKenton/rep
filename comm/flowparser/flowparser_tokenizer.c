 /***********************************************************************************
* @file     : flowparser_tokenizer.c
* @brief    : AT command stream tokenizer implementation.
* @details  : Emits line tokens on CRLF or LF and emits '>' as a prompt token.
* @author   : GitHub Copilot
* @date     : 2026-04-02
* @version  : V1.0.0
**********************************************************************************/
#include "flowparser_tokenizer.h"

#include <string.h>

static bool flowparserTokAppendByte(stFlowParserTokenizer *tokenizer, uint8_t byte);
static void flowparserTokSetLineToken(stFlowParserTokenizer *tokenizer, stFlowParserTok *token);
static void flowparserTokSetSimpleToken(stFlowParserTok *token, eFlowParserTokType type);

static bool flowparserTokAppendByte(stFlowParserTokenizer *tokenizer, uint8_t byte)
{
    if ((tokenizer == NULL) || (tokenizer->lineBuf == NULL) || (tokenizer->lineBufSize < 2U)) {
        return false;
    }

    if (tokenizer->lineLen >= (uint16_t)(tokenizer->lineBufSize - 1U)) {
        return false;
    }

    tokenizer->lineBuf[tokenizer->lineLen] = byte;
    tokenizer->lineLen++;
    tokenizer->lineBuf[tokenizer->lineLen] = 0U;
    return true;
}

static void flowparserTokSetLineToken(stFlowParserTokenizer *tokenizer, stFlowParserTok *token)
{
    token->type = FLOWPARSER_TOK_TYPE_LINE;
    token->buf = tokenizer->lineBuf;
    token->len = tokenizer->lineLen;
    tokenizer->lineBuf[tokenizer->lineLen] = 0U;
    tokenizer->lineLen = 0U;
}

static void flowparserTokSetSimpleToken(stFlowParserTok *token, eFlowParserTokType type)
{
    token->type = type;
    token->buf = NULL;
    token->len = 0U;
}

bool flowparserTokIsCfgValid(const stFlowParserTokCfg *cfg)
{
    if ((cfg == NULL) || (cfg->ringBuf == NULL) || (cfg->lineBuf == NULL) || (cfg->lineBufSize < 2U)) {
        return false;
    }

    return true;
}

eFlowParserTokSta flowparserTokInit(stFlowParserTokenizer *tokenizer, const stFlowParserTokCfg *cfg)
{
    if ((tokenizer == NULL) || (!flowparserTokIsCfgValid(cfg))) {
        return FLOWPARSER_TOK_INVALID_ARG;
    }

    (void)memset(tokenizer, 0, sizeof(*tokenizer));
    tokenizer->ringBuf = cfg->ringBuf;
    tokenizer->lineBuf = cfg->lineBuf;
    tokenizer->lineBufSize = cfg->lineBufSize;
    tokenizer->lineBuf[0] = 0U;
    tokenizer->isInit = true;
    return FLOWPARSER_TOK_OK;
}

void flowparserTokReset(stFlowParserTokenizer *tokenizer)
{
    if ((tokenizer == NULL) || (tokenizer->isInit == false)) {
        return;
    }

    tokenizer->lineLen = 0U;
    tokenizer->isOverflow = false;
    tokenizer->hasPendCr = false;
    tokenizer->lineBuf[0] = 0U;
}

eFlowParserTokSta flowparserTokGet(stFlowParserTokenizer *tokenizer, stFlowParserTok *token)
{
    uint8_t lByte;

    if ((tokenizer == NULL) || (token == NULL)) {
        return FLOWPARSER_TOK_INVALID_ARG;
    }

    if (tokenizer->isInit == false) {
        return FLOWPARSER_TOK_NOT_INIT;
    }

    flowparserTokSetSimpleToken(token, FLOWPARSER_TOK_TYPE_NONE);

    while (ringBufferPopByte(tokenizer->ringBuf, &lByte) == RINGBUFFER_OK) {
        if (tokenizer->isOverflow) {
            if (tokenizer->hasPendCr) {
                tokenizer->hasPendCr = false;
                if (lByte == '\n') {
                    flowparserTokSetSimpleToken(token, FLOWPARSER_TOK_TYPE_OVERFLOW);
                    tokenizer->isOverflow = false;
                    return FLOWPARSER_TOK_OK;
                }
            }

            if (lByte == '\r') {
                tokenizer->hasPendCr = true;
                continue;
            }

            if (lByte == '\n') {
                flowparserTokSetSimpleToken(token, FLOWPARSER_TOK_TYPE_OVERFLOW);
                tokenizer->isOverflow = false;
                return FLOWPARSER_TOK_OK;
            }

            continue;
        }

        if (tokenizer->hasPendCr) {
            tokenizer->hasPendCr = false;
            if (lByte == '\n') {
                flowparserTokSetLineToken(tokenizer, token);
                return FLOWPARSER_TOK_OK;
            }

            if (flowparserTokAppendByte(tokenizer, '\r') == false) {
                tokenizer->lineLen = 0U;
                tokenizer->lineBuf[0] = 0U;
                tokenizer->isOverflow = true;
            }
        }

        if (tokenizer->isOverflow) {
            if (lByte == '\r') {
                tokenizer->hasPendCr = true;
            } else if (lByte == '\n') {
                flowparserTokSetSimpleToken(token, FLOWPARSER_TOK_TYPE_OVERFLOW);
                tokenizer->isOverflow = false;
                return FLOWPARSER_TOK_OK;
            }
            continue;
        }

        if (lByte == '\r') {
            tokenizer->hasPendCr = true;
            continue;
        }

        if (lByte == '\n') {
            flowparserTokSetLineToken(tokenizer, token);
            return FLOWPARSER_TOK_OK;
        }

        if ((lByte == '>') && (tokenizer->lineLen == 0U)) {
            flowparserTokSetSimpleToken(token, FLOWPARSER_TOK_TYPE_PROMPT);
            return FLOWPARSER_TOK_OK;
        }

        if (flowparserTokAppendByte(tokenizer, lByte) == false) {
            tokenizer->lineLen = 0U;
            tokenizer->lineBuf[0] = 0U;
            tokenizer->isOverflow = true;
        }
    }

    return FLOWPARSER_TOK_EMPTY;
}
/**************************End of file********************************/
