/************************************************************************************
* @file     : log_port.c
* @brief    : Log project port-layer implementation.
* @details  : Keeps RTT and UART transport bindings out of the reusable log core.
***********************************************************************************/
#include "log_port.h"

#include <stddef.h>

#include "../../SEGGER/bsp_rtt.h"

static const stLogInterface gLogPortInterfaces[LOG_PORT_INTERFACE_COUNT] = {
    {
        .transport = LOG_TRANSPORT_RTT,
        .init = bspRttLogInit,
        .write = bspRttLogWrite,
        .getBuffer = bspRttLogGetInputBuffer,
        .isOutputEnabled = (LOG_PORT_RTT_OUTPUT_ENABLE != 0),
        .isInputEnabled = (LOG_PORT_RTT_INPUT_ENABLE != 0),
    },
};

const stLogInterface *logGetPlatformInterfaces(void)
{
    return gLogPortInterfaces;
}

uint32_t logGetPlatformInterfaceCount(void)
{
    return LOG_PORT_INTERFACE_COUNT;
}

const stLogInterface *logPortGetInterfaces(void)
{
    return logGetPlatformInterfaces();
}

uint32_t logPortGetInterfaceCount(void)
{
    return logGetPlatformInterfaceCount();
}
/**************************End of file********************************/
