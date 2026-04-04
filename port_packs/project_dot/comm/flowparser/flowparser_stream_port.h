 /************************************************************************************
* @file     : flowparser_stream_port.h
* @brief    : ESP-AT flow parser port helpers.
* @details  : Provides default specs, timeout values and URC classification.
* @author   : GitHub Copilot
* @date     : 2026-04-02
* @version  : V1.0.0
***********************************************************************************/
#ifndef FLOWPARSER_STREAM_PORT_H
#define FLOWPARSER_STREAM_PORT_H

#include <stdbool.h>
#include <stdint.h>

#include "flowparser_stream.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef FLOWPARSER_PORT_PROC_BUDGET
#define FLOWPARSER_PORT_PROC_BUDGET          16U
#endif

#ifndef FLOWPARSER_PORT_TOTAL_TOUT_MS
#define FLOWPARSER_PORT_TOTAL_TOUT_MS        5000U
#endif

#ifndef FLOWPARSER_PORT_RESPONSE_TOUT_MS
#define FLOWPARSER_PORT_RESPONSE_TOUT_MS     1000U
#endif

#ifndef FLOWPARSER_PORT_PROMPT_TOUT_MS
#define FLOWPARSER_PORT_PROMPT_TOUT_MS       1000U
#endif

#ifndef FLOWPARSER_PORT_FINAL_TOUT_MS
#define FLOWPARSER_PORT_FINAL_TOUT_MS        5000U
#endif

uint32_t flowparserPortGetTickMs(void);
void flowparserPortApplyDftCfg(stFlowParserStreamCfg *cfg);
void flowparserPortGetEspAtBaseSpec(stFlowParserSpec *spec);
void flowparserPortGetEspAtSendSpec(stFlowParserSpec *spec);
bool flowparserPortIsEspAtUrc(const uint8_t *lineBuf, uint16_t lineLen, void *userCtx);
eFlowParserStrmSta flowparserPortInitEspAt(stFlowParserStream *stream, stRingBuffer *ringBuf, uint8_t *lineBuf, uint16_t lineBufSize,
                                           uint8_t *cmdBuf, uint16_t cmdBufSize, uint8_t *payloadBuf, uint16_t payloadBufSize,
                                           flowparserStreamSendFunc sendFunc, void *portUserCtx, flowparserStreamLineFunc urcHandler, void *urcUserCtx);

#ifdef __cplusplus
}
#endif

#endif  // FLOWPARSER_STREAM_PORT_H
/**************************End of file********************************/
