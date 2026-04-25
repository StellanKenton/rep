/************************************************************************************
* @file     : esp32c5_http.c
* @brief    : ESP32-C5 HTTP AT helper implementation.
* @details  : Provides HTTP POST command assembly for ESP-AT prompt transactions.
* @author   : GitHub Copilot
* @date     : 2026-04-25
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "esp32c5_http.h"

static eEsp32c5Status esp32c5HttpAppendText(char *buffer, uint16_t bufferSize, uint16_t *length, const char *text);
static eEsp32c5Status esp32c5HttpAppendU16(char *buffer, uint16_t bufferSize, uint16_t *length, uint16_t value);
static eEsp32c5Status esp32c5HttpAppendQuotedText(char *buffer, uint16_t bufferSize, uint16_t *length, const char *text);

eEsp32c5Status esp32c5HttpBuildPostJsonCommand(const char *url, uint16_t payloadLen, char *buffer, uint16_t bufferSize)
{
    uint16_t length;
    eEsp32c5Status status;

    if ((url == NULL) || (payloadLen == 0U) || (buffer == NULL) || (bufferSize == 0U)) {
        return ESP32C5_STATUS_INVALID_PARAM;
    }

    length = 0U;
    buffer[0] = '\0';
    status = esp32c5HttpAppendText(buffer, bufferSize, &length, "AT+HTTPCPOST=");
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5HttpAppendQuotedText(buffer, bufferSize, &length, url);
    }
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5HttpAppendText(buffer, bufferSize, &length, ",");
    }
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5HttpAppendU16(buffer, bufferSize, &length, payloadLen);
    }
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5HttpAppendText(buffer, bufferSize, &length, ",1,\"Content-Type: application/json\"\r\n");
    }

    return status;
}

static eEsp32c5Status esp32c5HttpAppendText(char *buffer, uint16_t bufferSize, uint16_t *length, const char *text)
{
    if ((buffer == NULL) || (length == NULL) || (text == NULL)) {
        return ESP32C5_STATUS_INVALID_PARAM;
    }

    while (*text != '\0') {
        if (*length >= (uint16_t)(bufferSize - 1U)) {
            return ESP32C5_STATUS_OVERFLOW;
        }
        buffer[*length] = *text;
        *length = (uint16_t)(*length + 1U);
        text++;
    }
    buffer[*length] = '\0';
    return ESP32C5_STATUS_OK;
}

static eEsp32c5Status esp32c5HttpAppendU16(char *buffer, uint16_t bufferSize, uint16_t *length, uint16_t value)
{
    char digits[5];
    uint16_t index;

    if ((buffer == NULL) || (length == NULL)) {
        return ESP32C5_STATUS_INVALID_PARAM;
    }

    index = 0U;
    do {
        digits[index++] = (char)('0' + (value % 10U));
        value = (uint16_t)(value / 10U);
    } while ((value > 0U) && (index < (uint16_t)sizeof(digits)));

    while (index > 0U) {
        index--;
        if (*length >= (uint16_t)(bufferSize - 1U)) {
            return ESP32C5_STATUS_OVERFLOW;
        }
        buffer[*length] = digits[index];
        *length = (uint16_t)(*length + 1U);
    }
    buffer[*length] = '\0';
    return ESP32C5_STATUS_OK;
}

static eEsp32c5Status esp32c5HttpAppendQuotedText(char *buffer, uint16_t bufferSize, uint16_t *length, const char *text)
{
    eEsp32c5Status status;

    status = esp32c5HttpAppendText(buffer, bufferSize, length, "\"");
    while ((status == ESP32C5_STATUS_OK) && (*text != '\0')) {
        if ((*text == '"') || (*text == '\r') || (*text == '\n')) {
            return ESP32C5_STATUS_INVALID_PARAM;
        }
        if (*length >= (uint16_t)(bufferSize - 1U)) {
            return ESP32C5_STATUS_OVERFLOW;
        }
        buffer[*length] = *text;
        *length = (uint16_t)(*length + 1U);
        text++;
    }
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5HttpAppendText(buffer, bufferSize, length, "\"");
    }
    return status;
}

/**************************End of file********************************/
