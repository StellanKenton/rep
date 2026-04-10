/************************************************************************************
* @file     : drvanlogiic_debug.h
* @brief    : DrvAnlogIic debug helpers.
* @details  : Exposes optional console registration for software IIC debug commands.
* @author   : GitHub Copilot
* @date     : 2026-04-02
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef DRVANLOGIIC_DEBUG_H
#define DRVANLOGIIC_DEBUG_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool drvAnlogIicDebugConsoleRegister(void);

#ifdef __cplusplus
}
#endif

#endif  // DRVANLOGIIC_DEBUG_H
/**************************End of file********************************/
