/************************************************************************************
* @file     : framepareser.h
* @brief    : Stream packet parser public API.
* @details  : Reassembles complete packets from a byte stream ring buffer.
* @author   : GitHub Copilot
* @date     : 2026-04-01
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef FRM_PSR_H
#define FRM_PSR_H

#include <stdbool.h>
#include <stdint.h>

#include "ringbuffer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FRM_PSR_HEAD_PAT_MAX    5U

typedef enum eFrmPsrSta {
    FRM_PSR_OK = 0,
    FRM_PSR_EMPTY,
    FRM_PSR_NEED_MORE_DATA,
    FRM_PSR_INVALID_ARG,
    FRM_PSR_HEAD_NOT_FOUND,
    FRM_PSR_HEAD_INVALID,
    FRM_PSR_LEN_INVALID,
    FRM_PSR_CRC_FAIL,
    FRM_PSR_OUT_BUF_SMALL,
    FRM_PSR_PROTO_INVALID
} eFrmPsrSta;

typedef enum eFrmPsrCrcEnd {
    FRM_PSR_CRC_END_LITTLE = 0,
    FRM_PSR_CRC_END_BIG
} eFrmPsrCrcEnd;

typedef struct stFrmPsrHeadHit {
    uint32_t discardLen;
    uint32_t partHeadLen;
    uint16_t headPatLen;
    bool isFound;
} stFrmPsrHeadHit;

typedef uint32_t (*frmPsrHeadLenFunc)(const uint8_t *buf, uint32_t availLen, void *userCtx);
typedef uint32_t (*frmPsrPktLenFunc)(const uint8_t *buf, uint32_t headLen, uint32_t availLen, void *userCtx);
typedef uint32_t (*frmPsrCrcCalcFunc)(const uint8_t *buf, uint32_t len, void *userCtx);
typedef uint32_t (*frmPsrGetTickFunc)(void);

typedef struct stFrmPsrPkt {
    const uint8_t *buf;
    const uint8_t *headBuf;
    const uint8_t *dataBuf;
    const uint8_t *crcBuf;
    const uint8_t *cmdBuf;
    const uint8_t *pktLenBuf;
    uint16_t len;
    uint16_t headLen;
    uint16_t dataLen;
    uint16_t crcLen;
    uint16_t cmdFieldOff;
    uint16_t pktLenFieldOff;
    uint16_t crcFieldOff;
    uint8_t cmdFieldLen;
    uint8_t pktLenFieldLen;
    uint32_t crcVal;
} stFrmPsrPkt;

typedef struct stFrmPsrProtoCfg {
    const uint8_t *headPatList[FRM_PSR_HEAD_PAT_MAX];
    uint8_t headPatCount;
    uint16_t headPatLen;
    uint16_t minHeadLen;
    uint16_t minPktLen;
    uint16_t maxPktLen;
    uint16_t waitPktToutMs;
    int32_t crcRangeStartOff;
    int32_t crcRangeEndOff;
    int32_t crcFieldOff;
    uint8_t crcFieldLen;
    uint16_t cmdindex;
    uint8_t cmdLen;
    uint16_t packlenindex;
    uint8_t packlenLen;
    eFrmPsrCrcEnd crcFieldEnd;
    frmPsrHeadLenFunc headLenFunc;
    frmPsrPktLenFunc pktLenFunc;
    frmPsrCrcCalcFunc crcCalcFunc;
    frmPsrGetTickFunc getTick;
    void *userCtx;
} stFrmPsrProtoCfg;

typedef struct stFrmPsrCfg {
    uint32_t protocolId;
    uint8_t *streamBuf;
    uint16_t streamBufSize;
    uint8_t *frameBuf;
    uint16_t frameBufSize;
} stFrmPsrCfg;

typedef struct stFrmPsr {
    stRingBuffer ringBuf;
    stFrmPsrProtoCfg protoCfg;
    stFrmPsrPkt pkt;
    uint8_t *streamBuf;
    uint16_t streamBufSize;
    uint8_t *frameBuf;
    uint16_t frameBufSize;
    uint32_t pendPktTick;
    uint16_t pendPktLen;
    bool hasPendPkt;
    bool hasReadyPkt;
    bool isInit;
} stFrmPsr;

eFrmPsrSta frmPsrInit(stFrmPsr *psr, const stFrmPsrCfg *cfg);
eFrmPsrSta frmPsrFeed(stFrmPsr *psr, const uint8_t *buf, uint16_t len);
eFrmPsrSta frmPsrProcess(stFrmPsr *psr);
const stFrmPsrPkt *frmPsrRelease(stFrmPsr *psr);

#ifdef __cplusplus
}
#endif

#endif  // FRM_PSR_H
/**************************End of file********************************/
