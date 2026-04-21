/************************************************************************************
* @file     : fc41d.c
* @brief    : FC41D module top-level implementation.
* @details  : This file owns device instance management, public API entry points,
*             and the unified background pump that bridges transport, control, and
*             data planes.
* @author   : GitHub Copilot
* @date     : 2026-04-21
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "fc41d.h"

#include <string.h>

#include "fc41d_ctrl.h"

static stFc41dDevice gFc41dDevices[FC41D_DEV_MAX];
static bool gFc41dDefCfgDone[FC41D_DEV_MAX] = {false};

__attribute__((weak)) void fc41dLoadPlatformDefaultCfg(eFc41dMapType device, stFc41dCfg *cfg)
{
    (void)device;

    if (cfg == NULL) {
        return;
    }

    cfg->linkId = 0U;
    cfg->rxPollChunkSize = FC41D_RX_POLL_CHUNK_SIZE;
    cfg->txTimeoutMs = FC41D_DEFAULT_TX_TIMEOUT_MS;
    cfg->bootWaitMs = FC41D_DEFAULT_BOOT_WAIT_MS;
    cfg->resetPulseMs = FC41D_DEFAULT_RESET_PULSE_MS;
    cfg->resetWaitMs = FC41D_DEFAULT_RESET_WAIT_MS;
    cfg->readyTimeoutMs = FC41D_DEFAULT_READY_TIMEOUT_MS;
    cfg->readySettleMs = FC41D_DEFAULT_READY_SETTLE_MS;
    cfg->retryIntervalMs = FC41D_DEFAULT_RETRY_INTERVAL_MS;
}

__attribute__((weak)) const stFc41dTransportInterface *fc41dGetPlatformTransportInterface(const stFc41dCfg *cfg)
{
    (void)cfg;
    return NULL;
}

__attribute__((weak)) const stFc41dControlInterface *fc41dGetPlatformControlInterface(eFc41dMapType device)
{
    (void)device;
    return NULL;
}

__attribute__((weak)) bool fc41dPlatformIsValidCfg(const stFc41dCfg *cfg)
{
    (void)cfg;
    return false;
}

static bool fc41dIsValidDevice(eFc41dMapType device);
static stFc41dDevice *fc41dGetDevice(eFc41dMapType device);
static void fc41dLoadDefCfg(eFc41dMapType device, stFc41dCfg *cfg);
static eFc41dStatus fc41dBackGround(stFc41dDevice *device);
static eFlowParserStrmSta fc41dStreamSend(void *userData, const uint8_t *buf, uint16_t len);
static uint32_t fc41dStreamGetTickMs(void *userData);
static bool fc41dStreamIsUrc(void *userData, const uint8_t *lineBuf, uint16_t lineLen);
static void fc41dStreamDispatchUrc(void *userData, const uint8_t *lineBuf, uint16_t lineLen);

eFc41dStatus fc41dGetDefCfg(eFc41dMapType device, stFc41dCfg *cfg)
{
    if ((cfg == NULL) || !fc41dIsValidDevice(device)) {
        return FC41D_STATUS_INVALID_PARAM;
    }

    fc41dLoadDefCfg(device, cfg);
    return FC41D_STATUS_OK;
}

eFc41dStatus fc41dGetCfg(eFc41dMapType device, stFc41dCfg *cfg)
{
    stFc41dDevice *lDevice;

    if (cfg == NULL) {
        return FC41D_STATUS_INVALID_PARAM;
    }

    lDevice = fc41dGetDevice(device);
    if (lDevice == NULL) {
        return FC41D_STATUS_INVALID_PARAM;
    }

    *cfg = lDevice->cfg;
    return FC41D_STATUS_OK;
}

eFc41dStatus fc41dSetCfg(eFc41dMapType device, const stFc41dCfg *cfg)
{
    stFc41dDevice *lDevice;

    if ((cfg == NULL) || !fc41dPlatformIsValidCfg(cfg)) {
        return FC41D_STATUS_INVALID_PARAM;
    }

    lDevice = fc41dGetDevice(device);
    if (lDevice == NULL) {
        return FC41D_STATUS_INVALID_PARAM;
    }

    lDevice->cfg = *cfg;
    lDevice->info.isInit = false;
    lDevice->info.isReady = false;
    fc41dResetState(lDevice);
    gFc41dDefCfgDone[device] = true;
    return FC41D_STATUS_OK;
}

eFc41dStatus fc41dGetDefBleCfg(eFc41dMapType device, stFc41dBleCfg *cfg)
{
    (void)device;

    if (cfg == NULL) {
        return FC41D_STATUS_INVALID_PARAM;
    }

    fc41dLoadDefBleCfg(cfg);
    return FC41D_STATUS_OK;
}

eFc41dStatus fc41dSetBleCfg(eFc41dMapType device, const stFc41dBleCfg *cfg)
{
    stFc41dDevice *lDevice;

    if ((cfg == NULL) || !fc41dIsValidText(cfg->name, FC41D_BLE_NAME_MAX_LENGTH, true) ||
        !fc41dIsValidText(cfg->serviceUuid, FC41D_BLE_UUID_MAX_LENGTH, true) ||
        !fc41dIsValidText(cfg->rxCharUuid, FC41D_BLE_UUID_MAX_LENGTH, true) ||
        !fc41dIsValidText(cfg->txCharUuid, FC41D_BLE_UUID_MAX_LENGTH, true)) {
        return FC41D_STATUS_INVALID_PARAM;
    }

    lDevice = fc41dGetDevice(device);
    if (lDevice == NULL) {
        return FC41D_STATUS_INVALID_PARAM;
    }

    lDevice->bleCfg = *cfg;
    lDevice->state.hasMacAddress = false;
    lDevice->state.macAddress[0] = '\0';
    return FC41D_STATUS_OK;
}

eFc41dStatus fc41dInit(eFc41dMapType device)
{
    stFc41dDevice *lDevice;
    const stFc41dControlInterface *lControl;
    const stFc41dTransportInterface *lTransport;
    stFlowParserStreamCfg lStreamCfg;
    eDrvStatus lDrvStatus;
    eFlowParserStrmSta lStreamStatus;

    lDevice = fc41dGetDevice(device);
    if (lDevice == NULL) {
        return FC41D_STATUS_INVALID_PARAM;
    }

    if (!fc41dPlatformIsValidCfg(&lDevice->cfg)) {
        return FC41D_STATUS_INVALID_PARAM;
    }

    lTransport = fc41dGetTransport(lDevice);
    if ((lTransport == NULL) || (lTransport->init == NULL) || (lTransport->write == NULL) ||
        (lTransport->getRxLen == NULL) || (lTransport->read == NULL) || (lTransport->getTickMs == NULL)) {
        return FC41D_STATUS_NOT_READY;
    }

    lDrvStatus = lTransport->init(lDevice->cfg.linkId);
    if (lDrvStatus != DRV_STATUS_OK) {
        return fc41dMapDrvStatus(lDrvStatus);
    }

    lControl = fc41dGetControl(device);
    if ((lControl != NULL) && (lControl->init != NULL)) {
        lControl->init();
    }

    (void)memset(&lStreamCfg, 0, sizeof(lStreamCfg));
    lStreamCfg.rxStorage = lDevice->rxStorage;
    lStreamCfg.rxStorageSize = sizeof(lDevice->rxStorage);
    lStreamCfg.lineBuf = lDevice->lineBuf;
    lStreamCfg.lineBufSize = sizeof(lDevice->lineBuf);
    lStreamCfg.pfSend = fc41dStreamSend;
    lStreamCfg.sendUserData = lDevice;
    lStreamCfg.pfGetTickMs = fc41dStreamGetTickMs;
    lStreamCfg.tickUserData = lDevice;
    lStreamCfg.pfUrcHandler = fc41dStreamDispatchUrc;
    lStreamCfg.urcUserData = lDevice;
    lStreamCfg.pfIsUrc = fc41dStreamIsUrc;
    lStreamCfg.isUrcUserData = lDevice;

    lStreamStatus = flowparserStreamInit(&lDevice->stream, &lStreamCfg);
    if (lStreamStatus != FLOWPARSER_STREAM_OK) {
        return fc41dMapStreamStatus(lStreamStatus);
    }

    lDevice->info.isInit = true;
    lDevice->info.isReady = true;
    lDevice->info.isBusy = false;
    lDevice->info.hasLastResult = false;
    lDevice->info.lastResult = FLOWPARSER_RESULT_OK;
    lDevice->info.stage = FLOWPARSER_STAGE_IDLE;
    lDevice->info.rxBytes = 0U;
    lDevice->info.urcCount = 0U;
    fc41dResetState(lDevice);
    return FC41D_STATUS_OK;
}

void fc41dReset(eFc41dMapType device)
{
    stFc41dDevice *lDevice;

    lDevice = fc41dGetDevice(device);
    if ((lDevice == NULL) || !lDevice->info.isInit) {
        return;
    }

    flowparserStreamReset(&lDevice->stream);
    fc41dResetState(lDevice);
}

eFc41dStatus fc41dStart(eFc41dMapType device, eFc41dRole role)
{
    stFc41dDevice *lDevice;
    const stFc41dControlInterface *lControl;
    eFc41dStatus lStatus;

    if (!fc41dIsValidRole(role)) {
        return FC41D_STATUS_INVALID_PARAM;
    }

    lDevice = fc41dGetDevice(device);
    if ((lDevice == NULL) || !lDevice->info.isInit) {
        return FC41D_STATUS_NOT_READY;
    }

    lControl = fc41dGetControl(device);
    if ((lControl == NULL) || (lControl->setResetLevel == NULL)) {
        return FC41D_STATUS_UNSUPPORTED;
    }

    flowparserStreamReset(&lDevice->stream);
    lStatus = fc41dCtrlStart(lDevice, role);
    if (lStatus != FC41D_STATUS_OK) {
        return lStatus;
    }

    lControl->setResetLevel(false);
    return FC41D_STATUS_OK;
}

void fc41dStop(eFc41dMapType device)
{
    stFc41dDevice *lDevice;
    const stFc41dControlInterface *lControl;

    lDevice = fc41dGetDevice(device);
    if ((lDevice == NULL) || !lDevice->info.isInit) {
        return;
    }

    lControl = fc41dGetControl(device);
    if ((lControl != NULL) && (lControl->setResetLevel != NULL)) {
        lControl->setResetLevel(false);
    }

    flowparserStreamReset(&lDevice->stream);
    fc41dCtrlStop(lDevice);
}

eFc41dStatus fc41dProcess(eFc41dMapType device, uint32_t nowTickMs)
{
    stFc41dDevice *lDevice;
    eFc41dStatus lStatus;

    lDevice = fc41dGetDevice(device);
    if ((lDevice == NULL) || !lDevice->info.isInit) {
        return FC41D_STATUS_NOT_READY;
    }

    lStatus = fc41dBackGround(lDevice);
    if (lStatus != FC41D_STATUS_OK) {
        if (lDevice->state.role != FC41D_ROLE_NONE) {
            fc41dCtrlScheduleRetry(lDevice, device, nowTickMs, lStatus);
        }
        return lStatus;
    }

    return fc41dCtrlProcess(lDevice, device, nowTickMs);
}

bool fc41dIsReady(eFc41dMapType device)
{
    const stFc41dState *lpState;

    lpState = fc41dGetState(device);
    return (lpState != NULL) && lpState->isReady;
}

const stFc41dInfo *fc41dGetInfo(eFc41dMapType device)
{
    stFc41dDevice *lDevice;

    lDevice = fc41dGetDevice(device);
    if (lDevice == NULL) {
        return NULL;
    }

    fc41dSyncInfo(lDevice);
    return &lDevice->info;
}

const stFc41dState *fc41dGetState(eFc41dMapType device)
{
    stFc41dDevice *lDevice;

    lDevice = fc41dGetDevice(device);
    if (lDevice == NULL) {
        return NULL;
    }

    fc41dSyncState(lDevice);
    return &lDevice->state;
}

uint16_t fc41dGetRxLength(eFc41dMapType device)
{
    stFc41dDevice *lDevice;

    lDevice = fc41dGetDevice(device);
    if (lDevice == NULL) {
        return 0U;
    }

    return fc41dDataGetRxLength(&lDevice->dataPlane);
}

uint16_t fc41dReadData(eFc41dMapType device, uint8_t *buffer, uint16_t bufferSize)
{
    stFc41dDevice *lDevice;

    if ((buffer == NULL) || (bufferSize == 0U)) {
        return 0U;
    }

    lDevice = fc41dGetDevice(device);
    if (lDevice == NULL) {
        return 0U;
    }

    return fc41dDataRead(&lDevice->dataPlane, buffer, bufferSize);
}

eFc41dStatus fc41dWriteData(eFc41dMapType device, const uint8_t *buffer, uint16_t length)
{
    stFc41dDevice *lDevice;

    if ((buffer == NULL) || (length == 0U)) {
        return FC41D_STATUS_INVALID_PARAM;
    }

    lDevice = fc41dGetDevice(device);
    if ((lDevice == NULL) || !lDevice->info.isInit) {
        return FC41D_STATUS_NOT_READY;
    }

    if ((lDevice->state.role != FC41D_ROLE_BLE_PERIPHERAL) || !lDevice->state.isReady) {
        return FC41D_STATUS_NOT_READY;
    }

    return fc41dDataWrite(&lDevice->dataPlane, buffer, length);
}

bool fc41dGetCachedMac(eFc41dMapType device, char *buffer, uint16_t bufferSize)
{
    stFc41dDevice *lDevice;
    uint16_t lLength;

    if ((buffer == NULL) || (bufferSize == 0U)) {
        return false;
    }

    lDevice = fc41dGetDevice(device);
    if ((lDevice == NULL) || !lDevice->state.hasMacAddress) {
        return false;
    }

    lLength = (uint16_t)strlen(lDevice->state.macAddress);
    if (bufferSize <= lLength) {
        return false;
    }

    (void)memcpy(buffer, lDevice->state.macAddress, lLength + 1U);
    return true;
}

eFc41dStatus fc41dSetUrcHandler(eFc41dMapType device, fc41dLineFunc handler, void *userData)
{
    stFc41dDevice *lDevice;

    lDevice = fc41dGetDevice(device);
    if (lDevice == NULL) {
        return FC41D_STATUS_INVALID_PARAM;
    }

    lDevice->urcCb.pfHandler = handler;
    lDevice->urcCb.handlerUserData = userData;
    return FC41D_STATUS_OK;
}

eFc41dStatus fc41dSetUrcMatcher(eFc41dMapType device, fc41dUrcMatchFunc matcher, void *userData)
{
    stFc41dDevice *lDevice;

    lDevice = fc41dGetDevice(device);
    if (lDevice == NULL) {
        return FC41D_STATUS_INVALID_PARAM;
    }

    lDevice->urcCb.pfMatcher = matcher;
    lDevice->urcCb.matcherUserData = userData;
    return FC41D_STATUS_OK;
}

void fc41dTxnLineThunk(void *userData, const uint8_t *lineBuf, uint16_t lineLen)
{
    stFc41dDevice *lDevice;

    lDevice = (stFc41dDevice *)userData;
    fc41dCtrlHandleTxnLine(lDevice, lineBuf, lineLen);
}

void fc41dTxnDoneThunk(void *userData, eFlowParserResult result)
{
    stFc41dDevice *lDevice;

    lDevice = (stFc41dDevice *)userData;
    fc41dCtrlHandleTxnDone(lDevice, result);
}

static bool fc41dIsValidDevice(eFc41dMapType device)
{
    return (uint32_t)device < (uint32_t)FC41D_DEV_MAX;
}

static stFc41dDevice *fc41dGetDevice(eFc41dMapType device)
{
    if (!fc41dIsValidDevice(device)) {
        return NULL;
    }

    if (!gFc41dDefCfgDone[device]) {
        fc41dLoadDefCfg(device, &gFc41dDevices[device].cfg);
        fc41dLoadDefBleCfg(&gFc41dDevices[device].bleCfg);
        fc41dResetState(&gFc41dDevices[device]);
        gFc41dDefCfgDone[device] = true;
    }

    return &gFc41dDevices[device];
}

static void fc41dLoadDefCfg(eFc41dMapType device, stFc41dCfg *cfg)
{
    if (cfg == NULL) {
        return;
    }

    fc41dLoadPlatformDefaultCfg(device, cfg);
    if (cfg->rxPollChunkSize == 0U) {
        cfg->rxPollChunkSize = FC41D_RX_POLL_CHUNK_SIZE;
    }
    if (cfg->txTimeoutMs == 0U) {
        cfg->txTimeoutMs = FC41D_DEFAULT_TX_TIMEOUT_MS;
    }
    if (cfg->bootWaitMs == 0U) {
        cfg->bootWaitMs = FC41D_DEFAULT_BOOT_WAIT_MS;
    }
    if (cfg->resetPulseMs == 0U) {
        cfg->resetPulseMs = FC41D_DEFAULT_RESET_PULSE_MS;
    }
    if (cfg->resetWaitMs == 0U) {
        cfg->resetWaitMs = FC41D_DEFAULT_RESET_WAIT_MS;
    }
    if (cfg->readyTimeoutMs == 0U) {
        cfg->readyTimeoutMs = FC41D_DEFAULT_READY_TIMEOUT_MS;
    }
    if (cfg->readySettleMs == 0U) {
        cfg->readySettleMs = FC41D_DEFAULT_READY_SETTLE_MS;
    }
    if (cfg->retryIntervalMs == 0U) {
        cfg->retryIntervalMs = FC41D_DEFAULT_RETRY_INTERVAL_MS;
    }
}

static eFc41dStatus fc41dBackGround(stFc41dDevice *device)
{
    const stFc41dTransportInterface *lTransport;
    uint8_t lRxBuffer[FC41D_RX_POLL_CHUNK_SIZE];
    uint16_t lAvailLen;
    uint16_t lReadLen;
    uint16_t lChunkLimit;
    eDrvStatus lDrvStatus;
    eFlowParserStrmSta lStreamStatus;

    if ((device == NULL) || !device->info.isReady) {
        return FC41D_STATUS_NOT_READY;
    }

    lTransport = fc41dGetTransport(device);
    if (lTransport == NULL) {
        return FC41D_STATUS_NOT_READY;
    }

    lChunkLimit = device->cfg.rxPollChunkSize;
    if ((lChunkLimit == 0U) || (lChunkLimit > sizeof(lRxBuffer))) {
        lChunkLimit = sizeof(lRxBuffer);
    }

    lAvailLen = lTransport->getRxLen(device->cfg.linkId);
    while (lAvailLen > 0U) {
        lReadLen = (lAvailLen > lChunkLimit) ? lChunkLimit : lAvailLen;
        lDrvStatus = lTransport->read(device->cfg.linkId, lRxBuffer, lReadLen);
        if (lDrvStatus != DRV_STATUS_OK) {
            return fc41dMapDrvStatus(lDrvStatus);
        }

        lStreamStatus = flowparserStreamFeed(&device->stream, lRxBuffer, lReadLen);
        if (lStreamStatus != FLOWPARSER_STREAM_OK) {
            return fc41dMapStreamStatus(lStreamStatus);
        }

        device->info.rxBytes += lReadLen;
        lAvailLen = lTransport->getRxLen(device->cfg.linkId);
    }

    lStreamStatus = flowparserStreamProc(&device->stream);
    fc41dSyncState(device);
    if ((lStreamStatus == FLOWPARSER_STREAM_OK) || (lStreamStatus == FLOWPARSER_STREAM_EMPTY) ||
        (lStreamStatus == FLOWPARSER_STREAM_BUSY)) {
        return FC41D_STATUS_OK;
    }

    return fc41dMapStreamStatus(lStreamStatus);
}

static eFlowParserStrmSta fc41dStreamSend(void *userData, const uint8_t *buf, uint16_t len)
{
    stFc41dDevice *lDevice;
    const stFc41dTransportInterface *lTransport;

    lDevice = (stFc41dDevice *)userData;
    if ((lDevice == NULL) || (buf == NULL) || (len == 0U)) {
        return FLOWPARSER_STREAM_INVALID_PARAM;
    }

    lTransport = fc41dGetTransport(lDevice);
    if ((lTransport == NULL) || (lTransport->write == NULL)) {
        return FLOWPARSER_STREAM_NOT_INIT;
    }

    return (lTransport->write(lDevice->cfg.linkId, buf, len, lDevice->cfg.txTimeoutMs) == DRV_STATUS_OK) ?
           FLOWPARSER_STREAM_OK :
           FLOWPARSER_STREAM_PORT_FAIL;
}

static uint32_t fc41dStreamGetTickMs(void *userData)
{
    stFc41dDevice *lDevice;
    const stFc41dTransportInterface *lTransport;

    lDevice = (stFc41dDevice *)userData;
    if (lDevice == NULL) {
        return 0U;
    }

    lTransport = fc41dGetTransport(lDevice);
    if ((lTransport == NULL) || (lTransport->getTickMs == NULL)) {
        return 0U;
    }

    return lTransport->getTickMs();
}

static bool fc41dStreamIsUrc(void *userData, const uint8_t *lineBuf, uint16_t lineLen)
{
    stFc41dDevice *lDevice;

    lDevice = (stFc41dDevice *)userData;
    return fc41dCtrlIsUrc(lDevice, lineBuf, lineLen);
}

static void fc41dStreamDispatchUrc(void *userData, const uint8_t *lineBuf, uint16_t lineLen)
{
    stFc41dDevice *lDevice;

    lDevice = (stFc41dDevice *)userData;
    if ((lDevice == NULL) || (lineBuf == NULL) || (lineLen == 0U)) {
        return;
    }

    fc41dCtrlHandleUrc(lDevice, lineBuf, lineLen);
    (void)fc41dDataTryStoreUrcPayload(&lDevice->dataPlane, lineBuf, lineLen);
    if (lDevice->urcCb.pfHandler != NULL) {
        lDevice->urcCb.pfHandler(lDevice->urcCb.handlerUserData, lineBuf, lineLen);
    }
}
/**************************End of file********************************/
