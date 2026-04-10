/************************************************************************************
* @file     : drvspi_debug.h
* @brief    : DrvSpi debug helpers.
* @details  : Exposes optional console registration for SPI debug commands.
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef DRVSPI_DEBUG_H
#define DRVSPI_DEBUG_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool drvSpiDebugConsoleRegister(void);

#ifdef __cplusplus
}
#endif

#endif  // DRVSPI_DEBUG_H
/**************************End of file********************************/
