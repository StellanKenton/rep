/***********************************************************************************
* @file     : pca9535_debug.c
* @brief    : PCA9535 debug and console command implementation.
* @details  : This file hosts optional console bindings for PCA9535 operations.
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "pca9535_debug.h"

#include "pca9535.h"
#include "pca9535_port.h"

#if (PCA9535_CONSOLE_SUPPORT == 1)
#include <stdint.h>
#include <string.h>

#include "console.h"

static bool pca9535DebugParseDevice(const char *name, ePca9535MapType *device);
static bool pca9535DebugParseUint32(const char *text, uint32_t *value);
static bool pca9535DebugParseHexNibble(char value, uint8_t *nibble);
static bool pca9535DebugParseHexByte(const char *text, uint8_t *value);
static const char *pca9535DebugGetDeviceName(ePca9535MapType device);
static const char *pca9535DebugGetStatusText(eDrvStatus status);
static eConsoleCommandResult pca9535DebugReplyDeviceList(uint32_t transport);
static eConsoleCommandResult pca9535DebugHandleInit(uint32_t transport, ePca9535MapType device);
static eConsoleCommandResult pca9535DebugHandleInfo(uint32_t transport, ePca9535MapType device);
static eConsoleCommandResult pca9535DebugHandleRegGet(uint32_t transport, ePca9535MapType device, int argc, char *argv[]);
static eConsoleCommandResult pca9535DebugHandleRegSet(uint32_t transport, ePca9535MapType device, int argc, char *argv[]);
static eConsoleCommandResult pca9535DebugHandleInput(uint32_t transport, ePca9535MapType device);
static eConsoleCommandResult pca9535DebugHandleOutput(uint32_t transport, ePca9535MapType device, int argc, char *argv[]);
static eConsoleCommandResult pca9535DebugHandleDir(uint32_t transport, ePca9535MapType device, int argc, char *argv[]);
static eConsoleCommandResult pca9535DebugHandleLedOff(uint32_t transport);
static eConsoleCommandResult pca9535DebugHandleLedNum(uint32_t transport, int argc, char *argv[]);
static eConsoleCommandResult pca9535DebugHandleLedPower(uint32_t transport, int argc, char *argv[]);
static eConsoleCommandResult pca9535DebugHandleLedPress(uint32_t transport, int argc, char *argv[]);
static eConsoleCommandResult pca9535DebugReplyHelp(uint32_t transport, int argc, char *argv[]);
static eConsoleCommandResult pca9535DebugConsoleHandler(uint32_t transport, int argc, char *argv[]);

static const stConsoleCommand gPca9535ConsoleCommand = {
    .commandName = "pca9535",
    .helpText = "pca9535 <list|init|info|regget|regset|input|output|getdir|setdir|ledoff|lednum|ledpower|ledpress|help> ...",
    .ownerTag = "pca9535",
    .handler = pca9535DebugConsoleHandler,
};

static bool pca9535DebugParseDevice(const char *name, ePca9535MapType *device)
{
    if ((name == NULL) || (device == NULL)) {
        return false;
    }

    if ((strcmp(name, "dev0") == 0) ||
        (strcmp(name, "pca") == 0) ||
        (strcmp(name, "0") == 0)) {
        *device = PCA9535_DEV0;
        return true;
    }

    return false;
}

static bool pca9535DebugParseUint32(const char *text, uint32_t *value)
{
    uint32_t lValue = 0U;
    const char *lCursor = text;
    uint8_t lNibble = 0U;

    if ((text == NULL) || (value == NULL) || (text[0] == '\0')) {
        return false;
    }

    if ((lCursor[0] == '0') && ((lCursor[1] == 'x') || (lCursor[1] == 'X'))) {
        lCursor += 2;
        if (*lCursor == '\0') {
            return false;
        }

        while (*lCursor != '\0') {
            if (!pca9535DebugParseHexNibble(*lCursor, &lNibble)) {
                return false;
            }

            lValue = (lValue << 4U) | (uint32_t)lNibble;
            lCursor++;
        }

        *value = lValue;
        return true;
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

static bool pca9535DebugParseHexNibble(char value, uint8_t *nibble)
{
    if (nibble == NULL) {
        return false;
    }

    if ((value >= '0') && (value <= '9')) {
        *nibble = (uint8_t)(value - '0');
        return true;
    }

    if ((value >= 'a') && (value <= 'f')) {
        *nibble = (uint8_t)(value - 'a' + 10);
        return true;
    }

    if ((value >= 'A') && (value <= 'F')) {
        *nibble = (uint8_t)(value - 'A' + 10);
        return true;
    }

    return false;
}

static bool pca9535DebugParseHexByte(const char *text, uint8_t *value)
{
    uint32_t lValue = 0U;

    if (!pca9535DebugParseUint32(text, &lValue) || (lValue > 0xFFU)) {
        return false;
    }

    *value = (uint8_t)lValue;
    return true;
}

static const char *pca9535DebugGetDeviceName(ePca9535MapType device)
{
    switch (device) {
        case PCA9535_DEV0:
            return "dev0";
        default:
            return NULL;
    }
}

static const char *pca9535DebugGetStatusText(eDrvStatus status)
{
    switch (status) {
        case DRV_STATUS_OK:
            return "ok";
        case DRV_STATUS_INVALID_PARAM:
            return "invalid_param";
        case DRV_STATUS_NOT_READY:
            return "not_ready";
        case DRV_STATUS_BUSY:
            return "busy";
        case DRV_STATUS_TIMEOUT:
            return "timeout";
        case DRV_STATUS_NACK:
            return "nack";
        case DRV_STATUS_UNSUPPORTED:
            return "unsupported";
        default:
            return "error";
    }
}

static eConsoleCommandResult pca9535DebugReplyDeviceList(uint32_t transport)
{
    stPca9535Cfg lCfg;
    stPca9535PortAssembleCfg lAssembleCfg;
    uint32_t lIndex;

    for (lIndex = 0U; lIndex < (uint32_t)PCA9535_DEV_MAX; lIndex++) {
        pca9535GetDefCfg((ePca9535MapType)lIndex, &lCfg);
        pca9535PortGetAssembleCfg((ePca9535MapType)lIndex, &lAssembleCfg);
        if (consoleReply(transport,
            "%s bus=%u addr=%02X ready=%s\n",
            pca9535DebugGetDeviceName((ePca9535MapType)lIndex),
            (unsigned int)lAssembleCfg.linkId,
            (unsigned int)lCfg.address,
            pca9535IsReady((ePca9535MapType)lIndex) ? "yes" : "no") <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }
    }

    if (consoleReply(transport, "OK") <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult pca9535DebugHandleInit(uint32_t transport, ePca9535MapType device)
{
    eDrvStatus lStatus = pca9535Init(device);

    if (consoleReply(transport, "%s init=%s\nOK", pca9535DebugGetDeviceName(device), pca9535DebugGetStatusText(lStatus)) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return (lStatus == DRV_STATUS_OK) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
}

static eConsoleCommandResult pca9535DebugHandleInfo(uint32_t transport, ePca9535MapType device)
{
    uint16_t lOutputValue = 0U;
    uint16_t lDirValue = 0U;
    uint16_t lInputValue = 0U;
    eDrvStatus lOutputStatus;
    eDrvStatus lDirStatus;
    eDrvStatus lInputStatus;

    lOutputStatus = pca9535GetOutputPort(device, &lOutputValue);
    lDirStatus = pca9535GetDirectionPort(device, &lDirValue);
    lInputStatus = pca9535ReadInputPort(device, &lInputValue);

    if (consoleReply(transport,
        "%s ready=%s out=%04X(%s) dir=%04X(%s) in=%04X(%s)\nOK",
        pca9535DebugGetDeviceName(device),
        pca9535IsReady(device) ? "yes" : "no",
        (unsigned int)lOutputValue,
        pca9535DebugGetStatusText(lOutputStatus),
        (unsigned int)lDirValue,
        pca9535DebugGetStatusText(lDirStatus),
        (unsigned int)lInputValue,
        pca9535DebugGetStatusText(lInputStatus)) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return ((lOutputStatus == DRV_STATUS_OK) && (lDirStatus == DRV_STATUS_OK) && (lInputStatus == DRV_STATUS_OK)) ?
           CONSOLE_COMMAND_RESULT_OK :
           CONSOLE_COMMAND_RESULT_ERROR;
}

static eConsoleCommandResult pca9535DebugHandleRegGet(uint32_t transport, ePca9535MapType device, int argc, char *argv[])
{
    uint8_t lReg = 0U;
    uint8_t lValue = 0U;
    eDrvStatus lStatus;

    if ((argc != 4) || !pca9535DebugParseHexByte(argv[3], &lReg)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    lStatus = pca9535ReadReg(device, lReg, &lValue);
    if (lStatus != DRV_STATUS_OK) {
        if (consoleReply(transport, "%s reg=%02X status=%s", pca9535DebugGetDeviceName(device), (unsigned int)lReg, pca9535DebugGetStatusText(lStatus)) <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (consoleReply(transport, "%s reg=%02X value=%02X\nOK", pca9535DebugGetDeviceName(device), (unsigned int)lReg, (unsigned int)lValue) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult pca9535DebugHandleRegSet(uint32_t transport, ePca9535MapType device, int argc, char *argv[])
{
    uint8_t lReg = 0U;
    uint8_t lValue = 0U;
    eDrvStatus lStatus;

    if ((argc != 5) || !pca9535DebugParseHexByte(argv[3], &lReg) || !pca9535DebugParseHexByte(argv[4], &lValue)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    lStatus = pca9535WriteReg(device, lReg, lValue);
    if (consoleReply(transport,
        "%s reg=%02X write=%02X status=%s\nOK",
        pca9535DebugGetDeviceName(device),
        (unsigned int)lReg,
        (unsigned int)lValue,
        pca9535DebugGetStatusText(lStatus)) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return (lStatus == DRV_STATUS_OK) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
}

static eConsoleCommandResult pca9535DebugHandleInput(uint32_t transport, ePca9535MapType device)
{
    uint16_t lValue = 0U;
    eDrvStatus lStatus = pca9535ReadInputPort(device, &lValue);

    if (lStatus != DRV_STATUS_OK) {
        if (consoleReply(transport, "%s input=%s", pca9535DebugGetDeviceName(device), pca9535DebugGetStatusText(lStatus)) <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (consoleReply(transport, "%s input=%04X\nOK", pca9535DebugGetDeviceName(device), (unsigned int)lValue) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult pca9535DebugHandleOutput(uint32_t transport, ePca9535MapType device, int argc, char *argv[])
{
    uint16_t lValue = 0U;
    eDrvStatus lStatus;

    if (argc == 3) {
        lStatus = pca9535GetOutputPort(device, &lValue);
        if (lStatus != DRV_STATUS_OK) {
            if (consoleReply(transport, "%s output=%s", pca9535DebugGetDeviceName(device), pca9535DebugGetStatusText(lStatus)) <= 0) {
                return CONSOLE_COMMAND_RESULT_ERROR;
            }

            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        if (consoleReply(transport, "%s output=%04X\nOK", pca9535DebugGetDeviceName(device), (unsigned int)lValue) <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_OK;
    }

    if ((argc != 4) || !pca9535DebugParseUint32(argv[3], (uint32_t *)&lValue)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    lStatus = pca9535SetOutputPort(device, lValue);
    if (consoleReply(transport, "%s output_set=%04X status=%s\nOK", pca9535DebugGetDeviceName(device), (unsigned int)lValue, pca9535DebugGetStatusText(lStatus)) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return (lStatus == DRV_STATUS_OK) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
}

static eConsoleCommandResult pca9535DebugHandleDir(uint32_t transport, ePca9535MapType device, int argc, char *argv[])
{
    uint16_t lValue = 0U;
    eDrvStatus lStatus;

    if (strcmp(argv[1], "getdir") == 0) {
        if (argc != 3) {
            return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
        }

        lStatus = pca9535GetDirectionPort(device, &lValue);
        if (lStatus != DRV_STATUS_OK) {
            if (consoleReply(transport, "%s dir=%s", pca9535DebugGetDeviceName(device), pca9535DebugGetStatusText(lStatus)) <= 0) {
                return CONSOLE_COMMAND_RESULT_ERROR;
            }

            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        if (consoleReply(transport, "%s dir=%04X\nOK", pca9535DebugGetDeviceName(device), (unsigned int)lValue) <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_OK;
    }

    if ((argc != 4) || !pca9535DebugParseUint32(argv[3], (uint32_t *)&lValue)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    lStatus = pca9535SetDirectionPort(device, lValue);
    if (consoleReply(transport, "%s dir_set=%04X status=%s\nOK", pca9535DebugGetDeviceName(device), (unsigned int)lValue, pca9535DebugGetStatusText(lStatus)) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return (lStatus == DRV_STATUS_OK) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
}

static eConsoleCommandResult pca9535DebugHandleLedOff(uint32_t transport)
{
    eDrvStatus lStatus = pca9535PortLedOff();

    if (consoleReply(transport, "ledoff status=%s\nOK", pca9535DebugGetStatusText(lStatus)) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return (lStatus == DRV_STATUS_OK) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
}

static eConsoleCommandResult pca9535DebugHandleLedNum(uint32_t transport, int argc, char *argv[])
{
    uint32_t lValue = 0U;
    eDrvStatus lStatus;

    if ((argc != 3) || !pca9535DebugParseUint32(argv[2], &lValue)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    lStatus = pca9535PortLedLightNum((uint8_t)lValue);
    if (consoleReply(transport, "lednum=%u status=%s\nOK", (unsigned int)lValue, pca9535DebugGetStatusText(lStatus)) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return (lStatus == DRV_STATUS_OK) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
}

static eConsoleCommandResult pca9535DebugHandleLedPower(uint32_t transport, int argc, char *argv[])
{
    uint32_t lRed = 0U;
    uint32_t lGreen = 0U;
    uint32_t lBlue = 0U;
    eDrvStatus lStatus;

    if ((argc != 5) ||
        !pca9535DebugParseUint32(argv[2], &lRed) ||
        !pca9535DebugParseUint32(argv[3], &lGreen) ||
        !pca9535DebugParseUint32(argv[4], &lBlue)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    lStatus = pca9535PortLedPowerShow((lRed != 0U), (lGreen != 0U), (lBlue != 0U));
    if (consoleReply(transport, "ledpower=%u,%u,%u status=%s\nOK", (unsigned int)lRed, (unsigned int)lGreen, (unsigned int)lBlue, pca9535DebugGetStatusText(lStatus)) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return (lStatus == DRV_STATUS_OK) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
}

static eConsoleCommandResult pca9535DebugHandleLedPress(uint32_t transport, int argc, char *argv[])
{
    uint32_t lRed = 0U;
    uint32_t lGreen = 0U;
    uint32_t lBlue = 0U;
    eDrvStatus lStatus;

    if ((argc != 5) ||
        !pca9535DebugParseUint32(argv[2], &lRed) ||
        !pca9535DebugParseUint32(argv[3], &lGreen) ||
        !pca9535DebugParseUint32(argv[4], &lBlue)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    lStatus = pca9535PortLedPressShow((lRed != 0U), (lGreen != 0U), (lBlue != 0U));
    if (consoleReply(transport, "ledpress=%u,%u,%u status=%s\nOK", (unsigned int)lRed, (unsigned int)lGreen, (unsigned int)lBlue, pca9535DebugGetStatusText(lStatus)) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return (lStatus == DRV_STATUS_OK) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
}

static eConsoleCommandResult pca9535DebugReplyHelp(uint32_t transport, int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (consoleReply(transport,
        "pca9535 list\n"
        "pca9535 init <dev0|0|pca>\n"
        "pca9535 info <dev0|0|pca>\n"
        "pca9535 regget <dev0|0|pca> <reg>\n"
        "pca9535 regset <dev0|0|pca> <reg> <value>\n"
        "pca9535 input <dev0|0|pca>\n"
        "pca9535 output <dev0|0|pca> [value]\n"
        "pca9535 getdir <dev0|0|pca>\n"
        "pca9535 setdir <dev0|0|pca> <value>\n"
        "pca9535 ledoff\n"
        "pca9535 lednum <0-8>\n"
        "pca9535 ledpower <r> <g> <b>\n"
        "pca9535 ledpress <r> <g> <b>\n"
        "OK") <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult pca9535DebugConsoleHandler(uint32_t transport, int argc, char *argv[])
{
    ePca9535MapType lDevice = PCA9535_DEV0;

    if ((argc < 2) || (argv == NULL)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (strcmp(argv[1], "list") == 0) {
        return (argc == 2) ? pca9535DebugReplyDeviceList(transport) : CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (strcmp(argv[1], "ledoff") == 0) {
        return (argc == 2) ? pca9535DebugHandleLedOff(transport) : CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (strcmp(argv[1], "lednum") == 0) {
        return pca9535DebugHandleLedNum(transport, argc, argv);
    }

    if (strcmp(argv[1], "ledpower") == 0) {
        return pca9535DebugHandleLedPower(transport, argc, argv);
    }

    if (strcmp(argv[1], "ledpress") == 0) {
        return pca9535DebugHandleLedPress(transport, argc, argv);
    }

    if (strcmp(argv[1], "help") == 0) {
        return pca9535DebugReplyHelp(transport, argc, argv);
    }

    if ((argc < 3) || !pca9535DebugParseDevice(argv[2], &lDevice)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (strcmp(argv[1], "init") == 0) {
        return pca9535DebugHandleInit(transport, lDevice);
    }

    if (strcmp(argv[1], "info") == 0) {
        return (argc == 3) ? pca9535DebugHandleInfo(transport, lDevice) : CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (strcmp(argv[1], "regget") == 0) {
        return pca9535DebugHandleRegGet(transport, lDevice, argc, argv);
    }

    if (strcmp(argv[1], "regset") == 0) {
        return pca9535DebugHandleRegSet(transport, lDevice, argc, argv);
    }

    if (strcmp(argv[1], "input") == 0) {
        return (argc == 3) ? pca9535DebugHandleInput(transport, lDevice) : CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (strcmp(argv[1], "output") == 0) {
        return pca9535DebugHandleOutput(transport, lDevice, argc, argv);
    }

    if ((strcmp(argv[1], "getdir") == 0) || (strcmp(argv[1], "setdir") == 0)) {
        return pca9535DebugHandleDir(transport, lDevice, argc, argv);
    }

    return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
}

bool pca9535DebugConsoleRegister(void)
{
    return consoleRegisterCommand(&gPca9535ConsoleCommand);
}

#else

bool pca9535DebugConsoleRegister(void)
{
    return false;
}

#endif

/**************************End of file********************************/
