/************************************************************************************
* @file     : fc41d_assembly.h
* @brief    : FC41D core to platform assembly contract.
***********************************************************************************/
#ifndef FC41D_ASSEMBLY_H
#define FC41D_ASSEMBLY_H

#include "fc41d.h"
#include "../../comm/flowparser/flowparser_stream.h"

#ifdef __cplusplus
extern "C" {
#endif

void fc41dLoadPlatformDefaultCfg(eFc41dMapType device, stFc41dCfg *cfg);
bool fc41dPlatformIsValidAssemble(eFc41dMapType device);
uint32_t fc41dPlatformGetTickMs(void);
eFc41dStatus fc41dPlatformInitTransport(eFc41dMapType device);
void fc41dPlatformPollRx(eFc41dMapType device);
eFc41dStatus fc41dPlatformInitRxBuffers(eFc41dMapType device, stRingBuffer *bleRxRb, stRingBuffer *wifiRxRb);
eFlowParserStrmSta fc41dPlatformInitAtStream(eFc41dMapType device, stFlowParserStream *stream,
                                             flowparserStreamLineFunc urcHandler, void *urcUserCtx);
bool fc41dPlatformRouteLine(eFc41dMapType device, const uint8_t *lineBuf, uint16_t lineLen,
                            eFc41dRxChannel *channel, const uint8_t **payloadBuf, uint16_t *payloadLen);

#ifdef __cplusplus
}
#endif

#endif  // FC41D_ASSEMBLY_H
/**************************End of file********************************/