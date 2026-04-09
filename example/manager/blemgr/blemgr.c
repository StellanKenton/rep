/************************************************************************************
* @file     : blemgr.c
* @brief    : BLE manager service backed by the FC41D module.
* @details  : Brings up the FC41D BLE GATT service and parses packets received from
*             the PC-side BLE test script.
***********************************************************************************/
#include "blemgr.h"

#include <stdio.h>
#include <string.h>

#include "MD5.h"
#include "drv_delay.h"
#include "encryption.h"
#include "fc41d.h"
#include "lib_comm.h"
#include "log.h"

#define BLEMGR_DEVICE                       FC41D_DEV0
#define BLEMGR_NAME_PREFIX                  "PRIMEDIC-CPRSensor-"
#define BLEMGR_SERVICE_UUID                 0xFE60U
#define BLEMGR_WRITE_CHAR_UUID              0xFE61U
#define BLEMGR_NOTIFY_CHAR_UUID             0xFE62U
#define BLEMGR_ADV_INTERVAL_MIN             150U
#define BLEMGR_ADV_INTERVAL_MAX             150U
#define BLEMGR_RESET_DELAY_MS               300U
#define BLEMGR_QUERY_RESP_MAX               96U
#define BLEMGR_CMD_BUF_MAX                  128U
#define BLEMGR_ADV_DATA_MAX                 31U
#define BLEMGR_FRAME_BUF_MAX                (3U + 1U + 2U + BLE_MODULE_MAX_LEN + 2U)
#define BLEMGR_FRAME_HEADER_0               0xFAU
#define BLEMGR_FRAME_HEADER_1               0xFCU
#define BLEMGR_FRAME_HEADER_2               0x01U
#define BLEMGR_HANDSHAKE_CMD                0xF1U
#define BLEMGR_AES_BLOCK_SIZE               16U

static const char *const gBlemgrResetDonePatterns[] = {
    "OK",
    "ready",
    "Ready",
};

static const char *const gBlemgrAtErrorPatterns[] = {
    "ERROR",
    "FAIL",
};

static stBleMgrStatus gBleMgrStatus = {
    .lifecycle = {
        .classType = eMANAGER_SERVICE_CLASS_ACTIVE_SERVICE,
        .state = eMANAGER_LIFECYCLE_STATE_UNINIT,
        .lastError = eMANAGER_LIFECYCLE_ERROR_NONE,
        .initCount = 0U,
        .startCount = 0U,
        .stopCount = 0U,
        .processCount = 0U,
        .recoverCount = 0U,
        .isReady = false,
        .isStarted = false,
        .hasFault = false,
    },
    .state = eBLEMGR_STATE_UNINIT,
    .isModuleReady = false,
    .isConfigured = false,
    .isHandshakeDone = false,
    .rxPacketCount = 0U,
    .rxInvalidPacketCount = 0U,
    .rxDroppedBytes = 0U,
    .lastCmd = 0U,
    .lastPayloadLen = 0U,
    .bleMac = {0U},
    .bleName = {0},
    .bleVersion = {0},
    .lastPayloadPreview = {0U},
};

static uint8_t gBleMgrFrameBuf[BLEMGR_FRAME_BUF_MAX];
static uint16_t gBleMgrFrameLen = 0U;

static void blemgrFault(eManagerLifecycleError error);
static eFc41dStatus blemgrExecText(const char *cmdText, char *respLineBuf, uint16_t respLineBufSize);
static bool blemgrParseMacLine(const char *line, uint8_t mac[6]);
static void blemgrBuildDeviceName(char *nameBuf, uint16_t nameBufSize, const uint8_t mac[6]);
static bool blemgrQueryBleAddress(void);
static void blemgrQueryVersion(void);
static bool blemgrConfigureAdvertising(void);
static bool blemgrConfigureModule(void);
static eFc41dStatus blemgrExecReset(void);
static void blemgrResetRxAssembler(void);
static void blemgrStoreLastPayload(const uint8_t *payload, uint16_t payloadLen);
static void blemgrHandleFrame(const uint8_t *frameBuf, uint16_t frameLen);
static void blemgrConsumeRxBytes(const uint8_t *data, uint16_t length);
static void blemgrDrainBleRx(void);
static bool blemgrApplySessionKey(const uint8_t *seed, uint16_t seedLen);
static bool blemgrHandleHandshake(const uint8_t *payload, uint16_t payloadLen);
static void blemgrHandleDataPacket(uint8_t cmd, const uint8_t *payload, uint16_t payloadLen);

static void blemgrFault(eManagerLifecycleError error)
{
    managerLifecycleReportFault(&gBleMgrStatus.lifecycle, error);
    gBleMgrStatus.state = eBLEMGR_STATE_FAULT;
}

static eFc41dStatus blemgrExecText(const char *cmdText, char *respLineBuf, uint16_t respLineBufSize)
{
    stFc41dAtResp resp;

    (void)memset(&resp, 0, sizeof(resp));
    resp.lineBuf = (uint8_t *)respLineBuf;
    resp.lineBufSize = respLineBufSize;
    return fc41dExecAtText(BLEMGR_DEVICE, cmdText, NULL, (respLineBuf != NULL) ? &resp : NULL);
}

static bool blemgrParseMacLine(const char *line, uint8_t mac[6])
{
    uint8_t hexBytes[12];
    uint8_t hexCount = 0U;
    const char *cursor;
    uint8_t index;

    if ((line == NULL) || (mac == NULL)) {
        return false;
    }

    cursor = line;
    while ((*cursor != '\0') && (hexCount < (uint8_t)sizeof(hexBytes))) {
        char ch = *cursor;
        uint8_t nibble;

        if ((ch >= '0') && (ch <= '9')) {
            nibble = (uint8_t)(ch - '0');
        } else if ((ch >= 'a') && (ch <= 'f')) {
            nibble = (uint8_t)(ch - 'a' + 10);
        } else if ((ch >= 'A') && (ch <= 'F')) {
            nibble = (uint8_t)(ch - 'A' + 10);
        } else {
            cursor++;
            continue;
        }

        hexBytes[hexCount++] = nibble;
        cursor++;
    }

    if (hexCount < 12U) {
        return false;
    }

    for (index = 0U; index < 6U; index++) {
        mac[index] = (uint8_t)((hexBytes[index * 2U] << 4U) | hexBytes[index * 2U + 1U]);
    }

    return true;
}

static void blemgrBuildDeviceName(char *nameBuf, uint16_t nameBufSize, const uint8_t mac[6])
{
    if ((nameBuf == NULL) || (nameBufSize == 0U)) {
        return;
    }

    if (mac == NULL) {
        (void)snprintf(nameBuf, nameBufSize, "%s", BLEMGR_NAME_PREFIX "BOOT");
        return;
    }

    (void)snprintf(nameBuf, nameBufSize, "%s%02X%02X%02X",
                   BLEMGR_NAME_PREFIX,
                   mac[3],
                   mac[4],
                   mac[5]);
}

static bool blemgrQueryBleAddress(void)
{
    char cmdBuf[BLEMGR_CMD_BUF_MAX];
    char respBuf[BLEMGR_QUERY_RESP_MAX];
    eFc41dStatus status;

    if (fc41dAtBuildQueryCmd(cmdBuf, sizeof(cmdBuf), FC41D_AT_CATALOG_CMD_QBLEADDR) != FC41D_STATUS_OK) {
        return false;
    }

    (void)memset(respBuf, 0, sizeof(respBuf));
    status = blemgrExecText(cmdBuf, respBuf, sizeof(respBuf));
    if (status != FC41D_STATUS_OK) {
        return false;
    }

    return blemgrParseMacLine(respBuf, gBleMgrStatus.bleMac);
}

static void blemgrQueryVersion(void)
{
    char respBuf[BLEMGR_QUERY_RESP_MAX];
    eFc41dStatus status;
    const char *cursor;

    (void)memset(respBuf, 0, sizeof(respBuf));
    status = blemgrExecText("AT+QVERSION", respBuf, sizeof(respBuf));
    if (status != FC41D_STATUS_OK) {
        return;
    }

    cursor = strchr(respBuf, ':');
    if (cursor == NULL) {
        cursor = respBuf;
    } else {
        cursor++;
    }

    while (*cursor == ' ') {
        cursor++;
    }

    (void)snprintf(gBleMgrStatus.bleVersion, sizeof(gBleMgrStatus.bleVersion), "%s", cursor);
}

static bool blemgrConfigureAdvertising(void)
{
    char cmdBuf[BLEMGR_CMD_BUF_MAX];
    uint8_t advData[BLEMGR_ADV_DATA_MAX];
    uint16_t nameLen;
    uint16_t advLen;
    eFc41dStatus status;

    nameLen = (uint16_t)strnlen(gBleMgrStatus.bleName, sizeof(gBleMgrStatus.bleName));
    if ((nameLen == 0U) || ((uint32_t)nameLen + 2U > sizeof(advData))) {
        return false;
    }

    advData[0] = (uint8_t)(nameLen + 1U);
    advData[1] = 0x09U;
    (void)memcpy(&advData[2], gBleMgrStatus.bleName, nameLen);
    advLen = (uint16_t)(nameLen + 2U);

    if (fc41dAtBuildBleAdvDataCmd(cmdBuf, sizeof(cmdBuf), advData, advLen) != FC41D_STATUS_OK) {
        return false;
    }

    status = blemgrExecText(cmdBuf, NULL, 0U);
    return status == FC41D_STATUS_OK;
}

static eFc41dStatus blemgrExecReset(void)
{
    stFc41dAtOpt opt = {
        .responseDonePatterns = gBlemgrResetDonePatterns,
        .responseDonePatternCnt = (uint8_t)(sizeof(gBlemgrResetDonePatterns) / sizeof(gBlemgrResetDonePatterns[0])),
        .errorPatterns = gBlemgrAtErrorPatterns,
        .errorPatternCnt = (uint8_t)(sizeof(gBlemgrAtErrorPatterns) / sizeof(gBlemgrAtErrorPatterns[0])),
        .totalToutMs = 5000U,
        .responseToutMs = 1500U,
        .promptToutMs = 1000U,
        .finalToutMs = 5000U,
        .needPrompt = false,
    };

    return fc41dExecAtText(BLEMGR_DEVICE, "AT+QRST", &opt, NULL);
}

static bool blemgrConfigureModule(void)
{
    char cmdBuf[BLEMGR_CMD_BUF_MAX];
    eFc41dStatus status;

    gBleMgrStatus.state = eBLEMGR_STATE_CONFIGURING;

    status = blemgrExecReset();
    if (status != FC41D_STATUS_OK) {
        LOG_E(BLEMGR_TAG, "FC41D reset failed, status=%d", (int)status);
        return false;
    }
    Drv_Delay(BLEMGR_RESET_DELAY_MS);

    status = blemgrExecText("AT+QECHO=0", NULL, 0U);
    if (status != FC41D_STATUS_OK) {
        LOG_W(BLEMGR_TAG, "Disable echo failed, status=%d", (int)status);
    }

    status = blemgrExecText("AT+QURCCFG=1", NULL, 0U);
    if (status != FC41D_STATUS_OK) {
        LOG_W(BLEMGR_TAG, "Enable URC failed, status=%d", (int)status);
    }

    status = blemgrExecText("AT+QSTASTOP", NULL, 0U);
    if (status != FC41D_STATUS_OK) {
        LOG_W(BLEMGR_TAG, "Stop STA mode failed, status=%d", (int)status);
    }

    if (fc41dAtBuildSetCmd(cmdBuf, sizeof(cmdBuf), FC41D_AT_CATALOG_CMD_QBLEINIT, "2") != FC41D_STATUS_OK) {
        return false;
    }
    status = blemgrExecText(cmdBuf, NULL, 0U);
    if (status != FC41D_STATUS_OK) {
        LOG_E(BLEMGR_TAG, "BLE init failed, status=%d", (int)status);
        return false;
    }

    if (!blemgrQueryBleAddress()) {
        LOG_E(BLEMGR_TAG, "Query BLE MAC failed");
        return false;
    }

    blemgrBuildDeviceName(gBleMgrStatus.bleName, sizeof(gBleMgrStatus.bleName), gBleMgrStatus.bleMac);
    if (fc41dAtBuildBleNameCmd(cmdBuf, sizeof(cmdBuf), gBleMgrStatus.bleName) != FC41D_STATUS_OK) {
        return false;
    }
    status = blemgrExecText(cmdBuf, NULL, 0U);
    if (status != FC41D_STATUS_OK) {
        LOG_E(BLEMGR_TAG, "Set BLE name failed, status=%d", (int)status);
        return false;
    }

    if (fc41dAtBuildBleGattServiceCmd(cmdBuf, sizeof(cmdBuf), BLEMGR_SERVICE_UUID) != FC41D_STATUS_OK) {
        return false;
    }
    status = blemgrExecText(cmdBuf, NULL, 0U);
    if (status != FC41D_STATUS_OK) {
        LOG_E(BLEMGR_TAG, "Create BLE service failed, status=%d", (int)status);
        return false;
    }

    if (fc41dAtBuildBleGattCharCmd(cmdBuf, sizeof(cmdBuf), BLEMGR_WRITE_CHAR_UUID) != FC41D_STATUS_OK) {
        return false;
    }
    status = blemgrExecText(cmdBuf, NULL, 0U);
    if (status != FC41D_STATUS_OK) {
        LOG_E(BLEMGR_TAG, "Create write characteristic failed, status=%d", (int)status);
        return false;
    }

    if (fc41dAtBuildBleGattCharCmd(cmdBuf, sizeof(cmdBuf), BLEMGR_NOTIFY_CHAR_UUID) != FC41D_STATUS_OK) {
        return false;
    }
    status = blemgrExecText(cmdBuf, NULL, 0U);
    if (status != FC41D_STATUS_OK) {
        LOG_E(BLEMGR_TAG, "Create notify characteristic failed, status=%d", (int)status);
        return false;
    }

    if (fc41dAtBuildBleAdvParamCmd(cmdBuf, sizeof(cmdBuf), BLEMGR_ADV_INTERVAL_MIN, BLEMGR_ADV_INTERVAL_MAX) != FC41D_STATUS_OK) {
        return false;
    }
    status = blemgrExecText(cmdBuf, NULL, 0U);
    if (status != FC41D_STATUS_OK) {
        LOG_E(BLEMGR_TAG, "Set BLE advertising parameters failed, status=%d", (int)status);
        return false;
    }

    if (!blemgrConfigureAdvertising()) {
        LOG_E(BLEMGR_TAG, "Set BLE advertising payload failed");
        return false;
    }

    blemgrQueryVersion();

    status = blemgrExecText("AT+QBLEADVSTART", NULL, 0U);
    if (status != FC41D_STATUS_OK) {
        LOG_E(BLEMGR_TAG, "Start BLE advertising failed, status=%d", (int)status);
        return false;
    }

    (void)fc41dBleClearRx(BLEMGR_DEVICE);
    gBleMgrStatus.isConfigured = true;
    gBleMgrStatus.isHandshakeDone = false;
    gBleMgrStatus.state = eBLEMGR_STATE_WAIT_HANDSHAKE;
    LOG_I(BLEMGR_TAG, "BLE ready, name=%s", gBleMgrStatus.bleName);
    return true;
}

static void blemgrResetRxAssembler(void)
{
    gBleMgrFrameLen = 0U;
    (void)memset(gBleMgrFrameBuf, 0, sizeof(gBleMgrFrameBuf));
}

static void blemgrStoreLastPayload(const uint8_t *payload, uint16_t payloadLen)
{
    uint16_t copyLen;

    gBleMgrStatus.lastPayloadLen = payloadLen;
    (void)memset(gBleMgrStatus.lastPayloadPreview, 0, sizeof(gBleMgrStatus.lastPayloadPreview));

    if ((payload == NULL) || (payloadLen == 0U)) {
        return;
    }

    copyLen = payloadLen;
    if (copyLen > BLEMGR_LAST_PAYLOAD_PREVIEW_MAX) {
        copyLen = BLEMGR_LAST_PAYLOAD_PREVIEW_MAX;
    }
    (void)memcpy(gBleMgrStatus.lastPayloadPreview, payload, copyLen);
}

static bool blemgrApplySessionKey(const uint8_t *seed, uint16_t seedLen)
{
    uint8_t digest[16];

    if ((seed == NULL) || (seedLen == 0U)) {
        return false;
    }

    MD5_Data((uint8_t *)seed, seedLen, digest);
    (void)memcpy(aes_key, digest, sizeof(digest));
    my_aes_init();
    return true;
}

static bool blemgrHandleHandshake(const uint8_t *payload, uint16_t payloadLen)
{
    uint8_t plain[BLEMGR_AES_BLOCK_SIZE];
    const uint8_t *fullMac = gBleMgrStatus.bleMac;
    const uint8_t *nameTail = &gBleMgrStatus.bleMac[3];

    if ((payload == NULL) || (payloadLen != BLEMGR_AES_BLOCK_SIZE)) {
        return false;
    }

    if (!blemgrApplySessionKey(fullMac, 6U)) {
        return false;
    }

    (void)memset(plain, 0, sizeof(plain));
    my_aes_decrypt((unsigned char *)payload, plain, (unsigned char)payloadLen);
    if (memcmp(plain, fullMac, sizeof(gBleMgrStatus.bleMac)) != 0) {
        if (!blemgrApplySessionKey(nameTail, 3U)) {
            return false;
        }

        (void)memset(plain, 0, sizeof(plain));
        my_aes_decrypt((unsigned char *)payload, plain, (unsigned char)payloadLen);
        if (memcmp(plain, nameTail, 3U) != 0) {
            return false;
        }
    }

    gBleMgrStatus.isHandshakeDone = true;
    gBleMgrStatus.state = eBLEMGR_STATE_RUNNING;
    LOG_I(BLEMGR_TAG, "Handshake accepted");
    return true;
}

static void blemgrHandleDataPacket(uint8_t cmd, const uint8_t *payload, uint16_t payloadLen)
{
    uint8_t plain[BLE_MODULE_MAX_LEN];

    if (payload == NULL) {
        return;
    }

    if ((payloadLen > 0U) && (payloadLen <= sizeof(plain)) && ((payloadLen % BLEMGR_AES_BLOCK_SIZE) == 0U) && gBleMgrStatus.isHandshakeDone) {
        (void)memset(plain, 0, sizeof(plain));
        my_aes_decrypt((unsigned char *)payload, plain, (unsigned char)payloadLen);
        blemgrStoreLastPayload(plain, payloadLen);
    } else {
        blemgrStoreLastPayload(payload, payloadLen);
    }

    gBleMgrStatus.lastCmd = cmd;
    gBleMgrStatus.rxPacketCount++;

    if (cmd == E_CMD_TIME_SYCN) {
        LOG_I(BLEMGR_TAG, "Received time sync packet, payloadLen=%u", (unsigned int)payloadLen);
    } else {
        LOG_I(BLEMGR_TAG, "Received BLE packet, cmd=0x%02X, payloadLen=%u", (unsigned int)cmd, (unsigned int)payloadLen);
    }
}

static void blemgrHandleFrame(const uint8_t *frameBuf, uint16_t frameLen)
{
    uint8_t cmd;
    uint16_t payloadLen;
    uint16_t recvCrc;
    uint16_t calcCrc;
    const uint8_t *payload;

    if ((frameBuf == NULL) || (frameLen < 8U)) {
        return;
    }

    if ((frameBuf[0] != BLEMGR_FRAME_HEADER_0) || (frameBuf[1] != BLEMGR_FRAME_HEADER_1) || (frameBuf[2] != BLEMGR_FRAME_HEADER_2)) {
        gBleMgrStatus.rxInvalidPacketCount++;
        return;
    }

    cmd = frameBuf[3];
    payloadLen = (uint16_t)(((uint16_t)frameBuf[4] << 8U) | frameBuf[5]);
    payload = &frameBuf[6];
    recvCrc = (uint16_t)(((uint16_t)frameBuf[frameLen - 2U] << 8U) | frameBuf[frameLen - 1U]);
    calcCrc = Crc16Compute(&frameBuf[3], (uint16_t)(1U + 2U + payloadLen));

    if (recvCrc != calcCrc) {
        gBleMgrStatus.rxInvalidPacketCount++;
        LOG_W(BLEMGR_TAG, "BLE packet CRC mismatch, cmd=0x%02X", (unsigned int)cmd);
        return;
    }

    if (cmd == BLEMGR_HANDSHAKE_CMD) {
        if (!blemgrHandleHandshake(payload, payloadLen)) {
            gBleMgrStatus.rxInvalidPacketCount++;
            LOG_W(BLEMGR_TAG, "Handshake validation failed");
            return;
        }
        gBleMgrStatus.lastCmd = cmd;
        gBleMgrStatus.rxPacketCount++;
        blemgrStoreLastPayload(payload, payloadLen);
        return;
    }

    if (!gBleMgrStatus.isHandshakeDone) {
        gBleMgrStatus.rxInvalidPacketCount++;
        LOG_W(BLEMGR_TAG, "Ignore cmd=0x%02X before handshake", (unsigned int)cmd);
        return;
    }

    blemgrHandleDataPacket(cmd, payload, payloadLen);
}

static void blemgrConsumeRxBytes(const uint8_t *data, uint16_t length)
{
    uint16_t index = 0U;

    while (index < length) {
        if (gBleMgrFrameLen >= sizeof(gBleMgrFrameBuf)) {
            gBleMgrStatus.rxDroppedBytes += gBleMgrFrameLen;
            blemgrResetRxAssembler();
        }

        gBleMgrFrameBuf[gBleMgrFrameLen++] = data[index++];

        while (gBleMgrFrameLen >= 3U) {
            uint16_t payloadLen;
            uint16_t frameLen;

            if ((gBleMgrFrameBuf[0] != BLEMGR_FRAME_HEADER_0) ||
                (gBleMgrFrameBuf[1] != BLEMGR_FRAME_HEADER_1) ||
                (gBleMgrFrameBuf[2] != BLEMGR_FRAME_HEADER_2)) {
                (void)memmove(gBleMgrFrameBuf, &gBleMgrFrameBuf[1], (size_t)(gBleMgrFrameLen - 1U));
                gBleMgrFrameLen--;
                continue;
            }

            if (gBleMgrFrameLen < 6U) {
                break;
            }

            payloadLen = (uint16_t)(((uint16_t)gBleMgrFrameBuf[4] << 8U) | gBleMgrFrameBuf[5]);
            if (payloadLen > BLE_MODULE_MAX_LEN) {
                gBleMgrStatus.rxInvalidPacketCount++;
                (void)memmove(gBleMgrFrameBuf, &gBleMgrFrameBuf[1], (size_t)(gBleMgrFrameLen - 1U));
                gBleMgrFrameLen--;
                continue;
            }

            frameLen = (uint16_t)(3U + 1U + 2U + payloadLen + 2U);
            if (frameLen > sizeof(gBleMgrFrameBuf)) {
                gBleMgrStatus.rxInvalidPacketCount++;
                blemgrResetRxAssembler();
                break;
            }

            if (gBleMgrFrameLen < frameLen) {
                break;
            }

            blemgrHandleFrame(gBleMgrFrameBuf, frameLen);
            if (gBleMgrFrameLen > frameLen) {
                (void)memmove(gBleMgrFrameBuf, &gBleMgrFrameBuf[frameLen], (size_t)(gBleMgrFrameLen - frameLen));
            }
            gBleMgrFrameLen = (uint16_t)(gBleMgrFrameLen - frameLen);
        }
    }
}

static void blemgrDrainBleRx(void)
{
    uint8_t rxBuf[64];
    uint32_t readLen;

    do {
        readLen = fc41dBleRead(BLEMGR_DEVICE, rxBuf, sizeof(rxBuf));
        if (readLen > 0U) {
            blemgrConsumeRxBytes(rxBuf, (uint16_t)readLen);
        }
    } while (readLen > 0U);
}

bool blemgrInit(void)
{
    eFc41dStatus status;

    if (!managerLifecycleInit(&gBleMgrStatus.lifecycle)) {
        blemgrFault(eMANAGER_LIFECYCLE_ERROR_INTERNAL);
        return false;
    }

    if (gBleMgrStatus.state != eBLEMGR_STATE_UNINIT) {
        return true;
    }

    status = fc41dInit(BLEMGR_DEVICE);
    if (status != FC41D_STATUS_OK) {
        LOG_E(BLEMGR_TAG, "FC41D init failed, status=%d", (int)status);
        blemgrFault(eMANAGER_LIFECYCLE_ERROR_INTERNAL);
        return false;
    }

    blemgrResetRxAssembler();
    gBleMgrStatus.state = eBLEMGR_STATE_READY;
    gBleMgrStatus.isModuleReady = true;
    gBleMgrStatus.isConfigured = false;
    gBleMgrStatus.isHandshakeDone = false;
    gBleMgrStatus.rxPacketCount = 0U;
    gBleMgrStatus.rxInvalidPacketCount = 0U;
    gBleMgrStatus.rxDroppedBytes = 0U;
    gBleMgrStatus.lastCmd = 0U;
    gBleMgrStatus.lastPayloadLen = 0U;
    (void)memset(gBleMgrStatus.lastPayloadPreview, 0, sizeof(gBleMgrStatus.lastPayloadPreview));

    LOG_I(BLEMGR_TAG, "BLE manager initialized");
    return true;
}

bool blemgrStart(void)
{
    if (!blemgrInit()) {
        return false;
    }

    if (!managerLifecycleStart(&gBleMgrStatus.lifecycle)) {
        blemgrFault(eMANAGER_LIFECYCLE_ERROR_INTERNAL);
        return false;
    }

    if (!gBleMgrStatus.isConfigured) {
        if (!blemgrConfigureModule()) {
            blemgrFault(eMANAGER_LIFECYCLE_ERROR_INTERNAL);
            return false;
        }
    }

    if (gBleMgrStatus.isHandshakeDone) {
        gBleMgrStatus.state = eBLEMGR_STATE_RUNNING;
    } else {
        gBleMgrStatus.state = eBLEMGR_STATE_WAIT_HANDSHAKE;
    }

    return true;
}

void blemgrStop(void)
{
    if (!blemgrInit()) {
        return;
    }

    if (!managerLifecycleStop(&gBleMgrStatus.lifecycle)) {
        blemgrFault(eMANAGER_LIFECYCLE_ERROR_INTERNAL);
        return;
    }

    gBleMgrStatus.state = eBLEMGR_STATE_STOPPED;
}

void blemgrProcess(void)
{
    eFc41dStatus status;

    if (!blemgrStart()) {
        return;
    }

    if (!managerLifecycleNoteProcess(&gBleMgrStatus.lifecycle)) {
        if (gBleMgrStatus.lifecycle.hasFault) {
            gBleMgrStatus.state = eBLEMGR_STATE_FAULT;
        }
        return;
    }

    status = fc41dProcess(BLEMGR_DEVICE);
    if ((status != FC41D_STATUS_OK) && (status != FC41D_STATUS_NOT_READY)) {
        LOG_E(BLEMGR_TAG, "FC41D process failed, status=%d", (int)status);
        blemgrFault(eMANAGER_LIFECYCLE_ERROR_INTERNAL);
        return;
    }

    blemgrDrainBleRx();
}

eBleMgrState blemgrGetState(void)
{
    return gBleMgrStatus.state;
}

eManagerLifecycleError blemgrGetLastError(void)
{
    return gBleMgrStatus.lifecycle.lastError;
}

const stBleMgrStatus *blemgrGetStatus(void)
{
    return &gBleMgrStatus;
}
/**************************End of file********************************/