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
    stFc41dCfg cfg;
    stFlowParserStream atStream;
    stRingBuffer bleRxRb;
    stRingBuffer wifiRxRb;
    stFc41dInfo info;
    stFc41dAtResp *activeResp;
    eFc41dAtExecResult execResult;
    bool execDone;
    bool defCfgLoaded;
} stFc41dCtx;

bool fc41dIsValidDevMap(eFc41dMapType device);
stFc41dCtx *fc41dGetCtx(eFc41dMapType device);
bool fc41dIsReadyCtx(const stFc41dCtx *ctx);
stRingBuffer *fc41dGetRxRbByChannel(stFc41dCtx *ctx, eFc41dRxChannel channel);
void fc41dAtGetBaseOpt(stFc41dAtOpt *opt);
bool fc41dAtIsUrc(const uint8_t *lineBuf, uint16_t lineLen);
void fc41dBleSyncInfoFromCfg(stFc41dCtx *ctx);
void fc41dWifiSyncInfoFromCfg(stFc41dCtx *ctx);
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
