/************************************************************************************
* @file     : fc41d_ctrl.h
* @brief    : FC41D internal control-plane declarations.
* @details  : This file declares the internal startup state machine, device
*             runtime container, and control helpers used by fc41d.c.
* @author   : GitHub Copilot
* @date     : 2026-04-21
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef FC41D_CTRL_H
#define FC41D_CTRL_H

#include <stdbool.h>
#include <stdint.h>

#include "fc41d_assembly.h"
#include "fc41d_data.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FC41D_MAC_QUERY_PREFIX              "+QBLEADDR:"

typedef struct stFc41dUrcCb {
    fc41dLineFunc pfHandler;
    void *handlerUserData;
    fc41dUrcMatchFunc pfMatcher;
    void *matcherUserData;
} stFc41dUrcCb;

typedef enum eFc41dCtrlStage {
    FC41D_CTRL_STAGE_IDLE = 0,
    FC41D_CTRL_STAGE_BOOT_WAIT_ARMED,
    FC41D_CTRL_STAGE_BOOT_WAIT,
    FC41D_CTRL_STAGE_PROBE,
    FC41D_CTRL_STAGE_ASSERT_RESET,
    FC41D_CTRL_STAGE_RESET_HOLD,
    FC41D_CTRL_STAGE_WAIT_READY,
    FC41D_CTRL_STAGE_READY_SETTLE,
    FC41D_CTRL_STAGE_PROBE_AFTER_READY,
    FC41D_CTRL_STAGE_STOP_STA,
    FC41D_CTRL_STAGE_BLE_INIT,
    FC41D_CTRL_STAGE_BLE_SET_NAME,
    FC41D_CTRL_STAGE_BLE_SET_SERVICE,
    FC41D_CTRL_STAGE_BLE_SET_CHAR_RX,
    FC41D_CTRL_STAGE_BLE_SET_CHAR_TX,
    FC41D_CTRL_STAGE_BLE_ADV_START,
    FC41D_CTRL_STAGE_QUERY_MAC,
    FC41D_CTRL_STAGE_RUNNING,
} eFc41dCtrlStage;

typedef struct stFc41dCtrlPlane {
    char cmdBuf[FC41D_CTRL_CMD_BUFFER_SIZE];
    uint32_t nextActionTick;
    uint32_t readyDeadlineTick;
    eFc41dCtrlStage stage;
    bool isTxnDone;
    eFc41dStatus txnStatus;
} stFc41dCtrlPlane;

typedef struct stFc41dDevice {
    stFc41dCfg cfg;
    stFc41dBleCfg bleCfg;
    stFc41dInfo info;
    stFc41dState state;
    stFc41dUrcCb urcCb;
    stFlowParserStream stream;
    uint8_t rxStorage[FC41D_STREAM_RX_STORAGE_SIZE];
    uint8_t lineBuf[FC41D_STREAM_LINE_BUFFER_SIZE];
    stFc41dDataPlane dataPlane;
    stFc41dCtrlPlane ctrlPlane;
} stFc41dDevice;

bool fc41dIsValidRole(eFc41dRole role);
void fc41dLoadDefBleCfg(stFc41dBleCfg *cfg);
bool fc41dIsValidText(const char *text, uint16_t maxLength, bool allowEmpty);
void fc41dResetState(stFc41dDevice *device);
void fc41dSyncInfo(stFc41dDevice *device);
void fc41dSyncState(stFc41dDevice *device);
const stFc41dTransportInterface *fc41dGetTransport(const stFc41dDevice *device);
const stFc41dControlInterface *fc41dGetControl(eFc41dMapType device);
eFc41dStatus fc41dMapDrvStatus(eDrvStatus status);
eFc41dStatus fc41dMapStreamStatus(eFlowParserStrmSta status);
eFc41dStatus fc41dMapResult(eFlowParserResult result);
eFc41dStatus fc41dCtrlStart(stFc41dDevice *device, eFc41dRole role);
void fc41dCtrlStop(stFc41dDevice *device);
eFc41dStatus fc41dCtrlProcess(stFc41dDevice *device, eFc41dMapType deviceId, uint32_t nowTickMs);
void fc41dCtrlScheduleRetry(stFc41dDevice *device, eFc41dMapType deviceId, uint32_t nowTickMs, eFc41dStatus status);
bool fc41dCtrlIsUrc(const stFc41dDevice *device, const uint8_t *lineBuf, uint16_t lineLen);
void fc41dCtrlHandleUrc(stFc41dDevice *device, const uint8_t *lineBuf, uint16_t lineLen);
void fc41dCtrlHandleTxnLine(stFc41dDevice *device, const uint8_t *lineBuf, uint16_t lineLen);
void fc41dCtrlHandleTxnDone(stFc41dDevice *device, eFlowParserResult result);
void fc41dTxnLineThunk(void *userData, const uint8_t *lineBuf, uint16_t lineLen);
void fc41dTxnDoneThunk(void *userData, eFlowParserResult result);

#ifdef __cplusplus
}
#endif

#endif  // FC41D_CTRL_H
/**************************End of file********************************/
