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
#include "../../service/log/log.h"
#endif

#include <stdbool.h>
#include <stddef.h>

#define DRVGPIO_LOG_TAG                 "drvGpio"

static uint32_t gDrvGpioInvalidPinMask = 0U;
static bool gDrvGpioInvalidBspLogged = false;
static bool gDrvGpioInvalidStateLogged = false;
static bool gDrvGpioWriteHookMissingLogged = false;
static bool gDrvGpioReadHookMissingLogged = false;
static bool gDrvGpioToggleHookMissingLogged = false;

static void drvGpioLogInvalidPinOnce(uint8_t pin);

__attribute__((weak)) const stDrvGpioBspInterface *drvGpioGetPlatformBspInterface(void)
{
    return NULL;
}

static void drvGpioLogInvalidPinOnce(uint8_t pin)
{
    uint32_t lPinMask;

    if (pin >= 32U) {
        if ((gDrvGpioInvalidPinMask & 0x80000000UL) == 0U) {
            gDrvGpioInvalidPinMask |= 0x80000000UL;
            LOG_E(DRVGPIO_LOG_TAG, "Invalid GPIO pin: %d", pin);
        }
        return;
    }

    lPinMask = (uint32_t)1UL << pin;
    if ((gDrvGpioInvalidPinMask & lPinMask) != 0U) {
        return;
    }

    gDrvGpioInvalidPinMask |= lPinMask;
    LOG_E(DRVGPIO_LOG_TAG, "Invalid GPIO pin: %d", pin);
}

/**
* @brief : Check if the provided logical pin mapping is valid.
* @param : pin GPIO pin mapping identifier.
* @return: true if the pin mapping is valid, false otherwise.
**/
static bool drvGpioIsValidPin(uint8_t pin)
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
    const stDrvGpioBspInterface *lBspInterface = drvGpioGetPlatformBspInterface();

    return (lBspInterface != NULL) &&
           (lBspInterface->init != NULL) &&
           (lBspInterface->write != NULL) &&
           (lBspInterface->read != NULL) &&
           (lBspInterface->toggle != NULL);
}

/**
* @brief : Initialize the GPIO driver and configure pins.
* @param : None
* @return: None
**/
void drvGpioInit(void)
{
    const stDrvGpioBspInterface *lBspInterface = drvGpioGetPlatformBspInterface();

    if (!drvGpioHasValidBspInterface()) {
        #if (DRVGPIO_LOG_SUPPORT == 1)
        if (!gDrvGpioInvalidBspLogged) {
            gDrvGpioInvalidBspLogged = true;
            LOG_E(DRVGPIO_LOG_TAG, "Invalid BSP interface configuration");
            LOG_E(DRVGPIO_LOG_TAG, "Please ensure all BSP function hooks are properly assigned");
        }
        #endif
        return;
    }

    // bsp init function
    lBspInterface->init();
}

/**
* @brief : Drive the specified GPIO pin to the requested logic state.
* @param : pin   GPIO pin mapping identifier.
* @param : state Target GPIO output state.
* @return: None
**/
void drvGpioWrite(uint8_t pin, eDrvGpioPinState state)
{
    const stDrvGpioBspInterface *lBspInterface = drvGpioGetPlatformBspInterface();

    if (!drvGpioIsValidPin(pin)) {
        #if (DRVGPIO_LOG_SUPPORT == 1)
        drvGpioLogInvalidPinOnce(pin);
        #endif
        return;
    }

    if ((state != DRVGPIO_PIN_RESET) && (state != DRVGPIO_PIN_SET)) {
        #if (DRVGPIO_LOG_SUPPORT == 1)
        if (!gDrvGpioInvalidStateLogged) {
            gDrvGpioInvalidStateLogged = true;
            LOG_E(DRVGPIO_LOG_TAG, "Invalid GPIO state: %d", state);
        }
        #endif
        return;
    }

    if ((lBspInterface == NULL) || (lBspInterface->write == NULL)) {
        #if (DRVGPIO_LOG_SUPPORT == 1)
        if (!gDrvGpioWriteHookMissingLogged) {
            gDrvGpioWriteHookMissingLogged = true;
            LOG_E(DRVGPIO_LOG_TAG, "GPIO write hook is not configured");
        }
        #endif
        return;
    }

    // bsp write function
    lBspInterface->write(pin, state);
}

/**
* @brief : Read the current logic state of the specified GPIO pin.
* @param : pin GPIO pin mapping identifier.
* @return: Current GPIO pin state.
**/
eDrvGpioPinState drvGpioRead(uint8_t pin)
{
    eDrvGpioPinState lState;
    const stDrvGpioBspInterface *lBspInterface = drvGpioGetPlatformBspInterface();

    if (!drvGpioIsValidPin(pin)) {
        #if (DRVGPIO_LOG_SUPPORT == 1)
        drvGpioLogInvalidPinOnce(pin);
        #endif
        return DRVGPIO_PIN_STATE_INVALID;
    }

    if ((lBspInterface == NULL) || (lBspInterface->read == NULL)) {
        #if (DRVGPIO_LOG_SUPPORT == 1)
        if (!gDrvGpioReadHookMissingLogged) {
            gDrvGpioReadHookMissingLogged = true;
            LOG_E(DRVGPIO_LOG_TAG, "GPIO read hook is not configured");
        }
        #endif
        return DRVGPIO_PIN_STATE_INVALID;
    }

    // bsp read function
    lState = lBspInterface->read(pin);
    return lState;
}

/**
* @brief : Toggle the output state of the specified GPIO pin.
* @param : pin GPIO pin mapping identifier.
* @return: None
**/
void drvGpioToggle(uint8_t pin)
{
    eDrvGpioPinState lTargetState;
    const stDrvGpioBspInterface *lBspInterface = drvGpioGetPlatformBspInterface();

    if (!drvGpioIsValidPin(pin)) {
        #if (DRVGPIO_LOG_SUPPORT == 1)
        drvGpioLogInvalidPinOnce(pin);
        #endif
        return;
    }

    if ((lBspInterface == NULL) || (lBspInterface->toggle == NULL)) {
        #if (DRVGPIO_LOG_SUPPORT == 1)
        if (!gDrvGpioToggleHookMissingLogged) {
            gDrvGpioToggleHookMissingLogged = true;
            LOG_W(DRVGPIO_LOG_TAG, "GPIO toggle hook is not configured, using read/write fallback");
        }
        #endif
        lTargetState = (drvGpioRead(pin) == DRVGPIO_PIN_SET) ? DRVGPIO_PIN_RESET : DRVGPIO_PIN_SET;
        drvGpioWrite(pin, lTargetState);
        return;
    }

    // bsp toggle function
    lBspInterface->toggle(pin);
}

/**************************End of file********************************/
