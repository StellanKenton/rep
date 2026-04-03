/************************************************************************************
* @file     : frameprocess.c
* @brief    : Framed protocol process service implementation.
***********************************************************************************/
#include "frameprocess.h"

#include <string.h>

#include "Rep/console/log.h"
#include "frameprocess_pack.h"
#include "frameprocess_port.h"

#define FRM_PROC_TAG  "FrmProc"

static stFrmProcCfg gFrmProcCfg[FRAME_PROC_MAX];
static bool gFrmProcHasCfg[FRAME_PROC_MAX];
static stFrmProcCtx gFrmProcCtx[FRAME_PROC_MAX];

static bool frmProcIsValidProc(eFrmProcMapType proc);
static stFrmProcCtx *frmProcGetCtx(eFrmProcMapType proc);
static eFrmProcStatus frmProcEnsureCfgLoaded(eFrmProcMapType proc);
static uint32_t frmProcGetTickMs(const stFrmProcCtx *ctx);
static bool frmProcNeedsAckByCmd(uint8_t cmd);
static bool frmProcNeedsAckByFrame(const uint8_t *frameBuf, uint16_t frameLen);
static bool frmProcIsAckMatch(stFrmProcCtx *ctx, const uint8_t *frameBuf, uint16_t frameLen);
static void frmProcMarkAckReceived(stFrmProcCtx *ctx);
static eFrmProcStatus frmProcQueueFrame(stRingBuffer *ringBuf, const uint8_t *frameBuf, uint16_t frameLen);
static bool frmProcPeekFrame(const stRingBuffer *ringBuf, uint8_t *recordBuf, uint16_t recordBufSize, uint8_t *frameBuf, uint16_t frameBufSize, uint16_t *frameLen);
static void frmProcDiscardQueuedFrame(stRingBuffer *ringBuf, uint16_t frameLen);
static eFrmProcStatus frmProcBuildAndQueueTx(stFrmProcCtx *ctx, eFrmProcMapType proc, uint8_t cmd, uint32_t flagMask);
static void frmProcProcessRx(eFrmProcMapType proc, stFrmProcCtx *ctx);
static void frmProcBuildPendingTx(eFrmProcMapType proc, stFrmProcCtx *ctx);
static void frmProcProcessAckTimeout(eFrmProcMapType proc, stFrmProcCtx *ctx);
static void frmProcProcessTx(eFrmProcMapType proc, stFrmProcCtx *ctx);

static bool frmProcIsValidProc(eFrmProcMapType proc)
{
    return ((uint32_t)proc < (uint32_t)FRAME_PROC_MAX);
}

static stFrmProcCtx *frmProcGetCtx(eFrmProcMapType proc)
{
    if (!frmProcIsValidProc(proc)) {
        return NULL;
    }

    return &gFrmProcCtx[proc];
}

static eFrmProcStatus frmProcEnsureCfgLoaded(eFrmProcMapType proc)
{
    if (!frmProcIsValidProc(proc)) {
        return FRM_PROC_STATUS_INVALID_PARAM;
    }

    if (!gFrmProcHasCfg[proc]) {
        if (frmProcPortGetDefCfg(proc, &gFrmProcCfg[proc]) != FRM_PROC_STATUS_OK) {
            return FRM_PROC_STATUS_ERROR;
        }
        gFrmProcHasCfg[proc] = true;
    }

    return FRM_PROC_STATUS_OK;
}

static uint32_t frmProcGetTickMs(const stFrmProcCtx *ctx)
{
    if ((ctx == NULL) || (ctx->cfg.getTick == NULL)) {
        return 0U;
    }

    return ctx->cfg.getTick();
}

static bool frmProcNeedsAckByCmd(uint8_t cmd)
{
    return cmd == (uint8_t)FRM_PROC_CMD_HANDSHAKE;
}

static bool frmProcNeedsAckByFrame(const uint8_t *frameBuf, uint16_t frameLen)
{
    if ((frameBuf == NULL) || (frameLen < 4U)) {
        return false;
    }

    return frmProcNeedsAckByCmd(frameBuf[3]);
}

static bool frmProcIsAckMatch(stFrmProcCtx *ctx, const uint8_t *frameBuf, uint16_t frameLen)
{
    if ((ctx == NULL) || (!ctx->ackState.isWaiting) || (frameBuf == NULL)) {
        return false;
    }

    if (ctx->ackState.frameLen != frameLen) {
        return false;
    }

    return memcmp(ctx->ackState.frameBuf, frameBuf, frameLen) == 0;
}

static void frmProcMarkAckReceived(stFrmProcCtx *ctx)
{
    if (ctx == NULL) {
        return;
    }

    (void)memset(&ctx->ackState, 0, sizeof(ctx->ackState));
    ctx->runFlags.bits.isWaitingAck = 0U;
    ctx->rxStore.flags.bits.ackFrame = 1U;
}

static eFrmProcStatus frmProcQueueFrame(stRingBuffer *ringBuf, const uint8_t *frameBuf, uint16_t frameLen)
{
    uint8_t lHeader[FRM_PROC_QUEUE_RECORD_OVERHEAD];
    uint32_t lNeedSize;

    if ((ringBuf == NULL) || (frameBuf == NULL) || (frameLen == 0U)) {
        return FRM_PROC_STATUS_INVALID_PARAM;
    }

    lNeedSize = (uint32_t)frameLen + FRM_PROC_QUEUE_RECORD_OVERHEAD;
    if (ringBufferGetFree(ringBuf) < lNeedSize) {
        return FRM_PROC_STATUS_NO_SPACE;
    }

    lHeader[0] = (uint8_t)((frameLen >> 8U) & 0xFFU);
    lHeader[1] = (uint8_t)(frameLen & 0xFFU);
    if (ringBufferWrite(ringBuf, lHeader, sizeof(lHeader)) != sizeof(lHeader)) {
        return FRM_PROC_STATUS_ERROR;
    }
    if (ringBufferWrite(ringBuf, frameBuf, frameLen) != frameLen) {
        return FRM_PROC_STATUS_ERROR;
    }

    return FRM_PROC_STATUS_OK;
}

static bool frmProcPeekFrame(const stRingBuffer *ringBuf, uint8_t *recordBuf, uint16_t recordBufSize, uint8_t *frameBuf, uint16_t frameBufSize, uint16_t *frameLen)
{
    uint16_t lFrameLen;
    uint32_t lNeedSize;

    if ((ringBuf == NULL) || (recordBuf == NULL) || (frameBuf == NULL) || (frameLen == NULL)) {
        return false;
    }

    if (ringBufferGetUsed(ringBuf) < FRM_PROC_QUEUE_RECORD_OVERHEAD) {
        return false;
    }

    if (ringBufferPeek(ringBuf, recordBuf, FRM_PROC_QUEUE_RECORD_OVERHEAD) != FRM_PROC_QUEUE_RECORD_OVERHEAD) {
        return false;
    }

    lFrameLen = (uint16_t)(((uint16_t)recordBuf[0] << 8U) | (uint16_t)recordBuf[1]);
    lNeedSize = (uint32_t)lFrameLen + FRM_PROC_QUEUE_RECORD_OVERHEAD;
    if ((lFrameLen == 0U) || (lFrameLen > frameBufSize) || (lNeedSize > recordBufSize) || (ringBufferGetUsed(ringBuf) < lNeedSize)) {
        return false;
    }

    if (ringBufferPeek(ringBuf, recordBuf, lNeedSize) != lNeedSize) {
        return false;
    }

    (void)memcpy(frameBuf, &recordBuf[FRM_PROC_QUEUE_RECORD_OVERHEAD], lFrameLen);
    *frameLen = lFrameLen;
    return true;
}

static void frmProcDiscardQueuedFrame(stRingBuffer *ringBuf, uint16_t frameLen)
{
    if ((ringBuf == NULL) || (frameLen == 0U)) {
        return;
    }

    (void)ringBufferDiscard(ringBuf, (uint32_t)frameLen + FRM_PROC_QUEUE_RECORD_OVERHEAD);
}

static eFrmProcStatus frmProcBuildAndQueueTx(stFrmProcCtx *ctx, eFrmProcMapType proc, uint8_t cmd, uint32_t flagMask)
{
    uint16_t lPayloadLen;
    uint16_t lFrameLen;
    bool lIsUrgent;
    stRingBuffer *lTargetRb;

    if (ctx == NULL) {
        return FRM_PROC_STATUS_INVALID_PARAM;
    }

    if (!frmProcDataBuildTx(cmd, &ctx->txStore, ctx->txPayloadBuf, sizeof(ctx->txPayloadBuf), &lPayloadLen)) {
        return FRM_PROC_STATUS_BUILD_ERROR;
    }

    if (frmProcPortBuildPkt(proc, cmd, ctx->txPayloadBuf, lPayloadLen, ctx->txFrameBuf, sizeof(ctx->txFrameBuf), &lFrameLen) != FRM_PROC_STATUS_OK) {
        return FRM_PROC_STATUS_BUILD_ERROR;
    }

    lIsUrgent = (ctx->txUrgentMask & flagMask) != 0U;
    lTargetRb = lIsUrgent ? &ctx->urgentTxRb : &ctx->normalTxRb;
    if (frmProcQueueFrame(lTargetRb, ctx->txFrameBuf, lFrameLen) != FRM_PROC_STATUS_OK) {
        return FRM_PROC_STATUS_NO_SPACE;
    }

    ctx->txStore.flags.value &= ~flagMask;
    ctx->txUrgentMask &= ~flagMask;
    return FRM_PROC_STATUS_OK;
}

static void frmProcProcessRx(eFrmProcMapType proc, stFrmProcCtx *ctx)
{
    stFrmPsrPkt lPacket;
    eFrmPsrSta lPsrStatus;
    uint16_t lPayloadLen;
    uint8_t lCmd;
    uint32_t lCount;

    frmProcPortPollRx(proc);
    for (lCount = 0U; lCount < FRM_PROC_MAX_RX_PER_CALL; lCount++) {
        lPsrStatus = frmPsrProc(&ctx->parser, &lPacket);
        if (lPsrStatus != FRM_PSR_OK) {
            if ((lPsrStatus != FRM_PSR_EMPTY) && (lPsrStatus != FRM_PSR_NEED_MORE_DATA) && (lPsrStatus != FRM_PSR_HEAD_NOT_FOUND)) {
                LOG_W(FRM_PROC_TAG, "Parser status=%d", (int)lPsrStatus);
            }
            break;
        }

        if ((lPacket.buf == NULL) || (lPacket.len < 8U)) {
            frmPsrFreePkt(&ctx->parser);
            continue;
        }

        if (frmProcIsAckMatch(ctx, lPacket.buf, lPacket.len)) {
            frmProcMarkAckReceived(ctx);
            frmPsrFreePkt(&ctx->parser);
            continue;
        }

        lCmd = lPacket.buf[3];
        lPayloadLen = (uint16_t)(((uint16_t)lPacket.buf[4] << 8U) | (uint16_t)lPacket.buf[5]);
        if ((uint16_t)(6U + lPayloadLen + 2U) != lPacket.len) {
            frmPsrFreePkt(&ctx->parser);
            continue;
        }

        if (frmProcNeedsAckByCmd(lCmd)) {
            if (lPacket.len <= sizeof(ctx->immediateAckBuf)) {
                (void)memcpy(ctx->immediateAckBuf, lPacket.buf, lPacket.len);
                ctx->immediateAckLen = lPacket.len;
                ctx->runFlags.bits.hasImmediateAck = 1U;
            }
        }

        if (!frmProcDataParseRx(lCmd, &lPacket.buf[6], lPayloadLen, &ctx->rxStore)) {
            LOG_W(FRM_PROC_TAG, "RX decode fail cmd=0x%02X len=%u", (unsigned int)lCmd, (unsigned int)lPayloadLen);
            frmPsrFreePkt(&ctx->parser);
            continue;
        }

        if (!frmProcPackOnRx(ctx, lCmd)) {
            LOG_W(FRM_PROC_TAG, "Pack handle fail cmd=0x%02X", (unsigned int)lCmd);
        }

        frmPsrFreePkt(&ctx->parser);
    }
}

static void frmProcBuildPendingTx(eFrmProcMapType proc, stFrmProcCtx *ctx)
{
    if ((ctx->txStore.flags.value & FRM_PROC_TX_FLAG_HANDSHAKE_MASK) != 0U) {
        if (frmProcBuildAndQueueTx(ctx, proc, FRM_PROC_CMD_HANDSHAKE, FRM_PROC_TX_FLAG_HANDSHAKE_MASK) != FRM_PROC_STATUS_OK) {
            return;
        }
    }
    if ((ctx->txStore.flags.value & FRM_PROC_TX_FLAG_HEARTBEAT_MASK) != 0U) {
        if (frmProcBuildAndQueueTx(ctx, proc, FRM_PROC_CMD_HEARTBEAT, FRM_PROC_TX_FLAG_HEARTBEAT_MASK) != FRM_PROC_STATUS_OK) {
            return;
        }
    }
    if ((ctx->txStore.flags.value & FRM_PROC_TX_FLAG_DISCONNECT_MASK) != 0U) {
        if (frmProcBuildAndQueueTx(ctx, proc, FRM_PROC_CMD_DISCONNECT, FRM_PROC_TX_FLAG_DISCONNECT_MASK) != FRM_PROC_STATUS_OK) {
            return;
        }
    }
    if ((ctx->txStore.flags.value & FRM_PROC_TX_FLAG_SELFCHECK_MASK) != 0U) {
        if (frmProcBuildAndQueueTx(ctx, proc, FRM_PROC_CMD_SELF_CHECK, FRM_PROC_TX_FLAG_SELFCHECK_MASK) != FRM_PROC_STATUS_OK) {
            return;
        }
    }
    if ((ctx->txStore.flags.value & FRM_PROC_TX_FLAG_DEVICEINFO_MASK) != 0U) {
        if (frmProcBuildAndQueueTx(ctx, proc, FRM_PROC_CMD_GET_DEVICE_INFO, FRM_PROC_TX_FLAG_DEVICEINFO_MASK) != FRM_PROC_STATUS_OK) {
            return;
        }
    }
    if ((ctx->txStore.flags.value & FRM_PROC_TX_FLAG_BLEINFO_MASK) != 0U) {
        if (frmProcBuildAndQueueTx(ctx, proc, FRM_PROC_CMD_GET_BLE_INFO, FRM_PROC_TX_FLAG_BLEINFO_MASK) != FRM_PROC_STATUS_OK) {
            return;
        }
    }
    if ((ctx->txStore.flags.value & FRM_PROC_TX_FLAG_CPRDATA_MASK) != 0U) {
        (void)frmProcBuildAndQueueTx(ctx, proc, FRM_PROC_CMD_CPR_DATA, FRM_PROC_TX_FLAG_CPRDATA_MASK);
    }
}

static void frmProcProcessAckTimeout(eFrmProcMapType proc, stFrmProcCtx *ctx)
{
    uint32_t lNow;

    if ((ctx == NULL) || (!ctx->ackState.isWaiting)) {
        return;
    }

    lNow = frmProcGetTickMs(ctx);
    if ((uint32_t)(lNow - ctx->ackState.sendTickMs) < ctx->ackState.timeoutMs) {
        return;
    }

    if (ctx->ackState.retryCount >= ctx->ackState.maxRetryCount) {
        LOG_W(FRM_PROC_TAG, "ACK timeout give up len=%u", (unsigned int)ctx->ackState.frameLen);
        (void)memset(&ctx->ackState, 0, sizeof(ctx->ackState));
        ctx->runFlags.bits.isWaitingAck = 0U;
        return;
    }

    if (ctx->cfg.txFrame(proc, ctx->ackState.frameBuf, ctx->ackState.frameLen) == FRM_PROC_STATUS_OK) {
        ctx->ackState.retryCount++;
        ctx->ackState.sendTickMs = lNow;
    }
}

static void frmProcProcessTx(eFrmProcMapType proc, stFrmProcCtx *ctx)
{
    uint16_t lFrameLen;
    stRingBuffer *lSourceRb;

    if ((ctx == NULL) || (ctx->cfg.txFrame == NULL)) {
        return;
    }

    if (ctx->runFlags.bits.hasImmediateAck != 0U) {
        if (ctx->cfg.txFrame(proc, ctx->immediateAckBuf, ctx->immediateAckLen) == FRM_PROC_STATUS_OK) {
            ctx->runFlags.bits.hasImmediateAck = 0U;
            ctx->immediateAckLen = 0U;
            return;
        }
        if (frmProcQueueFrame(&ctx->urgentTxRb, ctx->immediateAckBuf, ctx->immediateAckLen) == FRM_PROC_STATUS_OK) {
            ctx->runFlags.bits.hasImmediateAck = 0U;
            ctx->immediateAckLen = 0U;
        }
        return;
    }

    lSourceRb = NULL;
    if (frmProcPeekFrame(&ctx->urgentTxRb, ctx->txPeekBuf, sizeof(ctx->txPeekBuf), ctx->txFrameBuf, sizeof(ctx->txFrameBuf), &lFrameLen)) {
        lSourceRb = &ctx->urgentTxRb;
    } else if (frmProcPeekFrame(&ctx->normalTxRb, ctx->txPeekBuf, sizeof(ctx->txPeekBuf), ctx->txFrameBuf, sizeof(ctx->txFrameBuf), &lFrameLen)) {
        lSourceRb = &ctx->normalTxRb;
    } else {
        return;
    }

    if (ctx->ackState.isWaiting && frmProcNeedsAckByFrame(ctx->txFrameBuf, lFrameLen)) {
        return;
    }

    if (ctx->cfg.txFrame(proc, ctx->txFrameBuf, lFrameLen) != FRM_PROC_STATUS_OK) {
        return;
    }

    frmProcDiscardQueuedFrame(lSourceRb, lFrameLen);

    if (frmProcNeedsAckByFrame(ctx->txFrameBuf, lFrameLen)) {
        (void)memcpy(ctx->ackState.frameBuf, ctx->txFrameBuf, lFrameLen);
        ctx->ackState.frameLen = lFrameLen;
        ctx->ackState.retryCount = 0U;
        ctx->ackState.maxRetryCount = ctx->cfg.ackCfg.maxRetryCount;
        ctx->ackState.timeoutMs = ctx->cfg.ackCfg.timeoutMs;
        ctx->ackState.sendTickMs = frmProcGetTickMs(ctx);
        ctx->ackState.isWaiting = true;
        ctx->runFlags.bits.isWaitingAck = 1U;
    }
}

eFrmProcStatus frmProcGetDefCfg(eFrmProcMapType proc, stFrmProcCfg *cfg)
{
    return frmProcPortGetDefCfg(proc, cfg);
}

eFrmProcStatus frmProcSetCfg(eFrmProcMapType proc, const stFrmProcCfg *cfg)
{
    if ((!frmProcIsValidProc(proc)) || (cfg == NULL)) {
        return FRM_PROC_STATUS_INVALID_PARAM;
    }

    gFrmProcCfg[proc] = *cfg;
    gFrmProcHasCfg[proc] = true;
    return FRM_PROC_STATUS_OK;
}

eFrmProcStatus frmProcInit(eFrmProcMapType proc)
{
    stFrmProcCtx *lCtx;

    if (frmProcEnsureCfgLoaded(proc) != FRM_PROC_STATUS_OK) {
        return FRM_PROC_STATUS_ERROR;
    }

    lCtx = frmProcGetCtx(proc);
    if (lCtx == NULL) {
        return FRM_PROC_STATUS_INVALID_PARAM;
    }

    if (lCtx->runFlags.bits.isInit != 0U) {
        return FRM_PROC_STATUS_OK;
    }

    (void)memset(lCtx, 0, sizeof(*lCtx));
    lCtx->cfg = gFrmProcCfg[proc];

    if (frmProcPortInit(proc) != FRM_PROC_STATUS_OK) {
        return FRM_PROC_STATUS_ERROR;
    }
    if (frmProcPortEnsureFmt(proc, &lCtx->cfg) != FRM_PROC_STATUS_OK) {
        return FRM_PROC_STATUS_BUILD_ERROR;
    }
    if (ringBufferInit(&lCtx->urgentTxRb, lCtx->cfg.urgentQueue.storage, lCtx->cfg.urgentQueue.capacity) != RINGBUFFER_OK) {
        return FRM_PROC_STATUS_ERROR;
    }
    if (ringBufferInit(&lCtx->normalTxRb, lCtx->cfg.normalQueue.storage, lCtx->cfg.normalQueue.capacity) != RINGBUFFER_OK) {
        return FRM_PROC_STATUS_ERROR;
    }
    if (frmPsrInitByProtoCfg(&lCtx->parser, &lCtx->cfg.protoCfg, NULL, lCtx->cfg.rxFrameBuf, lCtx->cfg.rxFrameBufSize) != FRM_PSR_OK) {
        return FRM_PROC_STATUS_PARSE_ERROR;
    }

    lCtx->ackState.timeoutMs = lCtx->cfg.ackCfg.timeoutMs;
    lCtx->ackState.maxRetryCount = lCtx->cfg.ackCfg.maxRetryCount;
    lCtx->runFlags.bits.isInit = 1U;
    return FRM_PROC_STATUS_OK;
}

bool frmProcIsReady(eFrmProcMapType proc)
{
    stFrmProcCtx *lCtx = frmProcGetCtx(proc);

    return (lCtx != NULL) && (lCtx->runFlags.bits.isInit != 0U);
}

void frmProcProcess(eFrmProcMapType proc)
{
    stFrmProcCtx *lCtx = frmProcGetCtx(proc);

    if ((lCtx == NULL) || (lCtx->runFlags.bits.isInit == 0U)) {
        return;
    }

    frmProcProcessRx(proc, lCtx);
    frmProcBuildPendingTx(proc, lCtx);
    frmProcProcessAckTimeout(proc, lCtx);
    frmProcProcessTx(proc, lCtx);
}

eFrmProcStatus frmProcPostSelfCheck(eFrmProcMapType proc, const stFrmDataTxSelfCheck *data, bool isUrgent)
{
    stFrmProcCtx *lCtx = frmProcGetCtx(proc);

    if ((lCtx == NULL) || (data == NULL) || (lCtx->runFlags.bits.isInit == 0U)) {
        return FRM_PROC_STATUS_INVALID_PARAM;
    }

    lCtx->txStore.selfCheck = *data;
    lCtx->txStore.flags.value |= FRM_PROC_TX_FLAG_SELFCHECK_MASK;
    if (isUrgent) {
        lCtx->txUrgentMask |= FRM_PROC_TX_FLAG_SELFCHECK_MASK;
    } else {
        lCtx->txUrgentMask &= ~FRM_PROC_TX_FLAG_SELFCHECK_MASK;
    }
    return FRM_PROC_STATUS_OK;
}

eFrmProcStatus frmProcPostDisconnect(eFrmProcMapType proc, bool isUrgent)
{
    stFrmProcCtx *lCtx = frmProcGetCtx(proc);

    if ((lCtx == NULL) || (lCtx->runFlags.bits.isInit == 0U)) {
        return FRM_PROC_STATUS_INVALID_PARAM;
    }

    lCtx->txStore.disconnect.reserved = 0U;
    lCtx->txStore.flags.value |= FRM_PROC_TX_FLAG_DISCONNECT_MASK;
    if (isUrgent) {
        lCtx->txUrgentMask |= FRM_PROC_TX_FLAG_DISCONNECT_MASK;
    } else {
        lCtx->txUrgentMask &= ~FRM_PROC_TX_FLAG_DISCONNECT_MASK;
    }
    return FRM_PROC_STATUS_OK;
}

eFrmProcStatus frmProcPostCprData(eFrmProcMapType proc, const stFrmDataTxCprData *data, bool isUrgent)
{
    stFrmProcCtx *lCtx = frmProcGetCtx(proc);

    if ((lCtx == NULL) || (data == NULL) || (lCtx->runFlags.bits.isInit == 0U)) {
        return FRM_PROC_STATUS_INVALID_PARAM;
    }

    lCtx->txStore.cprData = *data;
    if (lCtx->txStore.cprData.timestampMs == 0U) {
        lCtx->txStore.cprData.timestampMs = frmProcGetTickMs(lCtx);
    }
    lCtx->txStore.flags.value |= FRM_PROC_TX_FLAG_CPRDATA_MASK;
    if (isUrgent) {
        lCtx->txUrgentMask |= FRM_PROC_TX_FLAG_CPRDATA_MASK;
    } else {
        lCtx->txUrgentMask &= ~FRM_PROC_TX_FLAG_CPRDATA_MASK;
    }
    return FRM_PROC_STATUS_OK;
}

const stFrmDataRxStore *frmProcGetRxStore(eFrmProcMapType proc)
{
    stFrmProcCtx *lCtx = frmProcGetCtx(proc);

    if (lCtx == NULL) {
        return NULL;
    }

    return &lCtx->rxStore;
}

void frmProcClearRxFlags(eFrmProcMapType proc, uint32_t flags)
{
    stFrmProcCtx *lCtx = frmProcGetCtx(proc);

    if (lCtx == NULL) {
        return;
    }

    lCtx->rxStore.flags.value &= ~flags;
}
/**************************End of file********************************/
