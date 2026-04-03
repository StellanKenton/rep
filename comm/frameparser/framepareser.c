/***********************************************************************************
* @file     : framepareser.c
* @brief    : Stream packet parser implementation.
* @details  : Locates packet headers in a byte stream and validates complete frames.
* @author   : GitHub Copilot
* @date     : 2026-04-01
* @version  : V1.0.0
**********************************************************************************/
#include "framepareser.h"

#include <string.h>

#include "Rep/console/log.h"

#define FRM_PSR_LOG_TAG  "FrmPsr"

static uint32_t frmPsrMinU32(uint32_t left, uint32_t right);
static uint32_t frmPsrToPhyIdx(const stRingBuffer *ringBuf, uint32_t logIdx);
static uint32_t frmPsrPeekOff(const stRingBuffer *ringBuf, uint32_t off, uint8_t *buf, uint32_t len);
static bool frmPsrPeekByte(const stRingBuffer *ringBuf, uint32_t off, uint8_t *val);
static bool frmPsrIsHeadAt(const stFrmPsr *psr, uint32_t off);
static uint32_t frmPsrFindPartHeadLen(const stFrmPsr *psr, uint32_t usedLen);
static stFrmPsrHeadHit frmPsrFindHead(const stFrmPsr *psr, uint32_t usedLen);
static void frmPsrClrPend(stFrmPsr *psr);
static uint32_t frmPsrGetTickMs(const stFrmPsr *psr);
static bool frmPsrIsPktTout(const stFrmPsr *psr);
static bool frmPsrFixOff(uint32_t pktLen, int32_t off, uint32_t *realOff);
static bool frmPsrGetPktCrc(const stFrmPsrCfg *cfg, const uint8_t *pktBuf, uint32_t pktLen, uint32_t *pktCrc);
static bool frmPsrSetPktCrc(int32_t crcFieldOff, uint8_t crcFieldLen, eFrmPsrCrcEnd crcFieldEnd, uint8_t *pktBuf, uint32_t pktLen, uint32_t pktCrc);
static bool frmPsrChkPktCrc(const stFrmPsrCfg *cfg, const uint8_t *pktBuf, uint32_t pktLen);
static void frmPsrLoadCfgByFmt(stFrmPsrCfg *cfg, const stFrmPsrFmt *fmt, const stFrmPsrRunCfg *runCfg);
static eFrmPsrSta frmPsrMkPktInner(const stFrmPsrTxFmt *txFmt, const uint8_t *payloadBuf, uint32_t payloadLen, uint8_t *pktBuf, uint32_t pktBufSize, uint32_t *pktLen);

static uint32_t frmPsrMinU32(uint32_t left, uint32_t right)
{
    return (left < right) ? left : right;
}

static uint32_t frmPsrToPhyIdx(const stRingBuffer *ringBuf, uint32_t logIdx)
{
    if (ringBuf->isPowerOfTwo != 0U) {
        return logIdx & ringBuf->mask;
    }

    return logIdx % ringBuf->capacity;
}

static uint32_t frmPsrPeekOff(const stRingBuffer *ringBuf, uint32_t off, uint8_t *buf, uint32_t len)
{
    uint32_t lUsedLen;
    uint32_t lReadLen;
    uint32_t lLogIdx;
    uint32_t lPhyIdx;
    uint32_t lFirstLen;

    if ((ringBuf == NULL) || ((buf == NULL) && (len != 0U))) {
        return 0U;
    }

    lUsedLen = ringBufferGetUsed(ringBuf);
    if (off >= lUsedLen) {
        return 0U;
    }

    lReadLen = frmPsrMinU32(len, lUsedLen - off);
    if (lReadLen == 0U) {
        return 0U;
    }

    lLogIdx = ringBuf->tail + off;
    lPhyIdx = frmPsrToPhyIdx(ringBuf, lLogIdx);
    lFirstLen = frmPsrMinU32(lReadLen, ringBuf->capacity - lPhyIdx);
    (void)memcpy(buf, &ringBuf->buffer[lPhyIdx], lFirstLen);

    if (lReadLen > lFirstLen) {
        (void)memcpy(&buf[lFirstLen], ringBuf->buffer, lReadLen - lFirstLen);
    }

    return lReadLen;
}

static bool frmPsrPeekByte(const stRingBuffer *ringBuf, uint32_t off, uint8_t *val)
{
    if ((ringBuf == NULL) || (val == NULL)) {
        return false;
    }

    return frmPsrPeekOff(ringBuf, off, val, 1U) == 1U;
}

static bool frmPsrIsHeadAt(const stFrmPsr *psr, uint32_t off)
{
    uint32_t lIdx;
    uint8_t lVal;

    for (lIdx = 0U; lIdx < psr->cfg.headPatLen; lIdx++) {
        if ((!frmPsrPeekByte(psr->ringBuf, off + lIdx, &lVal)) || (lVal != psr->cfg.headPat[lIdx])) {
            return false;
        }
    }

    return true;
}

static uint32_t frmPsrFindPartHeadLen(const stFrmPsr *psr, uint32_t usedLen)
{
    uint32_t lCandLen;
    uint32_t lCmpIdx;
    uint8_t lVal;

    if ((psr->cfg.headPatLen <= 1U) || (usedLen == 0U)) {
        return 0U;
    }

    for (lCandLen = frmPsrMinU32(usedLen, psr->cfg.headPatLen - 1U); lCandLen > 0U; lCandLen--) {
        for (lCmpIdx = 0U; lCmpIdx < lCandLen; lCmpIdx++) {
            if ((!frmPsrPeekByte(psr->ringBuf, usedLen - lCandLen + lCmpIdx, &lVal)) || (lVal != psr->cfg.headPat[lCmpIdx])) {
                break;
            }
        }

        if (lCmpIdx == lCandLen) {
            return lCandLen;
        }
    }

    return 0U;
}

static stFrmPsrHeadHit frmPsrFindHead(const stFrmPsr *psr, uint32_t usedLen)
{
    stFrmPsrHeadHit lRes = {0U, 0U, false};
    uint32_t lOff;

    if (usedLen < psr->cfg.headPatLen) {
        lRes.partHeadLen = frmPsrFindPartHeadLen(psr, usedLen);
        lRes.discardLen = usedLen - lRes.partHeadLen;
        return lRes;
    }

    for (lOff = 0U; lOff <= (usedLen - psr->cfg.headPatLen); lOff++) {
        if (frmPsrIsHeadAt(psr, lOff)) {
            lRes.isFound = true;
            lRes.discardLen = lOff;
            return lRes;
        }
    }

    lRes.partHeadLen = frmPsrFindPartHeadLen(psr, usedLen);
    lRes.discardLen = usedLen - lRes.partHeadLen;
    return lRes;
}

static void frmPsrClrPend(stFrmPsr *psr)
{
    psr->pendPktTick = 0U;
    psr->pendPktLen = 0U;
    psr->hasPendPkt = false;
}

static uint32_t frmPsrGetTickMs(const stFrmPsr *psr)
{
    if ((psr == NULL) || (psr->cfg.getTick == NULL)) {
        return 0U;
    }

    return psr->cfg.getTick();
}

static bool frmPsrIsPktTout(const stFrmPsr *psr)
{
    uint32_t lNowTick;

    if ((psr == NULL) || (psr->hasPendPkt == false) || (psr->cfg.waitPktToutMs == 0U)) {
        return false;
    }

    if (psr->cfg.getTick == NULL) {
        return false;
    }

    lNowTick = frmPsrGetTickMs(psr);
    return (uint32_t)(lNowTick - psr->pendPktTick) >= psr->cfg.waitPktToutMs;
}

static bool frmPsrFixOff(uint32_t pktLen, int32_t off, uint32_t *realOff)
{
    uint32_t lEndDist;

    if (realOff == NULL) {
        return false;
    }

    if (off >= 0) {
        *realOff = (uint32_t)off;
        return *realOff < pktLen;
    }

    lEndDist = (uint32_t)(-off);
    if ((lEndDist == 0U) || (lEndDist > pktLen)) {
        return false;
    }

    *realOff = pktLen - lEndDist;
    return true;
}

static bool frmPsrGetPktCrc(const stFrmPsrCfg *cfg, const uint8_t *pktBuf, uint32_t pktLen, uint32_t *pktCrc)
{
    uint32_t lFieldOff;
    uint32_t lIdx;
    uint32_t lVal = 0U;

    if ((cfg == NULL) || (pktBuf == NULL) || (pktCrc == NULL)) {
        return false;
    }

    if ((!frmPsrFixOff(pktLen, cfg->crcFieldOff, &lFieldOff)) ||
        (cfg->crcFieldLen == 0U) ||
        (cfg->crcFieldLen > sizeof(uint32_t)) ||
        ((lFieldOff + cfg->crcFieldLen) > pktLen)) {
        return false;
    }

    if (cfg->crcFieldEnd == FRM_PSR_CRC_END_BIG) {
        for (lIdx = 0U; lIdx < cfg->crcFieldLen; lIdx++) {
            lVal = (lVal << 8U) | pktBuf[lFieldOff + lIdx];
        }
    } else {
        for (lIdx = 0U; lIdx < cfg->crcFieldLen; lIdx++) {
            lVal |= ((uint32_t)pktBuf[lFieldOff + lIdx]) << (8U * lIdx);
        }
    }

    *pktCrc = lVal;
    return true;
}

static bool frmPsrSetPktCrc(int32_t crcFieldOff, uint8_t crcFieldLen, eFrmPsrCrcEnd crcFieldEnd, uint8_t *pktBuf, uint32_t pktLen, uint32_t pktCrc)
{
    uint32_t lFieldOff;
    uint32_t lIdx;

    if ((pktBuf == NULL) || (crcFieldLen == 0U) || (crcFieldLen > sizeof(uint32_t))) {
        return false;
    }

    if ((!frmPsrFixOff(pktLen, crcFieldOff, &lFieldOff)) || ((lFieldOff + crcFieldLen) > pktLen)) {
        return false;
    }

    if (crcFieldEnd == FRM_PSR_CRC_END_BIG) {
        for (lIdx = 0U; lIdx < crcFieldLen; lIdx++) {
            pktBuf[lFieldOff + lIdx] = (uint8_t)((pktCrc >> (8U * (crcFieldLen - lIdx - 1U))) & 0xFFU);
        }
    } else {
        for (lIdx = 0U; lIdx < crcFieldLen; lIdx++) {
            pktBuf[lFieldOff + lIdx] = (uint8_t)((pktCrc >> (8U * lIdx)) & 0xFFU);
        }
    }

    return true;
}

static bool frmPsrChkPktCrc(const stFrmPsrCfg *cfg, const uint8_t *pktBuf, uint32_t pktLen)
{
    uint32_t lRangeStart;
    uint32_t lRangeEnd;
    uint32_t lPktCrc;
    uint32_t lCalcCrc;

    if ((cfg == NULL) || (pktBuf == NULL) || (cfg->crcCalcFunc == NULL)) {
        return false;
    }

    if ((!frmPsrFixOff(pktLen, cfg->crcRangeStartOff, &lRangeStart)) || (!frmPsrFixOff(pktLen, cfg->crcRangeEndOff, &lRangeEnd)) || (lRangeEnd < lRangeStart)) {
        return false;
    }

    if (!frmPsrGetPktCrc(cfg, pktBuf, pktLen, &lPktCrc)) {
        return false;
    }

    lCalcCrc = cfg->crcCalcFunc(&pktBuf[lRangeStart], (lRangeEnd - lRangeStart) + 1U, cfg->userCtx);
    return lCalcCrc == lPktCrc;
}

static void frmPsrLoadCfgByFmt(stFrmPsrCfg *cfg, const stFrmPsrFmt *fmt, const stFrmPsrRunCfg *runCfg)
{
    if ((cfg == NULL) || (fmt == NULL) || (runCfg == NULL)) {
        return;
    }

    (void)memset(cfg, 0, sizeof(*cfg));
    cfg->headPat = fmt->rxFmt.headPat;
    cfg->headPatLen = fmt->rxFmt.headPatLen;
    cfg->minHeadLen = fmt->rxFmt.minHeadLen;
    cfg->minPktLen = fmt->rxFmt.minPktLen;
    cfg->maxPktLen = fmt->rxFmt.maxPktLen;
    cfg->waitPktToutMs = runCfg->waitPktToutMs;
    cfg->crcRangeStartOff = fmt->rxFmt.crcRangeStartOff;
    cfg->crcRangeEndOff = fmt->rxFmt.crcRangeEndOff;
    cfg->crcFieldOff = fmt->rxFmt.crcFieldOff;
    cfg->crcFieldLen = fmt->rxFmt.crcFieldLen;
    cfg->crcFieldEnd = fmt->rxFmt.crcFieldEnd;
    cfg->outBuf = runCfg->outBuf;
    cfg->outBufSize = runCfg->outBufSize;
    cfg->headLenFunc = fmt->rxFmt.headLenFunc;
    cfg->pktLenFunc = fmt->rxFmt.pktLenFunc;
    cfg->crcCalcFunc = fmt->rxFmt.crcCalcFunc;
    cfg->getTick = runCfg->getTick;
    cfg->userCtx = fmt->rxFmt.userCtx;
}

static eFrmPsrSta frmPsrMkPktInner(const stFrmPsrTxFmt *txFmt, const uint8_t *payloadBuf, uint32_t payloadLen, uint8_t *pktBuf, uint32_t pktBufSize, uint32_t *pktLen)
{
    uint32_t lHeadLen;
    uint32_t lPktLen;
    uint32_t lRangeStart;
    uint32_t lRangeEnd;
    uint32_t lCalcCrc;

    if ((!frmPsrIsTxFmtValid(txFmt)) || (pktBuf == NULL) || (pktLen == NULL) || ((payloadBuf == NULL) && (payloadLen != 0U))) {
        return FRM_PSR_INVALID_ARG;
    }

    if (txFmt->headBuildFunc != NULL) {
        lHeadLen = txFmt->headBuildFunc(pktBuf, pktBufSize, payloadLen, txFmt->userCtx);
    } else {
        lHeadLen = txFmt->headPatLen;
        if (lHeadLen > pktBufSize) {
            return FRM_PSR_OUT_BUF_SMALL;
        }
        (void)memcpy(pktBuf, txFmt->headPat, lHeadLen);
    }

    if (lHeadLen == 0U) {
        return FRM_PSR_HEAD_INVALID;
    }

    if (txFmt->pktLenFunc != NULL) {
        lPktLen = txFmt->pktLenFunc(lHeadLen, payloadLen, txFmt->userCtx);
    } else {
        lPktLen = lHeadLen + payloadLen + txFmt->crcFieldLen;
    }

    if ((lPktLen < lHeadLen) || ((lHeadLen + payloadLen) > lPktLen) || (lPktLen < txFmt->minPktLen) || (lPktLen > txFmt->maxPktLen)) {
        return FRM_PSR_LEN_INVALID;
    }

    if (pktBufSize < lPktLen) {
        return FRM_PSR_OUT_BUF_SMALL;
    }

    if (lPktLen > lHeadLen) {
        (void)memset(&pktBuf[lHeadLen], 0, lPktLen - lHeadLen);
    }

    if (payloadLen > 0U) {
        (void)memcpy(&pktBuf[lHeadLen], payloadBuf, payloadLen);
    }

    if ((txFmt->pktFinFunc != NULL) && (!txFmt->pktFinFunc(pktBuf, lPktLen, lHeadLen, payloadBuf, payloadLen, txFmt->userCtx))) {
        return FRM_PSR_BUILD_FAIL;
    }

    if ((!frmPsrFixOff(lPktLen, txFmt->crcRangeStartOff, &lRangeStart)) || (!frmPsrFixOff(lPktLen, txFmt->crcRangeEndOff, &lRangeEnd)) || (lRangeEnd < lRangeStart)) {
        return FRM_PSR_CRC_FAIL;
    }

    lCalcCrc = txFmt->crcCalcFunc(&pktBuf[lRangeStart], (lRangeEnd - lRangeStart) + 1U, txFmt->userCtx);
    if (!frmPsrSetPktCrc(txFmt->crcFieldOff, txFmt->crcFieldLen, txFmt->crcFieldEnd, pktBuf, lPktLen, lCalcCrc)) {
        return FRM_PSR_CRC_FAIL;
    }

    *pktLen = lPktLen;
    return FRM_PSR_OK;
}

bool frmPsrIsRunCfgValid(const stFrmPsrRunCfg *runCfg)
{
    if ((runCfg == NULL) || (runCfg->outBuf == NULL) || (runCfg->outBufSize == 0U)) {
        return false;
    }

    return true;
}

bool frmPsrIsRxFmtValid(const stFrmPsrRxFmt *rxFmt)
{
    if ((rxFmt == NULL) ||
        (rxFmt->headPat == NULL) ||
        (rxFmt->headPatLen == 0U) ||
        (rxFmt->minHeadLen < rxFmt->headPatLen) ||
        (rxFmt->minPktLen < rxFmt->minHeadLen) ||
        (rxFmt->maxPktLen < rxFmt->minPktLen) ||
        (rxFmt->pktLenFunc == NULL) ||
        (rxFmt->crcCalcFunc == NULL) ||
        (rxFmt->crcFieldLen == 0U) ||
        (rxFmt->crcFieldLen > sizeof(uint32_t))) {
        return false;
    }

    return true;
}

bool frmPsrIsTxFmtValid(const stFrmPsrTxFmt *txFmt)
{
    if ((txFmt == NULL) ||
        ((txFmt->headBuildFunc == NULL) && ((txFmt->headPat == NULL) || (txFmt->headPatLen == 0U))) ||
        (txFmt->minPktLen == 0U) ||
        (txFmt->maxPktLen < txFmt->minPktLen) ||
        (txFmt->crcCalcFunc == NULL) ||
        (txFmt->crcFieldLen == 0U) ||
        (txFmt->crcFieldLen > sizeof(uint32_t))) {
        return false;
    }

    return true;
}

bool frmPsrIsFmtValid(const stFrmPsrFmt *fmt)
{
    if (fmt == NULL) {
        return false;
    }

    return frmPsrIsRxFmtValid(&fmt->rxFmt) && frmPsrIsTxFmtValid(&fmt->txFmt);
}

bool frmPsrIsCfgValid(const stFrmPsrCfg *cfg)
{
    if ((cfg == NULL) ||
        (cfg->headPat == NULL) ||
        (cfg->headPatLen == 0U) ||
        (cfg->minHeadLen < cfg->headPatLen) ||
        (cfg->minPktLen < cfg->minHeadLen) ||
        (cfg->maxPktLen < cfg->minPktLen) ||
        (cfg->outBuf == NULL) ||
        (cfg->outBufSize < cfg->minHeadLen) ||
        (cfg->pktLenFunc == NULL) ||
        (cfg->crcCalcFunc == NULL) ||
        (cfg->crcFieldLen == 0U) ||
        (cfg->crcFieldLen > sizeof(uint32_t))) {
        return false;
    }

    return true;
}

eFrmPsrSta frmPsrInit(stFrmPsr *psr, stRingBuffer *ringBuf, const stFrmPsrCfg *cfg)
{
    if ((psr == NULL) || (ringBuf == NULL) || (!frmPsrIsCfgValid(cfg))) {
        return FRM_PSR_INVALID_ARG;
    }

    (void)memset(psr, 0, sizeof(*psr));
    psr->ringBuf = ringBuf;
    psr->cfg = *cfg;
    psr->fmt = NULL;
    psr->isInit = true;
    return FRM_PSR_OK;
}

eFrmPsrSta frmPsrInitFmt(stFrmPsr *psr, stRingBuffer *ringBuf, const stFrmPsrFmt *fmt, const stFrmPsrRunCfg *runCfg)
{
    stFrmPsrCfg lCfg;
    eFrmPsrSta lSta;

    if ((!frmPsrIsFmtValid(fmt)) || (!frmPsrIsRunCfgValid(runCfg))) {
        return FRM_PSR_FMT_INVALID;
    }

    frmPsrLoadCfgByFmt(&lCfg, fmt, runCfg);
    if (!frmPsrIsCfgValid(&lCfg)) {
        return FRM_PSR_FMT_INVALID;
    }

    lSta = frmPsrInit(psr, ringBuf, &lCfg);
    if (lSta != FRM_PSR_OK) {
        return lSta;
    }

    psr->fmt = fmt;
    return FRM_PSR_OK;
}

void frmPsrReset(stFrmPsr *psr)
{
    if (psr == NULL) {
        return;
    }

    psr->pkt.buf = NULL;
    psr->pkt.len = 0U;
    psr->hasReadyPkt = false;
    frmPsrClrPend(psr);
}

eFrmPsrSta frmPsrSelFmt(stFrmPsr *psr, const stFrmPsrFmt *fmt)
{
    stFrmPsrRunCfg lRunCfg;

    if ((psr == NULL) || (!psr->isInit)) {
        return FRM_PSR_INVALID_ARG;
    }

    if (!frmPsrIsFmtValid(fmt)) {
        return FRM_PSR_FMT_INVALID;
    }

    lRunCfg.waitPktToutMs = psr->cfg.waitPktToutMs;
    lRunCfg.outBuf = psr->cfg.outBuf;
    lRunCfg.outBufSize = psr->cfg.outBufSize;
    lRunCfg.getTick = psr->cfg.getTick;
    frmPsrLoadCfgByFmt(&psr->cfg, fmt, &lRunCfg);
    if (!frmPsrIsCfgValid(&psr->cfg)) {
        return FRM_PSR_FMT_INVALID;
    }

    psr->fmt = fmt;
    frmPsrReset(psr);
    return FRM_PSR_OK;
}

const stFrmPsrFmt *frmPsrGetFmt(const stFrmPsr *psr)
{
    if ((psr == NULL) || (!psr->isInit)) {
        return NULL;
    }

    return psr->fmt;
}

eFrmPsrSta frmPsrMkPkt(const stFrmPsr *psr, const uint8_t *payloadBuf, uint16_t payloadLen, uint8_t *pktBuf, uint16_t pktBufSize, uint16_t *pktLen)
{
    uint32_t lPktLen;
    eFrmPsrSta lSta;

    if ((psr == NULL) || (!psr->isInit)) {
        return FRM_PSR_INVALID_ARG;
    }

    if (psr->fmt == NULL) {
        return FRM_PSR_FMT_NOT_SEL;
    }

    lSta = frmPsrMkPktInner(&psr->fmt->txFmt, payloadBuf, payloadLen, pktBuf, pktBufSize, &lPktLen);
    if ((lSta == FRM_PSR_OK) && (pktLen != NULL)) {
        *pktLen = (uint16_t)lPktLen;
    }

    return lSta;
}

eFrmPsrSta frmPsrMkPktByFmt(const stFrmPsrFmt *fmt, const uint8_t *payloadBuf, uint16_t payloadLen, uint8_t *pktBuf, uint16_t pktBufSize, uint16_t *pktLen)
{
    uint32_t lPktLen;
    eFrmPsrSta lSta;

    if (!frmPsrIsFmtValid(fmt)) {
        return FRM_PSR_FMT_INVALID;
    }

    lSta = frmPsrMkPktInner(&fmt->txFmt, payloadBuf, payloadLen, pktBuf, pktBufSize, &lPktLen);
    if ((lSta == FRM_PSR_OK) && (pktLen != NULL)) {
        *pktLen = (uint16_t)lPktLen;
    }

    return lSta;
}

eFrmPsrSta frmPsrProc(stFrmPsr *psr, stFrmPsrPkt *pkt)
{
    eFrmPsrSta lLastSta = FRM_PSR_OK;
    uint32_t lUsedLen;
    stFrmPsrHeadHit lHeadHit;
    uint32_t lHeadLen;
    uint32_t lPktLen;
    uint32_t lPeekLen;

    if ((psr == NULL) || (!psr->isInit) || (!frmPsrIsCfgValid(&psr->cfg))) {
        return FRM_PSR_INVALID_ARG;
    }

    if (psr->hasReadyPkt) {
        if (pkt != NULL) {
            *pkt = psr->pkt;
        }
        return FRM_PSR_OK;
    }

    while (true) {
        lUsedLen = ringBufferGetUsed(psr->ringBuf);
        if (lUsedLen == 0U) {
            frmPsrClrPend(psr);
            return (lLastSta != FRM_PSR_OK) ? lLastSta : FRM_PSR_EMPTY;
        }

        lHeadHit = frmPsrFindHead(psr, lUsedLen);
        if (!lHeadHit.isFound) {
            if (lHeadHit.discardLen > 0U) {
                (void)ringBufferDiscard(psr->ringBuf, lHeadHit.discardLen);
                frmPsrClrPend(psr);
            }

            if (lHeadHit.partHeadLen > 0U) {
                return (lLastSta != FRM_PSR_OK) ? lLastSta : FRM_PSR_NEED_MORE_DATA;
            }

            return (lLastSta != FRM_PSR_OK) ? lLastSta : FRM_PSR_HEAD_NOT_FOUND;
        }

        if (lHeadHit.discardLen > 0U) {
            (void)ringBufferDiscard(psr->ringBuf, lHeadHit.discardLen);
            frmPsrClrPend(psr);
        }

        lUsedLen = ringBufferGetUsed(psr->ringBuf);
        if (lUsedLen < psr->cfg.headPatLen) {
            return (lLastSta != FRM_PSR_OK) ? lLastSta : FRM_PSR_NEED_MORE_DATA;
        }

        if (lUsedLen < psr->cfg.minHeadLen) {
            if (!psr->hasPendPkt) {
                psr->hasPendPkt = true;
                psr->pendPktTick = frmPsrGetTickMs(psr);
                psr->pendPktLen = 0U;
            }

            if (frmPsrIsPktTout(psr)) {
                LOG_W(FRM_PSR_LOG_TAG,
                      "Packet wait timeout before head complete, used=%u discard=%u",
                      (unsigned int)lUsedLen,
                      (unsigned int)psr->cfg.headPatLen);
                (void)ringBufferDiscard(psr->ringBuf, psr->cfg.headPatLen);
                frmPsrClrPend(psr);
                lLastSta = FRM_PSR_NEED_MORE_DATA;
                continue;
            }

            return (lLastSta != FRM_PSR_OK) ? lLastSta : FRM_PSR_NEED_MORE_DATA;
        }

        lPeekLen = frmPsrMinU32(lUsedLen, psr->cfg.maxPktLen);
        lPeekLen = frmPsrMinU32(lPeekLen, psr->cfg.outBufSize);
        if (frmPsrPeekOff(psr->ringBuf, 0U, psr->cfg.outBuf, lPeekLen) != lPeekLen) {
            return FRM_PSR_INVALID_ARG;
        }

        if (psr->cfg.headLenFunc != NULL) {
            lHeadLen = psr->cfg.headLenFunc(psr->cfg.outBuf, lPeekLen, psr->cfg.userCtx);
        } else {
            lHeadLen = psr->cfg.headPatLen;
        }

        if ((lHeadLen < psr->cfg.minHeadLen) || (lHeadLen > psr->cfg.maxPktLen)) {
            (void)ringBufferDiscard(psr->ringBuf, 1U);
            frmPsrClrPend(psr);
            lLastSta = FRM_PSR_HEAD_INVALID;
            continue;
        }

        lPktLen = psr->cfg.pktLenFunc(psr->cfg.outBuf, lHeadLen, lPeekLen, psr->cfg.userCtx);
        if ((lPktLen < lHeadLen) || (lPktLen < psr->cfg.minPktLen) || (lPktLen > psr->cfg.maxPktLen)) {
            (void)ringBufferDiscard(psr->ringBuf, 1U);
            frmPsrClrPend(psr);
            lLastSta = FRM_PSR_LEN_INVALID;
            continue;
        }

        if (psr->cfg.outBufSize < lPktLen) {
            return FRM_PSR_OUT_BUF_SMALL;
        }

        if (lUsedLen < lPktLen) {
            if ((!psr->hasPendPkt) || (psr->pendPktLen != lPktLen)) {
                psr->hasPendPkt = true;
                psr->pendPktTick = frmPsrGetTickMs(psr);
                psr->pendPktLen = (uint16_t)lPktLen;
            }

            if (frmPsrIsPktTout(psr)) {
                LOG_W(FRM_PSR_LOG_TAG,
                      "Packet body timeout, used=%u pkt=%u discard=%u",
                      (unsigned int)lUsedLen,
                      (unsigned int)lPktLen,
                      (unsigned int)lHeadLen);
                (void)ringBufferDiscard(psr->ringBuf, lHeadLen);
                frmPsrClrPend(psr);
                lLastSta = FRM_PSR_NEED_MORE_DATA;
                continue;
            }

            return (lLastSta != FRM_PSR_OK) ? lLastSta : FRM_PSR_NEED_MORE_DATA;
        }

        if (frmPsrPeekOff(psr->ringBuf, 0U, psr->cfg.outBuf, lPktLen) != lPktLen) {
            return FRM_PSR_INVALID_ARG;
        }

        if (!frmPsrChkPktCrc(&psr->cfg, psr->cfg.outBuf, lPktLen)) {
            (void)ringBufferDiscard(psr->ringBuf, 1U);
            frmPsrClrPend(psr);
            lLastSta = FRM_PSR_CRC_FAIL;
            continue;
        }

        (void)ringBufferDiscard(psr->ringBuf, lPktLen);
        frmPsrClrPend(psr);
        psr->pkt.buf = psr->cfg.outBuf;
        psr->pkt.len = (uint16_t)lPktLen;
        psr->hasReadyPkt = true;

        if (pkt != NULL) {
            *pkt = psr->pkt;
        }

        return FRM_PSR_OK;
    }
}

bool frmPsrHasPkt(const stFrmPsr *psr)
{
    return (psr != NULL) && psr->hasReadyPkt;
}

const stFrmPsrPkt *frmPsrGetPkt(const stFrmPsr *psr)
{
    if ((psr == NULL) || (!psr->hasReadyPkt)) {
        return NULL;
    }

    return &psr->pkt;
}

void frmPsrFreePkt(stFrmPsr *psr)
{
    if (psr == NULL) {
        return;
    }

    psr->pkt.buf = NULL;
    psr->pkt.len = 0U;
    psr->hasReadyPkt = false;
}
/**************************End of file********************************/

