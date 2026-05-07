/***********************************************************************************
* @file     : drvgpio_debug.c
* @brief    : DrvGpio debug and console command implementation.
* @details  : This file hosts optional console bindings for GPIO debug operations.
* @author   : GitHub Copilot
* @date     : 2026-05-07
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "drvgpio_debug.h"

#include "drvgpio.h"

#if (DRVGPIO_CONSOLE_SUPPORT == 1)
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "drvgpio_port.h"

#include "../../service/log/console.h"

typedef struct stDrvGpioDebugPinInfo {
    const char *enumName;
    const char *alias;
} stDrvGpioDebugPinInfo;

static bool drvGpioDebugParsePin(const char *text, uint8_t *pin);
static bool drvGpioDebugParseUint32(const char *text, uint32_t *value);
static bool drvGpioDebugParseState(const char *text, eDrvGpioPinState *state);
static const char *drvGpioDebugGetPinName(uint8_t pin);
static const char *drvGpioDebugGetPinAlias(uint8_t pin);
static const char *drvGpioDebugGetStateText(eDrvGpioPinState state);
static eConsoleCommandResult drvGpioDebugReplyPinList(uint32_t transport);
static eConsoleCommandResult drvGpioDebugReplyPinState(uint32_t transport, uint8_t pin);
static eConsoleCommandResult drvGpioDebugReplyHelp(uint32_t transport);
static eConsoleCommandResult drvGpioDebugConsoleHandler(uint32_t transport, int argc, char *argv[]);

static const stDrvGpioDebugPinInfo gDrvGpioDebugPinInfo[DRVGPIO_MAX] = {
    [DRVGPIO_EN_AUDIO] = {
        .enumName = "DRVGPIO_EN_AUDIO",
        .alias = "audio_en",
    },
    [DRVGPIO_RESET_WIFI] = {
        .enumName = "DRVGPIO_RESET_WIFI",
        .alias = "wifi_reset",
    },
    [DRVGPIO_USB_SELECT] = {
        .enumName = "DRVGPIO_USB_SELECT",
        .alias = "usb_select",
    },
    [DRVGPIO_POWER_ON_CHECK] = {
        .enumName = "DRVGPIO_POWER_ON_CHECK",
        .alias = "power_on_check",
    },
    [DRVGPIO_BAT_CHARGING_STATUS] = {
        .enumName = "DRVGPIO_BAT_CHARGING_STATUS",
        .alias = "bat_charging",
    },
    [DRVGPIO_BAT_CHARGE_DONE_STATUS] = {
        .enumName = "DRVGPIO_BAT_CHARGE_DONE_STATUS",
        .alias = "bat_charge_done",
    },
    [DRVGPIO_PCA9535_SCL] = {
        .enumName = "DRVGPIO_PCA9535_SCL",
        .alias = "pca9535_scl",
    },
    [DRVGPIO_PCA9535_SDA] = {
        .enumName = "DRVGPIO_PCA9535_SDA",
        .alias = "pca9535_sda",
    },
    [DRVGPIO_PCA9535_RESET] = {
        .enumName = "DRVGPIO_PCA9535_RESET",
        .alias = "pca9535_reset",
    },
    [DRVGPIO_TM1651_CLK] = {
        .enumName = "DRVGPIO_TM1651_CLK",
        .alias = "tm1651_clk",
    },
    [DRVGPIO_TM1651_SDA] = {
        .enumName = "DRVGPIO_TM1651_SDA",
        .alias = "tm1651_sda",
    },
    [DRVGPIO_SPI_CS] = {
        .enumName = "DRVGPIO_SPI_CS",
        .alias = "spi_cs",
    },
    [DRVGPIO_POWER_ON_CTRL] = {
        .enumName = "DRVGPIO_POWER_ON_CTRL",
        .alias = "power_on_ctrl",
    },
};

static const stConsoleCommand gDrvGpioConsoleCommand = {
    .commandName = "gpio",
    .helpText = "gpio <list|read|write|toggle|init|help> ...",
    .ownerTag = "drvGpio",
    .handler = drvGpioDebugConsoleHandler,
};

static bool drvGpioDebugParseUint32(const char *text, uint32_t *value)
{
    uint32_t lValue = 0U;
    const char *lCursor = text;

    if ((text == NULL) || (value == NULL) || (text[0] == '\0')) {
        return false;
    }

    while (*lCursor != '\0') {
        if ((*lCursor < '0') || (*lCursor > '9')) {
            return false;
        }

        lValue = (lValue * 10U) + (uint32_t)(*lCursor - '0');
        lCursor++;
    }

    *value = lValue;
    return true;
}

static bool drvGpioDebugParsePin(const char *text, uint8_t *pin)
{
    uint32_t lValue;
    uint8_t lPin;

    if ((text == NULL) || (pin == NULL)) {
        return false;
    }

    if (drvGpioDebugParseUint32(text, &lValue) && (lValue < DRVGPIO_MAX)) {
        *pin = (uint8_t)lValue;
        return true;
    }

    for (lPin = 0U; lPin < DRVGPIO_MAX; lPin++) {
        if (((gDrvGpioDebugPinInfo[lPin].enumName != NULL) && (strcmp(text, gDrvGpioDebugPinInfo[lPin].enumName) == 0)) ||
            ((gDrvGpioDebugPinInfo[lPin].alias != NULL) && (strcmp(text, gDrvGpioDebugPinInfo[lPin].alias) == 0))) {
            *pin = lPin;
            return true;
        }
    }

    return false;
}

static bool drvGpioDebugParseState(const char *text, eDrvGpioPinState *state)
{
    if ((text == NULL) || (state == NULL)) {
        return false;
    }

    if ((strcmp(text, "0") == 0) ||
        (strcmp(text, "low") == 0) ||
        (strcmp(text, "reset") == 0)) {
        *state = DRVGPIO_PIN_RESET;
        return true;
    }

    if ((strcmp(text, "1") == 0) ||
        (strcmp(text, "high") == 0) ||
        (strcmp(text, "set") == 0)) {
        *state = DRVGPIO_PIN_SET;
        return true;
    }

    return false;
}

static const char *drvGpioDebugGetPinName(uint8_t pin)
{
    if ((pin >= DRVGPIO_MAX) || (gDrvGpioDebugPinInfo[pin].enumName == NULL)) {
        return "unknown";
    }

    return gDrvGpioDebugPinInfo[pin].enumName;
}

static const char *drvGpioDebugGetPinAlias(uint8_t pin)
{
    if ((pin >= DRVGPIO_MAX) || (gDrvGpioDebugPinInfo[pin].alias == NULL)) {
        return "-";
    }

    return gDrvGpioDebugPinInfo[pin].alias;
}

static const char *drvGpioDebugGetStateText(eDrvGpioPinState state)
{
    switch (state) {
        case DRVGPIO_PIN_RESET:
            return "0";
        case DRVGPIO_PIN_SET:
            return "1";
        default:
            return "invalid";
    }
}

static eConsoleCommandResult drvGpioDebugReplyPinList(uint32_t transport)
{
    uint8_t lPin;

    for (lPin = 0U; lPin < DRVGPIO_MAX; lPin++) {
        eDrvGpioPinState lState = drvGpioRead(lPin);

        if (consoleReply(transport,
            "%u %s alias=%s state=%s",
            (unsigned int)lPin,
            drvGpioDebugGetPinName(lPin),
            drvGpioDebugGetPinAlias(lPin),
            drvGpioDebugGetStateText(lState)) <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }
    }

    if (consoleReply(transport, "OK") <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult drvGpioDebugReplyPinState(uint32_t transport, uint8_t pin)
{
    eDrvGpioPinState lState = drvGpioRead(pin);

    if (consoleReply(transport,
        "%u %s alias=%s state=%s\nOK",
        (unsigned int)pin,
        drvGpioDebugGetPinName(pin),
        drvGpioDebugGetPinAlias(pin),
        drvGpioDebugGetStateText(lState)) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return (lState == DRVGPIO_PIN_STATE_INVALID) ? CONSOLE_COMMAND_RESULT_ERROR : CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult drvGpioDebugReplyHelp(uint32_t transport)
{
    if (consoleReply(transport,
        "gpio list\n"
        "gpio read <pin>\n"
        "gpio write <pin> <0|1|low|high|reset|set>\n"
        "gpio toggle <pin>\n"
        "gpio init\n"
        "pin supports index, enum name, or alias such as DRVGPIO_RESET_WIFI / wifi_reset\n"
        "OK") <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult drvGpioDebugConsoleHandler(uint32_t transport, int argc, char *argv[])
{
    uint8_t lPin;
    eDrvGpioPinState lState;

    if ((argc < 2) || (argv == NULL)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (strcmp(argv[1], "list") == 0) {
        return drvGpioDebugReplyPinList(transport);
    }

    if (strcmp(argv[1], "help") == 0) {
        return drvGpioDebugReplyHelp(transport);
    }

    if (strcmp(argv[1], "init") == 0) {
        drvGpioInit();
        if (consoleReply(transport, "gpio init=ok\nOK") <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }
        return CONSOLE_COMMAND_RESULT_OK;
    }

    if ((argc < 3) || !drvGpioDebugParsePin(argv[2], &lPin)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (strcmp(argv[1], "read") == 0) {
        return drvGpioDebugReplyPinState(transport, lPin);
    }

    if (strcmp(argv[1], "toggle") == 0) {
        drvGpioToggle(lPin);
        return drvGpioDebugReplyPinState(transport, lPin);
    }

    if ((strcmp(argv[1], "write") == 0) && (argc >= 4) && drvGpioDebugParseState(argv[3], &lState)) {
        drvGpioWrite(lPin, lState);
        return drvGpioDebugReplyPinState(transport, lPin);
    }

    return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
}
#endif

bool drvGpioDebugConsoleRegister(void)
{
#if (DRVGPIO_CONSOLE_SUPPORT == 1)
    return consoleRegisterCommand(&gDrvGpioConsoleCommand);
#else
    return true;
#endif
}
/**************************End of file********************************/
