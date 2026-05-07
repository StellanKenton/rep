/***********************************************************************************
* @file     : wt2003hx.c
* @brief    : WT2003HX audio module implementation.
* @details  : Builds UART command frames and uses frameparser to consume replies.
* @author   : 
* @date     : 2026-04-30
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "wt2003hx.h"

#include <stddef.h>
#include <string.h>

#include "wt2003hx_assembly.h"

static stWt2003hxDevice gWt2003hxDevices[WT2003HX_DEV_MAX];
static const uint8_t gWt2003hxFrameHead[] = {WT2003HX_FRAME_HEAD};

static bool wt2003hxIsValidDevice(eWt2003hxMapType device);
static bool wt2003hxIsValidCfg(const stWt2003hxCfg *cfg);
static stWt2003hxDevice *wt2003hxGetDevice(eWt2003hxMapType device);
static uint8_t wt2003hxChecksum(const uint8_t *buffer, uint8_t length);
static eWt2003hxStatus wt2003hxSendCmd(eWt2003hxMapType device, uint8_t cmd, const uint8_t *param, uint8_t paramLen);
static uint32_t wt2003hxProtocolHeadLen(const uint8_t *buf, uint32_t availLen, void *userCtx);
static uint32_t wt2003hxProtocolPktLen(const uint8_t *buf, uint32_t headLen, uint32_t availLen, void *userCtx);
static uint32_t wt2003hxProtocolChecksum(const uint8_t *buf, uint32_t len, void *userCtx);
static void wt2003hxHandleRawReply(stWt2003hxDevice *device, const uint8_t *buf, uint16_t len);
static void wt2003hxHandlePacket(stWt2003hxDevice *device, const stFrmPsrPkt *pkt);

__attribute__((weak)) void wt2003hxLoadPlatformDefaultCfg(eWt2003hxMapType device, stWt2003hxCfg *cfg)
{
    (void)device;

    if (cfg == NULL) {
        return;
    }

    (void)memset(cfg, 0, sizeof(*cfg));
    cfg->powerDelayMs = WT2003HX_POWER_DELAY_MS;
    cfg->txTimeoutMs = WT2003HX_TX_TIMEOUT_MS;
}

__attribute__((weak)) const stWt2003hxTransportInterface *wt2003hxGetPlatformTransportInterface(const stWt2003hxCfg *cfg)
{
    (void)cfg;
    return NULL;
}

__attribute__((weak)) const stWt2003hxControlInterface *wt2003hxGetPlatformControlInterface(eWt2003hxMapType device)
{
    (void)device;
    return NULL;
}

__attribute__((weak)) bool wt2003hxPlatformIsValidCfg(const stWt2003hxCfg *cfg)
{
    (void)cfg;
    return false;
}

void frmPsrLoadPlatformDefaultProtoCfg(uint32_t protocolId, stFrmPsrProtoCfg *protoCfg)
{
    if (protoCfg == NULL) {
        return;
    }

    (void)memset(protoCfg, 0, sizeof(*protoCfg));
    if (protocolId != WT2003HX_PROTOCOL_ID) {
        return;
    }

    protoCfg->headPatList[0] = gWt2003hxFrameHead;
    protoCfg->headPatCount = 1U;
    protoCfg->headPatLen = 1U;
    protoCfg->minHeadLen = 3U;
    protoCfg->minPktLen = WT2003HX_FRAME_MIN_LEN;
    protoCfg->maxPktLen = WT2003HX_FRAME_MAX_LEN;
    protoCfg->waitPktToutMs = 100U;
    protoCfg->crcRangeStartOff = 1;
    protoCfg->crcRangeEndOff = -3;
    protoCfg->crcFieldOff = -2;
    protoCfg->crcFieldLen = 1U;
    protoCfg->cmdindex = 2U;
    protoCfg->cmdLen = 1U;
    protoCfg->packlenindex = 1U;
    protoCfg->packlenLen = 1U;
    protoCfg->crcFieldEnd = FRM_PSR_CRC_END_LITTLE;
    protoCfg->headLenFunc = wt2003hxProtocolHeadLen;
    protoCfg->pktLenFunc = wt2003hxProtocolPktLen;
    protoCfg->crcCalcFunc = wt2003hxProtocolChecksum;
}

eWt2003hxStatus wt2003hxGetDefCfg(eWt2003hxMapType device, stWt2003hxCfg *cfg)
{
    if ((cfg == NULL) || !wt2003hxIsValidDevice(device)) {
        return WT2003HX_STATUS_INVALID_PARAM;
    }

    wt2003hxLoadPlatformDefaultCfg(device, cfg);
    return wt2003hxIsValidCfg(cfg) ? WT2003HX_STATUS_OK : WT2003HX_STATUS_INVALID_PARAM;
}

eWt2003hxStatus wt2003hxGetCfg(eWt2003hxMapType device, stWt2003hxCfg *cfg)
{
    stWt2003hxDevice *lDevice = wt2003hxGetDevice(device);

    if ((lDevice == NULL) || (cfg == NULL)) {
        return WT2003HX_STATUS_INVALID_PARAM;
    }

    *cfg = lDevice->cfg;
    return WT2003HX_STATUS_OK;
}

eWt2003hxStatus wt2003hxSetCfg(eWt2003hxMapType device, const stWt2003hxCfg *cfg)
{
    stWt2003hxDevice *lDevice = wt2003hxGetDevice(device);

    if ((lDevice == NULL) || !wt2003hxIsValidCfg(cfg)) {
        return WT2003HX_STATUS_INVALID_PARAM;
    }

    lDevice->cfg = *cfg;
    lDevice->isReady = false;
    lDevice->defCfgLoaded = true;
    return WT2003HX_STATUS_OK;
}

eWt2003hxStatus wt2003hxInit(eWt2003hxMapType device)
{
    stWt2003hxDevice *lDevice = wt2003hxGetDevice(device);
    const stWt2003hxTransportInterface *lTransport;
    const stWt2003hxControlInterface *lControl;
    stFrmPsrCfg lParserCfg;
    eWt2003hxStatus lStatus;

    if (lDevice == NULL) {
        return WT2003HX_STATUS_INVALID_PARAM;
    }

    if (!lDevice->defCfgLoaded) {
        lStatus = wt2003hxGetDefCfg(device, &lDevice->cfg);
        if (lStatus != WT2003HX_STATUS_OK) {
            return lStatus;
        }
        lDevice->defCfgLoaded = true;
    }

    if (!wt2003hxIsValidCfg(&lDevice->cfg)) {
        return WT2003HX_STATUS_INVALID_PARAM;
    }

    lTransport = wt2003hxGetPlatformTransportInterface(&lDevice->cfg);
    lControl = wt2003hxGetPlatformControlInterface(device);
    if ((lTransport == NULL) || (lTransport->init == NULL) || (lTransport->write == NULL) ||
        (lTransport->getRxLen == NULL) || (lTransport->read == NULL) || (lTransport->getTickMs == NULL) ||
        (lControl == NULL) || (lControl->setEnable == NULL) || (lControl->delayMs == NULL)) {
        return WT2003HX_STATUS_NOT_READY;
    }

    lDevice->isReady = false;
    (void)memset(&lDevice->info, 0, sizeof(lDevice->info));
    lDevice->info.playState = WT2003HX_PLAY_STATE_UNKNOWN;

    lControl->setEnable(lDevice->cfg.enablePin, false);
    lControl->delayMs(lDevice->cfg.powerDelayMs);
    lStatus = lTransport->init(lDevice->cfg.linkId);
    if (lStatus != WT2003HX_STATUS_OK) {
        return lStatus;
    }
    lControl->setEnable(lDevice->cfg.enablePin, true);
    lControl->delayMs(lDevice->cfg.powerDelayMs);

    lParserCfg.protocolId = WT2003HX_PROTOCOL_ID;
    lParserCfg.streamBuf = lDevice->streamBuf;
    lParserCfg.streamBufSize = (uint16_t)sizeof(lDevice->streamBuf);
    lParserCfg.frameBuf = lDevice->frameBuf;
    lParserCfg.frameBufSize = (uint16_t)sizeof(lDevice->frameBuf);
    if (frmPsrInit(&lDevice->parser, &lParserCfg) != FRM_PSR_OK) {
        return WT2003HX_STATUS_ERROR;
    }
    lDevice->parser.protoCfg.getTick = lTransport->getTickMs;

    lDevice->isReady = true;
    return WT2003HX_STATUS_OK;
}

bool wt2003hxIsReady(eWt2003hxMapType device)
{
    stWt2003hxDevice *lDevice = wt2003hxGetDevice(device);

    return (lDevice != NULL) && lDevice->isReady;
}

eWt2003hxStatus wt2003hxProcess(eWt2003hxMapType device)
{
    stWt2003hxDevice *lDevice = wt2003hxGetDevice(device);
    const stWt2003hxTransportInterface *lTransport;
    uint16_t lRxLen;
    uint16_t lReadLen;
    const stFrmPsrPkt *lPkt;

    if ((lDevice == NULL) || !lDevice->isReady) {
        return WT2003HX_STATUS_NOT_READY;
    }

    lTransport = wt2003hxGetPlatformTransportInterface(&lDevice->cfg);
    if ((lTransport == NULL) || (lTransport->getRxLen == NULL) || (lTransport->read == NULL)) {
        return WT2003HX_STATUS_NOT_READY;
    }

    lRxLen = lTransport->getRxLen(lDevice->cfg.linkId);
    while (lRxLen > 0U) {
        lReadLen = lRxLen;
        if (lReadLen > (uint16_t)sizeof(lDevice->rxTemp)) {
            lReadLen = (uint16_t)sizeof(lDevice->rxTemp);
        }
        if (lTransport->read(lDevice->cfg.linkId, lDevice->rxTemp, lReadLen) != WT2003HX_STATUS_OK) {
            return WT2003HX_STATUS_ERROR;
        }
        wt2003hxHandleRawReply(lDevice, lDevice->rxTemp, lReadLen);
        (void)frmPsrFeed(&lDevice->parser, lDevice->rxTemp, lReadLen);
        lRxLen = (uint16_t)(lRxLen - lReadLen);
    }

    while (frmPsrProcess(&lDevice->parser) == FRM_PSR_OK) {
        lPkt = frmPsrRelease(&lDevice->parser);
        if (lPkt == NULL) {
            break;
        }
        wt2003hxHandlePacket(lDevice, lPkt);
    }

    return WT2003HX_STATUS_OK;
}

eWt2003hxStatus wt2003hxPlayName(eWt2003hxMapType device, const uint8_t *name, uint8_t nameLen)
{
    if ((name == NULL) || (nameLen == 0U) || (nameLen > WT2003HX_PARAM_NAME_MAX_LEN)) {
        return WT2003HX_STATUS_INVALID_PARAM;
    }

    return wt2003hxSendCmd(device, WT2003HX_CMD_EXTFLASH_NAME_PLAY, name, nameLen);
}

eWt2003hxStatus wt2003hxPlayIndex(eWt2003hxMapType device, uint16_t index)
{
    uint8_t lParam[2];

    lParam[0] = (uint8_t)((index >> 8U) & 0xFFU);
    lParam[1] = (uint8_t)(index & 0xFFU);
    return wt2003hxSendCmd(device, WT2003HX_CMD_EXTFLASH_INDEX_PLAY, lParam, (uint8_t)sizeof(lParam));
}

eWt2003hxStatus wt2003hxStop(eWt2003hxMapType device)
{
    return wt2003hxSendCmd(device, WT2003HX_CMD_PLAY_STOP, NULL, 0U);
}

eWt2003hxStatus wt2003hxPause(eWt2003hxMapType device)
{
    return wt2003hxSendCmd(device, WT2003HX_CMD_PLAY_PAUSE, NULL, 0U);
}

eWt2003hxStatus wt2003hxSetVolume(eWt2003hxMapType device, uint8_t volume)
{
    if (volume > 31U) {
        return WT2003HX_STATUS_INVALID_PARAM;
    }

    return wt2003hxSendCmd(device, WT2003HX_CMD_VOLUME_SET, &volume, 1U);
}

eWt2003hxStatus wt2003hxSetPlayMode(eWt2003hxMapType device, eWt2003hxPlayMode mode)
{
    uint8_t lMode = (uint8_t)mode;

    if ((uint8_t)mode > (uint8_t)WT2003HX_PLAY_MODE_RANDOM) {
        return WT2003HX_STATUS_INVALID_PARAM;
    }

    return wt2003hxSendCmd(device, WT2003HX_CMD_PLAY_MODE, &lMode, 1U);
}

eWt2003hxStatus wt2003hxSetOutputMode(eWt2003hxMapType device, eWt2003hxOutputMode mode)
{
    uint8_t lMode = (uint8_t)mode;

    if ((mode != WT2003HX_OUTPUT_MODE_SPK) && (mode != WT2003HX_OUTPUT_MODE_DAC)) {
        return WT2003HX_STATUS_INVALID_PARAM;
    }

    return wt2003hxSendCmd(device, WT2003HX_CMD_OUTPUT_MODE_SWITCH, &lMode, 1U);
}

eWt2003hxStatus wt2003hxQuery(eWt2003hxMapType device, uint8_t cmd)
{
    switch (cmd) {
        case WT2003HX_CMD_CHECK_VERSION:
        case WT2003HX_CMD_CHECK_VOLUME_SET:
        case WT2003HX_CMD_CHECK_STATE:
        case WT2003HX_CMD_CHECK_MUSIC_NUM:
        case WT2003HX_CMD_CHECK_CONNECT_STATE:
            return wt2003hxSendCmd(device, cmd, NULL, 0U);
        default:
            return WT2003HX_STATUS_INVALID_PARAM;
    }
}

bool wt2003hxGetInfo(eWt2003hxMapType device, stWt2003hxInfo *info)
{
    stWt2003hxDevice *lDevice = wt2003hxGetDevice(device);

    if ((lDevice == NULL) || (info == NULL)) {
        return false;
    }

    *info = lDevice->info;
    return true;
}

static bool wt2003hxIsValidDevice(eWt2003hxMapType device)
{
    return ((uint32_t)device < (uint32_t)WT2003HX_DEV_MAX);
}

static bool wt2003hxIsValidCfg(const stWt2003hxCfg *cfg)
{
    return (cfg != NULL) && wt2003hxPlatformIsValidCfg(cfg);
}

static stWt2003hxDevice *wt2003hxGetDevice(eWt2003hxMapType device)
{
    if (!wt2003hxIsValidDevice(device)) {
        return NULL;
    }

    return &gWt2003hxDevices[device];
}

static uint8_t wt2003hxChecksum(const uint8_t *buffer, uint8_t length)
{
    uint8_t lIndex;
    uint8_t lChecksum = 0U;

    for (lIndex = 0U; lIndex < length; lIndex++) {
        lChecksum = (uint8_t)(lChecksum + buffer[lIndex]);
    }

    return lChecksum;
}

static eWt2003hxStatus wt2003hxSendCmd(eWt2003hxMapType device, uint8_t cmd, const uint8_t *param, uint8_t paramLen)
{
    stWt2003hxDevice *lDevice = wt2003hxGetDevice(device);
    const stWt2003hxTransportInterface *lTransport;
    uint8_t lFrame[WT2003HX_TX_BUF_SIZE];
    uint8_t lFrameLen;
    uint8_t lIndex;

    if ((lDevice == NULL) || !lDevice->isReady) {
        return WT2003HX_STATUS_NOT_READY;
    }

    if ((param == NULL) && (paramLen != 0U)) {
        return WT2003HX_STATUS_INVALID_PARAM;
    }

    lFrameLen = (uint8_t)(paramLen + 5U);
    if (lFrameLen > (uint8_t)sizeof(lFrame)) {
        return WT2003HX_STATUS_INVALID_PARAM;
    }

    lFrame[0] = WT2003HX_FRAME_HEAD;
    lFrame[1] = (uint8_t)(paramLen + 3U);
    lFrame[2] = cmd;
    for (lIndex = 0U; lIndex < paramLen; lIndex++) {
        lFrame[3U + lIndex] = param[lIndex];
    }
    lFrame[3U + paramLen] = wt2003hxChecksum(&lFrame[1], (uint8_t)(paramLen + 2U));
    lFrame[4U + paramLen] = WT2003HX_FRAME_TAIL;

    lTransport = wt2003hxGetPlatformTransportInterface(&lDevice->cfg);
    if ((lTransport == NULL) || (lTransport->write == NULL)) {
        return WT2003HX_STATUS_NOT_READY;
    }

    return lTransport->write(lDevice->cfg.linkId, lFrame, lFrameLen, lDevice->cfg.txTimeoutMs);
}

static uint32_t wt2003hxProtocolHeadLen(const uint8_t *buf, uint32_t availLen, void *userCtx)
{
    (void)buf;
    (void)userCtx;

    return (availLen >= 3U) ? 3U : 0U;
}

static uint32_t wt2003hxProtocolPktLen(const uint8_t *buf, uint32_t headLen, uint32_t availLen, void *userCtx)
{
    uint32_t lPktLen;

    (void)headLen;
    (void)userCtx;

    if ((buf == NULL) || (availLen < 2U)) {
        return 0U;
    }

    lPktLen = (uint32_t)buf[1] + 2U;
    if ((lPktLen < WT2003HX_FRAME_MIN_LEN) || (lPktLen > WT2003HX_FRAME_MAX_LEN)) {
        return 0U;
    }

    if ((availLen >= lPktLen) && (buf[lPktLen - 1U] != WT2003HX_FRAME_TAIL)) {
        return 0U;
    }

    return lPktLen;
}

static uint32_t wt2003hxProtocolChecksum(const uint8_t *buf, uint32_t len, void *userCtx)
{
    uint32_t lIndex;
    uint8_t lChecksum = 0U;

    (void)userCtx;

    if (buf == NULL) {
        return 0U;
    }

    for (lIndex = 0U; lIndex < len; lIndex++) {
        lChecksum = (uint8_t)(lChecksum + buf[lIndex]);
    }

    return lChecksum;
}

static void wt2003hxUpdateReplyTick(stWt2003hxDevice *device, uint8_t cmd)
{
    if (device == NULL) {
        return;
    }

    device->info.lastReplyCmd = cmd;
    if (device->parser.protoCfg.getTick != NULL) {
        device->info.lastReplyTick = device->parser.protoCfg.getTick();
    }
}

static void wt2003hxHandleRawReply(stWt2003hxDevice *device, const uint8_t *buf, uint16_t len)
{
    uint16_t lIndex;
    uint8_t lCmd;

    if ((device == NULL) || (buf == NULL) || (len == 0U)) {
        return;
    }

    for (lIndex = 0U; lIndex < len; lIndex++) {
        lCmd = buf[lIndex];
        switch (lCmd) {
            case WT2003HX_CMD_CHECK_VOLUME_SET:
                if ((uint16_t)(len - lIndex) >= 2U) {
                    wt2003hxUpdateReplyTick(device, lCmd);
                    device->info.volume = buf[lIndex + 1U];
                }
                break;
            case WT2003HX_CMD_CHECK_STATE:
                if ((uint16_t)(len - lIndex) >= 2U) {
                    wt2003hxUpdateReplyTick(device, lCmd);
                    device->info.playState = (eWt2003hxPlayState)buf[lIndex + 1U];
                }
                break;
            case WT2003HX_CMD_CHECK_MUSIC_NUM:
                if ((uint16_t)(len - lIndex) >= 3U) {
                    wt2003hxUpdateReplyTick(device, lCmd);
                    device->info.musicNum = (uint16_t)(((uint16_t)buf[lIndex + 1U] << 8U) | buf[lIndex + 2U]);
                }
                break;
            case WT2003HX_CMD_CHECK_CONNECT_STATE:
                if ((uint16_t)(len - lIndex) >= 2U) {
                    wt2003hxUpdateReplyTick(device, lCmd);
                    device->info.connectState = buf[lIndex + 1U];
                }
                break;
            default:
                break;
        }
    }
}

static void wt2003hxHandlePacket(stWt2003hxDevice *device, const stFrmPsrPkt *pkt)
{
    uint8_t lCmd;
    const uint8_t *lParam;
    uint16_t lParamLen;

    if ((device == NULL) || (pkt == NULL) || (pkt->buf == NULL) || (pkt->len < WT2003HX_FRAME_MIN_LEN)) {
        return;
    }

    lCmd = pkt->buf[2];
    lParam = (pkt->dataBuf != NULL) ? pkt->dataBuf : NULL;
    lParamLen = pkt->dataLen;
    wt2003hxUpdateReplyTick(device, lCmd);

    switch (lCmd) {
        case WT2003HX_CMD_CHECK_VERSION:
            device->info.versionLen = (lParamLen > sizeof(device->info.version)) ? (uint8_t)sizeof(device->info.version) : (uint8_t)lParamLen;
            if ((lParam != NULL) && (device->info.versionLen > 0U)) {
                (void)memcpy(device->info.version, lParam, device->info.versionLen);
            }
            break;
        case WT2003HX_CMD_CHECK_VOLUME_SET:
            if ((lParam != NULL) && (lParamLen >= 1U)) {
                device->info.volume = lParam[0];
            }
            break;
        case WT2003HX_CMD_CHECK_STATE:
            if ((lParam != NULL) && (lParamLen >= 1U)) {
                device->info.playState = (eWt2003hxPlayState)lParam[0];
            }
            break;
        case WT2003HX_CMD_CHECK_MUSIC_NUM:
            if ((lParam != NULL) && (lParamLen >= 2U)) {
                device->info.musicNum = (uint16_t)(((uint16_t)lParam[0] << 8U) | lParam[1]);
            } else if ((lParam != NULL) && (lParamLen == 1U)) {
                device->info.musicNum = lParam[0];
            }
            break;
        case WT2003HX_CMD_CHECK_CONNECT_STATE:
            if ((lParam != NULL) && (lParamLen >= 1U)) {
                device->info.connectState = lParam[0];
            }
            break;
        default:
            break;
    }
}

/**************************End of file********************************/
