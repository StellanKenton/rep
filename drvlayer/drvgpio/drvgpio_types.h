/************************************************************************************
* @file     : drvgpio_types.h
* @brief    : Public GPIO logical pin definitions.
* @details  : Keeps reusable GPIO API dependencies independent from the port layer.
***********************************************************************************/
#ifndef DRVGPIO_TYPES_H
#define DRVGPIO_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eDrvGpioPinState {
    DRVGPIO_PIN_RESET = 0,
    DRVGPIO_PIN_SET,
    DRVGPIO_PIN_STATE_INVALID
} eDrvGpioPinState;

typedef enum eDrvGpioPinMap {
    DRVGPIO_LEDR = 0,
    DRVGPIO_LEDG = 1,
    DRVGPIO_LEDB = 2,
    DRVGPIO_KEY = 3,
    DRVGPIO_MAX,
} eDrvGpioPinMap;

#ifdef __cplusplus
}
#endif

#endif  // DRVGPIO_TYPES_H
/**************************End of file********************************/