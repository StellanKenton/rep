/************************************************************************************
* @file     : fc41d_data.c
* @brief    : FC41D internal data-plane implementation.
* @details  : This file keeps RX/TX ring operations and payload extraction for
*             BLE data notifications and write URCs.
* @author   : GitHub Copilot
* @date     : 2026-04-21
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "fc41d_data.h"

#include <string.h>

#include "../../service/log/log.h"

#define FC41D_DATA_LOG_TAG              "fc41dData"

static void fc41dDataWriteOverwrite(stFc41dDataPlane *dataPlane, const uint8_t *buffer, uint16_t length);
static uint16_t fc41dDataPeekTx(const stFc41dDataPlane *dataPlane, uint8_t *buffer, uint16_t bufferSize);
static void fc41dDataDropTx(stFc41dDataPlane *dataPlane, uint16_t length);
static eFc41dStatus fc41dDataAppendChar(char *buffer, uint16_t bufferSize, uint16_t *length, char value);
static eFc41dStatus fc41dDataAppendText(char *buffer, uint16_t bufferSize, uint16_t *length, const char *text);
static eFc41dStatus fc41dDataAppendHexBytes(char *buffer, uint16_t bufferSize, uint16_t *length, const uint8_t *data, uint16_t dataLen);
static bool fc41dDataHasToken(const uint8_t *lineBuf, uint16_t lineLen, const char *token);
static bool fc41dDataTryWriteHexPayload(stFc41dDataPlane *dataPlane, const uint8_t *payload, uint16_t payloadLen);
static bool fc41dDataTryHexNibble(uint8_t ch, uint8_t *value);

void fc41dDataReset(stFc41dDataPlane *dataPlane)
{
    if (dataPlane == NULL) {
        return;
    }

    (void)memset(dataPlane, 0, sizeof(*dataPlane));
}

uint16_t fc41dDataGetRxLength(const stFc41dDataPlane *dataPlane)
{
    if (dataPlane == NULL) {
        return 0U;
    }

    return dataPlane->rxUsed;
}

uint16_t fc41dDataRead(stFc41dDataPlane *dataPlane, uint8_t *buffer, uint16_t bufferSize)
{
    uint16_t lReadLen;
    uint16_t lIndex;

    if ((dataPlane == NULL) || (buffer == NULL) || (bufferSize == 0U)) {
        return 0U;
    }

    lReadLen = (bufferSize < dataPlane->rxUsed) ? bufferSize : dataPlane->rxUsed;
    for (lIndex = 0U; lIndex < lReadLen; lIndex++) {
        buffer[lIndex] = dataPlane->rxStorage[dataPlane->rxTail];
        dataPlane->rxTail = (uint16_t)((dataPlane->rxTail + 1U) % (uint16_t)sizeof(dataPlane->rxStorage));
    }

    dataPlane->rxUsed = (uint16_t)(dataPlane->rxUsed - lReadLen);
    return lReadLen;
}

eFc41dStatus fc41dDataWrite(stFc41dDataPlane *dataPlane, const uint8_t *buffer, uint16_t length)
{
    uint16_t lCapacity;
    uint16_t lIndex;

    if ((dataPlane == NULL) || (buffer == NULL) || (length == 0U)) {
        return FC41D_STATUS_INVALID_PARAM;
    }

    lCapacity = (uint16_t)sizeof(dataPlane->txStorage);
    if ((length > lCapacity) || ((uint16_t)(lCapacity - dataPlane->txUsed) < length)) {
        LOG_W(FC41D_DATA_LOG_TAG,
              "tx overflow len=%u used=%u cap=%u",
              (unsigned int)length,
              (unsigned int)dataPlane->txUsed,
              (unsigned int)lCapacity);
        return FC41D_STATUS_OVERFLOW;
    }

    for (lIndex = 0U; lIndex < length; lIndex++) {
        dataPlane->txStorage[dataPlane->txHead] = buffer[lIndex];
        dataPlane->txHead = (uint16_t)((dataPlane->txHead + 1U) % lCapacity);
    }

    dataPlane->txUsed = (uint16_t)(dataPlane->txUsed + length);
    return FC41D_STATUS_OK;
}

bool fc41dDataHasPendingTx(const stFc41dDataPlane *dataPlane)
{
    return (dataPlane != NULL) && ((dataPlane->txUsed > 0U) || (dataPlane->txPendingLen > 0U));
}

void fc41dDataClearPendingTx(stFc41dDataPlane *dataPlane)
{
    if (dataPlane == NULL) {
        return;
    }

    dataPlane->txPendingLen = 0U;
}

void fc41dDataConfirmPendingTx(stFc41dDataPlane *dataPlane)
{
    if ((dataPlane == NULL) || (dataPlane->txPendingLen == 0U)) {
        return;
    }

    fc41dDataDropTx(dataPlane, dataPlane->txPendingLen);
    dataPlane->txPendingLen = 0U;
}

eFc41dStatus fc41dDataBuildBleNotify(stFc41dDataPlane *dataPlane, const char *charUuid, char *cmdBuf, uint16_t bufferSize)
{
    static const char lNotifyPrefix[] = "AT+QBLEGATTSNTFY=";
    uint16_t lLength;
    eFc41dStatus lStatus;

    if ((dataPlane == NULL) || (charUuid == NULL) || (charUuid[0] == '\0') || (cmdBuf == NULL) || (bufferSize == 0U)) {
        return FC41D_STATUS_INVALID_PARAM;
    }

    if (dataPlane->txPendingLen == 0U) {
        dataPlane->txPendingLen = fc41dDataPeekTx(dataPlane, dataPlane->txPendingBuf, FC41D_BLE_TX_CHUNK_SIZE);
        if (dataPlane->txPendingLen == 0U) {
            return FC41D_STATUS_NOT_READY;
        }
    }

    lLength = 0U;
    cmdBuf[0] = '\0';
    lStatus = fc41dDataAppendText(cmdBuf, bufferSize, &lLength, lNotifyPrefix);
    if (lStatus != FC41D_STATUS_OK) {
        return lStatus;
    }

    lStatus = fc41dDataAppendText(cmdBuf, bufferSize, &lLength, charUuid);
    if (lStatus != FC41D_STATUS_OK) {
        return lStatus;
    }

    lStatus = fc41dDataAppendChar(cmdBuf, bufferSize, &lLength, ',');
    if (lStatus != FC41D_STATUS_OK) {
        return lStatus;
    }

    lStatus = fc41dDataAppendHexBytes(cmdBuf, bufferSize, &lLength, dataPlane->txPendingBuf, dataPlane->txPendingLen);
    if (lStatus != FC41D_STATUS_OK) {
        return lStatus;
    }

    return fc41dDataAppendText(cmdBuf, bufferSize, &lLength, "\r\n");
}

bool fc41dDataTryStoreUrcPayload(stFc41dDataPlane *dataPlane, const uint8_t *lineBuf, uint16_t lineLen)
{
    uint16_t lStart;
    uint16_t lEnd;
    uint16_t lIndex;

    if ((dataPlane == NULL) || (lineBuf == NULL) || (lineLen == 0U)) {
        return false;
    }

    if (!fc41dDataHasToken(lineBuf, lineLen, "BLE") && !fc41dDataHasToken(lineBuf, lineLen, "WRITE")) {
        return false;
    }

    lStart = lineLen;
    lEnd = lineLen;
    for (lIndex = lineLen; lIndex > 0U; lIndex--) {
        if (lineBuf[lIndex - 1U] == '"') {
            if (lEnd == lineLen) {
                lEnd = (uint16_t)(lIndex - 1U);
            } else {
                lStart = lIndex;
                break;
            }
        }
    }

    if ((lStart < lEnd) && (lEnd <= lineLen)) {
        if (fc41dDataTryWriteHexPayload(dataPlane, &lineBuf[lStart], (uint16_t)(lEnd - lStart))) {
            return true;
        }

        fc41dDataWriteOverwrite(dataPlane, &lineBuf[lStart], (uint16_t)(lEnd - lStart));
        return true;
    }

    lStart = lineLen;
    for (lIndex = lineLen; lIndex > 0U; lIndex--) {
        if ((lineBuf[lIndex - 1U] == ':') || (lineBuf[lIndex - 1U] == ',')) {
            lStart = lIndex;
            break;
        }
    }

    if (lStart >= lineLen) {
        return false;
    }

    while ((lStart < lineLen) && ((lineBuf[lStart] == ' ') || (lineBuf[lStart] == '\t'))) {
        lStart++;
    }

    lEnd = lineLen;
    while ((lEnd > lStart) && ((lineBuf[lEnd - 1U] == ' ') || (lineBuf[lEnd - 1U] == '\t'))) {
        lEnd--;
    }

    if (lEnd <= lStart) {
        return false;
    }

    return fc41dDataTryWriteHexPayload(dataPlane, &lineBuf[lStart], (uint16_t)(lEnd - lStart));
}

static void fc41dDataWriteOverwrite(stFc41dDataPlane *dataPlane, const uint8_t *buffer, uint16_t length)
{
    uint16_t lIndex;

    if ((dataPlane == NULL) || (buffer == NULL) || (length == 0U)) {
        return;
    }

    if (length >= (uint16_t)sizeof(dataPlane->rxStorage)) {
        LOG_W(FC41D_DATA_LOG_TAG,
              "rx payload truncate len=%u cap=%u",
              (unsigned int)length,
              (unsigned int)sizeof(dataPlane->rxStorage));
        buffer = &buffer[length - (uint16_t)sizeof(dataPlane->rxStorage)];
        length = (uint16_t)sizeof(dataPlane->rxStorage);
    }

    for (lIndex = 0U; lIndex < length; lIndex++) {
        if (dataPlane->rxUsed >= (uint16_t)sizeof(dataPlane->rxStorage)) {
            if (lIndex == 0U) {
                LOG_W(FC41D_DATA_LOG_TAG,
                      "rx overwrite used=%u cap=%u",
                      (unsigned int)dataPlane->rxUsed,
                      (unsigned int)sizeof(dataPlane->rxStorage));
            }
            dataPlane->rxTail = (uint16_t)((dataPlane->rxTail + 1U) % (uint16_t)sizeof(dataPlane->rxStorage));
            dataPlane->rxUsed--;
        }

        dataPlane->rxStorage[dataPlane->rxHead] = buffer[lIndex];
        dataPlane->rxHead = (uint16_t)((dataPlane->rxHead + 1U) % (uint16_t)sizeof(dataPlane->rxStorage));
        dataPlane->rxUsed++;
    }
}

static uint16_t fc41dDataPeekTx(const stFc41dDataPlane *dataPlane, uint8_t *buffer, uint16_t bufferSize)
{
    uint16_t lReadLen;
    uint16_t lIndex;
    uint16_t lTail;
    uint16_t lCapacity;

    if ((dataPlane == NULL) || (buffer == NULL) || (bufferSize == 0U)) {
        return 0U;
    }

    lReadLen = dataPlane->txUsed;
    if (lReadLen > bufferSize) {
        lReadLen = bufferSize;
    }

    lCapacity = (uint16_t)sizeof(dataPlane->txStorage);
    lTail = dataPlane->txTail;
    for (lIndex = 0U; lIndex < lReadLen; lIndex++) {
        buffer[lIndex] = dataPlane->txStorage[lTail];
        lTail = (uint16_t)((lTail + 1U) % lCapacity);
    }

    return lReadLen;
}

static void fc41dDataDropTx(stFc41dDataPlane *dataPlane, uint16_t length)
{
    uint16_t lCapacity;

    if ((dataPlane == NULL) || (length == 0U)) {
        return;
    }

    if (length > dataPlane->txUsed) {
        length = dataPlane->txUsed;
    }

    lCapacity = (uint16_t)sizeof(dataPlane->txStorage);
    dataPlane->txTail = (uint16_t)((dataPlane->txTail + length) % lCapacity);
    dataPlane->txUsed = (uint16_t)(dataPlane->txUsed - length);
}

static eFc41dStatus fc41dDataAppendChar(char *buffer, uint16_t bufferSize, uint16_t *length, char value)
{
    if ((buffer == NULL) || (length == NULL) || (bufferSize == 0U) || (*length >= (uint16_t)(bufferSize - 1U))) {
        return FC41D_STATUS_OVERFLOW;
    }

    buffer[*length] = value;
    *length = (uint16_t)(*length + 1U);
    buffer[*length] = '\0';
    return FC41D_STATUS_OK;
}

static eFc41dStatus fc41dDataAppendText(char *buffer, uint16_t bufferSize, uint16_t *length, const char *text)
{
    if ((buffer == NULL) || (length == NULL) || (text == NULL)) {
        return FC41D_STATUS_INVALID_PARAM;
    }

    while (*text != '\0') {
        if (fc41dDataAppendChar(buffer, bufferSize, length, *text++) != FC41D_STATUS_OK) {
            return FC41D_STATUS_OVERFLOW;
        }
    }

    return FC41D_STATUS_OK;
}

static eFc41dStatus fc41dDataAppendHexBytes(char *buffer, uint16_t bufferSize, uint16_t *length, const uint8_t *data, uint16_t dataLen)
{
    static const char lHexDigits[] = "0123456789ABCDEF";
    uint16_t lIndex;
    eFc41dStatus lStatus;

    if ((buffer == NULL) || (length == NULL) || ((data == NULL) && (dataLen > 0U))) {
        return FC41D_STATUS_INVALID_PARAM;
    }

    for (lIndex = 0U; lIndex < dataLen; lIndex++) {
        lStatus = fc41dDataAppendChar(buffer, bufferSize, length, lHexDigits[(data[lIndex] >> 4U) & 0x0FU]);
        if (lStatus != FC41D_STATUS_OK) {
            return lStatus;
        }

        lStatus = fc41dDataAppendChar(buffer, bufferSize, length, lHexDigits[data[lIndex] & 0x0FU]);
        if (lStatus != FC41D_STATUS_OK) {
            return lStatus;
        }
    }

    return FC41D_STATUS_OK;
}

static bool fc41dDataHasToken(const uint8_t *lineBuf, uint16_t lineLen, const char *token)
{
    uint16_t lIndex;
    uint16_t lTokenLen;

    if ((lineBuf == NULL) || (token == NULL)) {
        return false;
    }

    lTokenLen = (uint16_t)strlen(token);
    if ((lTokenLen == 0U) || (lineLen < lTokenLen)) {
        return false;
    }

    for (lIndex = 0U; lIndex <= (uint16_t)(lineLen - lTokenLen); lIndex++) {
        if (memcmp(&lineBuf[lIndex], token, lTokenLen) == 0) {
            return true;
        }
    }

    return false;
}

static bool fc41dDataTryWriteHexPayload(stFc41dDataPlane *dataPlane, const uint8_t *payload, uint16_t payloadLen)
{
    uint16_t lIndex;
    uint8_t lHigh;
    uint8_t lLow;
    uint8_t lByte;

    if ((dataPlane == NULL) || (payload == NULL) || (payloadLen == 0U) || ((payloadLen & 0x01U) != 0U)) {
        return false;
    }

    for (lIndex = 0U; lIndex < payloadLen; lIndex += 2U) {
        if (!fc41dDataTryHexNibble(payload[lIndex], &lHigh) || !fc41dDataTryHexNibble(payload[lIndex + 1U], &lLow)) {
            return false;
        }
    }

    for (lIndex = 0U; lIndex < payloadLen; lIndex += 2U) {
        (void)fc41dDataTryHexNibble(payload[lIndex], &lHigh);
        (void)fc41dDataTryHexNibble(payload[lIndex + 1U], &lLow);
        lByte = (uint8_t)((lHigh << 4U) | lLow);
        fc41dDataWriteOverwrite(dataPlane, &lByte, 1U);
    }

    return true;
}

static bool fc41dDataTryHexNibble(uint8_t ch, uint8_t *value)
{
    if (value == NULL) {
        return false;
    }

    if ((ch >= '0') && (ch <= '9')) {
        *value = (uint8_t)(ch - '0');
        return true;
    }

    if ((ch >= 'a') && (ch <= 'f')) {
        *value = (uint8_t)(10U + ch - 'a');
        return true;
    }

    if ((ch >= 'A') && (ch <= 'F')) {
        *value = (uint8_t)(10U + ch - 'A');
        return true;
    }

    return false;
}
/**************************End of file********************************/
