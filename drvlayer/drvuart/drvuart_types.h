/************************************************************************************
* @file     : drvuart_types.h
* @brief    : Public UART logical port definitions.
* @details  : Keeps reusable UART API dependencies independent from the port layer.
***********************************************************************************/
#ifndef DRVUART_TYPES_H
#define DRVUART_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eDrvUartPortMapTable {
    DRVUART_WIRELESS = 0,
    DRVUART_MAX,
} eDrvUartPortMap;

#ifdef __cplusplus
}
#endif

#endif  // DRVUART_TYPES_H
/**************************End of file********************************/