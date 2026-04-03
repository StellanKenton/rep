/************************************************************************************
* @file     : drvgpio.h
* @brief    : Generic MCU GPIO driver abstraction.
* @details  : This module defines a small GPIO interface for project-level drivers.
* @author   : GitHub Copilot
* @date     : 2026-03-30
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef DRVGPIO_H
#define DRVGPIO_H

#include <stdbool.h>
#include "drvgpio_port.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef void (*drvGpioBspInitFunc)(void);
typedef void (*drvGpioBspWriteFunc)(eDrvGpioPinMap pin, eDrvGpioPinState state);
typedef eDrvGpioPinState (*drvGpioBspReadFunc)(eDrvGpioPinMap pin);
typedef void (*drvGpioBspToggleFunc)(eDrvGpioPinMap pin);

typedef struct stDrvGpioBspInterface {
    drvGpioBspInitFunc init;
    drvGpioBspWriteFunc write;
    drvGpioBspReadFunc read;
    drvGpioBspToggleFunc toggle;
} stDrvGpioBspInterface;

void drvGpioInit(void);
void drvGpioWrite(eDrvGpioPinMap pin, eDrvGpioPinState state);
eDrvGpioPinState drvGpioRead(eDrvGpioPinMap pin);
void drvGpioToggle(eDrvGpioPinMap pin);

#ifdef __cplusplus
}
#endif

#endif  // DRVGPIO_H
/**************************End of file********************************/
