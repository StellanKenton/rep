/************************************************************************************
* @file     : esp32c5_data.c
* @brief    : ESP32-C5 internal data-plane implementation.
* @details  : Keeps RX/TX ring operations and BLE notify payload assembly.
* @author   : GitHub Copilot
* @date     : 2026-04-23
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "esp32c5_data.h"

#include <string.h>

#include "../../service/log/log.h"

#define ESP32C5_DATA_LOG_TAG              "esp32c5Dat"

static void esp32c5DataWriteOverwrite(stEsp32c5DataPlane *dataPlane, const uint8_t *buffer, uint16_t length);
static uint16_t esp32c5DataPeekTx(const stEsp32c5DataPlane *dataPlane, uint8_t *buffer, uint16_t bufferSize);
static void esp32c5DataDropTx(stEsp32c5DataPlane *dataPlane, uint16_t length);
static eEsp32c5Status esp32c5DataAppendChar(char *buffer, uint16_t bufferSize, uint16_t *length, char value);
static eEsp32c5Status esp32c5DataAppendText(char *buffer, uint16_t bufferSize, uint16_t *length, const char *text);
static eEsp32c5Status esp32c5DataAppendUint(char *buffer, uint16_t bufferSize, uint16_t *length, uint16_t value);
static bool esp32c5DataHasToken(const uint8_t *lineBuf, uint16_t lineLen, const char *token);

void esp32c5DataReset(stEsp32c5DataPlane *dataPlane)
{
    if (dataPlane == NULL) {
        return;
    }

    (void)memset(dataPlane, 0, sizeof(*dataPlane));
}

uint16_t esp32c5DataGetRxLength(const stEsp32c5DataPlane *dataPlane)
{
    if (dataPlane == NULL) {
        return 0U;
    }

    return dataPlane->rxUsed;
}

uint16_t esp32c5DataRead(stEsp32c5DataPlane *dataPlane, uint8_t *buffer, uint16_t bufferSize)
{
    uint16_t readLen;
    uint16_t index;

    if ((dataPlane == NULL) || (buffer == NULL) || (bufferSize == 0U)) {
        return 0U;
    }

    readLen = (bufferSize < dataPlane->rxUsed) ? bufferSize : dataPlane->rxUsed;
    for (index = 0U; index < readLen; index++) {
        buffer[index] = dataPlane->rxStorage[dataPlane->rxTail];
        dataPlane->rxTail = (uint16_t)((dataPlane->rxTail + 1U) % (uint16_t)sizeof(dataPlane->rxStorage));
    }

    dataPlane->rxUsed = (uint16_t)(dataPlane->rxUsed - readLen);
    return readLen;
}

void esp32c5DataStoreRx(stEsp32c5DataPlane *dataPlane, const uint8_t *buffer, uint16_t length)
{
    esp32c5DataWriteOverwrite(dataPlane, buffer, length);
}

eEsp32c5Status esp32c5DataWrite(stEsp32c5DataPlane *dataPlane, const uint8_t *buffer, uint16_t length)
{
    uint16_t capacity;
    uint16_t index;

    if ((dataPlane == NULL) || (buffer == NULL) || (length == 0U)) {
        return ESP32C5_STATUS_INVALID_PARAM;
    }

    capacity = (uint16_t)sizeof(dataPlane->txStorage);
    if ((length > capacity) || ((uint16_t)(capacity - dataPlane->txUsed) < length)) {
        LOG_W(ESP32C5_DATA_LOG_TAG,
              "tx overflow len=%u used=%u cap=%u",
              (unsigned int)length,
              (unsigned int)dataPlane->txUsed,
              (unsigned int)capacity);
        return ESP32C5_STATUS_OVERFLOW;
    }

    for (index = 0U; index < length; index++) {
        dataPlane->txStorage[dataPlane->txHead] = buffer[index];
        dataPlane->txHead = (uint16_t)((dataPlane->txHead + 1U) % capacity);
    }

    dataPlane->txUsed = (uint16_t)(dataPlane->txUsed + length);
    return ESP32C5_STATUS_OK;
}

bool esp32c5DataHasPendingTx(const stEsp32c5DataPlane *dataPlane)
{
    return (dataPlane != NULL) && ((dataPlane->txUsed > 0U) || (dataPlane->txPendingLen > 0U));
}

void esp32c5DataClearPendingTx(stEsp32c5DataPlane *dataPlane)
{
    if (dataPlane == NULL) {
        return;
    }

    dataPlane->txPendingLen = 0U;
}

void esp32c5DataConfirmPendingTx(stEsp32c5DataPlane *dataPlane)
{
    if ((dataPlane == NULL) || (dataPlane->txPendingLen == 0U)) {
        return;
    }

    esp32c5DataDropTx(dataPlane, dataPlane->txPendingLen);
    dataPlane->txPendingLen = 0U;
}

eEsp32c5Status esp32c5DataBuildBleNotify(stEsp32c5DataPlane *dataPlane,
                                         uint8_t connIndex,
                                         uint8_t serviceIndex,
                                         uint8_t charIndex,
                                         uint16_t maxPayloadLen,
                                         char *cmdBuf,
                                         uint16_t bufferSize,
                                         const uint8_t **payloadBuf,
                                         uint16_t *payloadLen)
{
    static const char notifyPrefix[] = "AT+BLEGATTSNTFY=";
    uint16_t length;
    eEsp32c5Status status;

    if ((dataPlane == NULL) || (cmdBuf == NULL) || (bufferSize == 0U) || (payloadBuf == NULL) || (payloadLen == NULL)) {
        return ESP32C5_STATUS_INVALID_PARAM;
    }

    if (maxPayloadLen == 0U) {
        return ESP32C5_STATUS_INVALID_PARAM;
    }

    if (maxPayloadLen > ESP32C5_BLE_TX_CHUNK_SIZE) {
        maxPayloadLen = ESP32C5_BLE_TX_CHUNK_SIZE;
    }

    if (dataPlane->txPendingLen == 0U) {
        dataPlane->txPendingLen = esp32c5DataPeekTx(dataPlane, dataPlane->txPendingBuf, maxPayloadLen);
        if (dataPlane->txPendingLen == 0U) {
            return ESP32C5_STATUS_NOT_READY;
        }
    }

    length = 0U;
    cmdBuf[0] = '\0';
    status = esp32c5DataAppendText(cmdBuf, bufferSize, &length, notifyPrefix);
    if (status != ESP32C5_STATUS_OK) {
        return status;
    }

    status = esp32c5DataAppendUint(cmdBuf, bufferSize, &length, connIndex);
    if (status != ESP32C5_STATUS_OK) {
        return status;
    }

    status = esp32c5DataAppendChar(cmdBuf, bufferSize, &length, ',');
    if (status != ESP32C5_STATUS_OK) {
        return status;
    }

    status = esp32c5DataAppendUint(cmdBuf, bufferSize, &length, serviceIndex);
    if (status != ESP32C5_STATUS_OK) {
        return status;
    }

    status = esp32c5DataAppendChar(cmdBuf, bufferSize, &length, ',');
    if (status != ESP32C5_STATUS_OK) {
        return status;
    }

    status = esp32c5DataAppendUint(cmdBuf, bufferSize, &length, charIndex);
    if (status != ESP32C5_STATUS_OK) {
        return status;
    }

    status = esp32c5DataAppendChar(cmdBuf, bufferSize, &length, ',');
    if (status != ESP32C5_STATUS_OK) {
        return status;
    }

    status = esp32c5DataAppendUint(cmdBuf, bufferSize, &length, dataPlane->txPendingLen);
    if (status != ESP32C5_STATUS_OK) {
        return status;
    }

    status = esp32c5DataAppendText(cmdBuf, bufferSize, &length, "\r\n");
    if (status != ESP32C5_STATUS_OK) {
        return status;
    }

    *payloadBuf = dataPlane->txPendingBuf;
    *payloadLen = dataPlane->txPendingLen;
    return ESP32C5_STATUS_OK;
}

bool esp32c5DataTryStoreUrcPayload(stEsp32c5DataPlane *dataPlane, const uint8_t *lineBuf, uint16_t lineLen)
{
    if ((dataPlane == NULL) || (lineBuf == NULL) || (lineLen == 0U)) {
        return false;
    }

    if (!esp32c5DataHasToken(lineBuf, lineLen, "+WRITE") &&
        !esp32c5DataHasToken(lineBuf, lineLen, "+NOTIFY") &&
        !esp32c5DataHasToken(lineBuf, lineLen, "+INDICATE")) {
        return false;
    }

    esp32c5DataStoreRx(dataPlane, lineBuf, lineLen);
    return true;
}

static void esp32c5DataWriteOverwrite(stEsp32c5DataPlane *dataPlane, const uint8_t *buffer, uint16_t length)
{
    uint16_t index;

    if ((dataPlane == NULL) || (buffer == NULL) || (length == 0U)) {
        return;
    }

    if (length >= (uint16_t)sizeof(dataPlane->rxStorage)) {
        LOG_W(ESP32C5_DATA_LOG_TAG,
              "rx payload truncate len=%u cap=%u",
              (unsigned int)length,
              (unsigned int)sizeof(dataPlane->rxStorage));
        buffer = &buffer[length - (uint16_t)sizeof(dataPlane->rxStorage)];
        length = (uint16_t)sizeof(dataPlane->rxStorage);
    }

    for (index = 0U; index < length; index++) {
        if (dataPlane->rxUsed >= (uint16_t)sizeof(dataPlane->rxStorage)) {
            dataPlane->rxTail = (uint16_t)((dataPlane->rxTail + 1U) % (uint16_t)sizeof(dataPlane->rxStorage));
            dataPlane->rxUsed--;
        }

        dataPlane->rxStorage[dataPlane->rxHead] = buffer[index];
        dataPlane->rxHead = (uint16_t)((dataPlane->rxHead + 1U) % (uint16_t)sizeof(dataPlane->rxStorage));
        dataPlane->rxUsed++;
    }
}

static uint16_t esp32c5DataPeekTx(const stEsp32c5DataPlane *dataPlane, uint8_t *buffer, uint16_t bufferSize)
{
    uint16_t readLen;
    uint16_t index;
    uint16_t tail;
    uint16_t capacity;

    if ((dataPlane == NULL) || (buffer == NULL) || (bufferSize == 0U)) {
        return 0U;
    }

    readLen = dataPlane->txUsed;
    if (readLen > bufferSize) {
        readLen = bufferSize;
    }

    capacity = (uint16_t)sizeof(dataPlane->txStorage);
    tail = dataPlane->txTail;
    for (index = 0U; index < readLen; index++) {
        buffer[index] = dataPlane->txStorage[tail];
        tail = (uint16_t)((tail + 1U) % capacity);
    }

    return readLen;
}

static void esp32c5DataDropTx(stEsp32c5DataPlane *dataPlane, uint16_t length)
{
    uint16_t capacity;

    if ((dataPlane == NULL) || (length == 0U)) {
        return;
    }

    if (length > dataPlane->txUsed) {
        length = dataPlane->txUsed;
    }

    capacity = (uint16_t)sizeof(dataPlane->txStorage);
    dataPlane->txTail = (uint16_t)((dataPlane->txTail + length) % capacity);
    dataPlane->txUsed = (uint16_t)(dataPlane->txUsed - length);
}

static eEsp32c5Status esp32c5DataAppendChar(char *buffer, uint16_t bufferSize, uint16_t *length, char value)
{
    if ((buffer == NULL) || (length == NULL) || (bufferSize == 0U) || (*length >= (uint16_t)(bufferSize - 1U))) {
        return ESP32C5_STATUS_OVERFLOW;
    }

    buffer[*length] = value;
    *length = (uint16_t)(*length + 1U);
    buffer[*length] = '\0';
    return ESP32C5_STATUS_OK;
}

static eEsp32c5Status esp32c5DataAppendText(char *buffer, uint16_t bufferSize, uint16_t *length, const char *text)
{
    if ((buffer == NULL) || (length == NULL) || (text == NULL)) {
        return ESP32C5_STATUS_INVALID_PARAM;
    }

    while (*text != '\0') {
        if (esp32c5DataAppendChar(buffer, bufferSize, length, *text++) != ESP32C5_STATUS_OK) {
            return ESP32C5_STATUS_OVERFLOW;
        }
    }

    return ESP32C5_STATUS_OK;
}

static eEsp32c5Status esp32c5DataAppendUint(char *buffer, uint16_t bufferSize, uint16_t *length, uint16_t value)
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
        if (esp32c5DataAppendChar(buffer, bufferSize, length, digits[outIndex]) != ESP32C5_STATUS_OK) {
            return ESP32C5_STATUS_OVERFLOW;
        }
        index--;
    }

    return ESP32C5_STATUS_OK;
}

static bool esp32c5DataHasToken(const uint8_t *lineBuf, uint16_t lineLen, const char *token)
{
    uint16_t tokenLen;
    uint16_t index;
    uint16_t offset;

    if ((lineBuf == NULL) || (token == NULL) || (*token == '\0')) {
        return false;
    }

    tokenLen = (uint16_t)strlen(token);
    if ((tokenLen == 0U) || (lineLen < tokenLen)) {
        return false;
    }

    for (index = 0U; index <= (uint16_t)(lineLen - tokenLen); index++) {
        for (offset = 0U; offset < tokenLen; offset++) {
            if ((char)lineBuf[index + offset] != token[offset]) {
                break;
            }
        }
        if (offset == tokenLen) {
            return true;
        }
    }

    return false;
}

/**************************End of file********************************/
