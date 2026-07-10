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
#include <stdint.h>

#include "rep_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eDrvGpioPinState {
    DRVGPIO_PIN_RESET = 0,
    DRVGPIO_PIN_SET,
    DRVGPIO_PIN_STATE_INVALID
} eDrvGpioPinState;

typedef void (*drvGpioBspInitFunc)(void);
typedef void (*drvGpioBspWriteFunc)(uint8_t pin, eDrvGpioPinState state);
typedef eDrvGpioPinState (*drvGpioBspReadFunc)(uint8_t pin);
typedef void (*drvGpioBspToggleFunc)(uint8_t pin);

typedef struct stDrvGpioBspInterface {
    drvGpioBspInitFunc init;
    drvGpioBspWriteFunc write;
    drvGpioBspReadFunc read;
    drvGpioBspToggleFunc toggle;
} stDrvGpioBspInterface;

typedef struct stDrvGpioOps {
    const stDrvGpioBspInterface *(*getBspInterface)(void);
} stDrvGpioOps;

void drvGpioInit(void);
void drvGpioWrite(uint8_t pin, eDrvGpioPinState state);
eDrvGpioPinState drvGpioRead(uint8_t pin);
void drvGpioToggle(uint8_t pin);

#ifdef __cplusplus
}
#endif

#endif  // DRVGPIO_H
/**************************End of file********************************/
