/***********************************************************************************
* @file     : flowparser_stream_port.c
* @brief    : Flow parser generic platform helpers implementation.
* @details  : Supplies default timing values without binding protocol-specific rules.
* @author   : GitHub Copilot
* @date     : 2026-04-08
* @version  : V1.1.0
**********************************************************************************/
#include "flowparser_stream_port.h"

#include <stddef.h>

#include "rep_config.h"

#if (REP_RTOS_SYSTEM == REP_RTOS_FREERTOS)
#include "FreeRTOS.h"
#include "task.h"
#endif

uint32_t flowparserPortGetTickMs(void)
{
#if (REP_RTOS_SYSTEM == REP_RTOS_FREERTOS)
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
#else
    return 0U;
#endif
}

void flowparserPortApplyDftCfg(stFlowParserStreamCfg *cfg)
{
    if (cfg == NULL) {
        return;
    }

    if (cfg->getTick == NULL) {
        cfg->getTick = flowparserPortGetTickMs;
    }

    if (cfg->procBudget == 0U) {
        cfg->procBudget = FLOWPARSER_PORT_PROC_BUDGET;
    }
}
/**************************End of file********************************/
