/************************************************************************************
* @file     : drvspi_port.h
* @brief    : Shared SPI logical bus mapping definitions.
* @details  : This file keeps the project-level SPI identifiers and default
*             options independent from the driver interface.
***********************************************************************************/
#ifndef DRVSPI_PORT_H
#define DRVSPI_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DRVSPI_LOG_SUPPORT
#define DRVSPI_LOG_SUPPORT                    1
#endif

#ifndef DRVSPI_CONSOLE_SUPPORT
#define DRVSPI_CONSOLE_SUPPORT                1
#endif

#define DRVSPI_LOCK_WAIT_MS                   5U
#define DRVSPI_DEFAULT_TIMEOUT_MS             100U
#define DRVSPI_DEFAULT_READ_FILL_DATA         0xFFU

typedef enum eDrvSpiPortMap {
    DRVSPI_BUS0 = 0,
    DRVSPI_BUS1,
    DRVSPI_MAX,
} eDrvSpiPortMap;

#ifdef __cplusplus
}
#endif

#endif  // DRVSPI_PORT_H
/**************************End of file********************************/
