/***********************************************************************************
* @file     : drvgpio.c
* @brief    : Generic MCU GPIO driver abstraction implementation.
* @details  : The platform-specific GPIO operations are provided through port hooks.
* @author   : GitHub Copilot
* @date     : 2026-03-30
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "drvgpio.h"

#if (DRVGPIO_LOG_SUPPORT == 1)
#include "../../Console/log.h"
#endif

#include <stdbool.h>
#include <stddef.h>

#define DRVGPIO_LOG_TAG                 "drvGpio"

extern stDrvGpioBspInterface gDrvGpioBspInterface;

/**
* @brief : Check if the provided logical pin mapping is valid.
* @param : pin GPIO pin mapping identifier.
* @return: true if the pin mapping is valid, false otherwise.
**/
static bool drvGpioIsValidPin(eDrvGpioPinMap pin)
{
    return pin < DRVGPIO_MAX;
}

/**
* @brief : Check whether the BSP hook table is complete.
* @param : None
* @return: true when all required hooks are available.
**/
static bool drvGpioHasValidBspInterface(void)
{
    return (gDrvGpioBspInterface.init != NULL) &&
           (gDrvGpioBspInterface.write != NULL) &&
           (gDrvGpioBspInterface.read != NULL) &&
           (gDrvGpioBspInterface.toggle != NULL);
}

/**
* @brief : Initialize the GPIO driver and configure pins.
* @param : None
* @return: None
**/
void drvGpioInit(void)
{
    if (!drvGpioHasValidBspInterface()) {
        #if (DRVGPIO_LOG_SUPPORT == 1)
        LOG_E(DRVGPIO_LOG_TAG, "Invalid BSP interface configuration");
        LOG_E(DRVGPIO_LOG_TAG, "Please ensure all BSP function hooks are properly assigned");
        #endif
        return;
    }

    // bsp init function
    gDrvGpioBspInterface.init();
}

/**
* @brief : Drive the specified GPIO pin to the requested logic state.
* @param : pin   GPIO pin mapping identifier.
* @param : state Target GPIO output state.
* @return: None
**/
void drvGpioWrite(eDrvGpioPinMap pin, eDrvGpioPinState state)
{
    if (!drvGpioIsValidPin(pin)) {
        #if (DRVGPIO_LOG_SUPPORT == 1)
        LOG_E(DRVGPIO_LOG_TAG, "Invalid GPIO pin: %d", pin);
        #endif
        return;
    }

    if ((state != DRVGPIO_PIN_RESET) && (state != DRVGPIO_PIN_SET)) {
        #if (DRVGPIO_LOG_SUPPORT == 1)
        LOG_E(DRVGPIO_LOG_TAG, "Invalid GPIO state: %d", state);
        #endif
        return;
    }

    if (gDrvGpioBspInterface.write == NULL) {
        #if (DRVGPIO_LOG_SUPPORT == 1)
        LOG_E(DRVGPIO_LOG_TAG, "GPIO write hook is not configured");
        #endif
        return;
    }

    // bsp write function
    gDrvGpioBspInterface.write(pin, state);
}

/**
* @brief : Read the current logic state of the specified GPIO pin.
* @param : pin GPIO pin mapping identifier.
* @return: Current GPIO pin state.
**/
eDrvGpioPinState drvGpioRead(eDrvGpioPinMap pin)
{
    eDrvGpioPinState lState;

    if (!drvGpioIsValidPin(pin)) {
        #if (DRVGPIO_LOG_SUPPORT == 1)
        LOG_E(DRVGPIO_LOG_TAG, "Invalid GPIO pin: %d", pin);
        #endif
        return DRVGPIO_PIN_STATE_INVALID;
    }

    if (gDrvGpioBspInterface.read == NULL) {
        #if (DRVGPIO_LOG_SUPPORT == 1)
        LOG_E(DRVGPIO_LOG_TAG, "GPIO read hook is not configured");
        #endif
        return DRVGPIO_PIN_STATE_INVALID;
    }

    // bsp read function
    lState = gDrvGpioBspInterface.read(pin);
    return lState;
}

/**
* @brief : Toggle the output state of the specified GPIO pin.
* @param : pin GPIO pin mapping identifier.
* @return: None
**/
void drvGpioToggle(eDrvGpioPinMap pin)
{
    eDrvGpioPinState lTargetState;

    if (!drvGpioIsValidPin(pin)) {
        #if (DRVGPIO_LOG_SUPPORT == 1)
        LOG_E(DRVGPIO_LOG_TAG, "Invalid GPIO pin: %d", pin);
        #endif
        return;
    }

    if (gDrvGpioBspInterface.toggle == NULL) {
        #if (DRVGPIO_LOG_SUPPORT == 1)
        LOG_W(DRVGPIO_LOG_TAG, "GPIO toggle hook is not configured, using read/write fallback");
        #endif
        lTargetState = (drvGpioRead(pin) == DRVGPIO_PIN_SET) ? DRVGPIO_PIN_RESET : DRVGPIO_PIN_SET;
        drvGpioWrite(pin, lTargetState);
        return;
    }

    // bsp toggle function
    gDrvGpioBspInterface.toggle(pin);
}


/**************************End of file********************************/
