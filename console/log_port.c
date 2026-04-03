/************************************************************************************
* @file     : log_port.c
* @brief    : Log project port-layer implementation.
* @details  : Keeps RTT and UART transport bindings out of the reusable log core.
***********************************************************************************/
#include "log_port.h"

#include <stddef.h>

#include "../../../SEGGER/bsp_rtt.h"
#include "../drvlayer/drvuart/drvuart.h"

#ifndef LOG_PORT_UART_MAP
#define LOG_PORT_UART_MAP                DRVUART_DEBUG
#endif

#ifndef LOG_PORT_UART_TX_TIMEOUT_MS
#define LOG_PORT_UART_TX_TIMEOUT_MS      100U
#endif

static void logPortUartInit(void);
static int32_t logPortUartWrite(const uint8_t *buffer, uint16_t length);
static stRingBuffer *logPortUartGetInputBuffer(void);

static const stLogInterface gLogPortInterfaces[LOG_PORT_INTERFACE_COUNT] = {
    {
        .transport = LOG_TRANSPORT_RTT,
        .init = bspRttLogInit,
        .write = bspRttLogWrite,
        .getBuffer = bspRttLogGetInputBuffer,
        .isOutputEnabled = (LOG_PORT_RTT_OUTPUT_ENABLE != 0),
        .isInputEnabled = (LOG_PORT_RTT_INPUT_ENABLE != 0),
    },
    {
        .transport = LOG_TRANSPORT_UART,
        .init = logPortUartInit,
        .write = logPortUartWrite,
        .getBuffer = logPortUartGetInputBuffer,
        .isOutputEnabled = (LOG_PORT_UART_OUTPUT_ENABLE != 0),
        .isInputEnabled = (LOG_PORT_UART_INPUT_ENABLE != 0),
    },
};

const stLogInterface *logPortGetInterfaces(void)
{
    return gLogPortInterfaces;
}

uint32_t logPortGetInterfaceCount(void)
{
    return LOG_PORT_INTERFACE_COUNT;
}

static void logPortUartInit(void)
{
    (void)drvUartInit(LOG_PORT_UART_MAP);
}

static int32_t logPortUartWrite(const uint8_t *buffer, uint16_t length)
{
    eDrvStatus lStatus;

    if ((buffer == NULL) || (length == 0U)) {
        return 0;
    }

    lStatus = drvUartTransmitDma(LOG_PORT_UART_MAP, buffer, length);
    if (lStatus == DRV_STATUS_UNSUPPORTED) {
        lStatus = drvUartTransmit(LOG_PORT_UART_MAP, buffer, length, LOG_PORT_UART_TX_TIMEOUT_MS);
    }

    if (lStatus != DRV_STATUS_OK) {
        return 0;
    }

    return (int32_t)length;
}

static stRingBuffer *logPortUartGetInputBuffer(void)
{
    return drvUartGetRingBuffer(LOG_PORT_UART_MAP);
}
/**************************End of file********************************/
