/************************************************************************************
* @file     : console_port.h
* @brief    : Console project port-layer declarations.
* @details  : Provides the project default console integration entry that wires
*             reusable console core initialization to module command registration.
* @note     : Demo project pack variant used to validate script-based port switching.
* @author   : GitHub Copilot
* @date     : 2026-04-03
* @version  : V1.0.0
***********************************************************************************/
#ifndef CONSOLE_PORT_H
#define CONSOLE_PORT_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool consolePortInit(void);

#ifdef __cplusplus
}
#endif

#endif  // CONSOLE_PORT_H
/**************************End of file********************************/