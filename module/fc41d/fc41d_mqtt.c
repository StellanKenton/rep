/************************************************************************************
* @file     : fc41d_mqtt.c
* @brief    : FC41D MQTT AT helper implementation.
***********************************************************************************/
#include "fc41d_mqtt.h"

#include <string.h>

static eFc41dStatus appendText(char *buffer, uint16_t bufferSize, uint16_t *length, const char *text);
static eFc41dStatus appendU16(char *buffer, uint16_t bufferSize, uint16_t *length, uint16_t value);
static eFc41dStatus appendQuotedText(char *buffer, uint16_t bufferSize, uint16_t *length, const char *text);
static bool matchPrefix(const uint8_t *lineBuf, uint16_t lineLen, const char *prefix);
static bool tryParseUint(const uint8_t *buffer, uint16_t length, uint16_t *value);

bool fc41dMqttIsUrc(const uint8_t *lineBuf, uint16_t lineLen)
{
    return matchPrefix(lineBuf, lineLen, "+QMTRECV:") ||
           matchPrefix(lineBuf, lineLen, "+QMTCONN:") ||
           matchPrefix(lineBuf, lineLen, "+QMTSUB:") ||
           matchPrefix(lineBuf, lineLen, "+MQTTCONNECTED:") ||
           matchPrefix(lineBuf, lineLen, "+MQTTDISCONNECTED:") ||
           matchPrefix(lineBuf, lineLen, "+MQTTSUB:") ||
           matchPrefix(lineBuf, lineLen, "+MQTTSUBRECV:");
}

bool fc41dMqttTryParseConnState(const uint8_t *lineBuf, uint16_t lineLen, uint8_t *state)
{
    uint16_t index;
    uint16_t value;

    if ((lineBuf == NULL) || (state == NULL) ||
        (!matchPrefix(lineBuf, lineLen, "+MQTTCONN:") && !matchPrefix(lineBuf, lineLen, "+QMTCONN:"))) {
        return false;
    }

    index = matchPrefix(lineBuf, lineLen, "+QMTCONN:") ? (uint16_t)strlen("+QMTCONN:") : (uint16_t)strlen("+MQTTCONN:");
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

bool fc41dMqttTryParseSubRecv(const uint8_t *lineBuf, uint16_t lineLen, const uint8_t **payload, uint16_t *payloadLen)
{
    uint16_t commaPos[3];
    uint16_t commaCount;
    uint16_t index;
    uint16_t valueLen;
    uint16_t dataStart;
    uint16_t topicEnd;
    uint16_t lenStart;

    if ((lineBuf == NULL) || (payload == NULL) || (payloadLen == NULL)) {
        return false;
    }

    if (matchPrefix(lineBuf, lineLen, "+QMTRECV:")) {
        index = (uint16_t)strlen("+QMTRECV:");
        while ((index < lineLen) && (lineBuf[index] != '"')) index++;
        if (index >= lineLen) return false;
        index++;
        while ((index < lineLen) && (lineBuf[index] != '"')) index++;
        if ((index >= lineLen) || ((uint16_t)(index + 2U) >= lineLen) || (lineBuf[index + 1U] != ',')) return false;
        topicEnd = index;
        (void)topicEnd;
        lenStart = (uint16_t)(index + 2U);
        index = lenStart;
        while ((index < lineLen) && (lineBuf[index] != ',')) index++;
        if ((index >= lineLen) || !tryParseUint(&lineBuf[lenStart], (uint16_t)(index - lenStart), &valueLen)) return false;
        if (((uint16_t)(index + 1U) >= lineLen) || (lineBuf[index + 1U] != '"')) return false;
        dataStart = (uint16_t)(index + 2U);
        if (((uint16_t)(dataStart + valueLen) > lineLen) ||
            (((uint16_t)(dataStart + valueLen) < lineLen) && (lineBuf[dataStart + valueLen] != '"'))) {
            return false;
        }
        *payload = &lineBuf[dataStart];
        *payloadLen = valueLen;
        return true;
    }

    if (!matchPrefix(lineBuf, lineLen, "+MQTTSUBRECV:")) {
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
    if ((commaCount < 3U) || (commaPos[2] <= (uint16_t)(commaPos[1] + 1U)) ||
        !tryParseUint(&lineBuf[commaPos[1] + 1U], (uint16_t)(commaPos[2] - (uint16_t)(commaPos[1] + 1U)), &valueLen)) {
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

eFc41dStatus fc41dMqttBuildUserCfgCommand(const char *clientId, const char *username, const char *password, char *buffer, uint16_t bufferSize)
{
    uint16_t length = 0U;

    if ((clientId == NULL) || (username == NULL) || (password == NULL) || (buffer == NULL) || (bufferSize == 0U)) {
        return FC41D_STATUS_INVALID_PARAM;
    }

    (void)clientId;
    (void)username;
    (void)password;
    return appendText(buffer, bufferSize, &length, "AT+QMTCFG=\"keepalive\",0,60\r\n");
}

eFc41dStatus fc41dMqttBuildRecvModeCommand(uint8_t mode, char *buffer, uint16_t bufferSize)
{
    uint16_t length = 0U;
    eFc41dStatus status;

    if ((mode > 1U) || (buffer == NULL) || (bufferSize == 0U)) {
        return FC41D_STATUS_INVALID_PARAM;
    }

    buffer[0] = '\0';
    status = appendText(buffer, bufferSize, &length, "AT+QMTCFG=\"recv/mode\",0,");
    if (status == FC41D_STATUS_OK) status = appendU16(buffer, bufferSize, &length, mode);
    if (status == FC41D_STATUS_OK) status = appendText(buffer, bufferSize, &length, "\r\n");
    return status;
}

eFc41dStatus fc41dMqttBuildConnectCommand(const char *host, uint16_t port, char *buffer, uint16_t bufferSize)
{
    uint16_t length = 0U;
    eFc41dStatus status;

    if ((host == NULL) || (host[0] == '\0') || (port == 0U) || (buffer == NULL) || (bufferSize == 0U)) {
        return FC41D_STATUS_INVALID_PARAM;
    }

    buffer[0] = '\0';
    status = appendText(buffer, bufferSize, &length, "AT+QMTOPEN=0,");
    if (status == FC41D_STATUS_OK) status = appendQuotedText(buffer, bufferSize, &length, host);
    if (status == FC41D_STATUS_OK) status = appendText(buffer, bufferSize, &length, ",");
    if (status == FC41D_STATUS_OK) status = appendU16(buffer, bufferSize, &length, port);
    if (status == FC41D_STATUS_OK) status = appendText(buffer, bufferSize, &length, "\r\n");
    return status;
}

eFc41dStatus fc41dMqttBuildLoginCommand(const char *clientId, const char *username, const char *password, char *buffer, uint16_t bufferSize)
{
    uint16_t length = 0U;
    eFc41dStatus status;

    if ((clientId == NULL) || (username == NULL) || (password == NULL) || (buffer == NULL) || (bufferSize == 0U)) {
        return FC41D_STATUS_INVALID_PARAM;
    }

    buffer[0] = '\0';
    status = appendText(buffer, bufferSize, &length, "AT+QMTCONN=0,");
    if (status == FC41D_STATUS_OK) status = appendQuotedText(buffer, bufferSize, &length, clientId);
    if (status == FC41D_STATUS_OK) status = appendText(buffer, bufferSize, &length, ",");
    if (status == FC41D_STATUS_OK) status = appendQuotedText(buffer, bufferSize, &length, username);
    if (status == FC41D_STATUS_OK) status = appendText(buffer, bufferSize, &length, ",");
    if (status == FC41D_STATUS_OK) status = appendQuotedText(buffer, bufferSize, &length, password);
    if (status == FC41D_STATUS_OK) status = appendText(buffer, bufferSize, &length, "\r\n");
    return status;
}

eFc41dStatus fc41dMqttBuildQueryCommand(char *buffer, uint16_t bufferSize)
{
    if ((buffer == NULL) || (bufferSize < 16U)) {
        return FC41D_STATUS_INVALID_PARAM;
    }
    (void)memcpy(buffer, "AT+QMTCONN?\r\n", 14U);
    return FC41D_STATUS_OK;
}

eFc41dStatus fc41dMqttBuildSubscribeCommand(const char *topic, uint8_t qos, char *buffer, uint16_t bufferSize)
{
    uint16_t length = 0U;
    eFc41dStatus status;

    if ((topic == NULL) || (topic[0] == '\0') || (qos > 1U) || (buffer == NULL) || (bufferSize == 0U)) {
        return FC41D_STATUS_INVALID_PARAM;
    }

    buffer[0] = '\0';
    status = appendText(buffer, bufferSize, &length, "AT+QMTSUB=0,1,");
    if (status == FC41D_STATUS_OK) status = appendQuotedText(buffer, bufferSize, &length, topic);
    if (status == FC41D_STATUS_OK) status = appendText(buffer, bufferSize, &length, ",");
    if (status == FC41D_STATUS_OK) status = appendU16(buffer, bufferSize, &length, qos);
    if (status == FC41D_STATUS_OK) status = appendText(buffer, bufferSize, &length, "\r\n");
    return status;
}

eFc41dStatus fc41dMqttBuildPublishRawCommand(const char *topic, uint16_t payloadLen, uint8_t qos, uint8_t retain, char *buffer, uint16_t bufferSize)
{
    uint16_t length = 0U;
    eFc41dStatus status;

    if ((topic == NULL) || (topic[0] == '\0') || (payloadLen == 0U) || (qos > 1U) || (retain > 1U) ||
        (buffer == NULL) || (bufferSize == 0U)) {
        return FC41D_STATUS_INVALID_PARAM;
    }

    buffer[0] = '\0';
    status = appendText(buffer, bufferSize, &length, "AT+MQTTPUBRAW=0,");
    if (status == FC41D_STATUS_OK) status = appendQuotedText(buffer, bufferSize, &length, topic);
    if (status == FC41D_STATUS_OK) status = appendText(buffer, bufferSize, &length, ",");
    if (status == FC41D_STATUS_OK) status = appendU16(buffer, bufferSize, &length, payloadLen);
    if (status == FC41D_STATUS_OK) status = appendText(buffer, bufferSize, &length, ",");
    if (status == FC41D_STATUS_OK) status = appendU16(buffer, bufferSize, &length, qos);
    if (status == FC41D_STATUS_OK) status = appendText(buffer, bufferSize, &length, ",");
    if (status == FC41D_STATUS_OK) status = appendU16(buffer, bufferSize, &length, retain);
    if (status == FC41D_STATUS_OK) status = appendText(buffer, bufferSize, &length, "\r\n");
    return status;
}

static eFc41dStatus appendText(char *buffer, uint16_t bufferSize, uint16_t *length, const char *text)
{
    if ((buffer == NULL) || (length == NULL) || (text == NULL)) return FC41D_STATUS_INVALID_PARAM;
    while (*text != '\0') {
        if (*length >= (uint16_t)(bufferSize - 1U)) return FC41D_STATUS_OVERFLOW;
        buffer[*length] = *text++;
        *length = (uint16_t)(*length + 1U);
    }
    buffer[*length] = '\0';
    return FC41D_STATUS_OK;
}

static eFc41dStatus appendU16(char *buffer, uint16_t bufferSize, uint16_t *length, uint16_t value)
{
    char digits[5];
    uint16_t index = 0U;

    if ((buffer == NULL) || (length == NULL)) return FC41D_STATUS_INVALID_PARAM;
    do {
        digits[index++] = (char)('0' + (value % 10U));
        value = (uint16_t)(value / 10U);
    } while ((value > 0U) && (index < (uint16_t)sizeof(digits)));
    while (index > 0U) {
        index--;
        if (*length >= (uint16_t)(bufferSize - 1U)) return FC41D_STATUS_OVERFLOW;
        buffer[*length] = digits[index];
        *length = (uint16_t)(*length + 1U);
    }
    buffer[*length] = '\0';
    return FC41D_STATUS_OK;
}

static eFc41dStatus appendQuotedText(char *buffer, uint16_t bufferSize, uint16_t *length, const char *text)
{
    eFc41dStatus status = appendText(buffer, bufferSize, length, "\"");
    while ((status == FC41D_STATUS_OK) && (*text != '\0')) {
        if ((*text == '"') || (*text == '\r') || (*text == '\n')) return FC41D_STATUS_INVALID_PARAM;
        if (*length >= (uint16_t)(bufferSize - 1U)) return FC41D_STATUS_OVERFLOW;
        buffer[*length] = *text++;
        *length = (uint16_t)(*length + 1U);
    }
    if (status == FC41D_STATUS_OK) status = appendText(buffer, bufferSize, length, "\"");
    return status;
}

static bool matchPrefix(const uint8_t *lineBuf, uint16_t lineLen, const char *prefix)
{
    uint16_t prefixLen;
    if ((lineBuf == NULL) || (prefix == NULL)) return false;
    prefixLen = (uint16_t)strlen(prefix);
    return (lineLen >= prefixLen) && (memcmp(lineBuf, prefix, prefixLen) == 0);
}

static bool tryParseUint(const uint8_t *buffer, uint16_t length, uint16_t *value)
{
    uint32_t parsed = 0U;
    uint16_t index;
    if ((buffer == NULL) || (length == 0U) || (value == NULL)) return false;
    for (index = 0U; index < length; index++) {
        if ((buffer[index] < '0') || (buffer[index] > '9')) return false;
        parsed = (parsed * 10U) + (uint32_t)(buffer[index] - '0');
        if (parsed > 65535UL) return false;
    }
    *value = (uint16_t)parsed;
    return true;
}
