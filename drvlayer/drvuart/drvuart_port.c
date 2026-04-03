/***********************************************************************************
* @file     : drvuart_port.c
* @brief    : UART port-layer BSP binding implementation.
* @details  : This file binds the generic UART driver interface to the board BSP.
* @author   : GitHub Copilot
* @date     : 2026-03-31
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "drvuart.h"

#include <stddef.h>

#include "bsp_uart.h"

stRingBuffer gUartRxBuffer[DRVUART_MAX];
uint8_t gUartRxStorageDebug[DRVUART_RECVLEN_DEBUGUART];

stDrvUartBspInterface gDrvUartBspInterface[DRVUART_MAX] = {
    {
        .init = bspUartInit,
        .transmit = bspUartTransmit,
        .transmitIt = bspUartTransmitIt,
        .transmitDma = bspUartTransmitDma,
        .getDataLen = bspUartGetDataLen,
        .receive = bspUartReceive,
        .Buffer = gUartRxStorageDebug,
    }
};

stRingBuffer *drvUartPortGetRingBuffer(eDrvUartPortMap uart)
{
    if (uart >= DRVUART_MAX) {
        return NULL;
    }

    return &gUartRxBuffer[uart];
}

eDrvStatus drvUartPortGetStorageConfig(eDrvUartPortMap uart, uint8_t **storage, uint32_t *capacity)
{
    if ((storage == NULL) || (capacity == NULL)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    switch (uart) {
        case DRVUART_DEBUG:
            *storage = gDrvUartBspInterface[uart].Buffer;
            *capacity = DRVUART_RECVLEN_DEBUGUART;
            return DRV_STATUS_OK;
        default:
            *storage = NULL;
            *capacity = 0U;
            return DRV_STATUS_UNSUPPORTED;
    }
}
/**************************End of file********************************/
