/***********************************************************************************
* @file     : framepareser_port.c
* @brief    : Project port helpers for the stream packet parser.
* @details  : Provides the default millisecond tick source used by timeout logic.
* @author   : GitHub Copilot
* @date     : 2026-04-01
* @version  : V1.0.0
**********************************************************************************/
#include "framepareser_port.h"

#include <string.h>

#include "rep_config.h"
#include "Rep/drvlayer/drvuart/drvuart.h"

#if (REP_RTOS_SYSTEM == REP_RTOS_FREERTOS)
#include "FreeRTOS.h"
#include "task.h"
#endif

typedef struct stFrmPsrPortSlot {
    stFrmPsrFmt fmt;
    bool isUsed;
} stFrmPsrPortSlot;

static stFrmPsrPortSlot gFrmPsrPortSlots[FRAME_PROTOCOL_MAX];

static uint16_t frmPsrPortReadBe16(const uint8_t *buffer);
static uint16_t frmPsrPortCalcCrc16Ccitt(const uint8_t *buffer, uint32_t length);
static uint32_t frmPsrPortAppCommCalcCrc(const uint8_t *buffer, uint32_t length, void *userCtx);
static uint32_t frmPsrPortAppCommGetHeadLen(const uint8_t *buffer, uint32_t availLen, void *userCtx);
static uint32_t frmPsrPortAppCommGetPktLen(const uint8_t *buffer, uint32_t headLen, uint32_t availLen, void *userCtx);
static stRingBuffer *frmPsrPortGetDbgUartRxRingBuf(void *userCtx);
static bool frmPsrPortIsValidProtocol(eFrameParMapType protocol);

static const stFrmPsrProtoCfg gFrmPsrPortDefProtoCfg[FRAME_PROTOCOL_MAX] = {
    [FRAME_PROTOCOL0] = {
        .rxHeadPat = gFrmPsrAppCommHeadPat,
        .rxHeadPatLen = sizeof(gFrmPsrAppCommHeadPat),
        .txHeadPat = gFrmPsrAppSendHeadPat,
        .txHeadPatLen = sizeof(gFrmPsrAppSendHeadPat),
        .minHeadLen = FRM_PSR_APP_COMM_HEAD_LEN,
        .minPktLen = FRM_PSR_APP_COMM_MIN_PKT_LEN,
        .maxPktLen = FRM_PSR_APP_COMM_MAX_PKT_LEN,
        .waitPktToutMs = FRM_PSR_PORT_WAIT_PKT_TOUT_MS,
        .crcRangeStartOff = 3,
        .crcRangeEndOff = -3,
        .crcFieldOff = -2,
        .crcFieldLen = FRM_PSR_APP_COMM_CRC_LEN,
        .crcFieldEnd = FRM_PSR_CRC_END_BIG,
        .headLenFunc = frmPsrPortAppCommGetHeadLen,
        .pktLenFunc = frmPsrPortAppCommGetPktLen,
        .crcCalcFunc = frmPsrPortAppCommCalcCrc,
        .getTick = frmPsrPortGetTickMs,
        .getRingBuf = frmPsrPortGetDbgUartRxRingBuf,
        .ringBufUserCtx = NULL,
        .userCtx = NULL,
    },
    [FRAME_PROTOCOL1] = {
        .rxHeadPat = NULL,
        .rxHeadPatLen = 0U,
        .txHeadPat = NULL,
        .txHeadPatLen = 0U,
        .minHeadLen = 0U,
        .minPktLen = 0U,
        .maxPktLen = 0U,
        .waitPktToutMs = FRM_PSR_PORT_WAIT_PKT_TOUT_MS,
        .crcRangeStartOff = 0,
        .crcRangeEndOff = 0,
        .crcFieldOff = 0,
        .crcFieldLen = 0U,
        .crcFieldEnd = FRM_PSR_CRC_END_BIG,
        .headLenFunc = NULL,
        .pktLenFunc = NULL,
        .crcCalcFunc = NULL,
        .getTick = frmPsrPortGetTickMs,
        .getRingBuf = NULL,
        .ringBufUserCtx = NULL,
        .userCtx = NULL,
    },
};

static uint16_t frmPsrPortReadBe16(const uint8_t *buffer)
{
    return (uint16_t)(((uint16_t)buffer[0] << 8U) | (uint16_t)buffer[1]);
}

static uint16_t frmPsrPortCalcCrc16Ccitt(const uint8_t *buffer, uint32_t length)
{
    uint16_t lCrc = 0xFFFFU;
    uint32_t lIndex;
    uint8_t lBit;

    if ((buffer == NULL) && (length != 0U)) {
        return 0U;
    }

    for (lIndex = 0U; lIndex < length; lIndex++) {
        lCrc ^= (uint16_t)((uint16_t)buffer[lIndex] << 8U);
        for (lBit = 0U; lBit < 8U; lBit++) {
            if ((lCrc & 0x8000U) != 0U) {
                lCrc = (uint16_t)((lCrc << 1U) ^ 0x1021U);
            } else {
                lCrc <<= 1U;
            }
        }
    }

    return lCrc;
}

static uint32_t frmPsrPortAppCommCalcCrc(const uint8_t *buffer, uint32_t length, void *userCtx)
{
    (void)userCtx;
    return (uint32_t)frmPsrPortCalcCrc16Ccitt(buffer, length);
}

static uint32_t frmPsrPortAppCommGetHeadLen(const uint8_t *buffer, uint32_t availLen, void *userCtx)
{
    (void)buffer;
    (void)userCtx;

    if (availLen < FRM_PSR_APP_COMM_HEAD_LEN) {
        return 0U;
    }

    return FRM_PSR_APP_COMM_HEAD_LEN;
}

static uint32_t frmPsrPortAppCommGetPktLen(const uint8_t *buffer, uint32_t headLen, uint32_t availLen, void *userCtx)
{
    uint32_t lPayloadLen;
    uint32_t lPktLen;

    (void)userCtx;

    if ((buffer == NULL) || (headLen < FRM_PSR_APP_COMM_HEAD_LEN) || (availLen < FRM_PSR_APP_COMM_HEAD_LEN)) {
        return 0U;
    }

    lPayloadLen = (uint32_t)frmPsrPortReadBe16(&buffer[4]);
    lPktLen = FRM_PSR_APP_COMM_HEAD_LEN + lPayloadLen + FRM_PSR_APP_COMM_CRC_LEN;
    if ((lPktLen < FRM_PSR_APP_COMM_MIN_PKT_LEN) || (lPktLen > FRM_PSR_APP_COMM_MAX_PKT_LEN)) {
        return 0U;
    }

    return lPktLen;
}

static stRingBuffer *frmPsrPortGetDbgUartRxRingBuf(void *userCtx)
{
    (void)userCtx;
    return drvUartGetRingBuffer(DRVUART_WIRELESS);
}

static bool frmPsrPortIsValidProtocol(eFrameParMapType protocol)
{
    return ((uint32_t)protocol < (uint32_t)FRAME_PROTOCOL_MAX);
}

uint32_t frmPsrPortGetTickMs(void)
{
#if (REP_RTOS_SYSTEM == REP_RTOS_FREERTOS)
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
#else
    return 0U;
#endif
}

void frmPsrPortApplyDftCfg(stFrmPsrCfg *cfg)
{
    if (cfg == NULL) {
        return;
    }

    if (cfg->minHeadLen == 0U) {
        cfg->minHeadLen = cfg->headPatLen;
    }

    if (cfg->waitPktToutMs == 0U) {
        cfg->waitPktToutMs = FRM_PSR_PORT_WAIT_PKT_TOUT_MS;
    }

    if (cfg->getTick == NULL) {
        cfg->getTick = frmPsrPortGetTickMs;
    }
}

void frmPsrPortApplyDftRunCfg(stFrmPsrRunCfg *runCfg)
{
    if (runCfg == NULL) {
        return;
    }

    if (runCfg->waitPktToutMs == 0U) {
        runCfg->waitPktToutMs = FRM_PSR_PORT_WAIT_PKT_TOUT_MS;
    }

    if (runCfg->getTick == NULL) {
        runCfg->getTick = frmPsrPortGetTickMs;
    }
}

void frmPsrPortGetDefProtoCfg(eFrameParMapType protocol, stFrmPsrProtoCfg *protoCfg)
{
    if ((protoCfg == NULL) || (!frmPsrPortIsValidProtocol(protocol))) {
        return;
    }

    *protoCfg = gFrmPsrPortDefProtoCfg[protocol];
}

eFrmPsrSta frmPsrPortInitByProto(stFrmPsr *psr, const stFrmPsrProtoCfg *protoCfg, stRingBuffer *ringBuf, uint8_t *outBuf, uint16_t outBufSize)
{
    if (protoCfg == NULL) {
        return FRM_PSR_INVALID_ARG;
    }

    return frmPsrInitByProtoCfg(psr, protoCfg, ringBuf, outBuf, outBufSize);
}

uint32_t frmPsrPortGetFmtCnt(void)
{
    return (uint32_t)FRAME_PROTOCOL_MAX;
}

bool frmPsrPortSetFmt(eFrameParMapType protocol, const stFrmPsrFmt *fmt)
{
    if ((!frmPsrPortIsValidProtocol(protocol)) || (!frmPsrIsFmtValid(fmt))) {
        return false;
    }

    gFrmPsrPortSlots[protocol].fmt = *fmt;
    gFrmPsrPortSlots[protocol].isUsed = true;
    return true;
}

const stFrmPsrFmt *frmPsrPortGetFmt(eFrameParMapType protocol)
{
    if ((!frmPsrPortIsValidProtocol(protocol)) || (!gFrmPsrPortSlots[protocol].isUsed)) {
        return NULL;
    }

    return &gFrmPsrPortSlots[protocol].fmt;
}

eFrmPsrSta frmPsrPortInit(stFrmPsr *psr, stRingBuffer *ringBuf, stFrmPsrCfg *cfg)
{
    if (cfg == NULL) {
        return FRM_PSR_INVALID_ARG;
    }

    frmPsrPortApplyDftCfg(cfg);
    return frmPsrInit(psr, ringBuf, cfg);
}

eFrmPsrSta frmPsrPortInitFmt(stFrmPsr *psr, stRingBuffer *ringBuf, eFrameParMapType protocol, stFrmPsrRunCfg *runCfg)
{
    const stFrmPsrFmt *lFmt;

    if (runCfg == NULL) {
        return FRM_PSR_INVALID_ARG;
    }

    lFmt = frmPsrPortGetFmt(protocol);
    if (lFmt == NULL) {
        return FRM_PSR_FMT_INVALID;
    }

    frmPsrPortApplyDftRunCfg(runCfg);
    return frmPsrInitFmt(psr, ringBuf, lFmt, runCfg);
}

eFrmPsrSta frmPsrPortSelFmt(stFrmPsr *psr, eFrameParMapType protocol)
{
    const stFrmPsrFmt *lFmt;

    lFmt = frmPsrPortGetFmt(protocol);
    if (lFmt == NULL) {
        return FRM_PSR_FMT_INVALID;
    }

    return frmPsrSelFmt(psr, lFmt);
}

eFrmPsrSta frmPsrPortMkPkt(eFrameParMapType protocol, const uint8_t *payloadBuf, uint16_t payloadLen, uint8_t *pktBuf, uint16_t pktBufSize, uint16_t *pktLen)
{
    const stFrmPsrFmt *lFmt;

    lFmt = frmPsrPortGetFmt(protocol);
    if (lFmt == NULL) {
        return FRM_PSR_FMT_INVALID;
    }

    return frmPsrMkPktByFmt(lFmt, payloadBuf, payloadLen, pktBuf, pktBufSize, pktLen);
}
/**************************End of file********************************/
