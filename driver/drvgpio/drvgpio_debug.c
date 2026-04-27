/************************************************************************************
* @file     : drvgpio_debug.c
* @brief    : DrvGpio debug and console command implementation.
* @details  : Placeholder for optional GPIO console bindings.
* @author   : GitHub Copilot
* @date     : 2026-04-27
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "drvgpio_debug.h"

#include "drvgpio.h"

bool drvGpioDebugConsoleRegister(void)
{
#if (DRVGPIO_CONSOLE_SUPPORT == 1)
    return true;
#else
    return true;
#endif
}
/**************************End of file********************************/

