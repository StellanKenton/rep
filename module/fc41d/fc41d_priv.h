/************************************************************************************
* @file     : fc41d_priv.h
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef FC41D_PRIV_H
#define FC41D_PRIV_H

#include <stdbool.h>
#include <stdint.h>

#include "fc41d_assembly.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct stFc41dCtx {
    eFc41dMapType device;
    stFc41dCfg cfg;
    stFlowParserStream atStream;
    stFlowParserSpec execSpec;
    stRingBuffer bleRxRb;
    stRingBuffer wifiRxRb;
    stFc41dInfo info;
    stFc41dAtResp *activeResp;
    uint32_t execStartTick;
    eFc41dAtExecResult execResult;
    eFc41dAtExecResult lastExecResult;
    uint8_t execOwner;
    eFc41dMode pendingModeOnSuccess;
    bool execDone;
    bool hasPendingModeSwitch;
    bool defCfgLoaded;
    bool wifiStaStartIssued;
    uint8_t bleCmdStepIndex;
    uint8_t wifiCmdStepIndex;
    uint8_t wifiReconnectCount;
    uint32_t wifiReconnectTick;
} stFc41dCtx;

typedef enum eFc41dExecOwner {
    FC41D_EXEC_OWNER_NONE = 0,
    FC41D_EXEC_OWNER_API,
    FC41D_EXEC_OWNER_BLE,
    FC41D_EXEC_OWNER_WIFI,
} eFc41dExecOwner;

bool fc41dIsValidDevMap(eFc41dMapType device);
stFc41dCtx *fc41dGetCtx(eFc41dMapType device);
bool fc41dIsReadyCtx(const stFc41dCtx *ctx);
stRingBuffer *fc41dGetRxRbByChannel(stFc41dCtx *ctx, eFc41dRxChannel channel);
void fc41dAtGetBaseOpt(stFc41dAtOpt *opt);
bool fc41dAtIsUrc(const uint8_t *lineBuf, uint16_t lineLen);
bool fc41dAtMatchPattern(const uint8_t *lineBuf, uint16_t lineLen, const char *pattern);
eFc41dStatus fc41dBuildFmtCmd(char *cmdBuf, uint16_t cmdBufSize, const char *fmt, ...);
bool fc41dLineHasToken(const uint8_t *lineBuf, uint16_t lineLen, const char *token);
eFc41dStatus fc41dExecOptionalCmdText(eFc41dMapType device, eFc41dExecOwner owner, const char *cmdText);
eFc41dStatus fc41dExecCmdSeq(eFc41dMapType device, eFc41dExecOwner owner, const char *const *cmdSeq,
                             uint8_t cmdSeqLen, uint8_t *stepIndex);
void fc41dReleaseExecOwner(stFc41dCtx *ctx, eFc41dExecOwner owner);
void fc41dBleSyncInfoFromCfg(stFc41dCtx *ctx);
void fc41dWifiSyncInfoFromCfg(stFc41dCtx *ctx);
void fc41dBleResetState(stFc41dCtx *ctx);
void fc41dWifiResetState(stFc41dCtx *ctx);
void fc41dBleAccumulateRxStats(stFc41dCtx *ctx, uint32_t written, uint32_t dropped);
void fc41dWifiAccumulateRxStats(stFc41dCtx *ctx, uint32_t written, uint32_t dropped);
eFc41dStatus fc41dBleProcessStateMachine(stFc41dCtx *ctx, eFc41dMapType device);
eFc41dStatus fc41dWifiProcessStateMachine(stFc41dCtx *ctx, eFc41dMapType device);
void fc41dBleUpdateLinkStateByUrc(stFc41dCtx *ctx, const uint8_t *lineBuf, uint16_t lineLen);
void fc41dWifiUpdateLinkStateByUrc(stFc41dCtx *ctx, const uint8_t *lineBuf, uint16_t lineLen);

#ifdef __cplusplus
}
#endif

#endif  // FC41D_PRIV_H
/**************************End of file********************************/
