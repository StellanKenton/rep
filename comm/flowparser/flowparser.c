/***********************************************************************************
* @file     : flowparser.c
* @brief    : Pattern matching helpers for line-oriented command parsing.
* @details  : This file keeps matcher semantics independent from transport bindings.
* @author   : GitHub Copilot
* @date     : 2026-04-21
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "flowparser.h"

#include <stddef.h>
#include <string.h>

bool flowparserMatchPattern(const uint8_t *lineBuf, uint16_t lineLen, const char *pattern)
{
    uint32_t patLen;

    if ((lineBuf == NULL) || (pattern == NULL)) {
        return false;
    }

    patLen = (uint32_t)strlen(pattern);
    if (patLen == 0U) {
        return false;
    }

    if (pattern[patLen - 1U] == '*') {
        patLen--;
        if ((patLen == 0U) || ((uint32_t)lineLen < patLen)) {
            return false;
        }
        return memcmp(lineBuf, pattern, patLen) == 0;
    }

    if ((uint32_t)lineLen != patLen) {
        return false;
    }

    return memcmp(lineBuf, pattern, patLen) == 0;
}

bool flowparserMatchPatterns(const uint8_t *lineBuf, uint16_t lineLen, const char *const *patterns, uint8_t patternCnt)
{
    uint8_t idx;

    if ((patterns == NULL) || (patternCnt == 0U)) {
        return false;
    }

    for (idx = 0U; idx < patternCnt; idx++) {
        if (flowparserMatchPattern(lineBuf, lineLen, patterns[idx])) {
            return true;
        }
    }

    return false;
}
/**************************End of file********************************/
