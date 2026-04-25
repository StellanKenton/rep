/************************************************************************************
* @file     : esp32c5_ble.c
* @brief    : ESP32-C5 BLE AT helper implementation.
* @details  : Provides BLE command assembly and BLE URC parsers for ESP-AT.
* @author   : GitHub Copilot
* @date     : 2026-04-25
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "esp32c5_ble.h"

#include <string.h>

#define ESP32C5_BLE_MAC_QUERY_PREFIX       "+BLEADDR:"

static eEsp32c5Status esp32c5BleAppendChar(char *buffer, uint16_t bufferSize, uint16_t *length, char value);
static eEsp32c5Status esp32c5BleAppendText(char *buffer, uint16_t bufferSize, uint16_t *length, const char *text);
static eEsp32c5Status esp32c5BleAppendU16(char *buffer, uint16_t bufferSize, uint16_t *length, uint16_t value);
static eEsp32c5Status esp32c5BleAppendHexByte(char *buffer, uint16_t bufferSize, uint16_t *length, uint8_t value);
static bool esp32c5BleMatchPrefix(const uint8_t *lineBuf, uint16_t lineLen, const char *prefix);
static bool esp32c5BleTryParseMacCandidate(const uint8_t *lineBuf, uint16_t lineLen, uint16_t start, char *buffer, uint16_t bufferSize);
static bool esp32c5BleTryHexNibble(uint8_t ch, uint8_t *value);

void esp32c5BleLoadDefCfg(stEsp32c5BleCfg *cfg)
{
    if (cfg == NULL) {
        return;
    }

    (void)memset(cfg, 0, sizeof(*cfg));
    cfg->initMode = 2U;
    cfg->autoStartAdvertising = true;
    cfg->advIntervalMin = ESP32C5_DEFAULT_ADV_INTERVAL_MIN;
    cfg->advIntervalMax = ESP32C5_DEFAULT_ADV_INTERVAL_MAX;
    cfg->rxServiceIndex = 1U;
    cfg->rxCharIndex = 1U;
    cfg->txServiceIndex = 1U;
    cfg->txCharIndex = 2U;
}

bool esp32c5BleIsValidText(const char *text, uint16_t maxLength, bool allowEmpty)
{
    uint16_t length;
    char ch;

    if (text == NULL) {
        return false;
    }

    if (*text == '\0') {
        return allowEmpty;
    }

    length = (uint16_t)strlen(text);
    if ((length == 0U) || (length > maxLength)) {
        return false;
    }

    while (*text != '\0') {
        ch = *text++;
        if ((ch == '"') || (ch == '\r') || (ch == '\n')) {
            return false;
        }
    }

    return true;
}

eEsp32c5Status esp32c5BleBuildAdvParamCommand(const stEsp32c5BleCfg *cfg, char *buffer, uint16_t bufferSize)
{
    uint16_t length;
    eEsp32c5Status status;

    if ((cfg == NULL) || (buffer == NULL) || (bufferSize == 0U)) {
        return ESP32C5_STATUS_INVALID_PARAM;
    }

    length = 0U;
    buffer[0] = '\0';
    status = esp32c5BleAppendText(buffer, bufferSize, &length, "AT+BLEADVPARAM=");
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5BleAppendU16(buffer, bufferSize, &length, cfg->advIntervalMin);
    }
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5BleAppendChar(buffer, bufferSize, &length, ',');
    }
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5BleAppendU16(buffer, bufferSize, &length, cfg->advIntervalMax);
    }
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5BleAppendText(buffer, bufferSize, &length, ",0,0,7\r\n");
    }

    return status;
}

eEsp32c5Status esp32c5BleBuildAdvDataCommand(const stEsp32c5BleCfg *cfg, char *buffer, uint16_t bufferSize)
{
    uint16_t length;
    eEsp32c5Status status;
    const char *name;
    uint16_t nameLen;
    uint16_t maxNameLen;
    uint16_t index;

    if ((cfg == NULL) || (buffer == NULL) || (bufferSize == 0U)) {
        return ESP32C5_STATUS_INVALID_PARAM;
    }

    name = cfg->name;
    nameLen = (uint16_t)strlen(name);
    maxNameLen = 26U;
    if (nameLen > maxNameLen) {
        nameLen = maxNameLen;
    }

    length = 0U;
    buffer[0] = '\0';
    status = esp32c5BleAppendText(buffer, bufferSize, &length, "AT+BLEADVDATA=\"");
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5BleAppendHexByte(buffer, bufferSize, &length, 0x02U);
    }
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5BleAppendHexByte(buffer, bufferSize, &length, 0x01U);
    }
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5BleAppendHexByte(buffer, bufferSize, &length, 0x06U);
    }
    if ((status == ESP32C5_STATUS_OK) && (nameLen > 0U)) {
        status = esp32c5BleAppendHexByte(buffer, bufferSize, &length, (uint8_t)(nameLen + 1U));
        if (status == ESP32C5_STATUS_OK) {
            status = esp32c5BleAppendHexByte(buffer, bufferSize, &length, 0x09U);
        }
        for (index = 0U; (status == ESP32C5_STATUS_OK) && (index < nameLen); index++) {
            status = esp32c5BleAppendHexByte(buffer, bufferSize, &length, (uint8_t)name[index]);
        }
    }
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5BleAppendText(buffer, bufferSize, &length, "\"\r\n");
    }

    return status;
}

uint16_t esp32c5BleGetNotifyPayloadLimit(const stEsp32c5State *state)
{
    uint16_t payloadLimit;

    if (state == NULL) {
        return 20U;
    }

    payloadLimit = 20U;
    if (state->isBleMtuConfigured && (state->bleMtu > 3U)) {
        payloadLimit = (uint16_t)(state->bleMtu - 3U);
        if (payloadLimit > ESP32C5_BLE_NOTIFY_PREFERRED_CHUNK_SIZE) {
            payloadLimit = ESP32C5_BLE_NOTIFY_PREFERRED_CHUNK_SIZE;
        }
    }

    return payloadLimit;
}

bool esp32c5BleIsUrc(const uint8_t *lineBuf, uint16_t lineLen)
{
    return esp32c5BleMatchPrefix(lineBuf, lineLen, "+BLECONN") ||
           esp32c5BleMatchPrefix(lineBuf, lineLen, "+BLEDISCONN") ||
           esp32c5BleMatchPrefix(lineBuf, lineLen, "+BLECFGMTU") ||
           esp32c5BleMatchPrefix(lineBuf, lineLen, "+WRITE") ||
           esp32c5BleMatchPrefix(lineBuf, lineLen, "+READ") ||
           esp32c5BleMatchPrefix(lineBuf, lineLen, "+NOTIFY") ||
           esp32c5BleMatchPrefix(lineBuf, lineLen, "+INDICATE");
}

bool esp32c5BleTryParseConnIndex(const uint8_t *lineBuf, uint16_t lineLen, uint8_t *connIndex)
{
    uint16_t index;
    uint16_t value;

    if ((lineBuf == NULL) || (connIndex == NULL)) {
        return false;
    }

    for (index = 0U; index < lineLen; index++) {
        if ((lineBuf[index] >= '0') && (lineBuf[index] <= '9')) {
            value = 0U;
            while ((index < lineLen) && (lineBuf[index] >= '0') && (lineBuf[index] <= '9')) {
                value = (uint16_t)(value * 10U + (uint16_t)(lineBuf[index] - '0'));
                index++;
            }
            *connIndex = (uint8_t)value;
            return true;
        }
    }

    return false;
}

bool esp32c5BleTryParseMtu(const uint8_t *lineBuf, uint16_t lineLen, uint8_t *connIndex, uint16_t *mtu)
{
    uint16_t index;
    uint16_t value;
    uint8_t parsedConnIndex;

    if ((lineBuf == NULL) || (connIndex == NULL) || (mtu == NULL)) {
        return false;
    }

    parsedConnIndex = 0U;
    *mtu = 0U;
    for (index = 0U; index < lineLen; index++) {
        if ((lineBuf[index] >= '0') && (lineBuf[index] <= '9')) {
            value = 0U;
            while ((index < lineLen) && (lineBuf[index] >= '0') && (lineBuf[index] <= '9')) {
                value = (uint16_t)(value * 10U + (uint16_t)(lineBuf[index] - '0'));
                index++;
            }

            parsedConnIndex = (uint8_t)value;
            while ((index < lineLen) && (lineBuf[index] != ',')) {
                index++;
            }
            if (index >= lineLen) {
                return false;
            }

            index++;
            if ((index >= lineLen) || (lineBuf[index] < '0') || (lineBuf[index] > '9')) {
                return false;
            }

            value = 0U;
            while ((index < lineLen) && (lineBuf[index] >= '0') && (lineBuf[index] <= '9')) {
                value = (uint16_t)(value * 10U + (uint16_t)(lineBuf[index] - '0'));
                index++;
            }

            *connIndex = parsedConnIndex;
            *mtu = value;
            return true;
        }
    }

    return false;
}

bool esp32c5BleTryParseMacAddress(const uint8_t *lineBuf, uint16_t lineLen, char *buffer, uint16_t bufferSize)
{
    uint16_t index;
    const char *prefix;
    uint16_t prefixLen;

    if ((lineBuf == NULL) || (buffer == NULL) || (bufferSize < ESP32C5_MAC_ADDRESS_TEXT_MAX_LENGTH)) {
        return false;
    }

    prefix = ESP32C5_BLE_MAC_QUERY_PREFIX;
    prefixLen = (uint16_t)strlen(prefix);
    for (index = 0U; index < lineLen; index++) {
        if (((uint16_t)(index + prefixLen) <= lineLen) && (memcmp(&lineBuf[index], prefix, prefixLen) == 0)) {
            return esp32c5BleTryParseMacCandidate(lineBuf, lineLen, (uint16_t)(index + prefixLen), buffer, bufferSize);
        }
    }

    return false;
}

static eEsp32c5Status esp32c5BleAppendChar(char *buffer, uint16_t bufferSize, uint16_t *length, char value)
{
    if ((buffer == NULL) || (length == NULL) || (bufferSize == 0U) || (*length >= (uint16_t)(bufferSize - 1U))) {
        return ESP32C5_STATUS_OVERFLOW;
    }

    buffer[*length] = value;
    *length = (uint16_t)(*length + 1U);
    buffer[*length] = '\0';
    return ESP32C5_STATUS_OK;
}

static eEsp32c5Status esp32c5BleAppendText(char *buffer, uint16_t bufferSize, uint16_t *length, const char *text)
{
    if ((buffer == NULL) || (length == NULL) || (text == NULL)) {
        return ESP32C5_STATUS_INVALID_PARAM;
    }

    while (*text != '\0') {
        if (esp32c5BleAppendChar(buffer, bufferSize, length, *text++) != ESP32C5_STATUS_OK) {
            return ESP32C5_STATUS_OVERFLOW;
        }
    }

    return ESP32C5_STATUS_OK;
}

static eEsp32c5Status esp32c5BleAppendU16(char *buffer, uint16_t bufferSize, uint16_t *length, uint16_t value)
{
    char digits[5];
    uint16_t index;
    uint16_t outIndex;

    if ((buffer == NULL) || (length == NULL)) {
        return ESP32C5_STATUS_INVALID_PARAM;
    }

    index = 0U;
    do {
        digits[index++] = (char)('0' + (value % 10U));
        value = (uint16_t)(value / 10U);
    } while ((value > 0U) && (index < (uint16_t)sizeof(digits)));

    while (index > 0U) {
        outIndex = (uint16_t)(index - 1U);
        if (esp32c5BleAppendChar(buffer, bufferSize, length, digits[outIndex]) != ESP32C5_STATUS_OK) {
            return ESP32C5_STATUS_OVERFLOW;
        }
        index--;
    }

    return ESP32C5_STATUS_OK;
}

static eEsp32c5Status esp32c5BleAppendHexByte(char *buffer, uint16_t bufferSize, uint16_t *length, uint8_t value)
{
    static const char hexDigits[] = "0123456789ABCDEF";
    eEsp32c5Status status;

    status = esp32c5BleAppendChar(buffer, bufferSize, length, hexDigits[(value >> 4U) & 0x0FU]);
    if (status != ESP32C5_STATUS_OK) {
        return status;
    }

    return esp32c5BleAppendChar(buffer, bufferSize, length, hexDigits[value & 0x0FU]);
}

static bool esp32c5BleMatchPrefix(const uint8_t *lineBuf, uint16_t lineLen, const char *prefix)
{
    uint16_t prefixLen;

    if ((lineBuf == NULL) || (prefix == NULL)) {
        return false;
    }

    prefixLen = (uint16_t)strlen(prefix);
    return (lineLen >= prefixLen) && (memcmp(lineBuf, prefix, prefixLen) == 0);
}

static bool esp32c5BleTryParseMacCandidate(const uint8_t *lineBuf, uint16_t lineLen, uint16_t start, char *buffer, uint16_t bufferSize)
{
    uint16_t index;
    uint16_t out;
    uint8_t nibble;

    if ((lineBuf == NULL) || (buffer == NULL) || (bufferSize < ESP32C5_MAC_ADDRESS_TEXT_MAX_LENGTH)) {
        return false;
    }

    while ((start < lineLen) && ((lineBuf[start] == ' ') || (lineBuf[start] == '\t') || (lineBuf[start] == '"'))) {
        start++;
    }

    out = 0U;
    for (index = start; index < lineLen; index++) {
        if (out >= (ESP32C5_MAC_ADDRESS_TEXT_MAX_LENGTH - 1U)) {
            break;
        }

        if ((lineBuf[index] == ':') || esp32c5BleTryHexNibble(lineBuf[index], &nibble)) {
            buffer[out++] = (char)lineBuf[index];
            continue;
        }
        break;
    }

    buffer[out] = '\0';
    return out == (ESP32C5_MAC_ADDRESS_TEXT_MAX_LENGTH - 1U);
}

static bool esp32c5BleTryHexNibble(uint8_t ch, uint8_t *value)
{
    if ((ch >= '0') && (ch <= '9')) {
        if (value != NULL) {
            *value = (uint8_t)(ch - '0');
        }
        return true;
    }

    if ((ch >= 'A') && (ch <= 'F')) {
        if (value != NULL) {
            *value = (uint8_t)(ch - 'A' + 10U);
        }
        return true;
    }

    if ((ch >= 'a') && (ch <= 'f')) {
        if (value != NULL) {
            *value = (uint8_t)(ch - 'a' + 10U);
        }
        return true;
    }

    return false;
}

/**************************End of file********************************/
