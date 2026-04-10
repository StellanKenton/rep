/************************************************************************************
* @file     : pca9535_debug.h
* @brief    : PCA9535 debug helpers.
* @details  : Exposes optional console registration for PCA9535 debug commands.
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef PCA9535_DEBUG_H
#define PCA9535_DEBUG_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool pca9535DebugConsoleRegister(void);

#ifdef __cplusplus
}
#endif

#endif  // PCA9535_DEBUG_H
/**************************End of file********************************/
