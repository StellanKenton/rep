/************************************************************************************
* @file     : ec800m_http.c
* @brief    : EC800M HTTP AT helper implementation.
* @details  : Provides Quectel HTTP command assembly helpers.
* @author   : GitHub Copilot
* @date     : 2026-04-27
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "ec800m_http.h"

static eEc800mStatus ec800mHttpAppendText(char *buffer, uint16_t bufferSize, uint16_t *length, const char *text);
static eEc800mStatus ec800mHttpAppendU16(char *buffer, uint16_t bufferSize, uint16_t *length, uint16_t value);

eEc800mStatus ec800mHttpBuildUrlCommand(uint16_t urlLen, uint16_t inputTimeoutSec, char *buffer, uint16_t bufferSize)
{
    uint16_t length = 0U;
    eEc800mStatus status;

    if ((urlLen == 0U) || (buffer == NULL) || (bufferSize == 0U)) {
        return EC800M_STATUS_INVALID_PARAM;
    }

    buffer[0] = '\0';
    status = ec800mHttpAppendText(buffer, bufferSize, &length, "AT+QHTTPURL=");
    if (status == EC800M_STATUS_OK) {
        status = ec800mHttpAppendU16(buffer, bufferSize, &length, urlLen);
    }
    if (status == EC800M_STATUS_OK) {
        status = ec800mHttpAppendText(buffer, bufferSize, &length, ",");
    }
    if (status == EC800M_STATUS_OK) {
        status = ec800mHttpAppendU16(buffer, bufferSize, &length, inputTimeoutSec);
    }
    if (status == EC800M_STATUS_OK) {
        status = ec800mHttpAppendText(buffer, bufferSize, &length, "\r\n");
    }
    return status;
}

eEc800mStatus ec800mHttpBuildPostCommand(uint16_t inputTimeoutSec, uint16_t responseTimeoutSec, uint16_t outputTimeoutSec, char *buffer, uint16_t bufferSize)
{
    uint16_t length = 0U;
    eEc800mStatus status;

    if ((inputTimeoutSec == 0U) || (responseTimeoutSec == 0U) || (buffer == NULL) || (bufferSize == 0U)) {
        return EC800M_STATUS_INVALID_PARAM;
    }

    buffer[0] = '\0';
    status = ec800mHttpAppendText(buffer, bufferSize, &length, "AT+QHTTPPOST=");
    if (status == EC800M_STATUS_OK) {
        status = ec800mHttpAppendU16(buffer, bufferSize, &length, inputTimeoutSec);
    }
    if (status == EC800M_STATUS_OK) {
        status = ec800mHttpAppendText(buffer, bufferSize, &length, ",");
    }
    if (status == EC800M_STATUS_OK) {
        status = ec800mHttpAppendU16(buffer, bufferSize, &length, responseTimeoutSec);
    }
    if (status == EC800M_STATUS_OK) {
        status = ec800mHttpAppendText(buffer, bufferSize, &length, ",");
    }
    if (status == EC800M_STATUS_OK) {
        status = ec800mHttpAppendU16(buffer, bufferSize, &length, outputTimeoutSec);
    }
    if (status == EC800M_STATUS_OK) {
        status = ec800mHttpAppendText(buffer, bufferSize, &length, "\r\n");
    }
    return status;
}

eEc800mStatus ec800mHttpBuildReadCommand(uint16_t responseTimeoutSec, char *buffer, uint16_t bufferSize)
{
    uint16_t length = 0U;
    eEc800mStatus status;

    if ((responseTimeoutSec == 0U) || (buffer == NULL) || (bufferSize == 0U)) {
        return EC800M_STATUS_INVALID_PARAM;
    }

    buffer[0] = '\0';
    status = ec800mHttpAppendText(buffer, bufferSize, &length, "AT+QHTTPREAD=");
    if (status == EC800M_STATUS_OK) {
        status = ec800mHttpAppendU16(buffer, bufferSize, &length, responseTimeoutSec);
    }
    if (status == EC800M_STATUS_OK) {
        status = ec800mHttpAppendText(buffer, bufferSize, &length, "\r\n");
    }
    return status;
}

static eEc800mStatus ec800mHttpAppendText(char *buffer, uint16_t bufferSize, uint16_t *length, const char *text)
{
    if ((buffer == NULL) || (length == NULL) || (text == NULL)) {
        return EC800M_STATUS_INVALID_PARAM;
    }

    while (*text != '\0') {
        if (*length >= (uint16_t)(bufferSize - 1U)) {
            return EC800M_STATUS_OVERFLOW;
        }
        buffer[*length] = *text;
        *length = (uint16_t)(*length + 1U);
        text++;
    }
    buffer[*length] = '\0';
    return EC800M_STATUS_OK;
}

static eEc800mStatus ec800mHttpAppendU16(char *buffer, uint16_t bufferSize, uint16_t *length, uint16_t value)
{
    char digits[5];
    uint16_t index = 0U;

    if ((buffer == NULL) || (length == NULL)) {
        return EC800M_STATUS_INVALID_PARAM;
    }

    do {
        digits[index++] = (char)('0' + (value % 10U));
        value = (uint16_t)(value / 10U);
    } while ((value > 0U) && (index < (uint16_t)sizeof(digits)));

    while (index > 0U) {
        index--;
        if (*length >= (uint16_t)(bufferSize - 1U)) {
            return EC800M_STATUS_OVERFLOW;
        }
        buffer[*length] = digits[index];
        *length = (uint16_t)(*length + 1U);
    }
    buffer[*length] = '\0';
    return EC800M_STATUS_OK;
}

/**************************End of file********************************/
