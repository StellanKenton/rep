/************************************************************************************
* @file     : ec800m_mqtt.c
* @brief    : EC800M MQTT AT helper implementation.
* @details  : Provides Quectel MQTT command assembly helpers.
* @author   : GitHub Copilot
* @date     : 2026-04-27
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "ec800m_mqtt.h"

static eEc800mStatus ec800mMqttAppendText(char *buffer, uint16_t bufferSize, uint16_t *length, const char *text);
static eEc800mStatus ec800mMqttAppendU16(char *buffer, uint16_t bufferSize, uint16_t *length, uint16_t value);
static eEc800mStatus ec800mMqttAppendQuotedText(char *buffer, uint16_t bufferSize, uint16_t *length, const char *text, bool allowEmpty);

eEc800mStatus ec800mMqttBuildOpenCommand(uint8_t clientIndex, const char *host, uint16_t port, char *buffer, uint16_t bufferSize)
{
    uint16_t length = 0U;
    eEc800mStatus status;

    if ((clientIndex > 5U) || (host == NULL) || (host[0] == '\0') || (port == 0U) || (buffer == NULL)) {
        return EC800M_STATUS_INVALID_PARAM;
    }

    buffer[0] = '\0';
    status = ec800mMqttAppendText(buffer, bufferSize, &length, "AT+QMTOPEN=");
    if (status == EC800M_STATUS_OK) {
        status = ec800mMqttAppendU16(buffer, bufferSize, &length, clientIndex);
    }
    if (status == EC800M_STATUS_OK) {
        status = ec800mMqttAppendText(buffer, bufferSize, &length, ",");
    }
    if (status == EC800M_STATUS_OK) {
        status = ec800mMqttAppendQuotedText(buffer, bufferSize, &length, host, false);
    }
    if (status == EC800M_STATUS_OK) {
        status = ec800mMqttAppendText(buffer, bufferSize, &length, ",");
    }
    if (status == EC800M_STATUS_OK) {
        status = ec800mMqttAppendU16(buffer, bufferSize, &length, port);
    }
    if (status == EC800M_STATUS_OK) {
        status = ec800mMqttAppendText(buffer, bufferSize, &length, "\r\n");
    }
    return status;
}

eEc800mStatus ec800mMqttBuildConnectCommand(uint8_t clientIndex, const char *clientId, const char *username, const char *password, char *buffer, uint16_t bufferSize)
{
    uint16_t length = 0U;
    eEc800mStatus status;

    if ((clientIndex > 5U) || (clientId == NULL) || (clientId[0] == '\0') || (buffer == NULL)) {
        return EC800M_STATUS_INVALID_PARAM;
    }

    buffer[0] = '\0';
    status = ec800mMqttAppendText(buffer, bufferSize, &length, "AT+QMTCONN=");
    if (status == EC800M_STATUS_OK) {
        status = ec800mMqttAppendU16(buffer, bufferSize, &length, clientIndex);
    }
    if (status == EC800M_STATUS_OK) {
        status = ec800mMqttAppendText(buffer, bufferSize, &length, ",");
    }
    if (status == EC800M_STATUS_OK) {
        status = ec800mMqttAppendQuotedText(buffer, bufferSize, &length, clientId, false);
    }
    if ((status == EC800M_STATUS_OK) && (username != NULL)) {
        status = ec800mMqttAppendText(buffer, bufferSize, &length, ",");
    }
    if ((status == EC800M_STATUS_OK) && (username != NULL)) {
        status = ec800mMqttAppendQuotedText(buffer, bufferSize, &length, username, true);
    }
    if ((status == EC800M_STATUS_OK) && (password != NULL)) {
        status = ec800mMqttAppendText(buffer, bufferSize, &length, ",");
    }
    if ((status == EC800M_STATUS_OK) && (password != NULL)) {
        status = ec800mMqttAppendQuotedText(buffer, bufferSize, &length, password, true);
    }
    if (status == EC800M_STATUS_OK) {
        status = ec800mMqttAppendText(buffer, bufferSize, &length, "\r\n");
    }
    return status;
}

eEc800mStatus ec800mMqttBuildSubscribeCommand(uint8_t clientIndex, uint16_t msgId, const char *topic, uint8_t qos, char *buffer, uint16_t bufferSize)
{
    uint16_t length = 0U;
    eEc800mStatus status;

    if ((clientIndex > 5U) || (msgId == 0U) || (topic == NULL) || (topic[0] == '\0') || (qos > 2U) || (buffer == NULL)) {
        return EC800M_STATUS_INVALID_PARAM;
    }

    buffer[0] = '\0';
    status = ec800mMqttAppendText(buffer, bufferSize, &length, "AT+QMTSUB=");
    if (status == EC800M_STATUS_OK) {
        status = ec800mMqttAppendU16(buffer, bufferSize, &length, clientIndex);
    }
    if (status == EC800M_STATUS_OK) {
        status = ec800mMqttAppendText(buffer, bufferSize, &length, ",");
    }
    if (status == EC800M_STATUS_OK) {
        status = ec800mMqttAppendU16(buffer, bufferSize, &length, msgId);
    }
    if (status == EC800M_STATUS_OK) {
        status = ec800mMqttAppendText(buffer, bufferSize, &length, ",");
    }
    if (status == EC800M_STATUS_OK) {
        status = ec800mMqttAppendQuotedText(buffer, bufferSize, &length, topic, false);
    }
    if (status == EC800M_STATUS_OK) {
        status = ec800mMqttAppendText(buffer, bufferSize, &length, ",");
    }
    if (status == EC800M_STATUS_OK) {
        status = ec800mMqttAppendU16(buffer, bufferSize, &length, qos);
    }
    if (status == EC800M_STATUS_OK) {
        status = ec800mMqttAppendText(buffer, bufferSize, &length, "\r\n");
    }
    return status;
}

eEc800mStatus ec800mMqttBuildPublishExCommand(uint8_t clientIndex, uint16_t msgId, uint8_t qos, bool retain, const char *topic, uint16_t payloadLen, char *buffer, uint16_t bufferSize)
{
    uint16_t length = 0U;
    eEc800mStatus status;

    if ((clientIndex > 5U) || (msgId == 0U) || (qos > 2U) || (topic == NULL) || (topic[0] == '\0') ||
        (payloadLen == 0U) || (buffer == NULL)) {
        return EC800M_STATUS_INVALID_PARAM;
    }

    buffer[0] = '\0';
    status = ec800mMqttAppendText(buffer, bufferSize, &length, "AT+QMTPUBEX=");
    if (status == EC800M_STATUS_OK) {
        status = ec800mMqttAppendU16(buffer, bufferSize, &length, clientIndex);
    }
    if (status == EC800M_STATUS_OK) {
        status = ec800mMqttAppendText(buffer, bufferSize, &length, ",");
    }
    if (status == EC800M_STATUS_OK) {
        status = ec800mMqttAppendU16(buffer, bufferSize, &length, msgId);
    }
    if (status == EC800M_STATUS_OK) {
        status = ec800mMqttAppendText(buffer, bufferSize, &length, ",");
    }
    if (status == EC800M_STATUS_OK) {
        status = ec800mMqttAppendU16(buffer, bufferSize, &length, qos);
    }
    if (status == EC800M_STATUS_OK) {
        status = ec800mMqttAppendText(buffer, bufferSize, &length, retain ? ",1," : ",0,");
    }
    if (status == EC800M_STATUS_OK) {
        status = ec800mMqttAppendQuotedText(buffer, bufferSize, &length, topic, false);
    }
    if (status == EC800M_STATUS_OK) {
        status = ec800mMqttAppendText(buffer, bufferSize, &length, ",");
    }
    if (status == EC800M_STATUS_OK) {
        status = ec800mMqttAppendU16(buffer, bufferSize, &length, payloadLen);
    }
    if (status == EC800M_STATUS_OK) {
        status = ec800mMqttAppendText(buffer, bufferSize, &length, "\r\n");
    }
    return status;
}

eEc800mStatus ec800mMqttBuildCloseCommand(uint8_t clientIndex, char *buffer, uint16_t bufferSize)
{
    uint16_t length = 0U;
    eEc800mStatus status;

    if ((clientIndex > 5U) || (buffer == NULL)) {
        return EC800M_STATUS_INVALID_PARAM;
    }

    buffer[0] = '\0';
    status = ec800mMqttAppendText(buffer, bufferSize, &length, "AT+QMTCLOSE=");
    if (status == EC800M_STATUS_OK) {
        status = ec800mMqttAppendU16(buffer, bufferSize, &length, clientIndex);
    }
    if (status == EC800M_STATUS_OK) {
        status = ec800mMqttAppendText(buffer, bufferSize, &length, "\r\n");
    }
    return status;
}

static eEc800mStatus ec800mMqttAppendText(char *buffer, uint16_t bufferSize, uint16_t *length, const char *text)
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

static eEc800mStatus ec800mMqttAppendU16(char *buffer, uint16_t bufferSize, uint16_t *length, uint16_t value)
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

static eEc800mStatus ec800mMqttAppendQuotedText(char *buffer, uint16_t bufferSize, uint16_t *length, const char *text, bool allowEmpty)
{
    eEc800mStatus status;

    if (text == NULL) {
        return EC800M_STATUS_INVALID_PARAM;
    }
    if ((*text == '\0') && !allowEmpty) {
        return EC800M_STATUS_INVALID_PARAM;
    }

    status = ec800mMqttAppendText(buffer, bufferSize, length, "\"");
    while ((status == EC800M_STATUS_OK) && (*text != '\0')) {
        if ((*text == '"') || (*text == '\r') || (*text == '\n')) {
            return EC800M_STATUS_INVALID_PARAM;
        }
        if (*length >= (uint16_t)(bufferSize - 1U)) {
            return EC800M_STATUS_OVERFLOW;
        }
        buffer[*length] = *text;
        *length = (uint16_t)(*length + 1U);
        text++;
    }
    if (status == EC800M_STATUS_OK) {
        status = ec800mMqttAppendText(buffer, bufferSize, length, "\"");
    }
    return status;
}

/**************************End of file********************************/
