/************************************************************************************
* @file     : ec800m.c
* @brief    : EC800M-CN module top-level implementation.
* @details  : Owns device instances, startup probing, and flowparser-backed AT
*             transactions over a platform transport.
* @author   : GitHub Copilot
* @date     : 2026-04-27
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "ec800m.h"

#include <string.h>

#include "../../service/log/log.h"
#include "ec800m_ctrl.h"

#define EC800M_LOG_TAG                         "ec800m"

static const char *const gEc800mCommonDonePatterns[] = {"OK"};
static const char *const gEc800mPromptDonePatterns[] = {"OK", "SEND OK", "+QMTPUBEX:*", "+QHTTPPOST:*"};
static const char *const gEc800mCommonErrorPatterns[] = {"ERROR", "FAIL", "+CME ERROR:*", "+CMS ERROR:*"};
static const char *const gEc800mDefaultUrcPrefixes[] = {
    "RDY",
    "APP RDY",
    "PB DONE",
    "+QIURC",
    "+QMTSTAT",
    "+QMTRECV",
    "+QHTTPPOST",
    "+QHTTPREAD",
};

static const stFlowParserSpec gEc800mCommonSpec = {
    .responseDonePatterns = gEc800mCommonDonePatterns,
    .responseDonePatternCnt = 1U,
    .finalDonePatterns = NULL,
    .finalDonePatternCnt = 0U,
    .errorPatterns = gEc800mCommonErrorPatterns,
    .errorPatternCnt = 4U,
    .totalToutMs = EC800M_DEFAULT_CMD_TIMEOUT_MS,
    .responseToutMs = EC800M_DEFAULT_CMD_TIMEOUT_MS,
    .promptToutMs = 0U,
    .finalToutMs = 0U,
    .needPrompt = false,
};

static const stFlowParserSpec gEc800mPromptSpec = {
    .responseDonePatterns = NULL,
    .responseDonePatternCnt = 0U,
    .finalDonePatterns = gEc800mPromptDonePatterns,
    .finalDonePatternCnt = 4U,
    .errorPatterns = gEc800mCommonErrorPatterns,
    .errorPatternCnt = 4U,
    .totalToutMs = EC800M_DEFAULT_FINAL_TIMEOUT_MS,
    .responseToutMs = 0U,
    .promptToutMs = EC800M_DEFAULT_PROMPT_TIMEOUT_MS,
    .finalToutMs = EC800M_DEFAULT_FINAL_TIMEOUT_MS,
    .needPrompt = true,
};

static stEc800mDevice gEc800mDevices[EC800M_DEV_MAX];
static bool gEc800mDefCfgDone[EC800M_DEV_MAX] = {false};

__attribute__((weak)) void ec800mLoadPlatformDefaultCfg(eEc800mMapType device, stEc800mCfg *cfg)
{
    (void)device;

    if (cfg == NULL) {
        return;
    }

    cfg->linkId = 0U;
    cfg->pwrkeyPin = 0U;
    cfg->resetPin = 0U;
    cfg->rxPollChunkSize = EC800M_RX_POLL_CHUNK_SIZE;
    cfg->txTimeoutMs = EC800M_DEFAULT_TX_TIMEOUT_MS;
    cfg->bootWaitMs = EC800M_DEFAULT_BOOT_WAIT_MS;
    cfg->pwrkeyPulseMs = EC800M_DEFAULT_PWRKEY_PULSE_MS;
    cfg->resetPulseMs = EC800M_DEFAULT_RESET_PULSE_MS;
    cfg->resetWaitMs = EC800M_DEFAULT_RESET_WAIT_MS;
    cfg->readyTimeoutMs = EC800M_DEFAULT_READY_TIMEOUT_MS;
    cfg->retryIntervalMs = EC800M_DEFAULT_RETRY_INTERVAL_MS;
}

__attribute__((weak)) const stEc800mTransportInterface *ec800mGetPlatformTransportInterface(const stEc800mCfg *cfg)
{
    (void)cfg;
    return NULL;
}

__attribute__((weak)) const stEc800mControlInterface *ec800mGetPlatformControlInterface(eEc800mMapType device)
{
    (void)device;
    return NULL;
}

__attribute__((weak)) bool ec800mPlatformIsValidCfg(const stEc800mCfg *cfg)
{
    (void)cfg;
    return false;
}

static bool ec800mIsValidDevice(eEc800mMapType device);
static bool ec800mIsValidServiceMode(eEc800mServiceMode serviceMode);
static stEc800mDevice *ec800mGetDevice(eEc800mMapType device);
static void ec800mLoadDefCfg(eEc800mMapType device, stEc800mCfg *cfg);
static void ec800mResetState(stEc800mDevice *device);
static void ec800mSyncInfo(stEc800mDevice *device);
static void ec800mSyncState(stEc800mDevice *device);
static eEc800mStatus ec800mMapDrvStatus(eDrvStatus status);
static eEc800mStatus ec800mMapStreamStatus(eFlowParserStrmSta status);
static eEc800mStatus ec800mMapResult(eFlowParserResult result);
static eEc800mStatus ec800mBackGround(stEc800mDevice *device);
static eEc800mStatus ec800mPollTransport(stEc800mDevice *device);
static eEc800mStatus ec800mProcessCtrl(stEc800mDevice *device, eEc800mMapType deviceId, uint32_t nowTickMs);
static eEc800mStatus ec800mProcessCtrlDone(stEc800mDevice *device, eEc800mMapType deviceId, uint32_t nowTickMs);
static eEc800mStatus ec800mSubmitCtrlText(stEc800mDevice *device, const char *cmdText);
static eEc800mStatus ec800mSubmitTextInternal(stEc800mDevice *device, const char *cmdText, ec800mLineFunc lineHandler, void *userData, eEc800mCtrlTxnKind txnKind);
static eEc800mStatus ec800mSubmitPromptInternal(stEc800mDevice *device, const char *cmdText, const uint8_t *payloadBuf, uint16_t payloadLen, ec800mLineFunc lineHandler, void *userData);
static bool ec800mMatchPrefix(const uint8_t *lineBuf, uint16_t lineLen, const char *prefix);
static bool ec800mIsDefaultUrc(const uint8_t *lineBuf, uint16_t lineLen);
static eFlowParserStrmSta ec800mStreamSend(void *userData, const uint8_t *buf, uint16_t len);
static uint32_t ec800mStreamGetTickMs(void *userData);
static bool ec800mStreamIsUrc(void *userData, const uint8_t *lineBuf, uint16_t lineLen);
static void ec800mStreamDispatchUrc(void *userData, const uint8_t *lineBuf, uint16_t lineLen);
static void ec800mTxnLineThunk(void *userData, const uint8_t *lineBuf, uint16_t lineLen);
static void ec800mTxnDoneThunk(void *userData, eFlowParserResult result);

eEc800mStatus ec800mGetDefCfg(eEc800mMapType device, stEc800mCfg *cfg)
{
    if ((cfg == NULL) || !ec800mIsValidDevice(device)) {
        return EC800M_STATUS_INVALID_PARAM;
    }

    ec800mLoadDefCfg(device, cfg);
    return EC800M_STATUS_OK;
}

eEc800mStatus ec800mGetCfg(eEc800mMapType device, stEc800mCfg *cfg)
{
    stEc800mDevice *deviceObj;

    if (cfg == NULL) {
        return EC800M_STATUS_INVALID_PARAM;
    }

    deviceObj = ec800mGetDevice(device);
    if (deviceObj == NULL) {
        return EC800M_STATUS_INVALID_PARAM;
    }

    *cfg = deviceObj->cfg;
    return EC800M_STATUS_OK;
}

eEc800mStatus ec800mSetCfg(eEc800mMapType device, const stEc800mCfg *cfg)
{
    stEc800mDevice *deviceObj;

    if ((cfg == NULL) || !ec800mPlatformIsValidCfg(cfg)) {
        return EC800M_STATUS_INVALID_PARAM;
    }

    deviceObj = ec800mGetDevice(device);
    if (deviceObj == NULL) {
        return EC800M_STATUS_INVALID_PARAM;
    }

    deviceObj->cfg = *cfg;
    deviceObj->info.isInit = false;
    deviceObj->info.isReady = false;
    ec800mResetState(deviceObj);
    gEc800mDefCfgDone[device] = true;
    return EC800M_STATUS_OK;
}

eEc800mStatus ec800mInit(eEc800mMapType device)
{
    stEc800mDevice *deviceObj;
    const stEc800mTransportInterface *transport;
    const stEc800mControlInterface *control;
    stFlowParserStreamCfg streamCfg;
    eDrvStatus drvStatus;
    eFlowParserStrmSta streamStatus;

    deviceObj = ec800mGetDevice(device);
    if (deviceObj == NULL) {
        return EC800M_STATUS_INVALID_PARAM;
    }

    if (!ec800mPlatformIsValidCfg(&deviceObj->cfg)) {
        return EC800M_STATUS_INVALID_PARAM;
    }

    transport = ec800mGetPlatformTransportInterface(&deviceObj->cfg);
    if ((transport == NULL) || (transport->init == NULL) || (transport->write == NULL) ||
        (transport->getRxLen == NULL) || (transport->read == NULL) || (transport->getTickMs == NULL)) {
        return EC800M_STATUS_NOT_READY;
    }

    drvStatus = transport->init(deviceObj->cfg.linkId);
    if (drvStatus != DRV_STATUS_OK) {
        return ec800mMapDrvStatus(drvStatus);
    }

    control = ec800mGetPlatformControlInterface(device);
    if ((control != NULL) && (control->init != NULL)) {
        control->init(deviceObj->cfg.pwrkeyPin, deviceObj->cfg.resetPin);
    }

    (void)memset(&streamCfg, 0, sizeof(streamCfg));
    streamCfg.rxStorage = deviceObj->rxStorage;
    streamCfg.rxStorageSize = sizeof(deviceObj->rxStorage);
    streamCfg.lineBuf = deviceObj->lineBuf;
    streamCfg.lineBufSize = sizeof(deviceObj->lineBuf);
    streamCfg.pfSend = ec800mStreamSend;
    streamCfg.sendUserData = deviceObj;
    streamCfg.pfGetTickMs = ec800mStreamGetTickMs;
    streamCfg.tickUserData = deviceObj;
    streamCfg.pfUrcHandler = ec800mStreamDispatchUrc;
    streamCfg.urcUserData = deviceObj;
    streamCfg.pfIsUrc = ec800mStreamIsUrc;
    streamCfg.isUrcUserData = deviceObj;

    streamStatus = flowparserStreamInit(&deviceObj->stream, &streamCfg);
    if (streamStatus != FLOWPARSER_STREAM_OK) {
        return ec800mMapStreamStatus(streamStatus);
    }

    deviceObj->info.isInit = true;
    deviceObj->info.isReady = true;
    deviceObj->info.isBusy = false;
    deviceObj->info.hasLastResult = false;
    deviceObj->info.lastResult = FLOWPARSER_RESULT_OK;
    deviceObj->info.stage = FLOWPARSER_STAGE_IDLE;
    deviceObj->info.rxBytes = 0U;
    deviceObj->info.urcCount = 0U;
    ec800mResetState(deviceObj);
    return EC800M_STATUS_OK;
}

void ec800mReset(eEc800mMapType device)
{
    stEc800mDevice *deviceObj;

    deviceObj = ec800mGetDevice(device);
    if ((deviceObj == NULL) || !deviceObj->info.isInit) {
        return;
    }

    flowparserStreamReset(&deviceObj->stream);
    ec800mResetState(deviceObj);
}

eEc800mStatus ec800mStart(eEc800mMapType device, eEc800mServiceMode serviceMode)
{
    stEc800mDevice *deviceObj;
    const stEc800mControlInterface *control;

    if (!ec800mIsValidServiceMode(serviceMode)) {
        return EC800M_STATUS_INVALID_PARAM;
    }

    deviceObj = ec800mGetDevice(device);
    if ((deviceObj == NULL) || !deviceObj->info.isInit) {
        return EC800M_STATUS_NOT_READY;
    }

    control = ec800mGetPlatformControlInterface(device);
    if ((control == NULL) || (control->setPwrkeyLevel == NULL) || (control->setResetLevel == NULL)) {
        return EC800M_STATUS_UNSUPPORTED;
    }

    flowparserStreamReset(&deviceObj->stream);
    ec800mResetState(deviceObj);
    deviceObj->state.serviceMode = serviceMode;
    deviceObj->state.runState = EC800M_RUN_BOOTING;
    deviceObj->state.isBusy = true;
    deviceObj->ctrlPlane.stage = EC800M_CTRL_STAGE_ASSERT_RESET_AND_PWRKEY;
    deviceObj->ctrlPlane.nextActionTick = 0U;
    LOG_I(EC800M_LOG_TAG, "start service=%u", (unsigned int)serviceMode);
    return EC800M_STATUS_OK;
}

void ec800mStop(eEc800mMapType device)
{
    stEc800mDevice *deviceObj;

    deviceObj = ec800mGetDevice(device);
    if (deviceObj == NULL) {
        return;
    }

    flowparserStreamReset(&deviceObj->stream);
    ec800mResetState(deviceObj);
}

eEc800mStatus ec800mProcess(eEc800mMapType device, uint32_t nowTickMs)
{
    stEc800mDevice *deviceObj;
    eEc800mStatus status;

    deviceObj = ec800mGetDevice(device);
    if ((deviceObj == NULL) || !deviceObj->info.isInit) {
        return EC800M_STATUS_NOT_READY;
    }

    status = ec800mBackGround(deviceObj);
    if (status != EC800M_STATUS_OK) {
        deviceObj->state.lastError = status;
        deviceObj->state.runState = EC800M_RUN_ERROR;
        ec800mSyncState(deviceObj);
        return status;
    }

    status = ec800mProcessCtrl(deviceObj, device, nowTickMs);
    ec800mSyncState(deviceObj);
    return status;
}

bool ec800mIsReady(eEc800mMapType device)
{
    stEc800mDevice *deviceObj = ec800mGetDevice(device);
    return (deviceObj != NULL) && deviceObj->state.isReady;
}

const stEc800mInfo *ec800mGetInfo(eEc800mMapType device)
{
    stEc800mDevice *deviceObj = ec800mGetDevice(device);
    return (deviceObj == NULL) ? NULL : &deviceObj->info;
}

const stEc800mState *ec800mGetState(eEc800mMapType device)
{
    stEc800mDevice *deviceObj = ec800mGetDevice(device);
    return (deviceObj == NULL) ? NULL : &deviceObj->state;
}

eEc800mStatus ec800mSubmitTextCommand(eEc800mMapType device, const char *cmdText)
{
    return ec800mSubmitTextCommandEx(device, cmdText, NULL, NULL);
}

eEc800mStatus ec800mSubmitTextCommandEx(eEc800mMapType device, const char *cmdText, ec800mLineFunc lineHandler, void *userData)
{
    stEc800mDevice *deviceObj;

    deviceObj = ec800mGetDevice(device);
    if ((deviceObj == NULL) || !deviceObj->state.isReady) {
        return EC800M_STATUS_NOT_READY;
    }

    return ec800mSubmitTextInternal(deviceObj, cmdText, lineHandler, userData, EC800M_CTRL_TXN_USER_TEXT);
}

eEc800mStatus ec800mSubmitPromptCommandEx(eEc800mMapType device, const char *cmdText, const uint8_t *payloadBuf, uint16_t payloadLen, ec800mLineFunc lineHandler, void *userData)
{
    stEc800mDevice *deviceObj;

    deviceObj = ec800mGetDevice(device);
    if ((deviceObj == NULL) || !deviceObj->state.isReady) {
        return EC800M_STATUS_NOT_READY;
    }

    return ec800mSubmitPromptInternal(deviceObj, cmdText, payloadBuf, payloadLen, lineHandler, userData);
}

eEc800mStatus ec800mSetUrcHandler(eEc800mMapType device, ec800mLineFunc handler, void *userData)
{
    stEc800mDevice *deviceObj;

    deviceObj = ec800mGetDevice(device);
    if (deviceObj == NULL) {
        return EC800M_STATUS_INVALID_PARAM;
    }

    deviceObj->urcCb.pfHandler = handler;
    deviceObj->urcCb.handlerUserData = userData;
    return EC800M_STATUS_OK;
}

eEc800mStatus ec800mSetUrcMatcher(eEc800mMapType device, ec800mUrcMatchFunc matcher, void *userData)
{
    stEc800mDevice *deviceObj;

    deviceObj = ec800mGetDevice(device);
    if (deviceObj == NULL) {
        return EC800M_STATUS_INVALID_PARAM;
    }

    deviceObj->urcCb.pfMatcher = matcher;
    deviceObj->urcCb.matcherUserData = userData;
    return EC800M_STATUS_OK;
}

static bool ec800mIsValidDevice(eEc800mMapType device)
{
    return device < EC800M_DEV_MAX;
}

static bool ec800mIsValidServiceMode(eEc800mServiceMode serviceMode)
{
    return (serviceMode > EC800M_SERVICE_NONE) && (serviceMode < EC800M_SERVICE_MAX);
}

static stEc800mDevice *ec800mGetDevice(eEc800mMapType device)
{
    if (!ec800mIsValidDevice(device)) {
        return NULL;
    }

    if (!gEc800mDefCfgDone[device]) {
        ec800mLoadDefCfg(device, &gEc800mDevices[device].cfg);
        ec800mResetState(&gEc800mDevices[device]);
        gEc800mDefCfgDone[device] = true;
    }

    return &gEc800mDevices[device];
}

static void ec800mLoadDefCfg(eEc800mMapType device, stEc800mCfg *cfg)
{
    if (cfg == NULL) {
        return;
    }

    (void)memset(cfg, 0, sizeof(*cfg));
    ec800mLoadPlatformDefaultCfg(device, cfg);
}

static void ec800mResetState(stEc800mDevice *device)
{
    if (device == NULL) {
        return;
    }

    (void)memset(&device->state, 0, sizeof(device->state));
    device->state.runState = device->info.isInit ? EC800M_RUN_IDLE : EC800M_RUN_UNINIT;
    device->state.lastError = EC800M_STATUS_OK;
    device->ctrlPlane.nextActionTick = 0U;
    device->ctrlPlane.readyDeadlineTick = 0U;
    device->ctrlPlane.stage = EC800M_CTRL_STAGE_IDLE;
    device->ctrlPlane.txnKind = EC800M_CTRL_TXN_NONE;
    device->ctrlPlane.isTxnDone = false;
    device->ctrlPlane.txnStatus = EC800M_STATUS_OK;
    device->ctrlPlane.userLineHandler = NULL;
    device->ctrlPlane.userData = NULL;
    device->ctrlPlane.cmdBuf[0] = '\0';
}

static void ec800mSyncInfo(stEc800mDevice *device)
{
    if (device == NULL) {
        return;
    }

    device->info.stage = flowparserStreamGetStage(&device->stream);
    device->info.isBusy = flowparserStreamIsBusy(&device->stream);
}

static void ec800mSyncState(stEc800mDevice *device)
{
    if (device == NULL) {
        return;
    }

    ec800mSyncInfo(device);
    device->state.isBusy = device->info.isBusy ||
                           ((device->ctrlPlane.stage != EC800M_CTRL_STAGE_IDLE) &&
                            (device->ctrlPlane.stage != EC800M_CTRL_STAGE_RUNNING));
}

static eEc800mStatus ec800mMapDrvStatus(eDrvStatus status)
{
    switch (status) {
        case DRV_STATUS_OK:
            return EC800M_STATUS_OK;
        case DRV_STATUS_INVALID_PARAM:
            return EC800M_STATUS_INVALID_PARAM;
        case DRV_STATUS_NOT_READY:
            return EC800M_STATUS_NOT_READY;
        case DRV_STATUS_BUSY:
            return EC800M_STATUS_BUSY;
        case DRV_STATUS_TIMEOUT:
            return EC800M_STATUS_TIMEOUT;
        case DRV_STATUS_UNSUPPORTED:
            return EC800M_STATUS_UNSUPPORTED;
        default:
            return EC800M_STATUS_ERROR;
    }
}

static eEc800mStatus ec800mMapStreamStatus(eFlowParserStrmSta status)
{
    switch (status) {
        case FLOWPARSER_STREAM_OK:
        case FLOWPARSER_STREAM_EMPTY:
            return EC800M_STATUS_OK;
        case FLOWPARSER_STREAM_BUSY:
            return EC800M_STATUS_BUSY;
        case FLOWPARSER_STREAM_INVALID_PARAM:
            return EC800M_STATUS_INVALID_PARAM;
        case FLOWPARSER_STREAM_NOT_INIT:
            return EC800M_STATUS_NOT_READY;
        case FLOWPARSER_STREAM_OVERFLOW:
            return EC800M_STATUS_OVERFLOW;
        case FLOWPARSER_STREAM_TIMEOUT:
            return EC800M_STATUS_TIMEOUT;
        default:
            return EC800M_STATUS_STREAM_FAIL;
    }
}

static eEc800mStatus ec800mMapResult(eFlowParserResult result)
{
    switch (result) {
        case FLOWPARSER_RESULT_OK:
            return EC800M_STATUS_OK;
        case FLOWPARSER_RESULT_TIMEOUT:
            return EC800M_STATUS_TIMEOUT;
        case FLOWPARSER_RESULT_OVERFLOW:
            return EC800M_STATUS_OVERFLOW;
        case FLOWPARSER_RESULT_SEND_FAIL:
            return EC800M_STATUS_STREAM_FAIL;
        default:
            return EC800M_STATUS_ERROR;
    }
}

static eEc800mStatus ec800mBackGround(stEc800mDevice *device)
{
    eEc800mStatus status;
    eFlowParserStrmSta streamStatus;

    status = ec800mPollTransport(device);
    if (status != EC800M_STATUS_OK) {
        return status;
    }

    streamStatus = flowparserStreamProc(&device->stream);
    if ((streamStatus != FLOWPARSER_STREAM_OK) && (streamStatus != FLOWPARSER_STREAM_EMPTY)) {
        return ec800mMapStreamStatus(streamStatus);
    }

    return EC800M_STATUS_OK;
}

static eEc800mStatus ec800mPollTransport(stEc800mDevice *device)
{
    const stEc800mTransportInterface *transport;
    uint8_t tempBuf[EC800M_RX_POLL_CHUNK_SIZE];
    uint16_t availableLen;
    uint16_t readLen;
    eDrvStatus drvStatus;
    eFlowParserStrmSta streamStatus;

    if (device == NULL) {
        return EC800M_STATUS_INVALID_PARAM;
    }

    transport = ec800mGetPlatformTransportInterface(&device->cfg);
    if (transport == NULL) {
        return EC800M_STATUS_NOT_READY;
    }

    availableLen = transport->getRxLen(device->cfg.linkId);
    while (availableLen > 0U) {
        readLen = availableLen;
        if (readLen > device->cfg.rxPollChunkSize) {
            readLen = device->cfg.rxPollChunkSize;
        }
        if (readLen > sizeof(tempBuf)) {
            readLen = sizeof(tempBuf);
        }

        drvStatus = transport->read(device->cfg.linkId, tempBuf, readLen);
        if (drvStatus != DRV_STATUS_OK) {
            return ec800mMapDrvStatus(drvStatus);
        }

        streamStatus = flowparserStreamFeed(&device->stream, tempBuf, readLen);
        if (streamStatus != FLOWPARSER_STREAM_OK) {
            return ec800mMapStreamStatus(streamStatus);
        }

        device->info.rxBytes += readLen;
        availableLen = transport->getRxLen(device->cfg.linkId);
    }

    return EC800M_STATUS_OK;
}

static eEc800mStatus ec800mProcessCtrl(stEc800mDevice *device, eEc800mMapType deviceId, uint32_t nowTickMs)
{
    const stEc800mControlInterface *control;

    if ((device == NULL) || (device->ctrlPlane.stage == EC800M_CTRL_STAGE_IDLE) ||
        (device->ctrlPlane.stage == EC800M_CTRL_STAGE_RUNNING)) {
        return EC800M_STATUS_OK;
    }

    if (device->ctrlPlane.isTxnDone) {
        return ec800mProcessCtrlDone(device, deviceId, nowTickMs);
    }

    if ((device->ctrlPlane.nextActionTick != 0U) &&
        ((uint32_t)(nowTickMs - device->ctrlPlane.nextActionTick) > 0x7FFFFFFFUL)) {
        return EC800M_STATUS_OK;
    }

    control = ec800mGetPlatformControlInterface(deviceId);
    if (control == NULL) {
        return EC800M_STATUS_UNSUPPORTED;
    }

    switch (device->ctrlPlane.stage) {
        case EC800M_CTRL_STAGE_ASSERT_RESET_AND_PWRKEY:
            control->setResetLevel(device->cfg.resetPin, true);
            control->setPwrkeyLevel(device->cfg.pwrkeyPin, true);
            device->ctrlPlane.nextActionTick = nowTickMs + device->cfg.resetPulseMs;
            device->ctrlPlane.stage = EC800M_CTRL_STAGE_RELEASE_RESET;
            break;
        case EC800M_CTRL_STAGE_RELEASE_RESET:
            control->setResetLevel(device->cfg.resetPin, false);
            device->ctrlPlane.nextActionTick = nowTickMs + device->cfg.resetWaitMs;
            device->ctrlPlane.stage = EC800M_CTRL_STAGE_ASSERT_RESET_AGAIN;
            break;
        case EC800M_CTRL_STAGE_ASSERT_RESET_AGAIN:
            control->setResetLevel(device->cfg.resetPin, true);
            device->ctrlPlane.nextActionTick = nowTickMs + device->cfg.resetPulseMs;
            device->ctrlPlane.stage = EC800M_CTRL_STAGE_RELEASE_PWRKEY;
            break;
        case EC800M_CTRL_STAGE_RELEASE_PWRKEY:
            control->setPwrkeyLevel(device->cfg.pwrkeyPin, false);
            device->ctrlPlane.nextActionTick = nowTickMs + device->cfg.pwrkeyPulseMs;
            device->ctrlPlane.stage = EC800M_CTRL_STAGE_ASSERT_PWRKEY;
            break;
        case EC800M_CTRL_STAGE_ASSERT_PWRKEY:
            control->setPwrkeyLevel(device->cfg.pwrkeyPin, true);
            device->ctrlPlane.readyDeadlineTick = nowTickMs + device->cfg.readyTimeoutMs;
            device->ctrlPlane.nextActionTick = nowTickMs + device->cfg.bootWaitMs;
            device->ctrlPlane.stage = EC800M_CTRL_STAGE_WAIT_AT;
            break;
        case EC800M_CTRL_STAGE_WAIT_AT:
            return ec800mSubmitCtrlText(device, "AT\r\n");
        case EC800M_CTRL_STAGE_DISABLE_ECHO:
            return ec800mSubmitCtrlText(device, "ATE0\r\n");
        case EC800M_CTRL_STAGE_QUERY_CPIN:
            return ec800mSubmitCtrlText(device, "AT+CPIN?\r\n");
        case EC800M_CTRL_STAGE_QUERY_CSQ:
            return ec800mSubmitCtrlText(device, "AT+CSQ\r\n");
        default:
            break;
    }

    return EC800M_STATUS_OK;
}

static eEc800mStatus ec800mProcessCtrlDone(stEc800mDevice *device, eEc800mMapType deviceId, uint32_t nowTickMs)
{
    (void)deviceId;

    if (device->ctrlPlane.txnKind != EC800M_CTRL_TXN_STAGE) {
        device->ctrlPlane.isTxnDone = false;
        device->ctrlPlane.txnKind = EC800M_CTRL_TXN_NONE;
        return EC800M_STATUS_OK;
    }

    device->ctrlPlane.isTxnDone = false;
    device->ctrlPlane.txnKind = EC800M_CTRL_TXN_NONE;
    if (device->ctrlPlane.txnStatus != EC800M_STATUS_OK) {
        if ((device->ctrlPlane.stage == EC800M_CTRL_STAGE_WAIT_AT) && (nowTickMs < device->ctrlPlane.readyDeadlineTick)) {
            device->ctrlPlane.nextActionTick = nowTickMs + device->cfg.retryIntervalMs;
            return EC800M_STATUS_OK;
        }
        device->state.lastError = device->ctrlPlane.txnStatus;
        device->state.runState = EC800M_RUN_ERROR;
        return device->ctrlPlane.txnStatus;
    }

    switch (device->ctrlPlane.stage) {
        case EC800M_CTRL_STAGE_WAIT_AT:
            device->state.isAtReady = true;
            device->state.runState = EC800M_RUN_CONFIGURING;
            device->ctrlPlane.stage = EC800M_CTRL_STAGE_DISABLE_ECHO;
            break;
        case EC800M_CTRL_STAGE_DISABLE_ECHO:
            device->state.isEchoDisabled = true;
            device->ctrlPlane.stage = EC800M_CTRL_STAGE_QUERY_CPIN;
            break;
        case EC800M_CTRL_STAGE_QUERY_CPIN:
            device->state.isSimChecked = true;
            device->ctrlPlane.stage = EC800M_CTRL_STAGE_QUERY_CSQ;
            break;
        case EC800M_CTRL_STAGE_QUERY_CSQ:
            device->state.isSignalChecked = true;
            device->state.isReady = true;
            device->state.runState = EC800M_RUN_READY;
            device->ctrlPlane.stage = EC800M_CTRL_STAGE_RUNNING;
            LOG_I(EC800M_LOG_TAG, "ready");
            break;
        default:
            break;
    }

    device->ctrlPlane.nextActionTick = nowTickMs;
    return EC800M_STATUS_OK;
}

static eEc800mStatus ec800mSubmitCtrlText(stEc800mDevice *device, const char *cmdText)
{
    return ec800mSubmitTextInternal(device, cmdText, ec800mTxnLineThunk, device, EC800M_CTRL_TXN_STAGE);
}

static eEc800mStatus ec800mSubmitTextInternal(stEc800mDevice *device, const char *cmdText, ec800mLineFunc lineHandler, void *userData, eEc800mCtrlTxnKind txnKind)
{
    stFlowParserReq req;
    uint16_t cmdLen;
    eFlowParserStrmSta streamStatus;

    if ((device == NULL) || (cmdText == NULL)) {
        return EC800M_STATUS_INVALID_PARAM;
    }

    cmdLen = (uint16_t)strlen(cmdText);
    if ((cmdLen == 0U) || (cmdLen >= sizeof(device->ctrlPlane.cmdBuf))) {
        return EC800M_STATUS_INVALID_PARAM;
    }

    if (flowparserStreamIsBusy(&device->stream)) {
        return EC800M_STATUS_BUSY;
    }

    (void)memcpy(device->ctrlPlane.cmdBuf, cmdText, cmdLen + 1U);
    device->ctrlPlane.userLineHandler = lineHandler;
    device->ctrlPlane.userData = userData;
    device->ctrlPlane.txnKind = txnKind;
    device->ctrlPlane.isTxnDone = false;
    device->ctrlPlane.txnStatus = EC800M_STATUS_OK;

    (void)memset(&req, 0, sizeof(req));
    req.spec = &gEc800mCommonSpec;
    req.cmdBuf = (const uint8_t *)device->ctrlPlane.cmdBuf;
    req.cmdLen = cmdLen;
    req.lineHandler = ec800mTxnLineThunk;
    req.doneHandler = ec800mTxnDoneThunk;
    req.userData = device;
    streamStatus = flowparserStreamSubmit(&device->stream, &req);
    return ec800mMapStreamStatus(streamStatus);
}

static eEc800mStatus ec800mSubmitPromptInternal(stEc800mDevice *device, const char *cmdText, const uint8_t *payloadBuf, uint16_t payloadLen, ec800mLineFunc lineHandler, void *userData)
{
    stFlowParserReq req;
    uint16_t cmdLen;
    eFlowParserStrmSta streamStatus;

    if ((device == NULL) || (cmdText == NULL) || (payloadBuf == NULL) || (payloadLen == 0U)) {
        return EC800M_STATUS_INVALID_PARAM;
    }

    cmdLen = (uint16_t)strlen(cmdText);
    if ((cmdLen == 0U) || (cmdLen >= sizeof(device->ctrlPlane.cmdBuf))) {
        return EC800M_STATUS_INVALID_PARAM;
    }

    if (flowparserStreamIsBusy(&device->stream)) {
        return EC800M_STATUS_BUSY;
    }

    (void)memcpy(device->ctrlPlane.cmdBuf, cmdText, cmdLen + 1U);
    device->ctrlPlane.userLineHandler = lineHandler;
    device->ctrlPlane.userData = userData;
    device->ctrlPlane.txnKind = EC800M_CTRL_TXN_USER_PROMPT;
    device->ctrlPlane.isTxnDone = false;
    device->ctrlPlane.txnStatus = EC800M_STATUS_OK;

    (void)memset(&req, 0, sizeof(req));
    req.spec = &gEc800mPromptSpec;
    req.cmdBuf = (const uint8_t *)device->ctrlPlane.cmdBuf;
    req.cmdLen = cmdLen;
    req.payloadBuf = payloadBuf;
    req.payloadLen = payloadLen;
    req.lineHandler = ec800mTxnLineThunk;
    req.doneHandler = ec800mTxnDoneThunk;
    req.userData = device;
    streamStatus = flowparserStreamSubmit(&device->stream, &req);
    return ec800mMapStreamStatus(streamStatus);
}

static bool ec800mMatchPrefix(const uint8_t *lineBuf, uint16_t lineLen, const char *prefix)
{
    uint16_t prefixLen;

    if ((lineBuf == NULL) || (prefix == NULL)) {
        return false;
    }

    prefixLen = (uint16_t)strlen(prefix);
    return (lineLen >= prefixLen) && (memcmp(lineBuf, prefix, prefixLen) == 0);
}

static bool ec800mIsDefaultUrc(const uint8_t *lineBuf, uint16_t lineLen)
{
    uint8_t index;

    for (index = 0U; index < (uint8_t)(sizeof(gEc800mDefaultUrcPrefixes) / sizeof(gEc800mDefaultUrcPrefixes[0])); index++) {
        if (ec800mMatchPrefix(lineBuf, lineLen, gEc800mDefaultUrcPrefixes[index])) {
            return true;
        }
    }

    return false;
}

static eFlowParserStrmSta ec800mStreamSend(void *userData, const uint8_t *buf, uint16_t len)
{
    stEc800mDevice *device = (stEc800mDevice *)userData;
    const stEc800mTransportInterface *transport;
    eDrvStatus status;

    if ((device == NULL) || (buf == NULL) || (len == 0U)) {
        return FLOWPARSER_STREAM_INVALID_PARAM;
    }

    transport = ec800mGetPlatformTransportInterface(&device->cfg);
    if ((transport == NULL) || (transport->write == NULL)) {
        return FLOWPARSER_STREAM_PORT_FAIL;
    }

    status = transport->write(device->cfg.linkId, buf, len, device->cfg.txTimeoutMs);
    return (status == DRV_STATUS_OK) ? FLOWPARSER_STREAM_OK : FLOWPARSER_STREAM_PORT_FAIL;
}

static uint32_t ec800mStreamGetTickMs(void *userData)
{
    stEc800mDevice *device = (stEc800mDevice *)userData;
    const stEc800mTransportInterface *transport;

    if (device == NULL) {
        return 0U;
    }

    transport = ec800mGetPlatformTransportInterface(&device->cfg);
    if ((transport == NULL) || (transport->getTickMs == NULL)) {
        return 0U;
    }

    return transport->getTickMs();
}

static bool ec800mStreamIsUrc(void *userData, const uint8_t *lineBuf, uint16_t lineLen)
{
    stEc800mDevice *device = (stEc800mDevice *)userData;

    if ((device != NULL) && (device->urcCb.pfMatcher != NULL) &&
        device->urcCb.pfMatcher(device->urcCb.matcherUserData, lineBuf, lineLen)) {
        return true;
    }

    return ec800mIsDefaultUrc(lineBuf, lineLen);
}

static void ec800mStreamDispatchUrc(void *userData, const uint8_t *lineBuf, uint16_t lineLen)
{
    stEc800mDevice *device = (stEc800mDevice *)userData;

    if (device == NULL) {
        return;
    }

    device->info.urcCount++;
    if (device->urcCb.pfHandler != NULL) {
        device->urcCb.pfHandler(device->urcCb.handlerUserData, lineBuf, lineLen);
    }
}

static void ec800mTxnLineThunk(void *userData, const uint8_t *lineBuf, uint16_t lineLen)
{
    stEc800mDevice *device = (stEc800mDevice *)userData;

    if ((device != NULL) && (device->ctrlPlane.userLineHandler != NULL)) {
        device->ctrlPlane.userLineHandler(device->ctrlPlane.userData, lineBuf, lineLen);
    }
}

static void ec800mTxnDoneThunk(void *userData, eFlowParserResult result)
{
    stEc800mDevice *device = (stEc800mDevice *)userData;

    if (device == NULL) {
        return;
    }

    device->info.hasLastResult = true;
    device->info.lastResult = result;
    device->ctrlPlane.txnStatus = ec800mMapResult(result);
    device->ctrlPlane.isTxnDone = true;
}

/**************************End of file********************************/
