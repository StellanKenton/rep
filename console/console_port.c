/************************************************************************************
* @file     : console_port.c
* @brief    : Console project port-layer implementation.
* @details  : Keeps project command registration and integration policy out of the
*             reusable console core.
* @author   : GitHub Copilot
* @date     : 2026-04-03
* @version  : V1.0.0
***********************************************************************************/
#include "console_port.h"

#include "console.h"
#include "drvanlogiic_debug.h"
#include "drvgpio_debug.h"
#include "drviic_debug.h"
#include "drvuart_debug.h"
#include "mpu6050_debug.h"
#include "system_debug.h"
#include "drvspi_debug.h"
#include "w25qxxx_debug.h"

typedef bool (*consoleRegisterHook)(void);

static bool consolePortRegisterDefaultCommands(void);

static bool consolePortRegisterDefaultCommands(void)
{
    static const consoleRegisterHook gConsoleRegisterHooks[] = {
        systemDebugConsoleRegister,
        drvGpioDebugConsoleRegister,
        drvAnlogIicDebugConsoleRegister,
        drvIicDebugConsoleRegister,
        drvSpiDebugConsoleRegister,
        drvUartDebugConsoleRegister,
        mpu6050DebugConsoleRegister,
        w25qxxxDebugConsoleRegister,
    };
    uint32_t lIndex = 0U;

    for (lIndex = 0U; lIndex < (sizeof(gConsoleRegisterHooks) / sizeof(gConsoleRegisterHooks[0])); lIndex++) {
        if (!gConsoleRegisterHooks[lIndex]()) {
            return false;
        }
    }

    return true;
}

bool consolePortInit(void)
{
    static bool gConsolePortReady = false;

    if (gConsolePortReady) {
        return true;
    }

    if (!consoleInit()) {
        return false;
    }

    if (!consolePortRegisterDefaultCommands()) {
        return false;
    }

    gConsolePortReady = true;
    return true;
}
/**************************End of file********************************/