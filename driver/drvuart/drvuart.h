/************************************************************************************
* @file     : drvuart.h
* @brief    : Generic MCU UART driver abstraction.
* @details  : This module defines a stable UART interface for project-level drivers.
* @author   : GitHub Copilot
* @date     : 2026-03-30
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef DRVUART_H
#define DRVUART_H

#include <stdint.h>

#include "rep_config.h"
#include "ringbuffer.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DRVUART_LOG_SUPPORT
#define DRVUART_LOG_SUPPORT             1
#endif

#ifndef DRVUART_CONSOLE_SUPPORT
#define DRVUART_CONSOLE_SUPPORT         1
#endif

#ifndef DRVUART_MAX
#define DRVUART_MAX                     1U
#endif

#define DRVUART_BSP_SYNC_CHUNK_SIZE    256U

typedef eDrvStatus (*drvUartBspInitFunc)(uint8_t uart);
typedef eDrvStatus (*drvUartBspTransmitFunc)(uint8_t uart, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs);
typedef eDrvStatus (*drvUartBspTransmitItFunc)(uint8_t uart, const uint8_t *buffer, uint16_t length);
typedef eDrvStatus (*drvUartBspTransmitDmaFunc)(uint8_t uart, const uint8_t *buffer, uint16_t length);
typedef uint16_t (*drvUartBspGetDataLenFunc)(uint8_t uart);
typedef eDrvStatus (*drvUartBspReceiveFunc)(uint8_t uart, uint8_t *buffer, uint16_t length);

typedef struct stDrvUartBspInterface {
    drvUartBspInitFunc init;
    drvUartBspTransmitFunc transmit;
    drvUartBspTransmitItFunc transmitIt;
    drvUartBspTransmitDmaFunc transmitDma;
    drvUartBspGetDataLenFunc getDataLen;
    drvUartBspReceiveFunc receive;
    uint8_t *Buffer;
} stDrvUartBspInterface;

eDrvStatus drvUartInit(uint8_t uart);
eDrvStatus drvUartTransmit(uint8_t uart, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs);
eDrvStatus drvUartTransmitIt(uint8_t uart, const uint8_t *buffer, uint16_t length);
eDrvStatus drvUartTransmitDma(uint8_t uart, const uint8_t *buffer, uint16_t length);
eDrvStatus drvUartReceive(uint8_t uart, uint8_t *buffer, uint16_t length);
uint16_t drvUartGetDataLen(uint8_t uart);
stRingBuffer* drvUartGetRingBuffer(uint8_t uart);

#ifdef __cplusplus
}
#endif

#endif  // DRVUART_H
/**************************End of file********************************/
