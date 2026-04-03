/************************************************************************************
* @file     : w25qxxx_debug.h
* @brief    : W25Qxxx debug helpers.
* @details  : Exposes optional console registration for W25Qxxx debug commands.
***********************************************************************************/
#ifndef W25QXXX_DEBUG_H
#define W25QXXX_DEBUG_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool w25qxxxDebugConsoleRegister(void);

#ifdef __cplusplus
}
#endif

#endif  // W25QXXX_DEBUG_H
/**************************End of file********************************/
