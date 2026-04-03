/************************************************************************************
* @file     : frameprocess.h
* @brief    : Framed protocol process service.
* @details  : Manages RX parsing, TX scheduling, and ACK retry for framed packets.
***********************************************************************************/
#ifndef FRAMEPROCESS_H
#define FRAMEPROCESS_H

#include <stdbool.h>
#include <stdint.h>

#include "Rep/comm/frameparser/framepareser.h"
#include "Rep/rep_config.h"
#include "frameprocess_data.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FRM_PROC_MAX_PKT_LEN              128U
#define FRM_PROC_QUEUE_RECORD_OVERHEAD    2U
#define FRM_PROC_MAX_RX_PER_CALL          4U

#define FRM_PROC_TX_FLAG_HANDSHAKE_MASK   (1UL << 0)
#define FRM_PROC_TX_FLAG_HEARTBEAT_MASK   (1UL << 1)
#define FRM_PROC_TX_FLAG_DISCONNECT_MASK  (1UL << 2)
#define FRM_PROC_TX_FLAG_SELFCHECK_MASK   (1UL << 3)
#define FRM_PROC_TX_FLAG_DEVICEINFO_MASK  (1UL << 4)
#define FRM_PROC_TX_FLAG_BLEINFO_MASK     (1UL << 5)
#define FRM_PROC_TX_FLAG_CPRDATA_MASK     (1UL << 6)

typedef enum eFrmProcMap {
    FRAME_PROC0 = 0,
    FRAME_PROC1,
    FRAME_PROC_MAX,
} eFrmProcMapType;

typedef enum eFrmProcCmd {
    FRM_PROC_CMD_HANDSHAKE = 0x01,
    FRM_PROC_CMD_HEARTBEAT = 0x03,
    FRM_PROC_CMD_DISCONNECT = 0x04,
    FRM_PROC_CMD_SELF_CHECK = 0x05,
    FRM_PROC_CMD_GET_DEVICE_INFO = 0x11,
    FRM_PROC_CMD_GET_BLE_INFO = 0x13,
    FRM_PROC_CMD_CPR_DATA = 0x31,
} eFrmProcCmdType;

typedef enum eFrmProcStatus {
    FRM_PROC_STATUS_OK = DRV_STATUS_OK,
    FRM_PROC_STATUS_INVALID_PARAM = DRV_STATUS_INVALID_PARAM,
    FRM_PROC_STATUS_NOT_READY = DRV_STATUS_NOT_READY,
    FRM_PROC_STATUS_BUSY = DRV_STATUS_BUSY,
    FRM_PROC_STATUS_TIMEOUT = DRV_STATUS_TIMEOUT,
    FRM_PROC_STATUS_UNSUPPORTED = DRV_STATUS_UNSUPPORTED,
    FRM_PROC_STATUS_ERROR = DRV_STATUS_ERROR,
    FRM_PROC_STATUS_NO_SPACE,
    FRM_PROC_STATUS_PARSE_ERROR,
    FRM_PROC_STATUS_BUILD_ERROR
} eFrmProcStatus;

typedef union unFrmProcRunFlags {
    uint32_t value;
    struct {
        uint32_t isInit : 1;
        uint32_t isLinkUp : 1;
        uint32_t isWaitingAck : 1;
        uint32_t hasImmediateAck : 1;
        uint32_t reserved : 28;
    } bits;
} unFrmProcRunFlags;

typedef struct stFrmProcQueueCfg {
    uint8_t *storage;
    uint16_t capacity;
} stFrmProcQueueCfg;

typedef struct stFrmProcAckCfg {
    uint16_t timeoutMs;
    uint8_t maxRetryCount;
} stFrmProcAckCfg;

typedef struct stFrmProcAckState {
    uint8_t frameBuf[FRM_PROC_MAX_PKT_LEN];
    uint16_t frameLen;
    uint32_t sendTickMs;
    uint8_t retryCount;
    uint8_t maxRetryCount;
    uint16_t timeoutMs;
    bool isWaiting;
} stFrmProcAckState;

typedef uint32_t (*frmProcGetTickFunc)(void);
typedef eFrmProcStatus (*frmProcTxFunc)(eFrmProcMapType proc, const uint8_t *frameBuf, uint16_t frameLen);

typedef struct stFrmProcCfg {
    eFrameParMapType protocol;
    stFrmPsrProtoCfg protoCfg;
    frmProcGetTickFunc getTick;
    frmProcTxFunc txFrame;
    stFrmProcQueueCfg urgentQueue;
    stFrmProcQueueCfg normalQueue;
    uint8_t *rxFrameBuf;
    uint16_t rxFrameBufSize;
    stFrmProcAckCfg ackCfg;
} stFrmProcCfg;

typedef struct stFrmProcCtx {
    stFrmProcCfg cfg;
    stFrmPsr parser;
    stRingBuffer urgentTxRb;
    stRingBuffer normalTxRb;
    stFrmDataRxStore rxStore;
    stFrmDataTxStore txStore;
    unFrmProcRunFlags runFlags;
    uint32_t txUrgentMask;
    stFrmProcAckState ackState;
    uint8_t immediateAckBuf[FRM_PROC_MAX_PKT_LEN];
    uint16_t immediateAckLen;
    uint8_t txPayloadBuf[FRM_PROC_MAX_PKT_LEN];
    uint8_t txFrameBuf[FRM_PROC_MAX_PKT_LEN];
    uint8_t txPeekBuf[FRM_PROC_MAX_PKT_LEN + FRM_PROC_QUEUE_RECORD_OVERHEAD];
    bool hasReportedSelfCheck;
} stFrmProcCtx;

eFrmProcStatus frmProcGetDefCfg(eFrmProcMapType proc, stFrmProcCfg *cfg);
eFrmProcStatus frmProcSetCfg(eFrmProcMapType proc, const stFrmProcCfg *cfg);
eFrmProcStatus frmProcInit(eFrmProcMapType proc);
bool frmProcIsReady(eFrmProcMapType proc);
void frmProcProcess(eFrmProcMapType proc);

eFrmProcStatus frmProcPostSelfCheck(eFrmProcMapType proc, const stFrmDataTxSelfCheck *data, bool isUrgent);
eFrmProcStatus frmProcPostDisconnect(eFrmProcMapType proc, bool isUrgent);
eFrmProcStatus frmProcPostCprData(eFrmProcMapType proc, const stFrmDataTxCprData *data, bool isUrgent);

const stFrmDataRxStore *frmProcGetRxStore(eFrmProcMapType proc);
void frmProcClearRxFlags(eFrmProcMapType proc, uint32_t flags);

#ifdef __cplusplus
}
#endif

#endif  // FRAMEPROCESS_H
/**************************End of file********************************/
