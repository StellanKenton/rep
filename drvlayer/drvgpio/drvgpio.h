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

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DRVGPIO_LOG_SUPPORT
#define DRVGPIO_LOG_SUPPORT             1
#endif

#ifndef DRVGPIO_CONSOLE_SUPPORT
#define DRVGPIO_CONSOLE_SUPPORT         1
#endif

#ifndef DRVGPIO_MAX
#define DRVGPIO_MAX                     4U
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

void drvGpioInit(void);
void drvGpioWrite(uint8_t pin, eDrvGpioPinState state);
eDrvGpioPinState drvGpioRead(uint8_t pin);
void drvGpioToggle(uint8_t pin);

const stDrvGpioBspInterface *drvGpioGetPlatformBspInterface(void);

#ifdef __cplusplus
}
#endif

#endif  // DRVGPIO_H
/**************************End of file********************************/
