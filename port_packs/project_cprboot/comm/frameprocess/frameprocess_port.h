/************************************************************************************
* @file     : frameprocess_port.h
* @brief    : Frame process project port helpers.
* @details  : Supplies default config, frame format registration, and UART binding.
***********************************************************************************/
#ifndef FRAMEPROCESS_PORT_H
#define FRAMEPROCESS_PORT_H

#include "frameprocess.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FRM_PROC_URGENT_QUEUE_CAPACITY    256U
#define FRM_PROC_NORMAL_QUEUE_CAPACITY    1024U
#define FRM_PROC_ACK_TIMEOUT_MS           100U
#define FRM_PROC_ACK_RETRY_COUNT          2U

eFrmProcStatus frmProcPortGetDefCfg(eFrmProcMapType proc, stFrmProcCfg *cfg);
eFrmProcStatus frmProcPortInit(eFrmProcMapType proc);
void frmProcPortPollRx(eFrmProcMapType proc);
eFrmProcStatus frmProcPortEnsureFmt(eFrmProcMapType proc, const stFrmProcCfg *cfg);
eFrmProcStatus frmProcPortBuildPkt(eFrmProcMapType proc, uint8_t cmd, const uint8_t *payloadBuf, uint16_t payloadLen, uint8_t *pktBuf, uint16_t pktBufSize, uint16_t *pktLen);
eFrmProcStatus frmProcPortTxFrame(eFrmProcMapType proc, const uint8_t *frameBuf, uint16_t frameLen);
uint32_t frmProcPortGetTickMs(void);

#ifdef __cplusplus
}
#endif

#endif  // FRAMEPROCESS_PORT_H
/**************************End of file********************************/
