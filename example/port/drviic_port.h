/************************************************************************************
* @file     : drviic_port.h
* @brief    : Shared hardware IIC logical bus mapping definitions.
* @details  : This file keeps the project-level hardware IIC identifiers and
*             default options independent from the driver interface.
* @author   : GitHub Copilot
* @date     : 2026-04-01
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef DRVIIC_PORT_H
#define DRVIIC_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eDrvIicPortMap {
	DRVIIC_BUS0 = 0,
	DRVIIC_MAX,
} eDrvIicPortMap;

#ifndef DRVIIC_LOG_SUPPORT
#define DRVIIC_LOG_SUPPORT                    1
#endif

#ifndef DRVIIC_CONSOLE_SUPPORT
#define DRVIIC_CONSOLE_SUPPORT                1
#endif

#define DRVIIC_LOCK_WAIT_MS                   5U
#define DRVIIC_DEFAULT_TIMEOUT_MS             100U

#ifdef __cplusplus
}
#endif

#endif  // DRVIIC_PORT_H
/**************************End of file********************************/
