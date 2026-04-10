/************************************************************************************
* @file     : gd25qxxx_debug.h
* @brief    : GD25Qxxx debug helpers.
* @details  : Exposes optional console registration for GD25Qxxx debug commands.
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef GD25QXXX_DEBUG_H
#define GD25QXXX_DEBUG_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool gd25qxxxDebugConsoleRegister(void);

#ifdef __cplusplus
}
#endif

#endif  // GD25QXXX_DEBUG_H
/**************************End of file********************************/
