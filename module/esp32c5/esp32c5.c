/************************************************************************************
* @file     : esp32c5.c
* @brief    : ESP32-C5 module top-level implementation.
* @details  : Owns device instance management, public API entry points, and the
*             unified background pump bridging transport, control, and data planes.
* @author   : GitHub Copilot
* @date     : 2026-04-23
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "esp32c5.h"

#include <string.h>

#include "../../service/log/log.h"
#include "esp32c5_ctrl.h"

#define ESP32C5_LOG_TAG                     "esp32c5"

static stEsp32c5Device gEsp32c5Devices[ESP32C5_DEV_MAX];
static bool gEsp32c5DefCfgDone[ESP32C5_DEV_MAX] = {false};

__attribute__((weak)) void esp32c5LoadPlatformDefaultCfg(eEsp32c5MapType device, stEsp32c5Cfg *cfg)
{
    (void)device;

    if (cfg == NULL) {
        return;
    }

    cfg->linkId = 0U;
    cfg->resetPin = 0U;
    cfg->rxPollChunkSize = ESP32C5_RX_POLL_CHUNK_SIZE;
    cfg->txTimeoutMs = ESP32C5_DEFAULT_TX_TIMEOUT_MS;
    cfg->bootWaitMs = ESP32C5_DEFAULT_BOOT_WAIT_MS;
    cfg->resetPulseMs = ESP32C5_DEFAULT_RESET_PULSE_MS;
    cfg->resetWaitMs = ESP32C5_DEFAULT_RESET_WAIT_MS;
    cfg->readyTimeoutMs = ESP32C5_DEFAULT_READY_TIMEOUT_MS;
    cfg->readyProbeMs = ESP32C5_DEFAULT_READY_PROBE_MS;
    cfg->retryIntervalMs = ESP32C5_DEFAULT_RETRY_INTERVAL_MS;
}

__attribute__((weak)) const stEsp32c5TransportInterface *esp32c5GetPlatformTransportInterface(const stEsp32c5Cfg *cfg)
{
    (void)cfg;
    return NULL;
}

__attribute__((weak)) const stEsp32c5ControlInterface *esp32c5GetPlatformControlInterface(eEsp32c5MapType device)
{
    (void)device;
    return NULL;
}

__attribute__((weak)) bool esp32c5PlatformIsValidCfg(const stEsp32c5Cfg *cfg)
{
    (void)cfg;
    return false;
}

static bool esp32c5IsValidDevice(eEsp32c5MapType device);
static stEsp32c5Device *esp32c5GetDevice(eEsp32c5MapType device);
static void esp32c5LoadDefCfg(eEsp32c5MapType device, stEsp32c5Cfg *cfg);
static eEsp32c5Status esp32c5BackGround(stEsp32c5Device *device);
static eFlowParserStrmSta esp32c5StreamSend(void *userData, const uint8_t *buf, uint16_t len);
static uint32_t esp32c5StreamGetTickMs(void *userData);
static bool esp32c5StreamIsUrc(void *userData, const uint8_t *lineBuf, uint16_t lineLen);
static void esp32c5StreamDispatchUrc(void *userData, const uint8_t *lineBuf, uint16_t lineLen);
static eFlowParserRawMatchSta esp32c5StreamRawMatch(void *userData, const uint8_t *buf, uint16_t availLen, uint16_t *frameLen);
static void esp32c5StreamDispatchRaw(void *userData, const uint8_t *frameBuf, uint16_t frameLen);

eEsp32c5Status esp32c5GetDefCfg(eEsp32c5MapType device, stEsp32c5Cfg *cfg)
{
    if ((cfg == NULL) || !esp32c5IsValidDevice(device)) {
        return ESP32C5_STATUS_INVALID_PARAM;
    }

    esp32c5LoadDefCfg(device, cfg);
    return ESP32C5_STATUS_OK;
}

eEsp32c5Status esp32c5GetCfg(eEsp32c5MapType device, stEsp32c5Cfg *cfg)
{
    stEsp32c5Device *deviceObj;

    if (cfg == NULL) {
        return ESP32C5_STATUS_INVALID_PARAM;
    }

    deviceObj = esp32c5GetDevice(device);
    if (deviceObj == NULL) {
        return ESP32C5_STATUS_INVALID_PARAM;
    }

    *cfg = deviceObj->cfg;
    return ESP32C5_STATUS_OK;
}

eEsp32c5Status esp32c5SetCfg(eEsp32c5MapType device, const stEsp32c5Cfg *cfg)
{
    stEsp32c5Device *deviceObj;

    if ((cfg == NULL) || !esp32c5PlatformIsValidCfg(cfg)) {
        return ESP32C5_STATUS_INVALID_PARAM;
    }

    deviceObj = esp32c5GetDevice(device);
    if (deviceObj == NULL) {
        return ESP32C5_STATUS_INVALID_PARAM;
    }

    deviceObj->cfg = *cfg;
    deviceObj->info.isInit = false;
    deviceObj->info.isReady = false;
    esp32c5ResetState(deviceObj);
    gEsp32c5DefCfgDone[device] = true;
    return ESP32C5_STATUS_OK;
}

eEsp32c5Status esp32c5GetDefBleCfg(eEsp32c5MapType device, stEsp32c5BleCfg *cfg)
{
    (void)device;

    if (cfg == NULL) {
        return ESP32C5_STATUS_INVALID_PARAM;
    }

    esp32c5LoadDefBleCfg(cfg);
    return ESP32C5_STATUS_OK;
}

eEsp32c5Status esp32c5SetBleCfg(eEsp32c5MapType device, const stEsp32c5BleCfg *cfg)
{
    stEsp32c5Device *deviceObj;

    if ((cfg == NULL) || !esp32c5IsValidText(cfg->name, ESP32C5_BLE_NAME_MAX_LENGTH, true)) {
        return ESP32C5_STATUS_INVALID_PARAM;
    }

    deviceObj = esp32c5GetDevice(device);
    if (deviceObj == NULL) {
        return ESP32C5_STATUS_INVALID_PARAM;
    }

    deviceObj->bleCfg = *cfg;
    deviceObj->state.hasMacAddress = false;
    deviceObj->state.macAddress[0] = '\0';
    return ESP32C5_STATUS_OK;
}

eEsp32c5Status esp32c5Init(eEsp32c5MapType device)
{
    stEsp32c5Device *deviceObj;
    const stEsp32c5ControlInterface *control;
    const stEsp32c5TransportInterface *transport;
    stFlowParserStreamCfg streamCfg;
    eDrvStatus drvStatus;
    eFlowParserStrmSta streamStatus;

    deviceObj = esp32c5GetDevice(device);
    if (deviceObj == NULL) {
        return ESP32C5_STATUS_INVALID_PARAM;
    }

    if (!esp32c5PlatformIsValidCfg(&deviceObj->cfg)) {
        return ESP32C5_STATUS_INVALID_PARAM;
    }

    transport = esp32c5GetTransport(deviceObj);
    if ((transport == NULL) || (transport->init == NULL) || (transport->write == NULL) ||
        (transport->getRxLen == NULL) || (transport->read == NULL) || (transport->getTickMs == NULL)) {
        return ESP32C5_STATUS_NOT_READY;
    }

    drvStatus = transport->init(deviceObj->cfg.linkId);
    if (drvStatus != DRV_STATUS_OK) {
        return esp32c5MapDrvStatus(drvStatus);
    }

    control = esp32c5GetControl(device);
    if ((control != NULL) && (control->init != NULL)) {
        control->init(deviceObj->cfg.resetPin);
    }

    (void)memset(&streamCfg, 0, sizeof(streamCfg));
    streamCfg.rxStorage = deviceObj->rxStorage;
    streamCfg.rxStorageSize = sizeof(deviceObj->rxStorage);
    streamCfg.lineBuf = deviceObj->lineBuf;
    streamCfg.lineBufSize = sizeof(deviceObj->lineBuf);
    streamCfg.pfSend = esp32c5StreamSend;
    streamCfg.sendUserData = deviceObj;
    streamCfg.pfGetTickMs = esp32c5StreamGetTickMs;
    streamCfg.tickUserData = deviceObj;
    streamCfg.pfUrcHandler = esp32c5StreamDispatchUrc;
    streamCfg.urcUserData = deviceObj;
    streamCfg.pfIsUrc = esp32c5StreamIsUrc;
    streamCfg.isUrcUserData = deviceObj;
    streamCfg.pfRawMatcher = esp32c5StreamRawMatch;
    streamCfg.rawMatchUserData = deviceObj;
    streamCfg.pfRawHandler = esp32c5StreamDispatchRaw;
    streamCfg.rawHandlerUserData = deviceObj;

    streamStatus = flowparserStreamInit(&deviceObj->stream, &streamCfg);
    if (streamStatus != FLOWPARSER_STREAM_OK) {
        return esp32c5MapStreamStatus(streamStatus);
    }

    deviceObj->info.isInit = true;
    deviceObj->info.isReady = true;
    deviceObj->info.isBusy = false;
    deviceObj->info.hasLastResult = false;
    deviceObj->info.lastResult = FLOWPARSER_RESULT_OK;
    deviceObj->info.stage = FLOWPARSER_STAGE_IDLE;
    deviceObj->info.rxBytes = 0U;
    deviceObj->info.urcCount = 0U;
    esp32c5ResetState(deviceObj);
    return ESP32C5_STATUS_OK;
}

void esp32c5Reset(eEsp32c5MapType device)
{
    stEsp32c5Device *deviceObj;

    deviceObj = esp32c5GetDevice(device);
    if ((deviceObj == NULL) || !deviceObj->info.isInit) {
        return;
    }

    flowparserStreamReset(&deviceObj->stream);
    esp32c5ResetState(deviceObj);
}

eEsp32c5Status esp32c5Start(eEsp32c5MapType device, eEsp32c5Role role)
{
    stEsp32c5Device *deviceObj;
    const stEsp32c5ControlInterface *control;
    eEsp32c5Status status;

    if (!esp32c5IsValidRole(role)) {
        return ESP32C5_STATUS_INVALID_PARAM;
    }

    deviceObj = esp32c5GetDevice(device);
    if ((deviceObj == NULL) || !deviceObj->info.isInit) {
        return ESP32C5_STATUS_NOT_READY;
    }

    control = esp32c5GetControl(device);
    if ((control == NULL) || (control->setResetLevel == NULL)) {
        return ESP32C5_STATUS_UNSUPPORTED;
    }

    flowparserStreamReset(&deviceObj->stream);
    status = esp32c5CtrlStart(deviceObj, role);
    if (status != ESP32C5_STATUS_OK) {
        return status;
    }

    control->setResetLevel(deviceObj->cfg.resetPin, true);
    return ESP32C5_STATUS_OK;
}

eEsp32c5Status esp32c5DisconnectBle(eEsp32c5MapType device)
{
    stEsp32c5Device *deviceObj;

    deviceObj = esp32c5GetDevice(device);
    if ((deviceObj == NULL) || !deviceObj->info.isInit) {
        return ESP32C5_STATUS_NOT_READY;
    }

    if ((deviceObj->state.role != ESP32C5_ROLE_BLE_PERIPHERAL) || !deviceObj->state.isReady) {
        return ESP32C5_STATUS_NOT_READY;
    }

    if (!deviceObj->state.isBleConnected) {
        return ESP32C5_STATUS_OK;
    }

    esp32c5SyncState(deviceObj);
    if (deviceObj->state.isBusy) {
        return ESP32C5_STATUS_BUSY;
    }

    return esp32c5CtrlDisconnectBle(deviceObj);
}

void esp32c5Stop(eEsp32c5MapType device)
{
    stEsp32c5Device *deviceObj;
    const stEsp32c5ControlInterface *control;

    deviceObj = esp32c5GetDevice(device);
    if ((deviceObj == NULL) || !deviceObj->info.isInit) {
        return;
    }

    control = esp32c5GetControl(device);
    if ((control != NULL) && (control->setResetLevel != NULL)) {
        control->setResetLevel(deviceObj->cfg.resetPin, true);
    }

    flowparserStreamReset(&deviceObj->stream);
    esp32c5CtrlStop(deviceObj);
}

eEsp32c5Status esp32c5Process(eEsp32c5MapType device, uint32_t nowTickMs)
{
    stEsp32c5Device *deviceObj;
    eEsp32c5Status status;

    deviceObj = esp32c5GetDevice(device);
    if ((deviceObj == NULL) || !deviceObj->info.isInit) {
        return ESP32C5_STATUS_NOT_READY;
    }

    status = esp32c5BackGround(deviceObj);
    if (status != ESP32C5_STATUS_OK) {
        if (deviceObj->state.role != ESP32C5_ROLE_NONE) {
            esp32c5CtrlScheduleRetry(deviceObj, device, nowTickMs, status);
        }
        return status;
    }

    return esp32c5CtrlProcess(deviceObj, device, nowTickMs);
}

bool esp32c5IsReady(eEsp32c5MapType device)
{
    const stEsp32c5State *state;

    state = esp32c5GetState(device);
    return (state != NULL) && state->isReady;
}

const stEsp32c5Info *esp32c5GetInfo(eEsp32c5MapType device)
{
    stEsp32c5Device *deviceObj;

    deviceObj = esp32c5GetDevice(device);
    if (deviceObj == NULL) {
        return NULL;
    }

    esp32c5SyncInfo(deviceObj);
    return &deviceObj->info;
}

const stEsp32c5State *esp32c5GetState(eEsp32c5MapType device)
{
    stEsp32c5Device *deviceObj;

    deviceObj = esp32c5GetDevice(device);
    if (deviceObj == NULL) {
        return NULL;
    }

    esp32c5SyncState(deviceObj);
    return &deviceObj->state;
}

uint16_t esp32c5GetRxLength(eEsp32c5MapType device)
{
    stEsp32c5Device *deviceObj;

    deviceObj = esp32c5GetDevice(device);
    if (deviceObj == NULL) {
        return 0U;
    }

    return esp32c5DataGetRxLength(&deviceObj->dataPlane);
}

uint16_t esp32c5ReadData(eEsp32c5MapType device, uint8_t *buffer, uint16_t bufferSize)
{
    stEsp32c5Device *deviceObj;

    if ((buffer == NULL) || (bufferSize == 0U)) {
        return 0U;
    }

    deviceObj = esp32c5GetDevice(device);
    if (deviceObj == NULL) {
        return 0U;
    }

    return esp32c5DataRead(&deviceObj->dataPlane, buffer, bufferSize);
}

eEsp32c5Status esp32c5WriteData(eEsp32c5MapType device, const uint8_t *buffer, uint16_t length)
{
    stEsp32c5Device *deviceObj;

    if ((buffer == NULL) || (length == 0U)) {
        return ESP32C5_STATUS_INVALID_PARAM;
    }

    deviceObj = esp32c5GetDevice(device);
    if ((deviceObj == NULL) || !deviceObj->info.isInit) {
        return ESP32C5_STATUS_NOT_READY;
    }

    if ((deviceObj->state.role != ESP32C5_ROLE_BLE_PERIPHERAL) || !deviceObj->state.isReady) {
        return ESP32C5_STATUS_NOT_READY;
    }

    deviceObj->state.lastError = esp32c5DataWrite(&deviceObj->dataPlane, buffer, length);
    return deviceObj->state.lastError;
}

bool esp32c5GetCachedMac(eEsp32c5MapType device, char *buffer, uint16_t bufferSize)
{
    stEsp32c5Device *deviceObj;
    uint16_t length;

    if ((buffer == NULL) || (bufferSize == 0U)) {
        return false;
    }

    deviceObj = esp32c5GetDevice(device);
    if ((deviceObj == NULL) || !deviceObj->state.hasMacAddress) {
        return false;
    }

    length = (uint16_t)strlen(deviceObj->state.macAddress);
    if (bufferSize <= length) {
        return false;
    }

    (void)memcpy(buffer, deviceObj->state.macAddress, length + 1U);
    return true;
}

eEsp32c5Status esp32c5SetUrcHandler(eEsp32c5MapType device, esp32c5LineFunc handler, void *userData)
{
    stEsp32c5Device *deviceObj;

    deviceObj = esp32c5GetDevice(device);
    if (deviceObj == NULL) {
        return ESP32C5_STATUS_INVALID_PARAM;
    }

    deviceObj->urcCb.pfHandler = handler;
    deviceObj->urcCb.handlerUserData = userData;
    return ESP32C5_STATUS_OK;
}

eEsp32c5Status esp32c5SetUrcMatcher(eEsp32c5MapType device, esp32c5UrcMatchFunc matcher, void *userData)
{
    stEsp32c5Device *deviceObj;

    deviceObj = esp32c5GetDevice(device);
    if (deviceObj == NULL) {
        return ESP32C5_STATUS_INVALID_PARAM;
    }

    deviceObj->urcCb.pfMatcher = matcher;
    deviceObj->urcCb.matcherUserData = userData;
    return ESP32C5_STATUS_OK;
}

eEsp32c5Status esp32c5SetRawMatcher(eEsp32c5MapType device, esp32c5RawMatchFunc matcher, void *userData)
{
    stEsp32c5Device *deviceObj;

    deviceObj = esp32c5GetDevice(device);
    if (deviceObj == NULL) {
        return ESP32C5_STATUS_INVALID_PARAM;
    }

    deviceObj->urcCb.pfRawMatcher = matcher;
    deviceObj->urcCb.rawMatcherUserData = userData;
    flowparserStreamSetRawHook(&deviceObj->stream, esp32c5StreamRawMatch, deviceObj, esp32c5StreamDispatchRaw, deviceObj);
    return ESP32C5_STATUS_OK;
}

static bool esp32c5IsValidDevice(eEsp32c5MapType device)
{
    return (uint32_t)device < (uint32_t)ESP32C5_DEV_MAX;
}

static stEsp32c5Device *esp32c5GetDevice(eEsp32c5MapType device)
{
    if (!esp32c5IsValidDevice(device)) {
        return NULL;
    }

    if (!gEsp32c5DefCfgDone[device]) {
        esp32c5LoadDefCfg(device, &gEsp32c5Devices[device].cfg);
        esp32c5LoadDefBleCfg(&gEsp32c5Devices[device].bleCfg);
        esp32c5ResetState(&gEsp32c5Devices[device]);
        gEsp32c5DefCfgDone[device] = true;
    }

    return &gEsp32c5Devices[device];
}

static void esp32c5LoadDefCfg(eEsp32c5MapType device, stEsp32c5Cfg *cfg)
{
    if (cfg == NULL) {
        return;
    }

    esp32c5LoadPlatformDefaultCfg(device, cfg);
    if (cfg->rxPollChunkSize == 0U) {
        cfg->rxPollChunkSize = ESP32C5_RX_POLL_CHUNK_SIZE;
    }
    if (cfg->txTimeoutMs == 0U) {
        cfg->txTimeoutMs = ESP32C5_DEFAULT_TX_TIMEOUT_MS;
    }
    if (cfg->resetPulseMs == 0U) {
        cfg->resetPulseMs = ESP32C5_DEFAULT_RESET_PULSE_MS;
    }
    if (cfg->resetWaitMs == 0U) {
        cfg->resetWaitMs = ESP32C5_DEFAULT_RESET_WAIT_MS;
    }
    if (cfg->readyTimeoutMs == 0U) {
        cfg->readyTimeoutMs = ESP32C5_DEFAULT_READY_TIMEOUT_MS;
    }
    if (cfg->readyProbeMs == 0U) {
        cfg->readyProbeMs = ESP32C5_DEFAULT_READY_PROBE_MS;
    }
    if (cfg->retryIntervalMs == 0U) {
        cfg->retryIntervalMs = ESP32C5_DEFAULT_RETRY_INTERVAL_MS;
    }
}

static eEsp32c5Status esp32c5BackGround(stEsp32c5Device *device)
{
    const stEsp32c5TransportInterface *transport;
    uint8_t rxBuffer[ESP32C5_RX_POLL_CHUNK_SIZE];
    uint16_t availLen;
    uint16_t readLen;
    uint16_t chunkLimit;
    eDrvStatus drvStatus;
    eFlowParserStrmSta streamStatus;

    if ((device == NULL) || !device->info.isReady) {
        return ESP32C5_STATUS_NOT_READY;
    }

    transport = esp32c5GetTransport(device);
    if (transport == NULL) {
        return ESP32C5_STATUS_NOT_READY;
    }

    chunkLimit = device->cfg.rxPollChunkSize;
    if ((chunkLimit == 0U) || (chunkLimit > sizeof(rxBuffer))) {
        chunkLimit = sizeof(rxBuffer);
    }

    availLen = transport->getRxLen(device->cfg.linkId);
    while (availLen > 0U) {
        readLen = (availLen > chunkLimit) ? chunkLimit : availLen;
        drvStatus = transport->read(device->cfg.linkId, rxBuffer, readLen);
        if (drvStatus != DRV_STATUS_OK) {
            return esp32c5MapDrvStatus(drvStatus);
        }

        streamStatus = flowparserStreamFeed(&device->stream, rxBuffer, readLen);
        if (streamStatus != FLOWPARSER_STREAM_OK) {
            return esp32c5MapStreamStatus(streamStatus);
        }

        streamStatus = flowparserStreamProc(&device->stream);
        if ((streamStatus != FLOWPARSER_STREAM_OK) && (streamStatus != FLOWPARSER_STREAM_EMPTY) &&
            (streamStatus != FLOWPARSER_STREAM_BUSY)) {
            esp32c5SyncState(device);
            return esp32c5MapStreamStatus(streamStatus);
        }

        device->info.rxBytes += readLen;
        availLen = transport->getRxLen(device->cfg.linkId);
    }

    streamStatus = flowparserStreamProc(&device->stream);
    esp32c5SyncState(device);
    if ((streamStatus == FLOWPARSER_STREAM_OK) || (streamStatus == FLOWPARSER_STREAM_EMPTY) ||
        (streamStatus == FLOWPARSER_STREAM_BUSY)) {
        return ESP32C5_STATUS_OK;
    }

    return esp32c5MapStreamStatus(streamStatus);
}

static eFlowParserStrmSta esp32c5StreamSend(void *userData, const uint8_t *buf, uint16_t len)
{
    stEsp32c5Device *deviceObj;
    const stEsp32c5TransportInterface *transport;

    deviceObj = (stEsp32c5Device *)userData;
    if ((deviceObj == NULL) || (buf == NULL) || (len == 0U)) {
        return FLOWPARSER_STREAM_INVALID_PARAM;
    }

    transport = esp32c5GetTransport(deviceObj);
    if ((transport == NULL) || (transport->write == NULL)) {
        return FLOWPARSER_STREAM_NOT_INIT;
    }

    if (transport->write(deviceObj->cfg.linkId, buf, len, deviceObj->cfg.txTimeoutMs) != DRV_STATUS_OK) {
        return FLOWPARSER_STREAM_PORT_FAIL;
    }

    return FLOWPARSER_STREAM_OK;
}

static uint32_t esp32c5StreamGetTickMs(void *userData)
{
    stEsp32c5Device *deviceObj;
    const stEsp32c5TransportInterface *transport;

    deviceObj = (stEsp32c5Device *)userData;
    if (deviceObj == NULL) {
        return 0U;
    }

    transport = esp32c5GetTransport(deviceObj);
    if ((transport == NULL) || (transport->getTickMs == NULL)) {
        return 0U;
    }

    return transport->getTickMs();
}

static bool esp32c5StreamIsUrc(void *userData, const uint8_t *lineBuf, uint16_t lineLen)
{
    return esp32c5CtrlIsUrc((stEsp32c5Device *)userData, lineBuf, lineLen);
}

static void esp32c5StreamDispatchUrc(void *userData, const uint8_t *lineBuf, uint16_t lineLen)
{
    stEsp32c5Device *deviceObj;

    deviceObj = (stEsp32c5Device *)userData;
    if ((deviceObj == NULL) || (lineBuf == NULL) || (lineLen == 0U)) {
        return;
    }

    esp32c5CtrlHandleUrc(deviceObj, lineBuf, lineLen);
    (void)esp32c5DataTryStoreUrcPayload(&deviceObj->dataPlane, lineBuf, lineLen);
    if (deviceObj->urcCb.pfHandler != NULL) {
        deviceObj->urcCb.pfHandler(deviceObj->urcCb.handlerUserData, lineBuf, lineLen);
    }
}

static eFlowParserRawMatchSta esp32c5StreamRawMatch(void *userData, const uint8_t *buf, uint16_t availLen, uint16_t *frameLen)
{
    stEsp32c5Device *deviceObj;
    eEsp32c5RawMatchSta matchSta;

    deviceObj = (stEsp32c5Device *)userData;
    if ((deviceObj == NULL) || (deviceObj->urcCb.pfRawMatcher == NULL)) {
        return FLOWPARSER_RAW_MATCH_NONE;
    }

    matchSta = deviceObj->urcCb.pfRawMatcher(deviceObj->urcCb.rawMatcherUserData, buf, availLen, frameLen);
    switch (matchSta) {
        case ESP32C5_RAW_MATCH_NEED_MORE:
            return FLOWPARSER_RAW_MATCH_NEED_MORE;
        case ESP32C5_RAW_MATCH_OK:
            return FLOWPARSER_RAW_MATCH_OK;
        case ESP32C5_RAW_MATCH_NONE:
        default:
            return FLOWPARSER_RAW_MATCH_NONE;
    }
}

static void esp32c5StreamDispatchRaw(void *userData, const uint8_t *frameBuf, uint16_t frameLen)
{
    stEsp32c5Device *deviceObj;

    deviceObj = (stEsp32c5Device *)userData;
    if ((deviceObj == NULL) || (frameBuf == NULL) || (frameLen == 0U)) {
        return;
    }

    esp32c5DataStoreRx(&deviceObj->dataPlane, frameBuf, frameLen);
}

/**************************End of file********************************/
