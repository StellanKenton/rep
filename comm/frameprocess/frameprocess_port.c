/************************************************************************************
* @file     : frameprocess_port.c
* @brief    : Frame process project port helpers.
***********************************************************************************/
#include "frameprocess_port.h"

#include <string.h>

#include "Rep/drvlayer/drvuart/drvuart.h"

#if (REP_RTOS_SYSTEM == REP_RTOS_FREERTOS)
#include "FreeRTOS.h"
#include "task.h"
#endif

typedef struct stFrmProcPortFmtCtx {
    stFrmPsrPortProtoCfg protoCfg;
    uint8_t currentTxCmd;
    bool isFmtReady;
} stFrmProcPortFmtCtx;

static stFrmProcPortFmtCtx gFrmProcPortFmtCtx[FRAME_PROC_MAX];
static uint8_t gFrmProcPortUrgentStorage[FRAME_PROC_MAX][FRM_PROC_URGENT_QUEUE_CAPACITY];
static uint8_t gFrmProcPortNormalStorage[FRAME_PROC_MAX][FRM_PROC_NORMAL_QUEUE_CAPACITY];
static uint8_t gFrmProcPortRxFrameBuf[FRAME_PROC_MAX][FRM_PROC_MAX_PKT_LEN];

static uint16_t frmProcPortReadBe16(const uint8_t *buffer);
static uint16_t frmProcPortCalcCrc16Ccitt(const uint8_t *buffer, uint32_t length);
static uint32_t frmProcPortCalcCrc(const uint8_t *buffer, uint32_t length, void *userCtx);
static uint32_t frmProcPortGetHeadLen(const uint8_t *buffer, uint32_t availLen, void *userCtx);
static uint32_t frmProcPortGetPktLen(const uint8_t *buffer, uint32_t headLen, uint32_t availLen, void *userCtx);
static uint32_t frmProcPortBuildHead(uint8_t *buffer, uint32_t bufSize, uint32_t payloadLen, void *userCtx);
static uint32_t frmProcPortBuildPktLen(uint32_t headLen, uint32_t payloadLen, void *userCtx);
static bool frmProcPortFinalizePkt(uint8_t *pktBuf, uint32_t pktLen, uint32_t headLen, const uint8_t *payloadBuf, uint32_t payloadLen, void *userCtx);
static bool frmProcPortIsValidProc(eFrmProcMapType proc);
static eFrameParMapType frmProcPortGetProtocol(eFrmProcMapType proc);

static uint16_t frmProcPortReadBe16(const uint8_t *buffer)
{
    return (uint16_t)(((uint16_t)buffer[0] << 8U) | (uint16_t)buffer[1]);
}

static uint16_t frmProcPortCalcCrc16Ccitt(const uint8_t *buffer, uint32_t length)
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

static uint32_t frmProcPortCalcCrc(const uint8_t *buffer, uint32_t length, void *userCtx)
{
    (void)userCtx;
    return (uint32_t)frmProcPortCalcCrc16Ccitt(buffer, length);
}

static uint32_t frmProcPortGetHeadLen(const uint8_t *buffer, uint32_t availLen, void *userCtx)
{
    stFrmProcPortFmtCtx *lFmtCtx = (stFrmProcPortFmtCtx *)userCtx;

    (void)buffer;
    if ((lFmtCtx == NULL) || (availLen < lFmtCtx->protoCfg.minHeadLen)) {
        return 0U;
    }

    return lFmtCtx->protoCfg.minHeadLen;
}

static uint32_t frmProcPortGetPktLen(const uint8_t *buffer, uint32_t headLen, uint32_t availLen, void *userCtx)
{
    stFrmProcPortFmtCtx *lFmtCtx = (stFrmProcPortFmtCtx *)userCtx;
    uint32_t lPayloadLen;
    uint32_t lPktLen;

    if ((lFmtCtx == NULL) || (buffer == NULL) || (headLen < lFmtCtx->protoCfg.minHeadLen) || (availLen < lFmtCtx->protoCfg.minHeadLen)) {
        return 0U;
    }

    lPayloadLen = (uint32_t)frmProcPortReadBe16(&buffer[4]);
    lPktLen = lFmtCtx->protoCfg.minHeadLen + lPayloadLen + lFmtCtx->protoCfg.crcFieldLen;
    if ((lPktLen < lFmtCtx->protoCfg.minPktLen) || (lPktLen > lFmtCtx->protoCfg.maxPktLen)) {
        return 0U;
    }

    return lPktLen;
}

static uint32_t frmProcPortBuildHead(uint8_t *buffer, uint32_t bufSize, uint32_t payloadLen, void *userCtx)
{
    stFrmProcPortFmtCtx *lFmtCtx = (stFrmProcPortFmtCtx *)userCtx;

    if ((lFmtCtx == NULL) || (buffer == NULL) || (bufSize < lFmtCtx->protoCfg.minHeadLen) || (lFmtCtx->protoCfg.txHeadPat == NULL)) {
        return 0U;
    }

    (void)memcpy(buffer, lFmtCtx->protoCfg.txHeadPat, lFmtCtx->protoCfg.txHeadPatLen);
    buffer[lFmtCtx->protoCfg.txHeadPatLen] = lFmtCtx->currentTxCmd;
    buffer[lFmtCtx->protoCfg.txHeadPatLen + 1U] = (uint8_t)((payloadLen >> 8U) & 0xFFU);
    buffer[lFmtCtx->protoCfg.txHeadPatLen + 2U] = (uint8_t)(payloadLen & 0xFFU);
    return lFmtCtx->protoCfg.minHeadLen;
}

static uint32_t frmProcPortBuildPktLen(uint32_t headLen, uint32_t payloadLen, void *userCtx)
{
    stFrmProcPortFmtCtx *lFmtCtx = (stFrmProcPortFmtCtx *)userCtx;

    if (lFmtCtx == NULL) {
        return 0U;
    }

    return headLen + payloadLen + lFmtCtx->protoCfg.crcFieldLen;
}

static bool frmProcPortFinalizePkt(uint8_t *pktBuf, uint32_t pktLen, uint32_t headLen, const uint8_t *payloadBuf, uint32_t payloadLen, void *userCtx)
{
    (void)pktBuf;
    (void)pktLen;
    (void)headLen;
    (void)payloadBuf;
    (void)payloadLen;
    (void)userCtx;
    return true;
}

static bool frmProcPortIsValidProc(eFrmProcMapType proc)
{
    return ((uint32_t)proc < (uint32_t)FRAME_PROC_MAX);
}

static eFrameParMapType frmProcPortGetProtocol(eFrmProcMapType proc)
{
    return (proc == FRAME_PROC1) ? FRAME_PROTOCOL1 : FRAME_PROTOCOL0;
}

uint32_t frmProcPortGetTickMs(void)
{
#if (REP_RTOS_SYSTEM == REP_RTOS_FREERTOS)
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
#else
    return 0U;
#endif
}

eFrmProcStatus frmProcPortGetDefCfg(eFrmProcMapType proc, stFrmProcCfg *cfg)
{
    stFrmPsrPortProtoCfg lProtoCfg;

    if ((!frmProcPortIsValidProc(proc)) || (cfg == NULL)) {
        return FRM_PROC_STATUS_INVALID_PARAM;
    }

    (void)memset(cfg, 0, sizeof(*cfg));
    frmPsrPortGetDefProtoCfg(FRAME_PROTOCOL0, &lProtoCfg);
    cfg->protocol = frmProcPortGetProtocol(proc);
    cfg->protoCfg = lProtoCfg;
    cfg->getTick = frmProcPortGetTickMs;
    cfg->txFrame = frmProcPortTxFrame;
    cfg->urgentQueue.storage = gFrmProcPortUrgentStorage[proc];
    cfg->urgentQueue.capacity = FRM_PROC_URGENT_QUEUE_CAPACITY;
    cfg->normalQueue.storage = gFrmProcPortNormalStorage[proc];
    cfg->normalQueue.capacity = FRM_PROC_NORMAL_QUEUE_CAPACITY;
    cfg->rxFrameBuf = gFrmProcPortRxFrameBuf[proc];
    cfg->rxFrameBufSize = sizeof(gFrmProcPortRxFrameBuf[proc]);
    cfg->ackCfg.timeoutMs = FRM_PROC_ACK_TIMEOUT_MS;
    cfg->ackCfg.maxRetryCount = FRM_PROC_ACK_RETRY_COUNT;
    return FRM_PROC_STATUS_OK;
}

eFrmProcStatus frmProcPortInit(eFrmProcMapType proc)
{
    (void)proc;
    return (drvUartInit(DRVUART_DEBUG) == DRV_STATUS_OK) ? FRM_PROC_STATUS_OK : FRM_PROC_STATUS_ERROR;
}

void frmProcPortPollRx(eFrmProcMapType proc)
{
    (void)proc;
    (void)drvUartGetDataLen(DRVUART_DEBUG);
}

eFrmProcStatus frmProcPortEnsureFmt(eFrmProcMapType proc, const stFrmProcCfg *cfg)
{
    stFrmPsrFmt lFmt;
    stFrmProcPortFmtCtx *lFmtCtx;

    if ((!frmProcPortIsValidProc(proc)) || (cfg == NULL)) {
        return FRM_PROC_STATUS_INVALID_PARAM;
    }

    lFmtCtx = &gFrmProcPortFmtCtx[proc];
    lFmtCtx->protoCfg = cfg->protoCfg;

    (void)memset(&lFmt, 0, sizeof(lFmt));
    lFmt.name = "frameprocess";
    lFmt.rxFmt.headPat = cfg->protoCfg.rxHeadPat;
    lFmt.rxFmt.headPatLen = cfg->protoCfg.rxHeadPatLen;
    lFmt.rxFmt.minHeadLen = cfg->protoCfg.minHeadLen;
    lFmt.rxFmt.minPktLen = cfg->protoCfg.minPktLen;
    lFmt.rxFmt.maxPktLen = cfg->protoCfg.maxPktLen;
    lFmt.rxFmt.crcRangeStartOff = cfg->protoCfg.crcRangeStartOff;
    lFmt.rxFmt.crcRangeEndOff = cfg->protoCfg.crcRangeEndOff;
    lFmt.rxFmt.crcFieldOff = cfg->protoCfg.crcFieldOff;
    lFmt.rxFmt.crcFieldLen = cfg->protoCfg.crcFieldLen;
    lFmt.rxFmt.crcFieldEnd = cfg->protoCfg.crcFieldEnd;
    lFmt.rxFmt.headLenFunc = frmProcPortGetHeadLen;
    lFmt.rxFmt.pktLenFunc = frmProcPortGetPktLen;
    lFmt.rxFmt.crcCalcFunc = frmProcPortCalcCrc;
    lFmt.rxFmt.userCtx = lFmtCtx;
    lFmt.txFmt.headPat = cfg->protoCfg.txHeadPat;
    lFmt.txFmt.headPatLen = cfg->protoCfg.txHeadPatLen;
    lFmt.txFmt.minPktLen = cfg->protoCfg.minPktLen;
    lFmt.txFmt.maxPktLen = cfg->protoCfg.maxPktLen;
    lFmt.txFmt.crcRangeStartOff = cfg->protoCfg.crcRangeStartOff;
    lFmt.txFmt.crcRangeEndOff = cfg->protoCfg.crcRangeEndOff;
    lFmt.txFmt.crcFieldOff = cfg->protoCfg.crcFieldOff;
    lFmt.txFmt.crcFieldLen = cfg->protoCfg.crcFieldLen;
    lFmt.txFmt.crcFieldEnd = cfg->protoCfg.crcFieldEnd;
    lFmt.txFmt.headBuildFunc = frmProcPortBuildHead;
    lFmt.txFmt.pktLenFunc = frmProcPortBuildPktLen;
    lFmt.txFmt.pktFinFunc = frmProcPortFinalizePkt;
    lFmt.txFmt.crcCalcFunc = frmProcPortCalcCrc;
    lFmt.txFmt.userCtx = lFmtCtx;

    if (!frmPsrPortSetFmt(cfg->protocol, &lFmt)) {
        return FRM_PROC_STATUS_BUILD_ERROR;
    }

    lFmtCtx->isFmtReady = true;
    return FRM_PROC_STATUS_OK;
}

eFrmProcStatus frmProcPortBuildPkt(eFrmProcMapType proc, uint8_t cmd, const uint8_t *payloadBuf, uint16_t payloadLen, uint8_t *pktBuf, uint16_t pktBufSize, uint16_t *pktLen)
{
    eFrmPsrSta lPsrStatus;

    if ((!frmProcPortIsValidProc(proc)) || (pktBuf == NULL) || (pktLen == NULL)) {
        return FRM_PROC_STATUS_INVALID_PARAM;
    }

    gFrmProcPortFmtCtx[proc].currentTxCmd = cmd;
    lPsrStatus = frmPsrPortMkPkt(frmProcPortGetProtocol(proc), payloadBuf, payloadLen, pktBuf, pktBufSize, pktLen);
    return (lPsrStatus == FRM_PSR_OK) ? FRM_PROC_STATUS_OK : FRM_PROC_STATUS_BUILD_ERROR;
}

eFrmProcStatus frmProcPortTxFrame(eFrmProcMapType proc, const uint8_t *frameBuf, uint16_t frameLen)
{
    eDrvStatus lStatus;
    uint32_t lRetry;

    (void)proc;

    if ((frameBuf == NULL) || (frameLen == 0U)) {
        return FRM_PROC_STATUS_INVALID_PARAM;
    }

    for (lRetry = 0U; lRetry < 10U; lRetry++) {
        lStatus = drvUartTransmitDma(DRVUART_DEBUG, frameBuf, frameLen);
        if (lStatus == DRV_STATUS_OK) {
            return FRM_PROC_STATUS_OK;
        }

        if (lStatus != DRV_STATUS_BUSY) {
            break;
        }

#if (REP_RTOS_SYSTEM == REP_RTOS_FREERTOS)
        vTaskDelay(pdMS_TO_TICKS(2U));
#endif
    }

    lStatus = drvUartTransmit(DRVUART_DEBUG, frameBuf, frameLen, 100U);
    if (lStatus == DRV_STATUS_OK) {
        return FRM_PROC_STATUS_OK;
    }
    if (lStatus == DRV_STATUS_BUSY) {
        return FRM_PROC_STATUS_BUSY;
    }
    if (lStatus == DRV_STATUS_TIMEOUT) {
        return FRM_PROC_STATUS_TIMEOUT;
    }
    return FRM_PROC_STATUS_ERROR;
}
/**************************End of file********************************/
