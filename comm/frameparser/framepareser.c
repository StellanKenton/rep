/***********************************************************************************
* @file     : framepareser.c
* @brief    : Stream packet parser implementation.
* @details  : Locates packet headers in a byte stream and validates complete frames.
* @author   : GitHub Copilot
* @date     : 2026-04-01
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "framepareser.h"

#include <string.h>

#include "../../service/log/log.h"

#define FRM_PSR_LOG_TAG  "FrmPsr"

uint32_t frmPsrGetPlatformTickMs(void);
void frmPsrLoadPlatformDefaultProtoCfg(uint32_t protocolId, stFrmPsrProtoCfg *protoCfg);

__attribute__((weak)) uint32_t frmPsrGetPlatformTickMs(void)
{
    return 0U;
}

__attribute__((weak)) void frmPsrLoadPlatformDefaultProtoCfg(uint32_t protocolId, stFrmPsrProtoCfg *protoCfg)
{
    (void)protocolId;

    if (protoCfg != NULL) {
        (void)memset(protoCfg, 0, sizeof(*protoCfg));
    }
}

static uint32_t frmPsrMinU32(uint32_t left, uint32_t right);
static uint32_t frmPsrToPhyIdx(const stRingBuffer *ringBuf, uint32_t logIdx);
static uint32_t frmPsrPeekOff(const stRingBuffer *ringBuf, uint32_t off, uint8_t *buf, uint32_t len);
static bool frmPsrPeekByte(const stRingBuffer *ringBuf, uint32_t off, uint8_t *val);
static bool frmPsrGetHeadPatLenAt(const stFrmPsr *psr, uint32_t off, uint16_t *headPatLen);
static uint32_t frmPsrFindPartHeadLen(const stFrmPsr *psr, uint32_t usedLen);
static stFrmPsrHeadHit frmPsrFindHead(const stFrmPsr *psr, uint32_t usedLen);
static bool frmPsrIsProtoCfgValid(const stFrmPsrProtoCfg *protoCfg);
static bool frmPsrIsCfgValid(const stFrmPsrCfg *cfg);
static void frmPsrClrPend(stFrmPsr *psr);
static void frmPsrClrReady(stFrmPsr *psr);
static uint32_t frmPsrGetTickMs(const stFrmPsr *psr);
static bool frmPsrIsPktTout(const stFrmPsr *psr);
static bool frmPsrFixOff(uint32_t pktLen, int32_t off, uint32_t *realOff);
static bool frmPsrIsFieldCfgValid(uint16_t fieldOff, uint8_t fieldLen, uint16_t headLen);
static bool frmPsrGetPktCrc(const stFrmPsrProtoCfg *protoCfg, const uint8_t *pktBuf, uint32_t pktLen, uint32_t *pktCrc);
static bool frmPsrChkPktCrc(const stFrmPsrProtoCfg *protoCfg, const uint8_t *pktBuf, uint32_t pktLen);
static void frmPsrLoadPktView(stFrmPsr *psr, uint32_t pktLen, uint32_t headLen);

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

static bool frmPsrGetHeadPatLenAt(const stFrmPsr *psr, uint32_t off, uint16_t *headPatLen)
{
    uint32_t lPatIdx;
    uint32_t lCmpIdx;
    uint8_t lVal;

    if ((psr == NULL) || (headPatLen == NULL)) {
        return false;
    }

    for (lPatIdx = 0U; lPatIdx < psr->protoCfg.headPatCount; lPatIdx++) {
        for (lCmpIdx = 0U; lCmpIdx < psr->protoCfg.headPatLen; lCmpIdx++) {
            if ((!frmPsrPeekByte(&psr->ringBuf, off + lCmpIdx, &lVal)) || (lVal != psr->protoCfg.headPatList[lPatIdx][lCmpIdx])) {
                break;
            }
        }

        if (lCmpIdx == psr->protoCfg.headPatLen) {
            *headPatLen = psr->protoCfg.headPatLen;
            return true;
        }
    }

    return false;
}

static uint32_t frmPsrFindPartHeadLen(const stFrmPsr *psr, uint32_t usedLen)
{
    uint32_t lPatIdx;
    uint32_t lCandLen;
    uint32_t lCmpIdx;
    uint8_t lVal;

    if ((psr->protoCfg.headPatLen <= 1U) || (usedLen == 0U)) {
        return 0U;
    }

    for (lCandLen = frmPsrMinU32(usedLen, (uint32_t)psr->protoCfg.headPatLen - 1U); lCandLen > 0U; lCandLen--) {
        for (lPatIdx = 0U; lPatIdx < psr->protoCfg.headPatCount; lPatIdx++) {
            for (lCmpIdx = 0U; lCmpIdx < lCandLen; lCmpIdx++) {
                if ((!frmPsrPeekByte(&psr->ringBuf, usedLen - lCandLen + lCmpIdx, &lVal)) || (lVal != psr->protoCfg.headPatList[lPatIdx][lCmpIdx])) {
                    break;
                }
            }

            if (lCmpIdx == lCandLen) {
                return lCandLen;
            }
        }
    }

    return 0U;
}

static stFrmPsrHeadHit frmPsrFindHead(const stFrmPsr *psr, uint32_t usedLen)
{
    stFrmPsrHeadHit lRes = {0U, 0U, 0U, false};
    uint32_t lOff;

    for (lOff = 0U; lOff < usedLen; lOff++) {
        if (frmPsrGetHeadPatLenAt(psr, lOff, &lRes.headPatLen)) {
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

static bool frmPsrIsProtoCfgValid(const stFrmPsrProtoCfg *protoCfg)
{
    uint32_t lIdx;

    if ((protoCfg == NULL) ||
        (protoCfg->headPatCount == 0U) ||
        (protoCfg->headPatCount > FRM_PSR_HEAD_PAT_MAX) ||
        (protoCfg->headPatLen == 0U) ||
        (protoCfg->minHeadLen < protoCfg->headPatLen) ||
        (protoCfg->minPktLen < protoCfg->minHeadLen) ||
        (protoCfg->maxPktLen < protoCfg->minPktLen) ||
        (protoCfg->pktLenFunc == NULL) ||
        (protoCfg->crcCalcFunc == NULL) ||
        (!frmPsrIsFieldCfgValid(protoCfg->cmdindex, protoCfg->cmdLen, protoCfg->minHeadLen)) ||
        (!frmPsrIsFieldCfgValid(protoCfg->packlenindex, protoCfg->packlenLen, protoCfg->minHeadLen)) ||
        (protoCfg->crcFieldLen == 0U) ||
        (protoCfg->crcFieldLen > sizeof(uint32_t))) {
        return false;
    }

    for (lIdx = 0U; lIdx < protoCfg->headPatCount; lIdx++) {
        if (protoCfg->headPatList[lIdx] == NULL) {
            return false;
        }
    }

    return true;
}

static bool frmPsrIsCfgValid(const stFrmPsrCfg *cfg)
{
    if ((cfg == NULL) ||
        (cfg->streamBuf == NULL) ||
        (cfg->streamBufSize == 0U) ||
        (cfg->frameBuf == NULL) ||
        (cfg->frameBufSize == 0U)) {
        return false;
    }

    return true;
}

static void frmPsrClrReady(stFrmPsr *psr)
{
    psr->pkt.buf = NULL;
    psr->pkt.headBuf = NULL;
    psr->pkt.dataBuf = NULL;
    psr->pkt.crcBuf = NULL;
    psr->pkt.cmdBuf = NULL;
    psr->pkt.pktLenBuf = NULL;
    psr->pkt.len = 0U;
    psr->pkt.headLen = 0U;
    psr->pkt.dataLen = 0U;
    psr->pkt.crcLen = 0U;
    psr->pkt.cmdFieldOff = 0U;
    psr->pkt.pktLenFieldOff = 0U;
    psr->pkt.crcFieldOff = 0U;
    psr->pkt.cmdFieldLen = 0U;
    psr->pkt.pktLenFieldLen = 0U;
    psr->pkt.crcVal = 0U;
    psr->hasReadyPkt = false;
}

static uint32_t frmPsrGetTickMs(const stFrmPsr *psr)
{
    if ((psr == NULL) || (psr->protoCfg.getTick == NULL)) {
        return 0U;
    }

    return psr->protoCfg.getTick();
}

static bool frmPsrIsPktTout(const stFrmPsr *psr)
{
    uint32_t lNowTick;

    if ((psr == NULL) || (psr->hasPendPkt == false) || (psr->protoCfg.waitPktToutMs == 0U)) {
        return false;
    }

    if (psr->protoCfg.getTick == NULL) {
        return false;
    }

    lNowTick = frmPsrGetTickMs(psr);
    return (uint32_t)(lNowTick - psr->pendPktTick) >= psr->protoCfg.waitPktToutMs;
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

static bool frmPsrIsFieldCfgValid(uint16_t fieldOff, uint8_t fieldLen, uint16_t headLen)
{
    if (fieldLen == 0U) {
        return true;
    }

    return ((uint32_t)fieldOff + (uint32_t)fieldLen) <= headLen;
}

static bool frmPsrGetPktCrc(const stFrmPsrProtoCfg *protoCfg, const uint8_t *pktBuf, uint32_t pktLen, uint32_t *pktCrc)
{
    uint32_t lFieldOff;
    uint32_t lIdx;
    uint32_t lVal = 0U;

    if ((protoCfg == NULL) || (pktBuf == NULL) || (pktCrc == NULL)) {
        return false;
    }

    if ((!frmPsrFixOff(pktLen, protoCfg->crcFieldOff, &lFieldOff)) ||
        (protoCfg->crcFieldLen == 0U) ||
        (protoCfg->crcFieldLen > sizeof(uint32_t)) ||
        ((lFieldOff + protoCfg->crcFieldLen) > pktLen)) {
        return false;
    }

    if (protoCfg->crcFieldEnd == FRM_PSR_CRC_END_BIG) {
        for (lIdx = 0U; lIdx < protoCfg->crcFieldLen; lIdx++) {
            lVal = (lVal << 8U) | pktBuf[lFieldOff + lIdx];
        }
    } else {
        for (lIdx = 0U; lIdx < protoCfg->crcFieldLen; lIdx++) {
            lVal |= ((uint32_t)pktBuf[lFieldOff + lIdx]) << (8U * lIdx);
        }
    }

    *pktCrc = lVal;
    return true;
}

static bool frmPsrChkPktCrc(const stFrmPsrProtoCfg *protoCfg, const uint8_t *pktBuf, uint32_t pktLen)
{
    uint32_t lRangeStart;
    uint32_t lRangeEnd;
    uint32_t lPktCrc;
    uint32_t lCalcCrc;

    if ((protoCfg == NULL) || (pktBuf == NULL) || (protoCfg->crcCalcFunc == NULL)) {
        return false;
    }

    if ((!frmPsrFixOff(pktLen, protoCfg->crcRangeStartOff, &lRangeStart)) || (!frmPsrFixOff(pktLen, protoCfg->crcRangeEndOff, &lRangeEnd)) || (lRangeEnd < lRangeStart)) {
        return false;
    }

    if (!frmPsrGetPktCrc(protoCfg, pktBuf, pktLen, &lPktCrc)) {
        return false;
    }

    lCalcCrc = protoCfg->crcCalcFunc(&pktBuf[lRangeStart], (lRangeEnd - lRangeStart) + 1U, protoCfg->userCtx);
    return lCalcCrc == lPktCrc;
}

static void frmPsrLoadPktView(stFrmPsr *psr, uint32_t pktLen, uint32_t headLen)
{
    uint32_t lCrcFieldOff;

    if ((psr == NULL) || (psr->frameBuf == NULL)) {
        return;
    }

    frmPsrClrReady(psr);
    psr->pkt.buf = psr->frameBuf;
    psr->pkt.headBuf = psr->frameBuf;
    psr->pkt.len = (uint16_t)pktLen;
    psr->pkt.headLen = (uint16_t)headLen;

    if (frmPsrIsFieldCfgValid(psr->protoCfg.cmdindex, psr->protoCfg.cmdLen, psr->pkt.headLen)) {
        psr->pkt.cmdFieldOff = psr->protoCfg.cmdindex;
        psr->pkt.cmdFieldLen = psr->protoCfg.cmdLen;
        if (psr->pkt.cmdFieldLen > 0U) {
            psr->pkt.cmdBuf = &psr->frameBuf[psr->pkt.cmdFieldOff];
        }
    }

    if (frmPsrIsFieldCfgValid(psr->protoCfg.packlenindex, psr->protoCfg.packlenLen, psr->pkt.headLen)) {
        psr->pkt.pktLenFieldOff = psr->protoCfg.packlenindex;
        psr->pkt.pktLenFieldLen = psr->protoCfg.packlenLen;
        if (psr->pkt.pktLenFieldLen > 0U) {
            psr->pkt.pktLenBuf = &psr->frameBuf[psr->pkt.pktLenFieldOff];
        }
    }

    if (headLen < pktLen) {
        psr->pkt.dataBuf = &psr->frameBuf[headLen];
        psr->pkt.dataLen = (uint16_t)(pktLen - headLen);
    }

    if ((!frmPsrFixOff(pktLen, psr->protoCfg.crcFieldOff, &lCrcFieldOff)) ||
        (psr->protoCfg.crcFieldLen == 0U) ||
        ((lCrcFieldOff + psr->protoCfg.crcFieldLen) > pktLen)) {
        return;
    }

    psr->pkt.crcBuf = &psr->frameBuf[lCrcFieldOff];
    psr->pkt.crcLen = psr->protoCfg.crcFieldLen;
    psr->pkt.crcFieldOff = (uint16_t)lCrcFieldOff;
    (void)frmPsrGetPktCrc(&psr->protoCfg, psr->frameBuf, pktLen, &psr->pkt.crcVal);

    if (lCrcFieldOff >= headLen) {
        psr->pkt.dataLen = (uint16_t)(lCrcFieldOff - headLen);
    }
}

eFrmPsrSta frmPsrInit(stFrmPsr *psr, const stFrmPsrCfg *cfg)
{
    eRingBufferStatus lRbSta;

    if ((psr == NULL) || (!frmPsrIsCfgValid(cfg))) {
        return FRM_PSR_INVALID_ARG;
    }

    (void)memset(psr, 0, sizeof(*psr));
    frmPsrLoadPlatformDefaultProtoCfg(cfg->protocolId, &psr->protoCfg);
    if (!frmPsrIsProtoCfgValid(&psr->protoCfg)) {
        return FRM_PSR_PROTO_INVALID;
    }

    if (psr->protoCfg.getTick == NULL) {
        psr->protoCfg.getTick = frmPsrGetPlatformTickMs;
    }

    psr->streamBuf = cfg->streamBuf;
    psr->streamBufSize = cfg->streamBufSize;
    psr->frameBuf = cfg->frameBuf;
    psr->frameBufSize = cfg->frameBufSize;
    if (psr->frameBufSize < psr->protoCfg.maxPktLen) {
        return FRM_PSR_OUT_BUF_SMALL;
    }

    lRbSta = ringBufferInit(&psr->ringBuf, psr->streamBuf, psr->streamBufSize);
    if (lRbSta != RINGBUFFER_OK) {
        return FRM_PSR_INVALID_ARG;
    }

    frmPsrClrReady(psr);
    frmPsrClrPend(psr);
    psr->isInit = true;
    return FRM_PSR_OK;
}

eFrmPsrSta frmPsrFeed(stFrmPsr *psr, const uint8_t *buf, uint16_t len)
{
    if ((psr == NULL) || (!psr->isInit) || ((buf == NULL) && (len != 0U))) {
        return FRM_PSR_INVALID_ARG;
    }

    if (len == 0U) {
        return FRM_PSR_OK;
    }

    if (ringBufferGetFree(&psr->ringBuf) < len) {
        return FRM_PSR_OUT_BUF_SMALL;
    }

    if (ringBufferWrite(&psr->ringBuf, buf, len) != len) {
        return FRM_PSR_OUT_BUF_SMALL;
    }

    return FRM_PSR_OK;
}

eFrmPsrSta frmPsrProcess(stFrmPsr *psr)
{
    eFrmPsrSta lLastSta = FRM_PSR_OK;
    uint32_t lUsedLen;
    stFrmPsrHeadHit lHeadHit;
    uint32_t lHeadLen;
    uint32_t lPktLen;
    uint32_t lPeekLen;

    if ((psr == NULL) || (!psr->isInit) || (!frmPsrIsProtoCfgValid(&psr->protoCfg))) {
        return FRM_PSR_INVALID_ARG;
    }

    if (psr->hasReadyPkt) {
        return FRM_PSR_OK;
    }

    while (true) {
        lUsedLen = ringBufferGetUsed(&psr->ringBuf);
        if (lUsedLen == 0U) {
            frmPsrClrPend(psr);
            return (lLastSta != FRM_PSR_OK) ? lLastSta : FRM_PSR_EMPTY;
        }

        lHeadHit = frmPsrFindHead(psr, lUsedLen);
        if (!lHeadHit.isFound) {
            if (lHeadHit.discardLen > 0U) {
                (void)ringBufferDiscard(&psr->ringBuf, lHeadHit.discardLen);
                frmPsrClrPend(psr);
            }

            if (lHeadHit.partHeadLen > 0U) {
                return (lLastSta != FRM_PSR_OK) ? lLastSta : FRM_PSR_NEED_MORE_DATA;
            }

            return (lLastSta != FRM_PSR_OK) ? lLastSta : FRM_PSR_HEAD_NOT_FOUND;
        }

        if (lHeadHit.discardLen > 0U) {
            (void)ringBufferDiscard(&psr->ringBuf, lHeadHit.discardLen);
            frmPsrClrPend(psr);
        }

        lUsedLen = ringBufferGetUsed(&psr->ringBuf);
        if (lUsedLen < psr->protoCfg.minHeadLen) {
            if (!psr->hasPendPkt) {
                psr->hasPendPkt = true;
                psr->pendPktTick = frmPsrGetTickMs(psr);
                psr->pendPktLen = 0U;
            }

            if (frmPsrIsPktTout(psr)) {
                LOG_W(FRM_PSR_LOG_TAG,
                      "Packet wait timeout before head complete, used=%u discard=%u",
                      (unsigned int)lUsedLen,
                      (unsigned int)lHeadHit.headPatLen);
                (void)ringBufferDiscard(&psr->ringBuf, lHeadHit.headPatLen);
                frmPsrClrPend(psr);
                lLastSta = FRM_PSR_NEED_MORE_DATA;
                continue;
            }

            return (lLastSta != FRM_PSR_OK) ? lLastSta : FRM_PSR_NEED_MORE_DATA;
        }

        lPeekLen = frmPsrMinU32(lUsedLen, psr->protoCfg.maxPktLen);
        lPeekLen = frmPsrMinU32(lPeekLen, psr->frameBufSize);
        if (frmPsrPeekOff(&psr->ringBuf, 0U, psr->frameBuf, lPeekLen) != lPeekLen) {
            return FRM_PSR_INVALID_ARG;
        }

        if (psr->protoCfg.headLenFunc != NULL) {
            lHeadLen = psr->protoCfg.headLenFunc(psr->frameBuf, lPeekLen, psr->protoCfg.userCtx);
        } else {
            lHeadLen = lHeadHit.headPatLen;
        }

        if ((lHeadLen < psr->protoCfg.minHeadLen) || (lHeadLen > psr->protoCfg.maxPktLen)) {
            (void)ringBufferDiscard(&psr->ringBuf, 1U);
            frmPsrClrPend(psr);
            lLastSta = FRM_PSR_HEAD_INVALID;
            continue;
        }

        lPktLen = psr->protoCfg.pktLenFunc(psr->frameBuf, lHeadLen, lPeekLen, psr->protoCfg.userCtx);
        if ((lPktLen < lHeadLen) || (lPktLen < psr->protoCfg.minPktLen) || (lPktLen > psr->protoCfg.maxPktLen)) {
            (void)ringBufferDiscard(&psr->ringBuf, 1U);
            frmPsrClrPend(psr);
            lLastSta = FRM_PSR_LEN_INVALID;
            continue;
        }

        if (psr->frameBufSize < lPktLen) {
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
                    (void)ringBufferDiscard(&psr->ringBuf, lHeadLen);
                frmPsrClrPend(psr);
                lLastSta = FRM_PSR_NEED_MORE_DATA;
                continue;
            }

            return (lLastSta != FRM_PSR_OK) ? lLastSta : FRM_PSR_NEED_MORE_DATA;
        }

        if (frmPsrPeekOff(&psr->ringBuf, 0U, psr->frameBuf, lPktLen) != lPktLen) {
            return FRM_PSR_INVALID_ARG;
        }

        if (!frmPsrChkPktCrc(&psr->protoCfg, psr->frameBuf, lPktLen)) {
            (void)ringBufferDiscard(&psr->ringBuf, 1U);
            frmPsrClrPend(psr);
            lLastSta = FRM_PSR_CRC_FAIL;
            continue;
        }

        (void)ringBufferDiscard(&psr->ringBuf, lPktLen);
        frmPsrClrPend(psr);
        frmPsrLoadPktView(psr, lPktLen, lHeadLen);
        psr->hasReadyPkt = true;

        return FRM_PSR_OK;
    }
}

const stFrmPsrPkt *frmPsrRelease(stFrmPsr *psr)
{
    const stFrmPsrPkt *lPkt;

    if ((psr == NULL) || (!psr->hasReadyPkt)) {
        return NULL;
    }

    lPkt = &psr->pkt;
    psr->hasReadyPkt = false;
    return lPkt;
}

/**************************End of file********************************/
