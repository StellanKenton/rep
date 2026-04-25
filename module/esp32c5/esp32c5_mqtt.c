/************************************************************************************
* @file     : esp32c5_mqtt.c
* @brief    : ESP32-C5 MQTT AT helper implementation.
* @details  : Provides MQTT command assembly and MQTT URC parsing for ESP-AT.
* @author   : GitHub Copilot
* @date     : 2026-04-25
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "esp32c5_mqtt.h"

#include <string.h>

static eEsp32c5Status esp32c5MqttAppendText(char *buffer, uint16_t bufferSize, uint16_t *length, const char *text);
static eEsp32c5Status esp32c5MqttAppendU16(char *buffer, uint16_t bufferSize, uint16_t *length, uint16_t value);
static eEsp32c5Status esp32c5MqttAppendQuotedText(char *buffer, uint16_t bufferSize, uint16_t *length, const char *text);
static eEsp32c5Status esp32c5MqttAppendQuotedBytes(char *buffer, uint16_t bufferSize, uint16_t *length, const uint8_t *bytes, uint16_t byteLen);
static bool esp32c5MqttMatchPrefix(const uint8_t *lineBuf, uint16_t lineLen, const char *prefix);
static bool esp32c5MqttTryParseUint(const uint8_t *buffer, uint16_t length, uint16_t *value);

bool esp32c5MqttIsUrc(const uint8_t *lineBuf, uint16_t lineLen)
{
    return esp32c5MqttMatchPrefix(lineBuf, lineLen, "+MQTTCONNECTED:") ||
           esp32c5MqttMatchPrefix(lineBuf, lineLen, "+MQTTDISCONNECTED:") ||
           esp32c5MqttMatchPrefix(lineBuf, lineLen, "+MQTTSUB:") ||
           esp32c5MqttMatchPrefix(lineBuf, lineLen, "+MQTTSUBRECV:");
}

bool esp32c5MqttTryParseConnState(const uint8_t *lineBuf, uint16_t lineLen, uint8_t *state)
{
    uint16_t index;
    uint16_t value;

    if ((lineBuf == NULL) || (state == NULL) || !esp32c5MqttMatchPrefix(lineBuf, lineLen, "+MQTTCONN:")) {
        return false;
    }

    index = (uint16_t)strlen("+MQTTCONN:");
    while ((index < lineLen) && (lineBuf[index] != ',')) {
        index++;
    }
    if ((index >= lineLen) || ((uint16_t)(index + 1U) >= lineLen)) {
        return false;
    }
    index++;

    value = 0U;
    while ((index < lineLen) && (lineBuf[index] != ',')) {
        if ((lineBuf[index] < '0') || (lineBuf[index] > '9')) {
            return false;
        }
        value = (uint16_t)((value * 10U) + (uint16_t)(lineBuf[index] - '0'));
        index++;
    }

    *state = (uint8_t)value;
    return true;
}

bool esp32c5MqttTryParseSubRecv(const uint8_t *lineBuf, uint16_t lineLen, const uint8_t **payload, uint16_t *payloadLen)
{
    uint16_t commaPos[3];
    uint16_t commaCount;
    uint16_t index;
    uint16_t valueLen;
    uint16_t dataStart;

    if ((lineBuf == NULL) || (payload == NULL) || (payloadLen == NULL) ||
        !esp32c5MqttMatchPrefix(lineBuf, lineLen, "+MQTTSUBRECV:")) {
        return false;
    }

    commaCount = 0U;
    for (index = (uint16_t)strlen("+MQTTSUBRECV:"); index < lineLen; index++) {
        if (lineBuf[index] == ',') {
            if (commaCount >= (uint16_t)(sizeof(commaPos) / sizeof(commaPos[0]))) {
                break;
            }
            commaPos[commaCount++] = index;
        }
    }
    if (commaCount < 3U) {
        return false;
    }

    if ((commaPos[2] <= (uint16_t)(commaPos[1] + 1U)) ||
        !esp32c5MqttTryParseUint(&lineBuf[commaPos[1] + 1U],
                                 (uint16_t)(commaPos[2] - (uint16_t)(commaPos[1] + 1U)),
                                 &valueLen)) {
        return false;
    }

    dataStart = (uint16_t)(commaPos[2] + 1U);
    if ((dataStart > lineLen) || ((uint16_t)(lineLen - dataStart) != valueLen)) {
        return false;
    }

    *payload = &lineBuf[dataStart];
    *payloadLen = valueLen;
    return true;
}

eEsp32c5Status esp32c5MqttBuildUserCfgCommand(const char *clientId,
                                              const char *username,
                                              const char *password,
                                              char *buffer,
                                              uint16_t bufferSize)
{
    uint16_t length;
    eEsp32c5Status status;

    if ((clientId == NULL) || (username == NULL) || (password == NULL) || (buffer == NULL) || (bufferSize == 0U)) {
        return ESP32C5_STATUS_INVALID_PARAM;
    }

    length = 0U;
    buffer[0] = '\0';
    status = esp32c5MqttAppendText(buffer, bufferSize, &length, "AT+MQTTUSERCFG=0,1,");
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5MqttAppendQuotedText(buffer, bufferSize, &length, clientId);
    }
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5MqttAppendText(buffer, bufferSize, &length, ",");
    }
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5MqttAppendQuotedText(buffer, bufferSize, &length, username);
    }
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5MqttAppendText(buffer, bufferSize, &length, ",");
    }
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5MqttAppendQuotedText(buffer, bufferSize, &length, password);
    }
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5MqttAppendText(buffer, bufferSize, &length, ",0,0,\"\"\r\n");
    }

    return status;
}

eEsp32c5Status esp32c5MqttBuildConnectCommand(const char *host, uint16_t port, char *buffer, uint16_t bufferSize)
{
    uint16_t length;
    eEsp32c5Status status;

    if ((host == NULL) || (host[0] == '\0') || (port == 0U) || (buffer == NULL) || (bufferSize == 0U)) {
        return ESP32C5_STATUS_INVALID_PARAM;
    }

    length = 0U;
    buffer[0] = '\0';
    status = esp32c5MqttAppendText(buffer, bufferSize, &length, "AT+MQTTCONN=0,");
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5MqttAppendQuotedText(buffer, bufferSize, &length, host);
    }
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5MqttAppendText(buffer, bufferSize, &length, ",");
    }
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5MqttAppendU16(buffer, bufferSize, &length, port);
    }
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5MqttAppendText(buffer, bufferSize, &length, ",1\r\n");
    }

    return status;
}

eEsp32c5Status esp32c5MqttBuildQueryCommand(char *buffer, uint16_t bufferSize)
{
    if ((buffer == NULL) || (bufferSize < 16U)) {
        return ESP32C5_STATUS_INVALID_PARAM;
    }

    (void)memcpy(buffer, "AT+MQTTCONN?\r\n", 15U);
    return ESP32C5_STATUS_OK;
}

eEsp32c5Status esp32c5MqttBuildSubscribeCommand(const char *topic, uint8_t qos, char *buffer, uint16_t bufferSize)
{
    uint16_t length;
    eEsp32c5Status status;

    if ((topic == NULL) || (topic[0] == '\0') || (qos > 1U) || (buffer == NULL) || (bufferSize == 0U)) {
        return ESP32C5_STATUS_INVALID_PARAM;
    }

    length = 0U;
    buffer[0] = '\0';
    status = esp32c5MqttAppendText(buffer, bufferSize, &length, "AT+MQTTSUB=0,");
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5MqttAppendQuotedText(buffer, bufferSize, &length, topic);
    }
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5MqttAppendText(buffer, bufferSize, &length, ",");
    }
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5MqttAppendU16(buffer, bufferSize, &length, qos);
    }
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5MqttAppendText(buffer, bufferSize, &length, "\r\n");
    }

    return status;
}

eEsp32c5Status esp32c5MqttBuildPublishCommand(const char *topic,
                                              const uint8_t *payload,
                                              uint16_t payloadLen,
                                              uint8_t qos,
                                              uint8_t retain,
                                              char *buffer,
                                              uint16_t bufferSize)
{
    uint16_t length;
    eEsp32c5Status status;

    if ((topic == NULL) || (topic[0] == '\0') || (payload == NULL) || (payloadLen == 0U) ||
        (qos > 1U) || (retain > 1U) || (buffer == NULL) || (bufferSize == 0U)) {
        return ESP32C5_STATUS_INVALID_PARAM;
    }

    length = 0U;
    buffer[0] = '\0';
    status = esp32c5MqttAppendText(buffer, bufferSize, &length, "AT+MQTTPUB=0,");
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5MqttAppendQuotedText(buffer, bufferSize, &length, topic);
    }
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5MqttAppendText(buffer, bufferSize, &length, ",");
    }
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5MqttAppendQuotedBytes(buffer, bufferSize, &length, payload, payloadLen);
    }
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5MqttAppendText(buffer, bufferSize, &length, ",");
    }
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5MqttAppendU16(buffer, bufferSize, &length, qos);
    }
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5MqttAppendText(buffer, bufferSize, &length, ",");
    }
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5MqttAppendU16(buffer, bufferSize, &length, retain);
    }
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5MqttAppendText(buffer, bufferSize, &length, "\r\n");
    }

    return status;
}

eEsp32c5Status esp32c5MqttBuildPublishRawCommand(const char *topic,
                                                 uint16_t payloadLen,
                                                 uint8_t qos,
                                                 uint8_t retain,
                                                 char *buffer,
                                                 uint16_t bufferSize)
{
    uint16_t length;
    eEsp32c5Status status;

    if ((topic == NULL) || (topic[0] == '\0') || (payloadLen == 0U) ||
        (qos > 1U) || (retain > 1U) || (buffer == NULL) || (bufferSize == 0U)) {
        return ESP32C5_STATUS_INVALID_PARAM;
    }

    length = 0U;
    buffer[0] = '\0';
    status = esp32c5MqttAppendText(buffer, bufferSize, &length, "AT+MQTTPUBRAW=0,");
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5MqttAppendQuotedText(buffer, bufferSize, &length, topic);
    }
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5MqttAppendText(buffer, bufferSize, &length, ",");
    }
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5MqttAppendU16(buffer, bufferSize, &length, payloadLen);
    }
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5MqttAppendText(buffer, bufferSize, &length, ",");
    }
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5MqttAppendU16(buffer, bufferSize, &length, qos);
    }
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5MqttAppendText(buffer, bufferSize, &length, ",");
    }
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5MqttAppendU16(buffer, bufferSize, &length, retain);
    }
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5MqttAppendText(buffer, bufferSize, &length, "\r\n");
    }

    return status;
}

static eEsp32c5Status esp32c5MqttAppendText(char *buffer, uint16_t bufferSize, uint16_t *length, const char *text)
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

static eEsp32c5Status esp32c5MqttAppendU16(char *buffer, uint16_t bufferSize, uint16_t *length, uint16_t value)
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

static eEsp32c5Status esp32c5MqttAppendQuotedText(char *buffer, uint16_t bufferSize, uint16_t *length, const char *text)
{
    eEsp32c5Status status;

    status = esp32c5MqttAppendText(buffer, bufferSize, length, "\"");
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
        status = esp32c5MqttAppendText(buffer, bufferSize, length, "\"");
    }
    return status;
}

static eEsp32c5Status esp32c5MqttAppendQuotedBytes(char *buffer, uint16_t bufferSize, uint16_t *length, const uint8_t *bytes, uint16_t byteLen)
{
    uint16_t index;
    eEsp32c5Status status;

    if ((buffer == NULL) || (length == NULL) || ((bytes == NULL) && (byteLen > 0U))) {
        return ESP32C5_STATUS_INVALID_PARAM;
    }

    status = esp32c5MqttAppendText(buffer, bufferSize, length, "\"");
    for (index = 0U; (status == ESP32C5_STATUS_OK) && (index < byteLen); index++) {
        if ((bytes[index] < 0x20U) || (bytes[index] == '"') || (bytes[index] == '\\')) {
            return ESP32C5_STATUS_INVALID_PARAM;
        }
        if (*length >= (uint16_t)(bufferSize - 1U)) {
            return ESP32C5_STATUS_OVERFLOW;
        }
        buffer[*length] = (char)bytes[index];
        *length = (uint16_t)(*length + 1U);
    }
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5MqttAppendText(buffer, bufferSize, length, "\"");
    }
    return status;
}

static bool esp32c5MqttMatchPrefix(const uint8_t *lineBuf, uint16_t lineLen, const char *prefix)
{
    uint16_t prefixLen;

    if ((lineBuf == NULL) || (prefix == NULL)) {
        return false;
    }

    prefixLen = (uint16_t)strlen(prefix);
    return (lineLen >= prefixLen) && (memcmp(lineBuf, prefix, prefixLen) == 0);
}

static bool esp32c5MqttTryParseUint(const uint8_t *buffer, uint16_t length, uint16_t *value)
{
    uint32_t parsed;
    uint16_t index;

    if ((buffer == NULL) || (length == 0U) || (value == NULL)) {
        return false;
    }

    parsed = 0U;
    for (index = 0U; index < length; index++) {
        if ((buffer[index] < '0') || (buffer[index] > '9')) {
            return false;
        }
        parsed = (parsed * 10U) + (uint32_t)(buffer[index] - '0');
        if (parsed > 65535UL) {
            return false;
        }
    }

    *value = (uint16_t)parsed;
    return true;
}

/**************************End of file********************************/
