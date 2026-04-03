/************************************************************************************
* @file     : framepareser_port.h
* @brief    : Project port helpers for the stream packet parser.
* @details  : Supplies default timing hooks and parser initialization helpers.
* @author   : GitHub Copilot
* @date     : 2026-04-01
* @version  : V1.0.0
***********************************************************************************/
#ifndef FRM_PSR_PORT_H
#define FRM_PSR_PORT_H

#include <stdbool.h>
#include <stdint.h>

#include "framepareser.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef FRM_PSR_PORT_WAIT_PKT_TOUT_MS
#define FRM_PSR_PORT_WAIT_PKT_TOUT_MS    60U
#endif

#define FRM_PSR_APP_COMM_HEAD0               0xFAU
#define FRM_PSR_APP_COMM_HEAD1               0xFCU
#define FRM_PSR_APP_SEND_HEAD0               0xFEU
#define FRM_PSR_APP_SEND_HEAD1               0xFDU
#define FRM_PSR_APP_COMM_VER                 0x01U
#define FRM_PSR_APP_COMM_HEAD_LEN            6U
#define FRM_PSR_APP_COMM_CRC_LEN             2U
#define FRM_PSR_APP_COMM_MIN_PKT_LEN         (FRM_PSR_APP_COMM_HEAD_LEN + FRM_PSR_APP_COMM_CRC_LEN)
#define FRM_PSR_APP_COMM_MAX_PKT_LEN         128U

static const uint8_t gFrmPsrAppCommHeadPat[] = {
    FRM_PSR_APP_COMM_HEAD0,
    FRM_PSR_APP_COMM_HEAD1,
    FRM_PSR_APP_COMM_VER,
};

static const uint8_t gFrmPsrAppSendHeadPat[] = {
    FRM_PSR_APP_SEND_HEAD0,
    FRM_PSR_APP_SEND_HEAD1,
    FRM_PSR_APP_COMM_VER,
};

uint32_t frmPsrPortGetTickMs(void);
void frmPsrPortApplyDftCfg(stFrmPsrCfg *cfg);
void frmPsrPortApplyDftRunCfg(stFrmPsrRunCfg *runCfg);
void frmPsrPortGetDefProtoCfg(eFrameParMapType protocol, stFrmPsrPortProtoCfg *protoCfg);
eFrmPsrSta frmPsrPortInitByProto(stFrmPsr *psr, const stFrmPsrPortProtoCfg *protoCfg, stRingBuffer *ringBuf, uint8_t *outBuf, uint16_t outBufSize);
uint32_t frmPsrPortGetFmtCnt(void);
bool frmPsrPortSetFmt(eFrameParMapType protocol, const stFrmPsrFmt *fmt);
const stFrmPsrFmt *frmPsrPortGetFmt(eFrameParMapType protocol);
eFrmPsrSta frmPsrPortInit(stFrmPsr *psr, stRingBuffer *ringBuf, stFrmPsrCfg *cfg);
eFrmPsrSta frmPsrPortInitFmt(stFrmPsr *psr, stRingBuffer *ringBuf, eFrameParMapType protocol, stFrmPsrRunCfg *runCfg);
eFrmPsrSta frmPsrPortSelFmt(stFrmPsr *psr, eFrameParMapType protocol);
eFrmPsrSta frmPsrPortMkPkt(eFrameParMapType protocol, const uint8_t *payloadBuf, uint16_t payloadLen, uint8_t *pktBuf, uint16_t pktBufSize, uint16_t *pktLen);

#ifdef __cplusplus
}
#endif

#endif  // FRM_PSR_PORT_H
/**************************End of file********************************/
