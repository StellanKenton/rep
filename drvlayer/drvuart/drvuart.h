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
#include "drvuart_types.h"
#include "ringbuffer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DRVUART_BSP_SYNC_CHUNK_SIZE    256U

typedef eDrvStatus (*drvUartBspInitFunc)(eDrvUartPortMap uart);
typedef eDrvStatus (*drvUartBspTransmitFunc)(eDrvUartPortMap uart, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs);
typedef eDrvStatus (*drvUartBspTransmitItFunc)(eDrvUartPortMap uart, const uint8_t *buffer, uint16_t length);
typedef eDrvStatus (*drvUartBspTransmitDmaFunc)(eDrvUartPortMap uart, const uint8_t *buffer, uint16_t length);
typedef uint16_t (*drvUartBspGetDataLenFunc)(eDrvUartPortMap uart);
typedef eDrvStatus (*drvUartBspReceiveFunc)(eDrvUartPortMap uart, uint8_t *buffer, uint16_t length);

typedef struct stDrvUartBspInterface {
    drvUartBspInitFunc init;
    drvUartBspTransmitFunc transmit;
    drvUartBspTransmitItFunc transmitIt;
    drvUartBspTransmitDmaFunc transmitDma;
    drvUartBspGetDataLenFunc getDataLen;
    drvUartBspReceiveFunc receive;
    uint8_t *Buffer;
} stDrvUartBspInterface;

eDrvStatus drvUartInit(eDrvUartPortMap uart);
eDrvStatus drvUartTransmit(eDrvUartPortMap uart, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs);
eDrvStatus drvUartTransmitIt(eDrvUartPortMap uart, const uint8_t *buffer, uint16_t length);
eDrvStatus drvUartTransmitDma(eDrvUartPortMap uart, const uint8_t *buffer, uint16_t length);
eDrvStatus drvUartReceive(eDrvUartPortMap uart, uint8_t *buffer, uint16_t length);
uint16_t drvUartGetDataLen(eDrvUartPortMap uart);
stRingBuffer* drvUartGetRingBuffer(eDrvUartPortMap uart);

#ifdef __cplusplus
}
#endif

#endif  // DRVUART_H
/**************************End of file********************************/

