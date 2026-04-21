/************************************************************************************
* @file     : fc41d_ctrl.c
* @brief    : FC41D internal control-plane implementation.
* @details  : This file owns the startup state machine, transaction submission,
*             URC status mapping, and retry policy.
* @author   : GitHub Copilot
* @date     : 2026-04-21
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "fc41d_ctrl.h"

#include <string.h>

static const char *const gFc41dCommonDonePatterns[] = {"OK"};
static const char *const gFc41dCommonErrorPatterns[] = {"ERROR", "FAIL"};
static const char *const gFc41dDefaultUrcPrefixes[] = {
    "+QBLE",
    "+IPD,",
    "+CWJAP:",
    "+CWSTATE:",
    "+CIPRECVDATA,",
    "WIFI ",
    "ready",
};

static const stFlowParserSpec gFc41dCommonSpec = {
    .responseDonePatterns = gFc41dCommonDonePatterns,
    .responseDonePatternCnt = 1U,
    .finalDonePatterns = NULL,
    .finalDonePatternCnt = 0U,
    .errorPatterns = gFc41dCommonErrorPatterns,
    .errorPatternCnt = 2U,
    .totalToutMs = FC41D_DEFAULT_CMD_TIMEOUT_MS,
    .responseToutMs = FC41D_DEFAULT_CMD_TIMEOUT_MS,
    .promptToutMs = 0U,
    .finalToutMs = 0U,
    .needPrompt = false,
};

static bool fc41dContainsForbiddenChar(const char *text);
static eFc41dStatus fc41dSubmitCtrlText(stFc41dDevice *device, const char *cmdText);
static eFc41dStatus fc41dBuildCtrlText(stFc41dDevice *device, const char *prefix, const char *text);
static eFc41dStatus fc41dBuildCtrlU16(stFc41dDevice *device, const char *prefix, uint16_t value);
static eFc41dStatus fc41dAppendChar(char *buffer, uint16_t bufferSize, uint16_t *length, char value);
static eFc41dStatus fc41dAppendText(char *buffer, uint16_t bufferSize, uint16_t *length, const char *text);
static eFc41dStatus fc41dAppendU16(char *buffer, uint16_t bufferSize, uint16_t *length, uint16_t value);
static eFc41dStatus fc41dHandleCtrlDone(stFc41dDevice *device, eFc41dMapType deviceId, uint32_t nowTickMs);
static eFc41dStatus fc41dProcessCtrlStage(stFc41dDevice *device, eFc41dMapType deviceId, uint32_t nowTickMs);
static bool fc41dMatchPrefix(const uint8_t *lineBuf, uint16_t lineLen, const char *prefix);
static bool fc41dHasToken(const uint8_t *lineBuf, uint16_t lineLen, const char *token);
static bool fc41dTryParseMacAddress(const uint8_t *lineBuf, uint16_t lineLen, char *buffer, uint16_t bufferSize);
static bool fc41dTryHexNibble(uint8_t ch, uint8_t *value);

bool fc41dIsValidRole(eFc41dRole role)
{
    return (role > FC41D_ROLE_NONE) && (role < FC41D_ROLE_MAX);
}

void fc41dLoadDefBleCfg(stFc41dBleCfg *cfg)
{
    if (cfg == NULL) {
        return;
    }

    (void)memset(cfg, 0, sizeof(*cfg));
    cfg->initMode = 2U;
}

bool fc41dIsValidText(const char *text, uint16_t maxLength, bool allowEmpty)
{
    uint16_t lLength;

    if (text == NULL) {
        return false;
    }

    if (*text == '\0') {
        return allowEmpty;
    }

    lLength = (uint16_t)strlen(text);
    if ((lLength == 0U) || (lLength > maxLength)) {
        return false;
    }

    return !fc41dContainsForbiddenChar(text);
}

void fc41dResetState(stFc41dDevice *device)
{
    if (device == NULL) {
        return;
    }

    (void)memset(&device->state, 0, sizeof(device->state));
    device->state.runState = device->info.isInit ? FC41D_RUN_IDLE : FC41D_RUN_UNINIT;
    device->state.lastError = FC41D_STATUS_OK;
    device->ctrlPlane.nextActionTick = 0U;
    device->ctrlPlane.readyDeadlineTick = 0U;
    device->ctrlPlane.stage = FC41D_CTRL_STAGE_IDLE;
    device->ctrlPlane.isTxnDone = false;
    device->ctrlPlane.txnStatus = FC41D_STATUS_OK;
    device->ctrlPlane.cmdBuf[0] = '\0';
    fc41dDataReset(&device->dataPlane);
}

void fc41dSyncInfo(stFc41dDevice *device)
{
    if (device == NULL) {
        return;
    }

    device->info.stage = flowparserStreamGetStage(&device->stream);
    device->info.isBusy = flowparserStreamIsBusy(&device->stream);
}

void fc41dSyncState(stFc41dDevice *device)
{
    if (device == NULL) {
        return;
    }

    fc41dSyncInfo(device);
    device->state.isBusy = device->info.isBusy ||
                           ((device->state.role != FC41D_ROLE_NONE) &&
                            (device->ctrlPlane.stage != FC41D_CTRL_STAGE_IDLE) &&
                            (device->ctrlPlane.stage != FC41D_CTRL_STAGE_RUNNING));
}

const stFc41dTransportInterface *fc41dGetTransport(const stFc41dDevice *device)
{
    if (device == NULL) {
        return NULL;
    }

    return fc41dGetPlatformTransportInterface(&device->cfg);
}

const stFc41dControlInterface *fc41dGetControl(eFc41dMapType device)
{
    return fc41dGetPlatformControlInterface(device);
}

eFc41dStatus fc41dMapDrvStatus(eDrvStatus status)
{
    switch (status) {
        case DRV_STATUS_OK:
            return FC41D_STATUS_OK;
        case DRV_STATUS_INVALID_PARAM:
            return FC41D_STATUS_INVALID_PARAM;
        case DRV_STATUS_NOT_READY:
            return FC41D_STATUS_NOT_READY;
        case DRV_STATUS_BUSY:
            return FC41D_STATUS_BUSY;
        case DRV_STATUS_TIMEOUT:
            return FC41D_STATUS_TIMEOUT;
        default:
            return FC41D_STATUS_ERROR;
    }
}

eFc41dStatus fc41dMapStreamStatus(eFlowParserStrmSta status)
{
    switch (status) {
        case FLOWPARSER_STREAM_OK:
        case FLOWPARSER_STREAM_EMPTY:
            return FC41D_STATUS_OK;
        case FLOWPARSER_STREAM_BUSY:
            return FC41D_STATUS_BUSY;
        case FLOWPARSER_STREAM_INVALID_PARAM:
            return FC41D_STATUS_INVALID_PARAM;
        case FLOWPARSER_STREAM_NOT_INIT:
            return FC41D_STATUS_NOT_READY;
        case FLOWPARSER_STREAM_OVERFLOW:
            return FC41D_STATUS_OVERFLOW;
        case FLOWPARSER_STREAM_TIMEOUT:
            return FC41D_STATUS_TIMEOUT;
        case FLOWPARSER_STREAM_PORT_FAIL:
        case FLOWPARSER_STREAM_ERROR:
        default:
            return FC41D_STATUS_STREAM_FAIL;
    }
}

eFc41dStatus fc41dMapResult(eFlowParserResult result)
{
    switch (result) {
        case FLOWPARSER_RESULT_OK:
            return FC41D_STATUS_OK;
        case FLOWPARSER_RESULT_TIMEOUT:
            return FC41D_STATUS_TIMEOUT;
        case FLOWPARSER_RESULT_OVERFLOW:
            return FC41D_STATUS_OVERFLOW;
        case FLOWPARSER_RESULT_SEND_FAIL:
            return FC41D_STATUS_STREAM_FAIL;
        case FLOWPARSER_RESULT_ERROR:
        default:
            return FC41D_STATUS_ERROR;
    }
}

eFc41dStatus fc41dCtrlStart(stFc41dDevice *device, eFc41dRole role)
{
    if ((device == NULL) || !fc41dIsValidRole(role)) {
        return FC41D_STATUS_INVALID_PARAM;
    }

    fc41dDataReset(&device->dataPlane);
    device->info.hasLastResult = false;
    device->info.lastResult = FLOWPARSER_RESULT_OK;
    device->info.rxBytes = 0U;
    device->info.urcCount = 0U;
    device->state.role = role;
    device->state.runState = FC41D_RUN_BOOTING;
    device->state.isReady = false;
    device->state.isBusy = true;
    device->state.isBleAdvertising = false;
    device->state.isBleConnected = false;
    device->state.isReadyUrcSeen = false;
    device->state.hasMacAddress = false;
    device->state.lastError = FC41D_STATUS_OK;
    device->state.macAddress[0] = '\0';
    device->ctrlPlane.stage = FC41D_CTRL_STAGE_BOOT_WAIT_ARMED;
    device->ctrlPlane.nextActionTick = 0U;
    device->ctrlPlane.readyDeadlineTick = 0U;
    device->ctrlPlane.isTxnDone = false;
    device->ctrlPlane.txnStatus = FC41D_STATUS_OK;
    device->ctrlPlane.cmdBuf[0] = '\0';
    return FC41D_STATUS_OK;
}

void fc41dCtrlStop(stFc41dDevice *device)
{
    fc41dResetState(device);
}

eFc41dStatus fc41dCtrlProcess(stFc41dDevice *device, eFc41dMapType deviceId, uint32_t nowTickMs)
{
    eFc41dStatus lStatus;

    if (device == NULL) {
        return FC41D_STATUS_INVALID_PARAM;
    }

    fc41dSyncState(device);
    if (device->state.role == FC41D_ROLE_NONE) {
        return FC41D_STATUS_OK;
    }

    if (device->ctrlPlane.isTxnDone && !device->info.isBusy) {
        lStatus = fc41dHandleCtrlDone(device, deviceId, nowTickMs);
        if (lStatus != FC41D_STATUS_OK) {
            return lStatus;
        }
    }

    if (device->info.isBusy) {
        return FC41D_STATUS_OK;
    }

    return fc41dProcessCtrlStage(device, deviceId, nowTickMs);
}

void fc41dCtrlScheduleRetry(stFc41dDevice *device, eFc41dMapType deviceId, uint32_t nowTickMs, eFc41dStatus status)
{
    const stFc41dControlInterface *lpControl;
    eFc41dRole lRole;

    if (device == NULL) {
        return;
    }

    lpControl = fc41dGetControl(deviceId);
    if ((lpControl != NULL) && (lpControl->setResetLevel != NULL)) {
        lpControl->setResetLevel(false);
    }

    lRole = device->state.role;
    flowparserStreamReset(&device->stream);
    fc41dDataReset(&device->dataPlane);
    device->state.isReady = false;
    device->state.isBusy = true;
    device->state.isBleAdvertising = false;
    device->state.isBleConnected = false;
    device->state.isReadyUrcSeen = false;
    device->state.hasMacAddress = false;
    device->state.lastError = status;
    device->state.runState = FC41D_RUN_ERROR;
    device->state.macAddress[0] = '\0';
    device->state.role = lRole;
    device->ctrlPlane.stage = FC41D_CTRL_STAGE_BOOT_WAIT_ARMED;
    device->ctrlPlane.nextActionTick = nowTickMs + device->cfg.retryIntervalMs;
    device->ctrlPlane.readyDeadlineTick = 0U;
    device->ctrlPlane.isTxnDone = false;
    device->ctrlPlane.txnStatus = FC41D_STATUS_OK;
    device->ctrlPlane.cmdBuf[0] = '\0';
}

bool fc41dCtrlIsUrc(const stFc41dDevice *device, const uint8_t *lineBuf, uint16_t lineLen)
{
    uint32_t lIndex;

    if ((device == NULL) || (lineBuf == NULL) || (lineLen == 0U)) {
        return false;
    }

    if ((device->urcCb.pfMatcher != NULL) &&
        device->urcCb.pfMatcher(device->urcCb.matcherUserData, lineBuf, lineLen)) {
        return true;
    }

    for (lIndex = 0U; lIndex < (uint32_t)(sizeof(gFc41dDefaultUrcPrefixes) / sizeof(gFc41dDefaultUrcPrefixes[0])); lIndex++) {
        if (fc41dMatchPrefix(lineBuf, lineLen, gFc41dDefaultUrcPrefixes[lIndex])) {
            return true;
        }
    }

    if (fc41dHasToken(lineBuf, lineLen, "BLE") &&
        (fc41dHasToken(lineBuf, lineLen, "WRITE") || fc41dHasToken(lineBuf, lineLen, "CONNECT") ||
         fc41dHasToken(lineBuf, lineLen, "DISCONNECT"))) {
        return true;
    }

    return false;
}

void fc41dCtrlHandleUrc(stFc41dDevice *device, const uint8_t *lineBuf, uint16_t lineLen)
{
    if ((device == NULL) || (lineBuf == NULL) || (lineLen == 0U)) {
        return;
    }

    device->info.urcCount++;
    if ((lineLen == 5U) && (memcmp(lineBuf, "ready", 5U) == 0)) {
        device->state.isReadyUrcSeen = true;
    }

    if (fc41dHasToken(lineBuf, lineLen, "BLE") && fc41dHasToken(lineBuf, lineLen, "CONNECT")) {
        device->state.isBleConnected = true;
    }

    if (fc41dHasToken(lineBuf, lineLen, "BLE") && fc41dHasToken(lineBuf, lineLen, "DISCONNECT")) {
        device->state.isBleConnected = false;
    }
}

void fc41dCtrlHandleTxnLine(stFc41dDevice *device, const uint8_t *lineBuf, uint16_t lineLen)
{
    if ((device == NULL) || (lineBuf == NULL) || (lineLen == 0U)) {
        return;
    }

    if ((device->ctrlPlane.stage == FC41D_CTRL_STAGE_QUERY_MAC) &&
        fc41dTryParseMacAddress(lineBuf, lineLen, device->state.macAddress, (uint16_t)sizeof(device->state.macAddress))) {
        device->state.hasMacAddress = true;
    }
}

void fc41dCtrlHandleTxnDone(stFc41dDevice *device, eFlowParserResult result)
{
    if (device == NULL) {
        return;
    }

    device->info.hasLastResult = true;
    device->info.lastResult = result;
    device->ctrlPlane.isTxnDone = true;
    device->ctrlPlane.txnStatus = fc41dMapResult(result);
}

static bool fc41dContainsForbiddenChar(const char *text)
{
    char lCh;

    while ((text != NULL) && (*text != '\0')) {
        lCh = *text++;
        if ((lCh == '"') || (lCh == '\r') || (lCh == '\n')) {
            return true;
        }
    }

    return false;
}

static eFc41dStatus fc41dSubmitCtrlText(stFc41dDevice *device, const char *cmdText)
{
    stFlowParserReq lReq;
    eFlowParserStrmSta lStreamStatus;

    if ((device == NULL) || (cmdText == NULL)) {
        return FC41D_STATUS_INVALID_PARAM;
    }

    lReq.spec = &gFc41dCommonSpec;
    lReq.cmdBuf = (const uint8_t *)cmdText;
    lReq.cmdLen = (uint16_t)strlen(cmdText);
    lReq.payloadBuf = NULL;
    lReq.payloadLen = 0U;
    lReq.lineHandler = fc41dTxnLineThunk;
    lReq.doneHandler = fc41dTxnDoneThunk;
    lReq.userData = device;

    lStreamStatus = flowparserStreamSubmit(&device->stream, &lReq);
    return fc41dMapStreamStatus(lStreamStatus);
}

static eFc41dStatus fc41dBuildCtrlText(stFc41dDevice *device, const char *prefix, const char *text)
{
    uint16_t lLength;
    eFc41dStatus lStatus;

    if ((device == NULL) || (prefix == NULL) || (text == NULL)) {
        return FC41D_STATUS_INVALID_PARAM;
    }

    lLength = 0U;
    device->ctrlPlane.cmdBuf[0] = '\0';
    lStatus = fc41dAppendText(device->ctrlPlane.cmdBuf, sizeof(device->ctrlPlane.cmdBuf), &lLength, prefix);
    if (lStatus != FC41D_STATUS_OK) {
        return lStatus;
    }

    lStatus = fc41dAppendText(device->ctrlPlane.cmdBuf, sizeof(device->ctrlPlane.cmdBuf), &lLength, text);
    if (lStatus != FC41D_STATUS_OK) {
        return lStatus;
    }

    lStatus = fc41dAppendText(device->ctrlPlane.cmdBuf, sizeof(device->ctrlPlane.cmdBuf), &lLength, "\r\n");
    if (lStatus != FC41D_STATUS_OK) {
        return lStatus;
    }

    return fc41dSubmitCtrlText(device, device->ctrlPlane.cmdBuf);
}

static eFc41dStatus fc41dBuildCtrlU16(stFc41dDevice *device, const char *prefix, uint16_t value)
{
    uint16_t lLength;
    eFc41dStatus lStatus;

    if ((device == NULL) || (prefix == NULL)) {
        return FC41D_STATUS_INVALID_PARAM;
    }

    lLength = 0U;
    device->ctrlPlane.cmdBuf[0] = '\0';
    lStatus = fc41dAppendText(device->ctrlPlane.cmdBuf, sizeof(device->ctrlPlane.cmdBuf), &lLength, prefix);
    if (lStatus != FC41D_STATUS_OK) {
        return lStatus;
    }

    lStatus = fc41dAppendU16(device->ctrlPlane.cmdBuf, sizeof(device->ctrlPlane.cmdBuf), &lLength, value);
    if (lStatus != FC41D_STATUS_OK) {
        return lStatus;
    }

    lStatus = fc41dAppendText(device->ctrlPlane.cmdBuf, sizeof(device->ctrlPlane.cmdBuf), &lLength, "\r\n");
    if (lStatus != FC41D_STATUS_OK) {
        return lStatus;
    }

    return fc41dSubmitCtrlText(device, device->ctrlPlane.cmdBuf);
}

static eFc41dStatus fc41dAppendChar(char *buffer, uint16_t bufferSize, uint16_t *length, char value)
{
    if ((buffer == NULL) || (length == NULL) || (bufferSize == 0U) || (*length >= (uint16_t)(bufferSize - 1U))) {
        return FC41D_STATUS_OVERFLOW;
    }

    buffer[*length] = value;
    *length = (uint16_t)(*length + 1U);
    buffer[*length] = '\0';
    return FC41D_STATUS_OK;
}

static eFc41dStatus fc41dAppendText(char *buffer, uint16_t bufferSize, uint16_t *length, const char *text)
{
    if ((buffer == NULL) || (length == NULL) || (text == NULL)) {
        return FC41D_STATUS_INVALID_PARAM;
    }

    while (*text != '\0') {
        if (fc41dAppendChar(buffer, bufferSize, length, *text++) != FC41D_STATUS_OK) {
            return FC41D_STATUS_OVERFLOW;
        }
    }

    return FC41D_STATUS_OK;
}

static eFc41dStatus fc41dAppendU16(char *buffer, uint16_t bufferSize, uint16_t *length, uint16_t value)
{
    char lDigits[5];
    uint16_t lIndex;
    uint16_t lOutIndex;

    if ((buffer == NULL) || (length == NULL)) {
        return FC41D_STATUS_INVALID_PARAM;
    }

    lIndex = 0U;
    do {
        lDigits[lIndex++] = (char)('0' + (value % 10U));
        value = (uint16_t)(value / 10U);
    } while ((value > 0U) && (lIndex < (uint16_t)sizeof(lDigits)));

    while (lIndex > 0U) {
        lOutIndex = (uint16_t)(lIndex - 1U);
        if (fc41dAppendChar(buffer, bufferSize, length, lDigits[lOutIndex]) != FC41D_STATUS_OK) {
            return FC41D_STATUS_OVERFLOW;
        }
        lIndex--;
    }

    return FC41D_STATUS_OK;
}

static eFc41dStatus fc41dHandleCtrlDone(stFc41dDevice *device, eFc41dMapType deviceId, uint32_t nowTickMs)
{
    eFc41dStatus lStatus;

    if (device == NULL) {
        return FC41D_STATUS_INVALID_PARAM;
    }

    lStatus = device->ctrlPlane.txnStatus;
    device->ctrlPlane.isTxnDone = false;
    device->ctrlPlane.txnStatus = FC41D_STATUS_OK;

    if (lStatus != FC41D_STATUS_OK) {
        if (device->ctrlPlane.stage == FC41D_CTRL_STAGE_QUERY_MAC) {
            device->ctrlPlane.stage = FC41D_CTRL_STAGE_RUNNING;
            device->state.runState = FC41D_RUN_READY;
            device->state.isReady = true;
            return FC41D_STATUS_OK;
        }

        fc41dCtrlScheduleRetry(device, deviceId, nowTickMs, lStatus);
        return lStatus;
    }

    switch (device->ctrlPlane.stage) {
        case FC41D_CTRL_STAGE_PROBE:
            device->ctrlPlane.stage = FC41D_CTRL_STAGE_ASSERT_RESET;
            break;
        case FC41D_CTRL_STAGE_PROBE_AFTER_READY:
            device->ctrlPlane.stage = FC41D_CTRL_STAGE_STOP_STA;
            break;
        case FC41D_CTRL_STAGE_STOP_STA:
            device->ctrlPlane.stage = FC41D_CTRL_STAGE_BLE_INIT;
            break;
        case FC41D_CTRL_STAGE_BLE_INIT:
            device->ctrlPlane.stage = FC41D_CTRL_STAGE_BLE_SET_NAME;
            break;
        case FC41D_CTRL_STAGE_BLE_SET_NAME:
            device->ctrlPlane.stage = FC41D_CTRL_STAGE_BLE_SET_SERVICE;
            break;
        case FC41D_CTRL_STAGE_BLE_SET_SERVICE:
            device->ctrlPlane.stage = FC41D_CTRL_STAGE_BLE_SET_CHAR_RX;
            break;
        case FC41D_CTRL_STAGE_BLE_SET_CHAR_RX:
            device->ctrlPlane.stage = FC41D_CTRL_STAGE_BLE_SET_CHAR_TX;
            break;
        case FC41D_CTRL_STAGE_BLE_SET_CHAR_TX:
            device->ctrlPlane.stage = FC41D_CTRL_STAGE_BLE_ADV_START;
            break;
        case FC41D_CTRL_STAGE_BLE_ADV_START:
            device->state.isBleAdvertising = true;
            device->ctrlPlane.stage = FC41D_CTRL_STAGE_QUERY_MAC;
            break;
        case FC41D_CTRL_STAGE_QUERY_MAC:
            device->ctrlPlane.stage = FC41D_CTRL_STAGE_RUNNING;
            device->state.runState = FC41D_RUN_READY;
            device->state.isReady = true;
            break;
        case FC41D_CTRL_STAGE_RUNNING:
            fc41dDataConfirmPendingTx(&device->dataPlane);
            break;
        default:
            break;
    }

    return FC41D_STATUS_OK;
}

static eFc41dStatus fc41dProcessCtrlStage(stFc41dDevice *device, eFc41dMapType deviceId, uint32_t nowTickMs)
{
    const stFc41dControlInterface *lpControl;
    eFc41dStatus lStatus;

    if (device == NULL) {
        return FC41D_STATUS_INVALID_PARAM;
    }

    switch (device->ctrlPlane.stage) {
        case FC41D_CTRL_STAGE_IDLE:
            return FC41D_STATUS_OK;
        case FC41D_CTRL_STAGE_BOOT_WAIT_ARMED:
            if ((device->ctrlPlane.nextActionTick != 0U) && (nowTickMs < device->ctrlPlane.nextActionTick)) {
                return FC41D_STATUS_OK;
            }

            device->state.runState = FC41D_RUN_BOOTING;
            device->ctrlPlane.nextActionTick = nowTickMs + device->cfg.bootWaitMs;
            device->ctrlPlane.stage = FC41D_CTRL_STAGE_BOOT_WAIT;
            return FC41D_STATUS_OK;
        case FC41D_CTRL_STAGE_BOOT_WAIT:
            if (nowTickMs < device->ctrlPlane.nextActionTick) {
                return FC41D_STATUS_OK;
            }

            device->ctrlPlane.stage = FC41D_CTRL_STAGE_PROBE;
            return FC41D_STATUS_OK;
        case FC41D_CTRL_STAGE_PROBE:
            return fc41dSubmitCtrlText(device, "AT\r\n");
        case FC41D_CTRL_STAGE_ASSERT_RESET:
            lpControl = fc41dGetControl(deviceId);
            if ((lpControl == NULL) || (lpControl->setResetLevel == NULL)) {
                fc41dCtrlScheduleRetry(device, deviceId, nowTickMs, FC41D_STATUS_UNSUPPORTED);
                return FC41D_STATUS_UNSUPPORTED;
            }

            lpControl->setResetLevel(true);
            device->ctrlPlane.nextActionTick = nowTickMs + device->cfg.resetPulseMs;
            device->ctrlPlane.stage = FC41D_CTRL_STAGE_RESET_HOLD;
            return FC41D_STATUS_OK;
        case FC41D_CTRL_STAGE_RESET_HOLD:
            if (nowTickMs < device->ctrlPlane.nextActionTick) {
                return FC41D_STATUS_OK;
            }

            lpControl = fc41dGetControl(deviceId);
            if ((lpControl == NULL) || (lpControl->setResetLevel == NULL)) {
                fc41dCtrlScheduleRetry(device, deviceId, nowTickMs, FC41D_STATUS_UNSUPPORTED);
                return FC41D_STATUS_UNSUPPORTED;
            }

            lpControl->setResetLevel(false);
            device->state.isReadyUrcSeen = false;
            device->ctrlPlane.nextActionTick = nowTickMs + device->cfg.resetWaitMs;
            device->ctrlPlane.readyDeadlineTick = nowTickMs + device->cfg.readyTimeoutMs;
            device->ctrlPlane.stage = FC41D_CTRL_STAGE_WAIT_READY;
            return FC41D_STATUS_OK;
        case FC41D_CTRL_STAGE_WAIT_READY:
            if (nowTickMs < device->ctrlPlane.nextActionTick) {
                return FC41D_STATUS_OK;
            }

            if (device->state.isReadyUrcSeen) {
                device->state.runState = FC41D_RUN_CONFIGURING;
                device->ctrlPlane.nextActionTick = nowTickMs + device->cfg.readySettleMs;
                device->ctrlPlane.stage = FC41D_CTRL_STAGE_READY_SETTLE;
                return FC41D_STATUS_OK;
            }

            if (nowTickMs >= device->ctrlPlane.readyDeadlineTick) {
                fc41dCtrlScheduleRetry(device, deviceId, nowTickMs, FC41D_STATUS_TIMEOUT);
                return FC41D_STATUS_TIMEOUT;
            }

            device->ctrlPlane.nextActionTick = nowTickMs + 5U;
            return FC41D_STATUS_OK;
        case FC41D_CTRL_STAGE_READY_SETTLE:
            if (nowTickMs < device->ctrlPlane.nextActionTick) {
                return FC41D_STATUS_OK;
            }

            device->ctrlPlane.stage = FC41D_CTRL_STAGE_PROBE_AFTER_READY;
            return FC41D_STATUS_OK;
        case FC41D_CTRL_STAGE_PROBE_AFTER_READY:
            return fc41dSubmitCtrlText(device, "AT\r\n");
        case FC41D_CTRL_STAGE_STOP_STA:
            return fc41dSubmitCtrlText(device, "AT+QSTASTOP\r\n");
        case FC41D_CTRL_STAGE_BLE_INIT:
            return fc41dBuildCtrlU16(device, "AT+QBLEINIT=", device->bleCfg.initMode);
        case FC41D_CTRL_STAGE_BLE_SET_NAME:
            if (device->bleCfg.name[0] == '\0') {
                device->ctrlPlane.stage = FC41D_CTRL_STAGE_BLE_SET_SERVICE;
                return FC41D_STATUS_OK;
            }

            return fc41dBuildCtrlText(device, "AT+QBLENAME=", device->bleCfg.name);
        case FC41D_CTRL_STAGE_BLE_SET_SERVICE:
            if (device->bleCfg.serviceUuid[0] == '\0') {
                device->ctrlPlane.stage = FC41D_CTRL_STAGE_BLE_SET_CHAR_RX;
                return FC41D_STATUS_OK;
            }

            return fc41dBuildCtrlText(device, "AT+QBLEGATTSSRV=", device->bleCfg.serviceUuid);
        case FC41D_CTRL_STAGE_BLE_SET_CHAR_RX:
            if (device->bleCfg.rxCharUuid[0] == '\0') {
                device->ctrlPlane.stage = FC41D_CTRL_STAGE_BLE_SET_CHAR_TX;
                return FC41D_STATUS_OK;
            }

            return fc41dBuildCtrlText(device, "AT+QBLEGATTSCHAR=", device->bleCfg.rxCharUuid);
        case FC41D_CTRL_STAGE_BLE_SET_CHAR_TX:
            if (device->bleCfg.txCharUuid[0] == '\0') {
                device->ctrlPlane.stage = FC41D_CTRL_STAGE_BLE_ADV_START;
                return FC41D_STATUS_OK;
            }

            return fc41dBuildCtrlText(device, "AT+QBLEGATTSCHAR=", device->bleCfg.txCharUuid);
        case FC41D_CTRL_STAGE_BLE_ADV_START:
            return fc41dSubmitCtrlText(device, "AT+QBLEADVSTART\r\n");
        case FC41D_CTRL_STAGE_QUERY_MAC:
            return fc41dSubmitCtrlText(device, "AT+QBLEADDR?\r\n");
        case FC41D_CTRL_STAGE_RUNNING:
            device->state.runState = FC41D_RUN_READY;
            device->state.isReady = true;
            if (device->state.isBleConnected && fc41dDataHasPendingTx(&device->dataPlane) && (device->bleCfg.txCharUuid[0] != '\0')) {
                lStatus = fc41dDataBuildBleNotify(&device->dataPlane,
                                                  device->bleCfg.txCharUuid,
                                                  device->ctrlPlane.cmdBuf,
                                                  (uint16_t)sizeof(device->ctrlPlane.cmdBuf));
                if (lStatus == FC41D_STATUS_NOT_READY) {
                    return FC41D_STATUS_OK;
                }
                if (lStatus != FC41D_STATUS_OK) {
                    return lStatus;
                }
                return fc41dSubmitCtrlText(device, device->ctrlPlane.cmdBuf);
            }
            return FC41D_STATUS_OK;
        default:
            return FC41D_STATUS_ERROR;
    }
}

static bool fc41dMatchPrefix(const uint8_t *lineBuf, uint16_t lineLen, const char *prefix)
{
    uint16_t lPrefixLen;

    if ((lineBuf == NULL) || (prefix == NULL)) {
        return false;
    }

    lPrefixLen = (uint16_t)strlen(prefix);
    return (lineLen >= lPrefixLen) && (memcmp(lineBuf, prefix, lPrefixLen) == 0);
}

static bool fc41dHasToken(const uint8_t *lineBuf, uint16_t lineLen, const char *token)
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

static bool fc41dTryParseMacAddress(const uint8_t *lineBuf, uint16_t lineLen, char *buffer, uint16_t bufferSize)
{
    uint16_t lStart;
    uint16_t lEnd;
    uint16_t lOutLen;
    uint16_t lPrefixLen;
    uint8_t lNibble;

    if ((lineBuf == NULL) || (lineLen == 0U) || (buffer == NULL) || (bufferSize < 13U)) {
        return false;
    }

    lPrefixLen = (uint16_t)strlen(FC41D_MAC_QUERY_PREFIX);
    lStart = 0U;
    if ((lineLen > lPrefixLen) && (memcmp(lineBuf, FC41D_MAC_QUERY_PREFIX, lPrefixLen) == 0)) {
        lStart = lPrefixLen;
    }

    while ((lStart < lineLen) && ((lineBuf[lStart] == ' ') || (lineBuf[lStart] == '\t') || (lineBuf[lStart] == '"'))) {
        lStart++;
    }

    lEnd = lineLen;
    while (lEnd > lStart) {
        uint8_t lCh = lineBuf[lEnd - 1U];

        if ((lCh != ' ') && (lCh != '\t') && (lCh != '"')) {
            break;
        }

        lEnd--;
    }

    lOutLen = 0U;
    while (lStart < lEnd) {
        uint8_t lCh = lineBuf[lStart++];

        if ((lCh >= 'a') && (lCh <= 'f')) {
            lCh = (uint8_t)(lCh - ('a' - 'A'));
        }

        if ((lCh == '-') || (lCh == ':')) {
            lCh = ':';
        } else if (!fc41dTryHexNibble(lCh, &lNibble)) {
            return false;
        }

        if (lOutLen >= (uint16_t)(bufferSize - 1U)) {
            return false;
        }

        buffer[lOutLen++] = (char)lCh;
    }

    buffer[lOutLen] = '\0';
    return (lOutLen == 12U) || (lOutLen == 17U);
}

static bool fc41dTryHexNibble(uint8_t ch, uint8_t *value)
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
