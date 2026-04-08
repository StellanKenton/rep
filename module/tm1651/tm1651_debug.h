/************************************************************************************
* @file     : tm1651_debug.h
* @brief    : TM1651 debug helpers.
* @details  : Exposes optional console registration for TM1651 debug commands.
***********************************************************************************/
#ifndef TM1651_DEBUG_H
#define TM1651_DEBUG_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool tm1651DebugConsoleRegister(void);

#ifdef __cplusplus
}
#endif

#endif  // TM1651_DEBUG_H
/**************************End of file********************************/