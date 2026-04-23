/************************************************************************************
* @file     : esp32c5_ctrl.c
* @brief    : ESP32-C5 internal control-plane implementation.
* @details  : Owns the BLE startup state machine, transaction submission,
*             URC state mapping, and retry policy.
* @author   : GitHub Copilot
* @date     : 2026-04-23
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "esp32c5_ctrl.h"

#include <string.h>

#include "../../service/log/log.h"

#define ESP32C5_CTRL_LOG_TAG                 "esp32c5Ctl"
#define ESP32C5_DEFAULT_ADV_TYPE             0U
#define ESP32C5_DEFAULT_OWN_ADDR_TYPE        0U
#define ESP32C5_DEFAULT_CHANNEL_MAP          7U

static const char *const gEsp32c5CommonDonePatterns[] = {"OK"};
static const char *const gEsp32c5CommonErrorPatterns[] = {"ERROR", "FAIL"};
static const char *const gEsp32c5DefaultUrcPrefixes[] = {
    "+BLECONN",
    "+BLEDISCONN",
    "+WRITE",
    "+READ",
    "+NOTIFY",
    "+INDICATE",
    "ready",
};

static const stFlowParserSpec gEsp32c5CommonSpec = {
    .responseDonePatterns = gEsp32c5CommonDonePatterns,
    .responseDonePatternCnt = 1U,
    .finalDonePatterns = NULL,
    .finalDonePatternCnt = 0U,
    .errorPatterns = gEsp32c5CommonErrorPatterns,
    .errorPatternCnt = 2U,
    .totalToutMs = ESP32C5_DEFAULT_CMD_TIMEOUT_MS,
    .responseToutMs = ESP32C5_DEFAULT_CMD_TIMEOUT_MS,
    .promptToutMs = 0U,
    .finalToutMs = 0U,
    .needPrompt = false,
};

static const stFlowParserSpec gEsp32c5PromptSpec = {
    .responseDonePatterns = NULL,
    .responseDonePatternCnt = 0U,
    .finalDonePatterns = gEsp32c5CommonDonePatterns,
    .finalDonePatternCnt = 1U,
    .errorPatterns = gEsp32c5CommonErrorPatterns,
    .errorPatternCnt = 2U,
    .totalToutMs = ESP32C5_DEFAULT_FINAL_TIMEOUT_MS,
    .responseToutMs = 0U,
    .promptToutMs = ESP32C5_DEFAULT_PROMPT_TIMEOUT_MS,
    .finalToutMs = ESP32C5_DEFAULT_FINAL_TIMEOUT_MS,
    .needPrompt = true,
};

static bool esp32c5ContainsForbiddenChar(const char *text);
static eEsp32c5Status esp32c5SubmitCtrlText(stEsp32c5Device *device, const char *cmdText);
static eEsp32c5Status esp32c5SubmitCtrlPrompt(stEsp32c5Device *device, const char *cmdText, const uint8_t *payloadBuf, uint16_t payloadLen);
static eEsp32c5Status esp32c5BuildCtrlU16(stEsp32c5Device *device, const char *prefix, uint16_t value);
static eEsp32c5Status esp32c5BuildCtrlQuotedText(stEsp32c5Device *device, const char *prefix, const char *text);
static eEsp32c5Status esp32c5BuildCtrlAdvParam(stEsp32c5Device *device);
static eEsp32c5Status esp32c5BuildCtrlAdvData(stEsp32c5Device *device);
static eEsp32c5Status esp32c5AppendChar(char *buffer, uint16_t bufferSize, uint16_t *length, char value);
static eEsp32c5Status esp32c5AppendText(char *buffer, uint16_t bufferSize, uint16_t *length, const char *text);
static eEsp32c5Status esp32c5AppendU16(char *buffer, uint16_t bufferSize, uint16_t *length, uint16_t value);
static eEsp32c5Status esp32c5AppendQuotedText(char *buffer, uint16_t bufferSize, uint16_t *length, const char *text);
static eEsp32c5Status esp32c5AppendHexByte(char *buffer, uint16_t bufferSize, uint16_t *length, uint8_t value);
static eEsp32c5Status esp32c5HandleCtrlDone(stEsp32c5Device *device, eEsp32c5MapType deviceId, uint32_t nowTickMs);
static eEsp32c5Status esp32c5ProcessCtrlStage(stEsp32c5Device *device, eEsp32c5MapType deviceId, uint32_t nowTickMs);
static bool esp32c5MatchPrefix(const uint8_t *lineBuf, uint16_t lineLen, const char *prefix);
static bool esp32c5TryParseConnIndex(const uint8_t *lineBuf, uint16_t lineLen, uint8_t *connIndex);
static bool esp32c5TryParseMacAddress(const uint8_t *lineBuf, uint16_t lineLen, char *buffer, uint16_t bufferSize);
static bool esp32c5TryParseMacCandidate(const uint8_t *lineBuf, uint16_t lineLen, uint16_t start, char *buffer, uint16_t bufferSize);
static bool esp32c5TryHexNibble(uint8_t ch, uint8_t *value);
static const char *esp32c5CtrlGetStageName(eEsp32c5CtrlStage stage);
static const char *esp32c5CtrlGetRoleName(eEsp32c5Role role);
static void esp32c5CtrlSetStage(stEsp32c5Device *device, eEsp32c5CtrlStage stage);

bool esp32c5IsValidRole(eEsp32c5Role role)
{
    return (role > ESP32C5_ROLE_NONE) && (role < ESP32C5_ROLE_MAX);
}

void esp32c5LoadDefBleCfg(stEsp32c5BleCfg *cfg)
{
    if (cfg == NULL) {
        return;
    }

    (void)memset(cfg, 0, sizeof(*cfg));
    cfg->initMode = 2U;
    cfg->advIntervalMin = ESP32C5_DEFAULT_ADV_INTERVAL_MIN;
    cfg->advIntervalMax = ESP32C5_DEFAULT_ADV_INTERVAL_MAX;
    cfg->rxServiceIndex = 1U;
    cfg->rxCharIndex = 1U;
    cfg->txServiceIndex = 1U;
    cfg->txCharIndex = 2U;
}

bool esp32c5IsValidText(const char *text, uint16_t maxLength, bool allowEmpty)
{
    uint16_t length;

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

    return !esp32c5ContainsForbiddenChar(text);
}

void esp32c5ResetState(stEsp32c5Device *device)
{
    if (device == NULL) {
        return;
    }

    (void)memset(&device->state, 0, sizeof(device->state));
    device->state.runState = device->info.isInit ? ESP32C5_RUN_IDLE : ESP32C5_RUN_UNINIT;
    device->state.lastError = ESP32C5_STATUS_OK;
    device->ctrlPlane.nextActionTick = 0U;
    device->ctrlPlane.readyDeadlineTick = 0U;
    device->ctrlPlane.stage = ESP32C5_CTRL_STAGE_IDLE;
    device->ctrlPlane.txnKind = ESP32C5_CTRL_TXN_NONE;
    device->ctrlPlane.isTxnDone = false;
    device->ctrlPlane.txnStatus = ESP32C5_STATUS_OK;
    device->ctrlPlane.cmdBuf[0] = '\0';
    esp32c5DataReset(&device->dataPlane);
}

void esp32c5SyncInfo(stEsp32c5Device *device)
{
    if (device == NULL) {
        return;
    }

    device->info.stage = flowparserStreamGetStage(&device->stream);
    device->info.isBusy = flowparserStreamIsBusy(&device->stream);
}

void esp32c5SyncState(stEsp32c5Device *device)
{
    if (device == NULL) {
        return;
    }

    esp32c5SyncInfo(device);
    device->state.isBusy = device->info.isBusy ||
                           ((device->state.role != ESP32C5_ROLE_NONE) &&
                            (device->ctrlPlane.stage != ESP32C5_CTRL_STAGE_IDLE) &&
                            (device->ctrlPlane.stage != ESP32C5_CTRL_STAGE_RUNNING));
}

const stEsp32c5TransportInterface *esp32c5GetTransport(const stEsp32c5Device *device)
{
    if (device == NULL) {
        return NULL;
    }

    return esp32c5GetPlatformTransportInterface(&device->cfg);
}

const stEsp32c5ControlInterface *esp32c5GetControl(eEsp32c5MapType device)
{
    return esp32c5GetPlatformControlInterface(device);
}

eEsp32c5Status esp32c5MapDrvStatus(eDrvStatus status)
{
    switch (status) {
        case DRV_STATUS_OK:
            return ESP32C5_STATUS_OK;
        case DRV_STATUS_INVALID_PARAM:
            return ESP32C5_STATUS_INVALID_PARAM;
        case DRV_STATUS_NOT_READY:
            return ESP32C5_STATUS_NOT_READY;
        case DRV_STATUS_BUSY:
            return ESP32C5_STATUS_BUSY;
        case DRV_STATUS_TIMEOUT:
            return ESP32C5_STATUS_TIMEOUT;
        default:
            return ESP32C5_STATUS_ERROR;
    }
}

eEsp32c5Status esp32c5MapStreamStatus(eFlowParserStrmSta status)
{
    switch (status) {
        case FLOWPARSER_STREAM_OK:
        case FLOWPARSER_STREAM_EMPTY:
            return ESP32C5_STATUS_OK;
        case FLOWPARSER_STREAM_BUSY:
            return ESP32C5_STATUS_BUSY;
        case FLOWPARSER_STREAM_INVALID_PARAM:
            return ESP32C5_STATUS_INVALID_PARAM;
        case FLOWPARSER_STREAM_NOT_INIT:
            return ESP32C5_STATUS_NOT_READY;
        case FLOWPARSER_STREAM_OVERFLOW:
            return ESP32C5_STATUS_OVERFLOW;
        case FLOWPARSER_STREAM_TIMEOUT:
            return ESP32C5_STATUS_TIMEOUT;
        case FLOWPARSER_STREAM_PORT_FAIL:
        case FLOWPARSER_STREAM_ERROR:
        default:
            return ESP32C5_STATUS_STREAM_FAIL;
    }
}

eEsp32c5Status esp32c5MapResult(eFlowParserResult result)
{
    switch (result) {
        case FLOWPARSER_RESULT_OK:
            return ESP32C5_STATUS_OK;
        case FLOWPARSER_RESULT_TIMEOUT:
            return ESP32C5_STATUS_TIMEOUT;
        case FLOWPARSER_RESULT_OVERFLOW:
            return ESP32C5_STATUS_OVERFLOW;
        case FLOWPARSER_RESULT_SEND_FAIL:
            return ESP32C5_STATUS_STREAM_FAIL;
        case FLOWPARSER_RESULT_ERROR:
        default:
            return ESP32C5_STATUS_ERROR;
    }
}

eEsp32c5Status esp32c5CtrlStart(stEsp32c5Device *device, eEsp32c5Role role)
{
    if ((device == NULL) || !esp32c5IsValidRole(role)) {
        return ESP32C5_STATUS_INVALID_PARAM;
    }

    esp32c5DataReset(&device->dataPlane);
    device->info.hasLastResult = false;
    device->info.lastResult = FLOWPARSER_RESULT_OK;
    device->info.rxBytes = 0U;
    device->info.urcCount = 0U;
    device->state.role = role;
    device->state.runState = ESP32C5_RUN_BOOTING;
    device->state.isReady = false;
    device->state.isBusy = true;
    device->state.isBleAdvertising = false;
    device->state.isBleConnected = false;
    device->state.isReadyUrcSeen = false;
    device->state.hasMacAddress = false;
    device->state.connIndex = 0U;
    device->state.lastError = ESP32C5_STATUS_OK;
    device->state.macAddress[0] = '\0';
    esp32c5CtrlSetStage(device, ESP32C5_CTRL_STAGE_ASSERT_RESET);
    device->ctrlPlane.nextActionTick = 0U;
    device->ctrlPlane.readyDeadlineTick = 0U;
    device->ctrlPlane.isTxnDone = false;
    device->ctrlPlane.txnKind = ESP32C5_CTRL_TXN_NONE;
    device->ctrlPlane.txnStatus = ESP32C5_STATUS_OK;
    device->ctrlPlane.cmdBuf[0] = '\0';
    LOG_I(ESP32C5_CTRL_LOG_TAG, "start role=%s", esp32c5CtrlGetRoleName(role));
    return ESP32C5_STATUS_OK;
}

eEsp32c5Status esp32c5CtrlDisconnectBle(stEsp32c5Device *device)
{
    eEsp32c5Status status;

    if (device == NULL) {
        return ESP32C5_STATUS_INVALID_PARAM;
    }

    esp32c5SyncState(device);
    if ((device->state.role != ESP32C5_ROLE_BLE_PERIPHERAL) || !device->state.isReady) {
        return ESP32C5_STATUS_NOT_READY;
    }

    if (device->ctrlPlane.stage != ESP32C5_CTRL_STAGE_RUNNING) {
        return ESP32C5_STATUS_BUSY;
    }

    if (device->info.isBusy) {
        return ESP32C5_STATUS_BUSY;
    }

    device->ctrlPlane.txnKind = ESP32C5_CTRL_TXN_BLE_DISCONNECT;
    device->ctrlPlane.cmdBuf[0] = '\0';
    status = esp32c5AppendText(device->ctrlPlane.cmdBuf, sizeof(device->ctrlPlane.cmdBuf), &(uint16_t){0U}, "");
    (void)status;

    device->ctrlPlane.cmdBuf[0] = '\0';
    {
        uint16_t length = 0U;
        status = esp32c5AppendText(device->ctrlPlane.cmdBuf, sizeof(device->ctrlPlane.cmdBuf), &length, "AT+BLEDISCONN=");
        if (status == ESP32C5_STATUS_OK) {
            status = esp32c5AppendU16(device->ctrlPlane.cmdBuf, sizeof(device->ctrlPlane.cmdBuf), &length, device->state.connIndex);
        }
        if (status == ESP32C5_STATUS_OK) {
            status = esp32c5AppendText(device->ctrlPlane.cmdBuf, sizeof(device->ctrlPlane.cmdBuf), &length, "\r\n");
        }
    }
    if (status != ESP32C5_STATUS_OK) {
        device->ctrlPlane.txnKind = ESP32C5_CTRL_TXN_NONE;
        return status;
    }

    status = esp32c5SubmitCtrlText(device, device->ctrlPlane.cmdBuf);
    if (status != ESP32C5_STATUS_OK) {
        device->ctrlPlane.txnKind = ESP32C5_CTRL_TXN_NONE;
        LOG_W(ESP32C5_CTRL_LOG_TAG, "submit ble disconnect failed status=%d", (int)status);
    }

    return status;
}

void esp32c5CtrlStop(stEsp32c5Device *device)
{
    esp32c5ResetState(device);
}

eEsp32c5Status esp32c5CtrlProcess(stEsp32c5Device *device, eEsp32c5MapType deviceId, uint32_t nowTickMs)
{
    eEsp32c5Status status;

    if (device == NULL) {
        return ESP32C5_STATUS_INVALID_PARAM;
    }

    esp32c5SyncState(device);
    if (device->state.role == ESP32C5_ROLE_NONE) {
        return ESP32C5_STATUS_OK;
    }

    if (device->ctrlPlane.isTxnDone && !device->info.isBusy) {
        status = esp32c5HandleCtrlDone(device, deviceId, nowTickMs);
        if (status != ESP32C5_STATUS_OK) {
            return status;
        }
    }

    if (device->info.isBusy) {
        return ESP32C5_STATUS_OK;
    }

    return esp32c5ProcessCtrlStage(device, deviceId, nowTickMs);
}

void esp32c5CtrlScheduleRetry(stEsp32c5Device *device, eEsp32c5MapType deviceId, uint32_t nowTickMs, eEsp32c5Status status)
{
    const stEsp32c5ControlInterface *control;
    eEsp32c5Role role;

    if (device == NULL) {
        return;
    }

    control = esp32c5GetControl(deviceId);
    if ((control != NULL) && (control->setResetLevel != NULL)) {
        control->setResetLevel(device->cfg.resetPin, true);
    }

    role = device->state.role;
    flowparserStreamReset(&device->stream);
    esp32c5DataClearPendingTx(&device->dataPlane);
    device->state.isReady = false;
    device->state.isBusy = true;
    device->state.isBleAdvertising = false;
    device->state.isBleConnected = false;
    device->state.isReadyUrcSeen = false;
    device->state.hasMacAddress = false;
    device->state.connIndex = 0U;
    device->state.lastError = status;
    device->state.runState = ESP32C5_RUN_ERROR;
    device->state.macAddress[0] = '\0';
    device->state.role = role;
    esp32c5CtrlSetStage(device, ESP32C5_CTRL_STAGE_ASSERT_RESET);
    device->ctrlPlane.txnKind = ESP32C5_CTRL_TXN_NONE;
    device->ctrlPlane.nextActionTick = nowTickMs + device->cfg.retryIntervalMs;
    device->ctrlPlane.readyDeadlineTick = 0U;
    device->ctrlPlane.isTxnDone = false;
    device->ctrlPlane.txnStatus = ESP32C5_STATUS_OK;
    device->ctrlPlane.cmdBuf[0] = '\0';
    LOG_W(ESP32C5_CTRL_LOG_TAG,
          "schedule retry role=%s status=%d after=%lu stage=%s",
          esp32c5CtrlGetRoleName(role),
          (int)status,
          (unsigned long)device->cfg.retryIntervalMs,
          esp32c5CtrlGetStageName(device->ctrlPlane.stage));
}

bool esp32c5CtrlIsUrc(const stEsp32c5Device *device, const uint8_t *lineBuf, uint16_t lineLen)
{
    uint32_t index;

    if ((device == NULL) || (lineBuf == NULL) || (lineLen == 0U)) {
        return false;
    }

    if ((device->urcCb.pfMatcher != NULL) &&
        device->urcCb.pfMatcher(device->urcCb.matcherUserData, lineBuf, lineLen)) {
        return true;
    }

    for (index = 0U; index < (uint32_t)(sizeof(gEsp32c5DefaultUrcPrefixes) / sizeof(gEsp32c5DefaultUrcPrefixes[0])); index++) {
        if (esp32c5MatchPrefix(lineBuf, lineLen, gEsp32c5DefaultUrcPrefixes[index])) {
            return true;
        }
    }

    return false;
}

void esp32c5CtrlHandleUrc(stEsp32c5Device *device, const uint8_t *lineBuf, uint16_t lineLen)
{
    uint8_t connIndex;

    if ((device == NULL) || (lineBuf == NULL) || (lineLen == 0U)) {
        return;
    }

    device->info.urcCount++;
    if ((lineLen == 5U) && (memcmp(lineBuf, "ready", 5U) == 0)) {
        device->state.isReadyUrcSeen = true;
        LOG_I(ESP32C5_CTRL_LOG_TAG, "urc ready");
        return;
    }

    if (esp32c5MatchPrefix(lineBuf, lineLen, "+BLECONN")) {
        connIndex = 0U;
        (void)esp32c5TryParseConnIndex(lineBuf, lineLen, &connIndex);
        device->state.connIndex = connIndex;
        device->state.isBleConnected = true;
        LOG_I(ESP32C5_CTRL_LOG_TAG, "urc ble connected idx=%u", (unsigned int)connIndex);
        return;
    }

    if (esp32c5MatchPrefix(lineBuf, lineLen, "+BLEDISCONN")) {
        device->state.isBleConnected = false;
        device->state.connIndex = 0U;
        LOG_I(ESP32C5_CTRL_LOG_TAG, "urc ble disconnected");
    }
}

void esp32c5CtrlHandleTxnLine(stEsp32c5Device *device, const uint8_t *lineBuf, uint16_t lineLen)
{
    if ((device == NULL) || (lineBuf == NULL) || (lineLen == 0U)) {
        return;
    }

    if ((device->ctrlPlane.stage == ESP32C5_CTRL_STAGE_QUERY_MAC) &&
        esp32c5TryParseMacAddress(lineBuf, lineLen, device->state.macAddress, (uint16_t)sizeof(device->state.macAddress))) {
        device->state.hasMacAddress = true;
    }
}

void esp32c5CtrlHandleTxnDone(stEsp32c5Device *device, eFlowParserResult result)
{
    if (device == NULL) {
        return;
    }

    device->info.hasLastResult = true;
    device->info.lastResult = result;
    device->ctrlPlane.isTxnDone = true;
    device->ctrlPlane.txnStatus = esp32c5MapResult(result);
    if (device->ctrlPlane.txnStatus != ESP32C5_STATUS_OK) {
        LOG_W(ESP32C5_CTRL_LOG_TAG,
              "txn done failed stage=%s result=%d status=%d",
              esp32c5CtrlGetStageName(device->ctrlPlane.stage),
              (int)result,
              (int)device->ctrlPlane.txnStatus);
    }
}

void esp32c5TxnLineThunk(void *userData, const uint8_t *lineBuf, uint16_t lineLen)
{
    esp32c5CtrlHandleTxnLine((stEsp32c5Device *)userData, lineBuf, lineLen);
}

void esp32c5TxnDoneThunk(void *userData, eFlowParserResult result)
{
    esp32c5CtrlHandleTxnDone((stEsp32c5Device *)userData, result);
}

static bool esp32c5ContainsForbiddenChar(const char *text)
{
    char ch;

    while ((text != NULL) && (*text != '\0')) {
        ch = *text++;
        if ((ch == '"') || (ch == '\r') || (ch == '\n')) {
            return true;
        }
    }

    return false;
}

static eEsp32c5Status esp32c5SubmitCtrlText(stEsp32c5Device *device, const char *cmdText)
{
    stFlowParserReq req;
    eFlowParserStrmSta streamStatus;

    if ((device == NULL) || (cmdText == NULL)) {
        return ESP32C5_STATUS_INVALID_PARAM;
    }

    req.spec = &gEsp32c5CommonSpec;
    req.cmdBuf = (const uint8_t *)cmdText;
    req.cmdLen = (uint16_t)strlen(cmdText);
    req.payloadBuf = NULL;
    req.payloadLen = 0U;
    req.lineHandler = esp32c5TxnLineThunk;
    req.doneHandler = esp32c5TxnDoneThunk;
    req.userData = device;

    streamStatus = flowparserStreamSubmit(&device->stream, &req);
    if (streamStatus != FLOWPARSER_STREAM_OK) {
        LOG_W(ESP32C5_CTRL_LOG_TAG,
              "submit cmd failed stage=%s status=%d cmd=%s",
              esp32c5CtrlGetStageName(device->ctrlPlane.stage),
              (int)streamStatus,
              cmdText);
    }

    return esp32c5MapStreamStatus(streamStatus);
}

static eEsp32c5Status esp32c5SubmitCtrlPrompt(stEsp32c5Device *device, const char *cmdText, const uint8_t *payloadBuf, uint16_t payloadLen)
{
    stFlowParserReq req;
    eFlowParserStrmSta streamStatus;

    if ((device == NULL) || (cmdText == NULL) || (payloadBuf == NULL) || (payloadLen == 0U)) {
        return ESP32C5_STATUS_INVALID_PARAM;
    }

    req.spec = &gEsp32c5PromptSpec;
    req.cmdBuf = (const uint8_t *)cmdText;
    req.cmdLen = (uint16_t)strlen(cmdText);
    req.payloadBuf = payloadBuf;
    req.payloadLen = payloadLen;
    req.lineHandler = esp32c5TxnLineThunk;
    req.doneHandler = esp32c5TxnDoneThunk;
    req.userData = device;

    streamStatus = flowparserStreamSubmit(&device->stream, &req);
    if (streamStatus != FLOWPARSER_STREAM_OK) {
        LOG_W(ESP32C5_CTRL_LOG_TAG,
              "submit prompt cmd failed stage=%s status=%d cmd=%s",
              esp32c5CtrlGetStageName(device->ctrlPlane.stage),
              (int)streamStatus,
              cmdText);
    }

    return esp32c5MapStreamStatus(streamStatus);
}

static eEsp32c5Status esp32c5BuildCtrlU16(stEsp32c5Device *device, const char *prefix, uint16_t value)
{
    uint16_t length;
    eEsp32c5Status status;

    if ((device == NULL) || (prefix == NULL)) {
        return ESP32C5_STATUS_INVALID_PARAM;
    }

    length = 0U;
    device->ctrlPlane.cmdBuf[0] = '\0';
    status = esp32c5AppendText(device->ctrlPlane.cmdBuf, sizeof(device->ctrlPlane.cmdBuf), &length, prefix);
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5AppendU16(device->ctrlPlane.cmdBuf, sizeof(device->ctrlPlane.cmdBuf), &length, value);
    }
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5AppendText(device->ctrlPlane.cmdBuf, sizeof(device->ctrlPlane.cmdBuf), &length, "\r\n");
    }

    return (status == ESP32C5_STATUS_OK) ? esp32c5SubmitCtrlText(device, device->ctrlPlane.cmdBuf) : status;
}

static eEsp32c5Status esp32c5BuildCtrlQuotedText(stEsp32c5Device *device, const char *prefix, const char *text)
{
    uint16_t length;
    eEsp32c5Status status;

    if ((device == NULL) || (prefix == NULL) || (text == NULL)) {
        return ESP32C5_STATUS_INVALID_PARAM;
    }

    length = 0U;
    device->ctrlPlane.cmdBuf[0] = '\0';
    status = esp32c5AppendText(device->ctrlPlane.cmdBuf, sizeof(device->ctrlPlane.cmdBuf), &length, prefix);
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5AppendQuotedText(device->ctrlPlane.cmdBuf, sizeof(device->ctrlPlane.cmdBuf), &length, text);
    }
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5AppendText(device->ctrlPlane.cmdBuf, sizeof(device->ctrlPlane.cmdBuf), &length, "\r\n");
    }

    return (status == ESP32C5_STATUS_OK) ? esp32c5SubmitCtrlText(device, device->ctrlPlane.cmdBuf) : status;
}

static eEsp32c5Status esp32c5BuildCtrlAdvParam(stEsp32c5Device *device)
{
    uint16_t length;
    eEsp32c5Status status;

    if (device == NULL) {
        return ESP32C5_STATUS_INVALID_PARAM;
    }

    length = 0U;
    device->ctrlPlane.cmdBuf[0] = '\0';
    status = esp32c5AppendText(device->ctrlPlane.cmdBuf, sizeof(device->ctrlPlane.cmdBuf), &length, "AT+BLEADVPARAM=");
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5AppendU16(device->ctrlPlane.cmdBuf, sizeof(device->ctrlPlane.cmdBuf), &length, device->bleCfg.advIntervalMin);
    }
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5AppendChar(device->ctrlPlane.cmdBuf, sizeof(device->ctrlPlane.cmdBuf), &length, ',');
    }
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5AppendU16(device->ctrlPlane.cmdBuf, sizeof(device->ctrlPlane.cmdBuf), &length, device->bleCfg.advIntervalMax);
    }
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5AppendText(device->ctrlPlane.cmdBuf,
                                   sizeof(device->ctrlPlane.cmdBuf),
                                   &length,
                                   ",0,0,7\r\n");
    }

    return (status == ESP32C5_STATUS_OK) ? esp32c5SubmitCtrlText(device, device->ctrlPlane.cmdBuf) : status;
}

static eEsp32c5Status esp32c5BuildCtrlAdvData(stEsp32c5Device *device)
{
    uint16_t length;
    eEsp32c5Status status;
    const char *name;
    uint16_t nameLen;
    uint16_t maxNameLen;
    uint16_t index;

    if (device == NULL) {
        return ESP32C5_STATUS_INVALID_PARAM;
    }

    name = device->bleCfg.name;
    nameLen = (uint16_t)strlen(name);
    maxNameLen = 26U;
    if (nameLen > maxNameLen) {
        nameLen = maxNameLen;
    }

    length = 0U;
    device->ctrlPlane.cmdBuf[0] = '\0';
    status = esp32c5AppendText(device->ctrlPlane.cmdBuf, sizeof(device->ctrlPlane.cmdBuf), &length, "AT+BLEADVDATA=\"");
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5AppendHexByte(device->ctrlPlane.cmdBuf, sizeof(device->ctrlPlane.cmdBuf), &length, 0x02U);
    }
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5AppendHexByte(device->ctrlPlane.cmdBuf, sizeof(device->ctrlPlane.cmdBuf), &length, 0x01U);
    }
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5AppendHexByte(device->ctrlPlane.cmdBuf, sizeof(device->ctrlPlane.cmdBuf), &length, 0x06U);
    }

    if ((status == ESP32C5_STATUS_OK) && (nameLen > 0U)) {
        status = esp32c5AppendHexByte(device->ctrlPlane.cmdBuf, sizeof(device->ctrlPlane.cmdBuf), &length, (uint8_t)(nameLen + 1U));
        if (status == ESP32C5_STATUS_OK) {
            status = esp32c5AppendHexByte(device->ctrlPlane.cmdBuf, sizeof(device->ctrlPlane.cmdBuf), &length, 0x09U);
        }
        for (index = 0U; (status == ESP32C5_STATUS_OK) && (index < nameLen); index++) {
            status = esp32c5AppendHexByte(device->ctrlPlane.cmdBuf,
                                          sizeof(device->ctrlPlane.cmdBuf),
                                          &length,
                                          (uint8_t)name[index]);
        }
    }

    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5AppendText(device->ctrlPlane.cmdBuf, sizeof(device->ctrlPlane.cmdBuf), &length, "\"\r\n");
    }

    return (status == ESP32C5_STATUS_OK) ? esp32c5SubmitCtrlText(device, device->ctrlPlane.cmdBuf) : status;
}

static eEsp32c5Status esp32c5AppendChar(char *buffer, uint16_t bufferSize, uint16_t *length, char value)
{
    if ((buffer == NULL) || (length == NULL) || (bufferSize == 0U) || (*length >= (uint16_t)(bufferSize - 1U))) {
        return ESP32C5_STATUS_OVERFLOW;
    }

    buffer[*length] = value;
    *length = (uint16_t)(*length + 1U);
    buffer[*length] = '\0';
    return ESP32C5_STATUS_OK;
}

static eEsp32c5Status esp32c5AppendText(char *buffer, uint16_t bufferSize, uint16_t *length, const char *text)
{
    if ((buffer == NULL) || (length == NULL) || (text == NULL)) {
        return ESP32C5_STATUS_INVALID_PARAM;
    }

    while (*text != '\0') {
        if (esp32c5AppendChar(buffer, bufferSize, length, *text++) != ESP32C5_STATUS_OK) {
            return ESP32C5_STATUS_OVERFLOW;
        }
    }

    return ESP32C5_STATUS_OK;
}

static eEsp32c5Status esp32c5AppendU16(char *buffer, uint16_t bufferSize, uint16_t *length, uint16_t value)
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
        if (esp32c5AppendChar(buffer, bufferSize, length, digits[outIndex]) != ESP32C5_STATUS_OK) {
            return ESP32C5_STATUS_OVERFLOW;
        }
        index--;
    }

    return ESP32C5_STATUS_OK;
}

static eEsp32c5Status esp32c5AppendQuotedText(char *buffer, uint16_t bufferSize, uint16_t *length, const char *text)
{
    eEsp32c5Status status;

    status = esp32c5AppendChar(buffer, bufferSize, length, '"');
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5AppendText(buffer, bufferSize, length, text);
    }
    if (status == ESP32C5_STATUS_OK) {
        status = esp32c5AppendChar(buffer, bufferSize, length, '"');
    }

    return status;
}

static eEsp32c5Status esp32c5AppendHexByte(char *buffer, uint16_t bufferSize, uint16_t *length, uint8_t value)
{
    static const char hexDigits[] = "0123456789ABCDEF";
    eEsp32c5Status status;

    status = esp32c5AppendChar(buffer, bufferSize, length, hexDigits[(value >> 4U) & 0x0FU]);
    if (status != ESP32C5_STATUS_OK) {
        return status;
    }

    return esp32c5AppendChar(buffer, bufferSize, length, hexDigits[value & 0x0FU]);
}

static eEsp32c5Status esp32c5HandleCtrlDone(stEsp32c5Device *device, eEsp32c5MapType deviceId, uint32_t nowTickMs)
{
    eEsp32c5Status status;

    if (device == NULL) {
        return ESP32C5_STATUS_INVALID_PARAM;
    }

    status = device->ctrlPlane.txnStatus;
    device->ctrlPlane.isTxnDone = false;
    device->ctrlPlane.txnStatus = ESP32C5_STATUS_OK;

    if (status != ESP32C5_STATUS_OK) {
        if (device->ctrlPlane.stage == ESP32C5_CTRL_STAGE_WAIT_READY) {
            if (nowTickMs >= device->ctrlPlane.readyDeadlineTick) {
                esp32c5CtrlScheduleRetry(device, deviceId, nowTickMs, status);
                return status;
            }

            device->ctrlPlane.txnKind = ESP32C5_CTRL_TXN_NONE;
            device->ctrlPlane.nextActionTick = nowTickMs + device->cfg.readyProbeMs;
            return ESP32C5_STATUS_OK;
        }

        if (device->ctrlPlane.stage == ESP32C5_CTRL_STAGE_QUERY_MAC) {
            device->ctrlPlane.txnKind = ESP32C5_CTRL_TXN_NONE;
            esp32c5CtrlSetStage(device, ESP32C5_CTRL_STAGE_RUNNING);
            device->state.runState = ESP32C5_RUN_READY;
            device->state.isReady = true;
            LOG_W(ESP32C5_CTRL_LOG_TAG, "mac query failed, continue without cached mac");
            return ESP32C5_STATUS_OK;
        }

        if (device->ctrlPlane.stage == ESP32C5_CTRL_STAGE_RUNNING) {
            device->state.lastError = status;
            if (device->ctrlPlane.txnKind == ESP32C5_CTRL_TXN_DATA_TX) {
                esp32c5DataClearPendingTx(&device->dataPlane);
            }
            device->ctrlPlane.txnKind = ESP32C5_CTRL_TXN_NONE;
            return ESP32C5_STATUS_OK;
        }

        esp32c5CtrlScheduleRetry(device, deviceId, nowTickMs, status);
        return status;
    }

    switch (device->ctrlPlane.stage) {
        case ESP32C5_CTRL_STAGE_WAIT_READY:
            device->ctrlPlane.txnKind = ESP32C5_CTRL_TXN_NONE;
            device->state.runState = ESP32C5_RUN_CONFIGURING;
            esp32c5CtrlSetStage(device, ESP32C5_CTRL_STAGE_DISABLE_ECHO);
            break;
        case ESP32C5_CTRL_STAGE_DISABLE_ECHO:
            device->ctrlPlane.txnKind = ESP32C5_CTRL_TXN_NONE;
            esp32c5CtrlSetStage(device, ESP32C5_CTRL_STAGE_BLE_INIT);
            break;
        case ESP32C5_CTRL_STAGE_BLE_INIT:
            device->ctrlPlane.txnKind = ESP32C5_CTRL_TXN_NONE;
            esp32c5CtrlSetStage(device, ESP32C5_CTRL_STAGE_BLE_SET_NAME);
            break;
        case ESP32C5_CTRL_STAGE_BLE_SET_NAME:
            device->ctrlPlane.txnKind = ESP32C5_CTRL_TXN_NONE;
            esp32c5CtrlSetStage(device, ESP32C5_CTRL_STAGE_BLE_SET_ADV_PARAM);
            break;
        case ESP32C5_CTRL_STAGE_BLE_SET_ADV_PARAM:
            device->ctrlPlane.txnKind = ESP32C5_CTRL_TXN_NONE;
            esp32c5CtrlSetStage(device, ESP32C5_CTRL_STAGE_BLE_SET_ADV_DATA);
            break;
        case ESP32C5_CTRL_STAGE_BLE_SET_ADV_DATA:
            device->ctrlPlane.txnKind = ESP32C5_CTRL_TXN_NONE;
            esp32c5CtrlSetStage(device, ESP32C5_CTRL_STAGE_BLE_ADV_START);
            break;
        case ESP32C5_CTRL_STAGE_BLE_ADV_START:
            device->ctrlPlane.txnKind = ESP32C5_CTRL_TXN_NONE;
            device->state.isBleAdvertising = true;
            esp32c5CtrlSetStage(device, ESP32C5_CTRL_STAGE_QUERY_MAC);
            break;
        case ESP32C5_CTRL_STAGE_QUERY_MAC:
            device->ctrlPlane.txnKind = ESP32C5_CTRL_TXN_NONE;
            esp32c5CtrlSetStage(device, ESP32C5_CTRL_STAGE_RUNNING);
            device->state.runState = ESP32C5_RUN_READY;
            device->state.isReady = true;
            LOG_I(ESP32C5_CTRL_LOG_TAG,
                  "module ready mac=%s",
                  device->state.hasMacAddress ? device->state.macAddress : "<unknown>");
            break;
        case ESP32C5_CTRL_STAGE_RUNNING:
            if (device->ctrlPlane.txnKind == ESP32C5_CTRL_TXN_DATA_TX) {
                esp32c5DataConfirmPendingTx(&device->dataPlane);
            } else if (device->ctrlPlane.txnKind == ESP32C5_CTRL_TXN_BLE_DISCONNECT) {
                device->state.isBleConnected = false;
                device->state.connIndex = 0U;
            }
            device->ctrlPlane.txnKind = ESP32C5_CTRL_TXN_NONE;
            break;
        default:
            break;
    }

    return ESP32C5_STATUS_OK;
}

static eEsp32c5Status esp32c5ProcessCtrlStage(stEsp32c5Device *device, eEsp32c5MapType deviceId, uint32_t nowTickMs)
{
    const stEsp32c5ControlInterface *control;
    eEsp32c5Status status;
    const uint8_t *payloadBuf;
    uint16_t payloadLen;

    if (device == NULL) {
        return ESP32C5_STATUS_INVALID_PARAM;
    }

    switch (device->ctrlPlane.stage) {
        case ESP32C5_CTRL_STAGE_IDLE:
            return ESP32C5_STATUS_OK;
        case ESP32C5_CTRL_STAGE_ASSERT_RESET:
            if ((device->ctrlPlane.nextActionTick != 0U) && (nowTickMs < device->ctrlPlane.nextActionTick)) {
                return ESP32C5_STATUS_OK;
            }

            control = esp32c5GetControl(deviceId);
            if ((control == NULL) || (control->setResetLevel == NULL)) {
                esp32c5CtrlScheduleRetry(device, deviceId, nowTickMs, ESP32C5_STATUS_UNSUPPORTED);
                return ESP32C5_STATUS_UNSUPPORTED;
            }

            device->state.runState = ESP32C5_RUN_BOOTING;
            control->setResetLevel(device->cfg.resetPin, true);
            device->ctrlPlane.nextActionTick = nowTickMs + device->cfg.resetPulseMs;
            esp32c5CtrlSetStage(device, ESP32C5_CTRL_STAGE_RESET_HOLD);
            LOG_I(ESP32C5_CTRL_LOG_TAG,
                  "assert reset pin=%u pulse=%lu",
                  (unsigned int)device->cfg.resetPin,
                  (unsigned long)device->cfg.resetPulseMs);
            return ESP32C5_STATUS_OK;
        case ESP32C5_CTRL_STAGE_RESET_HOLD:
            if (nowTickMs < device->ctrlPlane.nextActionTick) {
                return ESP32C5_STATUS_OK;
            }

            control = esp32c5GetControl(deviceId);
            if ((control == NULL) || (control->setResetLevel == NULL)) {
                esp32c5CtrlScheduleRetry(device, deviceId, nowTickMs, ESP32C5_STATUS_UNSUPPORTED);
                return ESP32C5_STATUS_UNSUPPORTED;
            }

            control->setResetLevel(device->cfg.resetPin, false);
            device->state.isReadyUrcSeen = false;
            device->ctrlPlane.nextActionTick = nowTickMs + device->cfg.resetWaitMs + device->cfg.bootWaitMs;
            device->ctrlPlane.readyDeadlineTick = device->ctrlPlane.nextActionTick + device->cfg.readyTimeoutMs;
            esp32c5CtrlSetStage(device, ESP32C5_CTRL_STAGE_WAIT_READY);
            LOG_I(ESP32C5_CTRL_LOG_TAG,
                  "release reset waitReady timeout=%lu probe=%lu",
                  (unsigned long)device->cfg.readyTimeoutMs,
                  (unsigned long)device->cfg.readyProbeMs);
            return ESP32C5_STATUS_OK;
        case ESP32C5_CTRL_STAGE_WAIT_READY:
            if (nowTickMs < device->ctrlPlane.nextActionTick) {
                return ESP32C5_STATUS_OK;
            }

            if (device->state.isReadyUrcSeen) {
                device->state.runState = ESP32C5_RUN_CONFIGURING;
                esp32c5CtrlSetStage(device, ESP32C5_CTRL_STAGE_DISABLE_ECHO);
                LOG_I(ESP32C5_CTRL_LOG_TAG, "ready urc seen, continue init");
                return ESP32C5_STATUS_OK;
            }

            if (nowTickMs >= device->ctrlPlane.readyDeadlineTick) {
                esp32c5CtrlScheduleRetry(device, deviceId, nowTickMs, ESP32C5_STATUS_TIMEOUT);
                return ESP32C5_STATUS_TIMEOUT;
            }

            device->ctrlPlane.nextActionTick = nowTickMs + device->cfg.readyProbeMs;
            return ESP32C5_STATUS_OK;
        case ESP32C5_CTRL_STAGE_DISABLE_ECHO:
            device->ctrlPlane.txnKind = ESP32C5_CTRL_TXN_STAGE;
            return esp32c5SubmitCtrlText(device, "ATE0\r\n");
        case ESP32C5_CTRL_STAGE_BLE_INIT:
            device->ctrlPlane.txnKind = ESP32C5_CTRL_TXN_STAGE;
            return esp32c5BuildCtrlU16(device, "AT+BLEINIT=", device->bleCfg.initMode);
        case ESP32C5_CTRL_STAGE_BLE_SET_NAME:
            if (device->bleCfg.name[0] == '\0') {
                esp32c5CtrlSetStage(device, ESP32C5_CTRL_STAGE_BLE_SET_ADV_PARAM);
                return ESP32C5_STATUS_OK;
            }

            device->ctrlPlane.txnKind = ESP32C5_CTRL_TXN_STAGE;
            return esp32c5BuildCtrlQuotedText(device, "AT+BLENAME=", device->bleCfg.name);
        case ESP32C5_CTRL_STAGE_BLE_SET_ADV_PARAM:
            device->ctrlPlane.txnKind = ESP32C5_CTRL_TXN_STAGE;
            return esp32c5BuildCtrlAdvParam(device);
        case ESP32C5_CTRL_STAGE_BLE_SET_ADV_DATA:
            device->ctrlPlane.txnKind = ESP32C5_CTRL_TXN_STAGE;
            return esp32c5BuildCtrlAdvData(device);
        case ESP32C5_CTRL_STAGE_BLE_ADV_START:
            device->ctrlPlane.txnKind = ESP32C5_CTRL_TXN_STAGE;
            return esp32c5SubmitCtrlText(device, "AT+BLEADVSTART\r\n");
        case ESP32C5_CTRL_STAGE_QUERY_MAC:
            device->ctrlPlane.txnKind = ESP32C5_CTRL_TXN_STAGE;
            return esp32c5SubmitCtrlText(device, "AT+BLEADDR?\r\n");
        case ESP32C5_CTRL_STAGE_RUNNING:
            device->state.runState = ESP32C5_RUN_READY;
            device->state.isReady = true;
            if (device->state.isBleConnected && esp32c5DataHasPendingTx(&device->dataPlane)) {
                status = esp32c5DataBuildBleNotify(&device->dataPlane,
                                                   device->state.connIndex,
                                                   device->bleCfg.txServiceIndex,
                                                   device->bleCfg.txCharIndex,
                                                   device->ctrlPlane.cmdBuf,
                                                   (uint16_t)sizeof(device->ctrlPlane.cmdBuf),
                                                   &payloadBuf,
                                                   &payloadLen);
                if (status == ESP32C5_STATUS_NOT_READY) {
                    return ESP32C5_STATUS_OK;
                }
                if (status != ESP32C5_STATUS_OK) {
                    device->state.lastError = status;
                    return ESP32C5_STATUS_OK;
                }

                device->ctrlPlane.txnKind = ESP32C5_CTRL_TXN_DATA_TX;
                status = esp32c5SubmitCtrlPrompt(device, device->ctrlPlane.cmdBuf, payloadBuf, payloadLen);
                if (status != ESP32C5_STATUS_OK) {
                    device->ctrlPlane.txnKind = ESP32C5_CTRL_TXN_NONE;
                    device->state.lastError = status;
                }
            }
            return ESP32C5_STATUS_OK;
        default:
            return ESP32C5_STATUS_OK;
    }
}

static bool esp32c5MatchPrefix(const uint8_t *lineBuf, uint16_t lineLen, const char *prefix)
{
    uint16_t prefixLen;

    if ((lineBuf == NULL) || (prefix == NULL)) {
        return false;
    }

    prefixLen = (uint16_t)strlen(prefix);
    return (lineLen >= prefixLen) && (memcmp(lineBuf, prefix, prefixLen) == 0);
}

static bool esp32c5TryParseConnIndex(const uint8_t *lineBuf, uint16_t lineLen, uint8_t *connIndex)
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

static bool esp32c5TryParseMacAddress(const uint8_t *lineBuf, uint16_t lineLen, char *buffer, uint16_t bufferSize)
{
    uint16_t index;
    const char *prefix;
    uint16_t prefixLen;

    if ((lineBuf == NULL) || (buffer == NULL) || (bufferSize < ESP32C5_MAC_ADDRESS_TEXT_MAX_LENGTH)) {
        return false;
    }

    prefix = ESP32C5_MAC_QUERY_PREFIX;
    prefixLen = (uint16_t)strlen(prefix);
    for (index = 0U; index < lineLen; index++) {
        if ((index + prefixLen) <= lineLen && (memcmp(&lineBuf[index], prefix, prefixLen) == 0)) {
            return esp32c5TryParseMacCandidate(lineBuf, lineLen, (uint16_t)(index + prefixLen), buffer, bufferSize);
        }
    }

    return false;
}

static bool esp32c5TryParseMacCandidate(const uint8_t *lineBuf, uint16_t lineLen, uint16_t start, char *buffer, uint16_t bufferSize)
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

        if ((lineBuf[index] == ':') || esp32c5TryHexNibble(lineBuf[index], &nibble)) {
            buffer[out++] = (char)lineBuf[index];
            continue;
        }
        break;
    }

    buffer[out] = '\0';
    return out == (ESP32C5_MAC_ADDRESS_TEXT_MAX_LENGTH - 1U);
}

static bool esp32c5TryHexNibble(uint8_t ch, uint8_t *value)
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

static const char *esp32c5CtrlGetStageName(eEsp32c5CtrlStage stage)
{
    switch (stage) {
        case ESP32C5_CTRL_STAGE_IDLE:
            return "idle";
        case ESP32C5_CTRL_STAGE_ASSERT_RESET:
            return "assert_reset";
        case ESP32C5_CTRL_STAGE_RESET_HOLD:
            return "reset_hold";
        case ESP32C5_CTRL_STAGE_WAIT_READY:
            return "wait_ready";
        case ESP32C5_CTRL_STAGE_DISABLE_ECHO:
            return "disable_echo";
        case ESP32C5_CTRL_STAGE_BLE_INIT:
            return "ble_init";
        case ESP32C5_CTRL_STAGE_BLE_SET_NAME:
            return "ble_set_name";
        case ESP32C5_CTRL_STAGE_BLE_SET_ADV_PARAM:
            return "ble_set_adv_param";
        case ESP32C5_CTRL_STAGE_BLE_SET_ADV_DATA:
            return "ble_set_adv_data";
        case ESP32C5_CTRL_STAGE_BLE_ADV_START:
            return "ble_adv_start";
        case ESP32C5_CTRL_STAGE_QUERY_MAC:
            return "query_mac";
        case ESP32C5_CTRL_STAGE_RUNNING:
            return "running";
        default:
            return "unknown";
    }
}

static const char *esp32c5CtrlGetRoleName(eEsp32c5Role role)
{
    switch (role) {
        case ESP32C5_ROLE_BLE_PERIPHERAL:
            return "ble_peripheral";
        case ESP32C5_ROLE_NONE:
        default:
            return "none";
    }
}

static void esp32c5CtrlSetStage(stEsp32c5Device *device, eEsp32c5CtrlStage stage)
{
    if (device == NULL) {
        return;
    }

    device->ctrlPlane.stage = stage;
}

/**************************End of file********************************/
