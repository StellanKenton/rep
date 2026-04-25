/************************************************************************************
* @file     : esp32c5_wifi.c
* @brief    : ESP32-C5 WiFi AT helper implementation.
* @details  : Provides WiFi station command assembly and URC prefix matching.
* @author   : GitHub Copilot
* @date     : 2026-04-25
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "esp32c5_wifi.h"

#include <string.h>

static eEsp32c5Status esp32c5WifiAppendText(char *buffer, uint16_t bufferSize, uint16_t *length, const char *text);
static eEsp32c5Status esp32c5WifiAppendQuotedText(char *buffer, uint16_t bufferSize, uint16_t *length, const char *text);
static bool esp32c5WifiMatchPrefix(const uint8_t *lineBuf, uint16_t lineLen, const char *prefix);

bool esp32c5WifiIsUrc(const uint8_t *lineBuf, uint16_t lineLen)
{
    return esp32c5WifiMatchPrefix(lineBuf, lineLen, "WIFI CONNECTED") ||
           esp32c5WifiMatchPrefix(lineBuf, lineLen, "WIFI GOT IP") ||
           esp32c5WifiMatchPrefix(lineBuf, lineLen, "WIFI DISCONNECT") ||
           esp32c5WifiMatchPrefix(lineBuf, lineLen, "WIFI DISCONNECTED");
}

eEsp32c5Status esp32c5WifiBuildStationModeCommand(char *buffer, uint16_t bufferSize)
{
    if ((buffer == NULL) || (bufferSize < 14U)) {
        return ESP32C5_STATUS_INVALID_PARAM;
    }

    (void)memcpy(buffer, "AT+CWMODE=1\r\n", 14U);
    return ESP32C5_STATUS_OK;
}

eEsp32c5Status esp32c5WifiBuildJoinCommand(const char *ssid, const char *password, char *buffer, uint16_t bufferSize)
{
    uint16_t length;
    eEsp32c5Status status;

    if ((ssid == NULL) || (password == NULL) || (buffer == NULL) || (bufferSize == 0U)) {
        return ESP32C5_STATUS_INVALID_PARAM;
    }

    length = 0U;
    buffer[0] = '\0';
    status = esp32c5WifiAppendText(buffer, bufferSize, &length, "AT+CWJAP=");
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5WifiAppendQuotedText(buffer, bufferSize, &length, ssid);
    }
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5WifiAppendText(buffer, bufferSize, &length, ",");
    }
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5WifiAppendQuotedText(buffer, bufferSize, &length, password);
    }
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5WifiAppendText(buffer, bufferSize, &length, "\r\n");
    }

    return status;
}

static eEsp32c5Status esp32c5WifiAppendText(char *buffer, uint16_t bufferSize, uint16_t *length, const char *text)
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

static eEsp32c5Status esp32c5WifiAppendQuotedText(char *buffer, uint16_t bufferSize, uint16_t *length, const char *text)
{
    eEsp32c5Status status;

    status = esp32c5WifiAppendText(buffer, bufferSize, length, "\"");
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
        status = esp32c5WifiAppendText(buffer, bufferSize, length, "\"");
    }
    return status;
}

static bool esp32c5WifiMatchPrefix(const uint8_t *lineBuf, uint16_t lineLen, const char *prefix)
{
    uint16_t prefixLen;

    if ((lineBuf == NULL) || (prefix == NULL)) {
        return false;
    }

    prefixLen = (uint16_t)strlen(prefix);
    return (lineLen >= prefixLen) && (memcmp(lineBuf, prefix, prefixLen) == 0);
}

/**************************End of file********************************/
