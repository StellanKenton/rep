/************************************************************************************
* @file     : flowparser_stream_port.h
* @brief    : Flow parser generic platform helpers.
* @details  : Provides platform tick access and generic stream defaults.
* @author   : GitHub Copilot
* @date     : 2026-04-08
* @version  : V1.1.0
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

#ifdef __cplusplus
}
#endif

#endif  // FLOWPARSER_STREAM_PORT_H
/**************************End of file********************************/
