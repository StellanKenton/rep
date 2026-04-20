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
    FRM_PSR_FMT_NOT_SEL,
    FRM_PSR_FMT_INVALID,
    FRM_PSR_BUILD_FAIL
} eFrmPsrSta;

typedef enum eFrmPsrCrcEnd {
    FRM_PSR_CRC_END_LITTLE = 0,
    FRM_PSR_CRC_END_BIG
} eFrmPsrCrcEnd;

typedef struct stFrmPsrHeadHit {
    uint32_t discardLen;
    uint32_t partHeadLen;
    bool isFound;
} stFrmPsrHeadHit;

typedef uint32_t (*frmPsrHeadLenFunc)(const uint8_t *buf, uint32_t availLen, void *userCtx);
typedef uint32_t (*frmPsrPktLenFunc)(const uint8_t *buf, uint32_t headLen, uint32_t availLen, void *userCtx);
typedef uint32_t (*frmPsrCrcCalcFunc)(const uint8_t *buf, uint32_t len, void *userCtx);
typedef uint32_t (*frmPsrGetTickFunc)(void);
typedef uint32_t (*frmPsrTxHeadBuildFunc)(uint8_t *buf, uint32_t bufSize, uint32_t payloadLen, void *userCtx);
typedef uint32_t (*frmPsrTxPktLenFunc)(uint32_t headLen, uint32_t payloadLen, void *userCtx);
typedef bool (*frmPsrTxPktFinFunc)(uint8_t *pktBuf, uint32_t pktLen, uint32_t headLen, const uint8_t *payloadBuf, uint32_t payloadLen, void *userCtx);

typedef struct stFrmPsrPkt {
    const uint8_t *buf;
    uint16_t len;
} stFrmPsrPkt;

typedef struct stFrmPsrRunCfg {
    uint16_t waitPktToutMs;
    uint8_t *outBuf;
    uint16_t outBufSize;
    frmPsrGetTickFunc getTick;
} stFrmPsrRunCfg;

typedef struct stFrmPsrRxFmt {
    const uint8_t *headPat;
    uint16_t headPatLen;
    uint16_t minHeadLen;
    uint16_t minPktLen;
    uint16_t maxPktLen;
    int32_t crcRangeStartOff;
    int32_t crcRangeEndOff;
    int32_t crcFieldOff;
    uint8_t crcFieldLen;
    eFrmPsrCrcEnd crcFieldEnd;
    frmPsrHeadLenFunc headLenFunc;
    frmPsrPktLenFunc pktLenFunc;
    frmPsrCrcCalcFunc crcCalcFunc;
    void *userCtx;
} stFrmPsrRxFmt;

typedef struct stFrmPsrTxFmt {
    const uint8_t *headPat;
    uint16_t headPatLen;
    uint16_t minPktLen;
    uint16_t maxPktLen;
    int32_t crcRangeStartOff;
    int32_t crcRangeEndOff;
    int32_t crcFieldOff;
    uint8_t crcFieldLen;
    eFrmPsrCrcEnd crcFieldEnd;
    frmPsrTxHeadBuildFunc headBuildFunc;
    frmPsrTxPktLenFunc pktLenFunc;
    frmPsrTxPktFinFunc pktFinFunc;
    frmPsrCrcCalcFunc crcCalcFunc;
    void *userCtx;
} stFrmPsrTxFmt;

typedef struct stFrmPsrFmt {
    const char *name;
    stFrmPsrRxFmt rxFmt;
    stFrmPsrTxFmt txFmt;
} stFrmPsrFmt;

typedef struct stFrmPsrCfg {
    const uint8_t *headPat;
    uint16_t headPatLen;
    uint16_t minHeadLen;
    uint16_t minPktLen;
    uint16_t maxPktLen;
    uint16_t waitPktToutMs;
    int32_t crcRangeStartOff;
    int32_t crcRangeEndOff;
    int32_t crcFieldOff;
    uint8_t crcFieldLen;
    eFrmPsrCrcEnd crcFieldEnd;
    uint8_t *outBuf;
    uint16_t outBufSize;
    frmPsrHeadLenFunc headLenFunc;
    frmPsrPktLenFunc pktLenFunc;
    frmPsrCrcCalcFunc crcCalcFunc;
    frmPsrGetTickFunc getTick;
    void *userCtx;
} stFrmPsrCfg;

typedef stRingBuffer *(*frmPsrGetRingBufFunc)(void *userCtx);

typedef struct stFrmPsrProtoCfg {
    const uint8_t *rxHeadPat;
    uint16_t rxHeadPatLen;
    const uint8_t *txHeadPat;
    uint16_t txHeadPatLen;
    uint16_t minHeadLen;
    uint16_t minPktLen;
    uint16_t maxPktLen;
    uint16_t waitPktToutMs;
    int32_t crcRangeStartOff;
    int32_t crcRangeEndOff;
    int32_t crcFieldOff;
    uint8_t crcFieldLen;
    eFrmPsrCrcEnd crcFieldEnd;
    frmPsrHeadLenFunc headLenFunc;
    frmPsrPktLenFunc pktLenFunc;
    frmPsrCrcCalcFunc crcCalcFunc;
    frmPsrGetTickFunc getTick;
    frmPsrGetRingBufFunc getRingBuf;
    void *ringBufUserCtx;
    void *userCtx;
} stFrmPsrProtoCfg;

typedef struct stFrmPsr {
    stRingBuffer *ringBuf;
    stFrmPsrCfg cfg;
    const stFrmPsrFmt *fmt;
    stFrmPsrPkt pkt;
    uint32_t pendPktTick;
    uint16_t pendPktLen;
    bool hasPendPkt;
    bool hasReadyPkt;
    bool isInit;
} stFrmPsr;

uint32_t frmPsrGetPlatformTickMs(void);
void frmPsrLoadPlatformDefaultCfg(stFrmPsrCfg *cfg);
void frmPsrLoadPlatformDefaultRunCfg(stFrmPsrRunCfg *runCfg);
void frmPsrLoadPlatformDefaultProtoCfg(uint32_t protocolId, stFrmPsrProtoCfg *protoCfg);
uint32_t frmPsrGetPlatformFmtCount(void);
bool frmPsrSetPlatformFmt(uint32_t protocolId, const stFrmPsrFmt *fmt);
const stFrmPsrFmt *frmPsrGetPlatformFmt(uint32_t protocolId);

bool frmPsrIsRunCfgValid(const stFrmPsrRunCfg *runCfg);
bool frmPsrIsRxFmtValid(const stFrmPsrRxFmt *rxFmt);
bool frmPsrIsTxFmtValid(const stFrmPsrTxFmt *txFmt);
bool frmPsrIsFmtValid(const stFrmPsrFmt *fmt);
bool frmPsrIsCfgValid(const stFrmPsrCfg *cfg);
bool frmPsrIsProtoCfgValid(const stFrmPsrProtoCfg *protoCfg);
eFrmPsrSta frmPsrInit(stFrmPsr *psr, stRingBuffer *ringBuf, const stFrmPsrCfg *cfg);
eFrmPsrSta frmPsrInitByProtoCfg(stFrmPsr *psr, const stFrmPsrProtoCfg *protoCfg, stRingBuffer *ringBuf, uint8_t *outBuf, uint16_t outBufSize);
eFrmPsrSta frmPsrInitFmt(stFrmPsr *psr, stRingBuffer *ringBuf, const stFrmPsrFmt *fmt, const stFrmPsrRunCfg *runCfg);
void frmPsrReset(stFrmPsr *psr);
eFrmPsrSta frmPsrProc(stFrmPsr *psr, stFrmPsrPkt *pkt);
eFrmPsrSta frmPsrSelFmt(stFrmPsr *psr, const stFrmPsrFmt *fmt);
const stFrmPsrFmt *frmPsrGetFmt(const stFrmPsr *psr);
eFrmPsrSta frmPsrMkPkt(const stFrmPsr *psr, const uint8_t *payloadBuf, uint16_t payloadLen, uint8_t *pktBuf, uint16_t pktBufSize, uint16_t *pktLen);
eFrmPsrSta frmPsrMkPktByFmt(const stFrmPsrFmt *fmt, const uint8_t *payloadBuf, uint16_t payloadLen, uint8_t *pktBuf, uint16_t pktBufSize, uint16_t *pktLen);
bool frmPsrHasPkt(const stFrmPsr *psr);
const stFrmPsrPkt *frmPsrGetPkt(const stFrmPsr *psr);
void frmPsrFreePkt(stFrmPsr *psr);

#ifdef __cplusplus
}
#endif

#endif  // FRM_PSR_H
/**************************End of file********************************/
