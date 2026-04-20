/***********************************************************************************
* @file     : drvgpio_debug.c
* @brief    : DrvGpio debug and console command implementation.
* @details  : This file hosts optional console bindings for GPIO debug operations.
* @author   : GitHub Copilot
* @date     : 2026-04-01
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "drvgpio_debug.h"

#include "drvgpio.h"
#include "drvgpio_port.h"

#if (DRVGPIO_CONSOLE_SUPPORT == 1)
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "../../service/log/console.h"

static bool drvGpioDebugFindPinByName(const char *pinName, eDrvGpioPinMap *pin);
static const char *drvGpioDebugGetPinName(eDrvGpioPinMap pin);
static bool drvGpioDebugIsPinReadable(eDrvGpioPinMap pin);
static bool drvGpioDebugIsPinWritable(eDrvGpioPinMap pin);
static bool drvGpioDebugIsPinToggleSupported(eDrvGpioPinMap pin);
static bool drvGpioDebugParseConsoleState(const char *argument, eDrvGpioPinState *state);
static const char *drvGpioDebugGetStateText(eDrvGpioPinState state);
static eConsoleCommandResult drvGpioDebugReplyPinState(uint32_t transport, eDrvGpioPinMap pin);
static eConsoleCommandResult drvGpioDebugReplyPinList(uint32_t transport);
static eConsoleCommandResult drvGpioDebugReplyHelp(uint32_t transport, int argc, char *argv[]);
static eConsoleCommandResult drvGpioDebugConsoleHandler(uint32_t transport, int argc, char *argv[]);

static const stConsoleCommand gDrvGpioConsoleCommand = {
    .commandName = "gpio",
    .helpText = "gpio <list|get|set|toggle|help> ...",
    .ownerTag = "drvGpio",
    .handler = drvGpioDebugConsoleHandler,
};

/**
* @brief : Find a GPIO pin descriptor by logical pin name.
* @param : pinName Logical pin name.
* @param : pin Parsed logical pin output.
* @return: true when the pin name is valid.
**/
static bool drvGpioDebugFindPinByName(const char *pinName, eDrvGpioPinMap *pin)
{
    if ((pinName == NULL) || (pin == NULL)) {
        return false;
    }

    if (strcmp(pinName, "ledr") == 0) {
        *pin = DRVGPIO_LEDR;
        return true;
    }

    if (strcmp(pinName, "ledg") == 0) {
        *pin = DRVGPIO_LEDG;
        return true;
    }

    if (strcmp(pinName, "ledb") == 0) {
        *pin = DRVGPIO_LEDB;
        return true;
    }

    if (strcmp(pinName, "key") == 0) {
        *pin = DRVGPIO_KEY;
        return true;
    }

    if (strcmp(pinName, "key1") == 0) {
        *pin = DRVGPIO_KEY;
        return true;
    }

    return false;
}

/**
* @brief : Convert a logical pin into its console name.
* @param : pin Logical pin.
* @return: Pin name text, or NULL when invalid.
**/
static const char *drvGpioDebugGetPinName(eDrvGpioPinMap pin)
{
    switch (pin) {
        case DRVGPIO_LEDR:
            return "ledr";
        case DRVGPIO_LEDG:
            return "ledg";
        case DRVGPIO_LEDB:
            return "ledb";
        case DRVGPIO_KEY:
            return "key";
        default:
            return NULL;
    }
}

/**
* @brief : Check whether a logical pin supports read access.
* @param : pin Logical pin.
* @return: true when the pin is readable.
**/
static bool drvGpioDebugIsPinReadable(eDrvGpioPinMap pin)
{
    switch (pin) {
        case DRVGPIO_LEDR:
        case DRVGPIO_LEDG:
        case DRVGPIO_LEDB:
        case DRVGPIO_KEY:
            return true;
        default:
            return false;
    }
}

/**
* @brief : Check whether a logical pin supports write access.
* @param : pin Logical pin.
* @return: true when the pin is writable.
**/
static bool drvGpioDebugIsPinWritable(eDrvGpioPinMap pin)
{
    switch (pin) {
        case DRVGPIO_LEDR:
        case DRVGPIO_LEDG:
        case DRVGPIO_LEDB:
            return true;
        default:
            return false;
    }
}

/**
* @brief : Check whether a logical pin supports toggle access.
* @param : pin Logical pin.
* @return: true when the pin supports toggle.
**/
static bool drvGpioDebugIsPinToggleSupported(eDrvGpioPinMap pin)
{
    switch (pin) {
        case DRVGPIO_LEDR:
        case DRVGPIO_LEDG:
        case DRVGPIO_LEDB:
            return true;
        default:
            return false;
    }
}

/**
* @brief : Parse the console on/off argument into logical GPIO state.
* @param : argument Console command state argument.
* @param : state Parsed logical GPIO state output.
* @return: true when the argument is valid.
**/
static bool drvGpioDebugParseConsoleState(const char *argument, eDrvGpioPinState *state)
{
    if ((argument == NULL) || (state == NULL)) {
        return false;
    }

    if ((strcmp(argument, "0") == 0) ||
        (strcmp(argument, "on") == 0) ||
        (strcmp(argument, "set") == 0) ||
        (strcmp(argument, "active") == 0)) {
        *state = DRVGPIO_PIN_SET;
        return true;
    }

    if ((strcmp(argument, "1") == 0) ||
        (strcmp(argument, "off") == 0) ||
        (strcmp(argument, "reset") == 0) ||
        (strcmp(argument, "inactive") == 0)) {
        *state = DRVGPIO_PIN_RESET;
        return true;
    }

    return false;
}

/**
* @brief : Convert a GPIO state into console text.
* @param : state Logical GPIO state.
* @return: Text description for console replies.
**/
static const char *drvGpioDebugGetStateText(eDrvGpioPinState state)
{
    if (state == DRVGPIO_PIN_SET) {
        return "set";
    }

    if (state == DRVGPIO_PIN_RESET) {
        return "reset";
    }

    return "invalid";
}

/**
* @brief : Reply with the current state of a logical pin.
* @param : transport Console reply transport.
* @param : pin Target logical pin.
* @return: Console command execution result.
**/
static eConsoleCommandResult drvGpioDebugReplyPinState(uint32_t transport, eDrvGpioPinMap pin)
{
    eDrvGpioPinState lState;
    const char *lPinName;

    if (!drvGpioDebugIsPinReadable(pin)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    lPinName = drvGpioDebugGetPinName(pin);
    if (lPinName == NULL) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    lState = drvGpioRead(pin);
    if (lState == DRVGPIO_PIN_STATE_INVALID) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (consoleReply(transport, "%s=%s\nOK", lPinName, drvGpioDebugGetStateText(lState)) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

/**
* @brief : Reply with all registered logical pins and capabilities.
* @param : transport Console reply transport.
* @return: Console command execution result.
**/
static eConsoleCommandResult drvGpioDebugReplyPinList(uint32_t transport)
{
    int32_t lReplyResult;
    eDrvGpioPinMap lPins[] = {
        DRVGPIO_LEDR,
        DRVGPIO_LEDG,
        DRVGPIO_LEDB,
        DRVGPIO_KEY,
    };
    uint32_t lIndex;

    for (lIndex = 0U; lIndex < ((uint32_t)(sizeof(lPins) / sizeof(lPins[0]))); ++lIndex) {
        lReplyResult = consoleReply(transport,
            "%s read=%s write=%s toggle=%s\n",
            drvGpioDebugGetPinName(lPins[lIndex]),
            drvGpioDebugIsPinReadable(lPins[lIndex]) ? "yes" : "no",
            drvGpioDebugIsPinWritable(lPins[lIndex]) ? "yes" : "no",
            drvGpioDebugIsPinToggleSupported(lPins[lIndex]) ? "yes" : "no");
        if (lReplyResult <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }
    }

    if (consoleReply(transport, "OK") <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

/**
* @brief : Handle the generic GPIO console command.
* @param : transport Console reply transport.
* @param : argc Argument count.
* @param : argv Argument vector.
* @return: Console command execution result.
**/
static eConsoleCommandResult drvGpioDebugConsoleHandler(uint32_t transport, int argc, char *argv[])
{
    eDrvGpioPinMap lPin;
    eDrvGpioPinState lTargetState;

    if ((argc < 2) || (argv == NULL) || (argv[1] == NULL)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (strcmp(argv[1], "list") == 0) {
        if (argc != 2) {
            return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
        }

        return drvGpioDebugReplyPinList(transport);
    }

    if (strcmp(argv[1], "help") == 0) {
        return drvGpioDebugReplyHelp(transport, argc, argv);
    }

    if (argc < 3) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (!drvGpioDebugFindPinByName(argv[2], &lPin)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if ((strcmp(argv[1], "get") == 0) || (strcmp(argv[1], "read") == 0)) {
        if (argc != 3) {
            return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
        }

        return drvGpioDebugReplyPinState(transport, lPin);
    }

    if ((strcmp(argv[1], "set") == 0) || (strcmp(argv[1], "write") == 0)) {
        if ((argc != 4) || !drvGpioDebugIsPinWritable(lPin)) {
            return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
        }

        if (!drvGpioDebugParseConsoleState(argv[3], &lTargetState)) {
            return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
        }

        drvGpioWrite(lPin, lTargetState);
        if (consoleReply(transport, "OK") <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_OK;
    }

    if (strcmp(argv[1], "toggle") == 0) {
        if ((argc != 3) || !drvGpioDebugIsPinToggleSupported(lPin)) {
            return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
        }

        drvGpioToggle(lPin);
        if (consoleReply(transport, "OK") <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_OK;
    }

    return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
}

static eConsoleCommandResult drvGpioDebugReplyHelp(uint32_t transport, int argc, char *argv[])
{
    if (argc == 2) {
        if (consoleReply(transport,
            "gpio <list|get|set|toggle|help> ...\n"
            "  list\n"
            "  help <get|set|toggle>\n"
            "    example: gpio help set\n"
            "OK") <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_OK;
    }

    if (argc != 3) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if ((strcmp(argv[2], "get") == 0) || (strcmp(argv[2], "read") == 0)) {
        if (consoleReply(transport,
            "gpio get <pin>\n"
            "  pin: ledr|ledg|ledb|key\n"
            "  alias: read\n"
            "  example: gpio get key\n"
            "OK") <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_OK;
    }

    if ((strcmp(argv[2], "set") == 0) || (strcmp(argv[2], "write") == 0)) {
        if (consoleReply(transport,
            "gpio set <pin> <state>\n"
            "  pin: ledr|ledg|ledb\n"
            "  state: 0|1|on|off|set|reset|active|inactive\n"
            "  alias: write\n"
            "  example: gpio set ledr on\n"
            "OK") <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_OK;
    }

    if (strcmp(argv[2], "toggle") == 0) {
        if (consoleReply(transport,
            "gpio toggle <pin>\n"
            "  pin: ledr|ledg|ledb\n"
            "  example: gpio toggle ledr\n"
            "OK") <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_OK;
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
