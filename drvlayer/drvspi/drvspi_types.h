/************************************************************************************
* @file     : drvspi_types.h
* @brief    : Public SPI logical bus definitions.
* @details  : Keeps reusable SPI API dependencies independent from the port layer.
***********************************************************************************/
#ifndef DRVSPI_TYPES_H
#define DRVSPI_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eDrvSpiPortMap {
    DRVSPI_BUS0 = 0,
    DRVSPI_BUS1,
    DRVSPI_MAX,
} eDrvSpiPortMap;

#ifdef __cplusplus
}
#endif

#endif  // DRVSPI_TYPES_H
/**************************End of file********************************/