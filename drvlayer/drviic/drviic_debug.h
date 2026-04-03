/************************************************************************************
* @file     : drviic_debug.h
* @brief    : DrvIic debug helpers.
* @details  : Exposes optional console registration for hardware IIC debug commands.
* @author   : GitHub Copilot
* @date     : 2026-04-02
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef DRVIIC_DEBUG_H
#define DRVIIC_DEBUG_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool drvIicDebugConsoleRegister(void);

#ifdef __cplusplus
}
#endif

#endif  // DRVIIC_DEBUG_H
/**************************End of file********************************/
