/************************************************************************************
* @file     : tm1651_debug.c
* @brief    : TM1651 debug and console command implementation.
* @details  : This file hosts optional console bindings for TM1651 operations.
***********************************************************************************/
#include "tm1651_debug.h"

#include "tm1651.h"
#include "tm1651_port.h"

#if (TM1651_CONSOLE_SUPPORT == 1)
#include <stdint.h>
#include <string.h>

#include "console.h"

static bool tm1651DebugParseDevice(const char *name, eTm1651MapType *device);
static bool tm1651DebugParseUint32(const char *text, uint32_t *value);
static bool tm1651DebugParseHexNibble(char value, uint8_t *nibble);
static bool tm1651DebugParseHexByte(const char *text, uint8_t *value);
static bool tm1651DebugParseBool(const char *text, bool *value);
static bool tm1651DebugParseSymbol(const char *text, uint8_t *symbol);
static const char *tm1651DebugGetDeviceName(eTm1651MapType device);
static const char *tm1651DebugGetStatusText(eTm1651Status status);
static eConsoleCommandResult tm1651DebugReplyDeviceList(uint32_t transport);
static eConsoleCommandResult tm1651DebugHandleInit(uint32_t transport, eTm1651MapType device);
static eConsoleCommandResult tm1651DebugHandleInfo(uint32_t transport, eTm1651MapType device);
static eConsoleCommandResult tm1651DebugHandleBrightness(uint32_t transport, eTm1651MapType device, int argc, char *argv[]);
static eConsoleCommandResult tm1651DebugHandleDisplay(uint32_t transport, eTm1651MapType device, int argc, char *argv[]);
static eConsoleCommandResult tm1651DebugHandleDigits(uint32_t transport, eTm1651MapType device, int argc, char *argv[]);
static eConsoleCommandResult tm1651DebugHandleRaw(uint32_t transport, eTm1651MapType device, int argc, char *argv[]);
static eConsoleCommandResult tm1651DebugHandleClear(uint32_t transport, eTm1651MapType device);
static eConsoleCommandResult tm1651DebugHandleNone(uint32_t transport, eTm1651MapType device);
static eConsoleCommandResult tm1651DebugHandleNumber(uint32_t transport, eTm1651MapType device, int argc, char *argv[]);
static eConsoleCommandResult tm1651DebugHandleError(uint32_t transport, eTm1651MapType device, int argc, char *argv[]);
static eConsoleCommandResult tm1651DebugReplyHelp(uint32_t transport, int argc, char *argv[]);
static eConsoleCommandResult tm1651DebugConsoleHandler(uint32_t transport, int argc, char *argv[]);

static const stConsoleCommand gTm1651ConsoleCommand = {
    .commandName = "tm1651",
    .helpText = "tm1651 <list|init|info|brightness|display|digits|raw|clear|none|number|error|help> ...",
    .ownerTag = "tm1651",
    .handler = tm1651DebugConsoleHandler,
};

static bool tm1651DebugParseDevice(const char *name, eTm1651MapType *device)
{
    if ((name == NULL) || (device == NULL)) {
        return false;
    }

    if ((strcmp(name, "dev0") == 0) ||
        (strcmp(name, "tm") == 0) ||
        (strcmp(name, "tm1651") == 0) ||
        (strcmp(name, "0") == 0)) {
        *device = TM1651_DEV0;
        return true;
    }

    return false;
}

static bool tm1651DebugParseUint32(const char *text, uint32_t *value)
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
            if (!tm1651DebugParseHexNibble(*lCursor, &lNibble)) {
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

static bool tm1651DebugParseHexNibble(char value, uint8_t *nibble)
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

static bool tm1651DebugParseHexByte(const char *text, uint8_t *value)
{
    uint32_t lValue = 0U;

    if (!tm1651DebugParseUint32(text, &lValue) || (lValue > 0xFFU)) {
        return false;
    }

    *value = (uint8_t)lValue;
    return true;
}

static bool tm1651DebugParseBool(const char *text, bool *value)
{
    if ((text == NULL) || (value == NULL)) {
        return false;
    }

    if ((strcmp(text, "1") == 0) || (strcmp(text, "on") == 0) || (strcmp(text, "true") == 0)) {
        *value = true;
        return true;
    }

    if ((strcmp(text, "0") == 0) || (strcmp(text, "off") == 0) || (strcmp(text, "false") == 0)) {
        *value = false;
        return true;
    }

    return false;
}

static bool tm1651DebugParseSymbol(const char *text, uint8_t *symbol)
{
    uint32_t lValue = 0U;

    if ((text == NULL) || (symbol == NULL)) {
        return false;
    }

    if ((strcmp(text, "blank") == 0) || (strcmp(text, "_") == 0)) {
        *symbol = TM1651_SYMBOL_BLANK;
        return true;
    }

    if ((strcmp(text, "dash") == 0) || (strcmp(text, "-") == 0)) {
        *symbol = TM1651_SYMBOL_DASH;
        return true;
    }

    if ((strcmp(text, "e") == 0) || (strcmp(text, "E") == 0)) {
        *symbol = TM1651_SYMBOL_E;
        return true;
    }

    if (!tm1651DebugParseUint32(text, &lValue) || (lValue > (uint32_t)TM1651_SYMBOL_E)) {
        return false;
    }

    *symbol = (uint8_t)lValue;
    return true;
}

static const char *tm1651DebugGetDeviceName(eTm1651MapType device)
{
    switch (device) {
        case TM1651_DEV0:
            return "dev0";
        default:
            return NULL;
    }
}

static const char *tm1651DebugGetStatusText(eTm1651Status status)
{
    switch (status) {
        case TM1651_STATUS_OK:
            return "ok";
        case TM1651_STATUS_INVALID_PARAM:
            return "invalid_param";
        case TM1651_STATUS_NOT_READY:
            return "not_ready";
        case TM1651_STATUS_BUSY:
            return "busy";
        case TM1651_STATUS_TIMEOUT:
            return "timeout";
        case TM1651_STATUS_NACK:
            return "nack";
        case TM1651_STATUS_UNSUPPORTED:
            return "unsupported";
        default:
            return "error";
    }
}

static eConsoleCommandResult tm1651DebugReplyDeviceList(uint32_t transport)
{
    stTm1651Cfg lCfg;
    stTm1651PortAssembleCfg lAssembleCfg;
    uint32_t lIndex;

    for (lIndex = 0U; lIndex < (uint32_t)TM1651_DEV_MAX; lIndex++) {
        if ((tm1651GetCfg((eTm1651MapType)lIndex, &lCfg) != TM1651_STATUS_OK) ||
            (tm1651PortGetAssembleCfg((eTm1651MapType)lIndex, &lAssembleCfg) != DRV_STATUS_OK)) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        if (consoleReply(transport,
            "%s bus=%u bright=%u digits=%u display=%s ready=%s\n",
            tm1651DebugGetDeviceName((eTm1651MapType)lIndex),
            (unsigned int)lAssembleCfg.linkId,
            (unsigned int)lCfg.brightness,
            (unsigned int)lCfg.digitCount,
            lCfg.isDisplayOn ? "on" : "off",
            tm1651IsReady((eTm1651MapType)lIndex) ? "yes" : "no") <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }
    }

    if (consoleReply(transport, "OK") <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult tm1651DebugHandleInit(uint32_t transport, eTm1651MapType device)
{
    eTm1651Status lStatus = tm1651Init(device);

    if (consoleReply(transport, "%s init=%s\nOK", tm1651DebugGetDeviceName(device), tm1651DebugGetStatusText(lStatus)) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return (lStatus == TM1651_STATUS_OK) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
}

static eConsoleCommandResult tm1651DebugHandleInfo(uint32_t transport, eTm1651MapType device)
{
    stTm1651Cfg lCfg;
    stTm1651PortAssembleCfg lAssembleCfg;

    if ((tm1651GetCfg(device, &lCfg) != TM1651_STATUS_OK) ||
        (tm1651PortGetAssembleCfg(device, &lAssembleCfg) != DRV_STATUS_OK)) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (consoleReply(transport,
        "%s bus=%u bright=%u digits=%u display=%s ready=%s\nOK",
        tm1651DebugGetDeviceName(device),
        (unsigned int)lAssembleCfg.linkId,
        (unsigned int)lCfg.brightness,
        (unsigned int)lCfg.digitCount,
        lCfg.isDisplayOn ? "on" : "off",
        tm1651IsReady(device) ? "yes" : "no") <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult tm1651DebugHandleBrightness(uint32_t transport, eTm1651MapType device, int argc, char *argv[])
{
    uint32_t lBrightness = 0U;
    eTm1651Status lStatus;

    if ((argc != 4) || !tm1651DebugParseUint32(argv[3], &lBrightness) || (lBrightness > 7U)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    lStatus = tm1651SetBrightness(device, (uint8_t)lBrightness);
    if (consoleReply(transport,
        "%s brightness=%u status=%s\nOK",
        tm1651DebugGetDeviceName(device),
        (unsigned int)lBrightness,
        tm1651DebugGetStatusText(lStatus)) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return (lStatus == TM1651_STATUS_OK) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
}

static eConsoleCommandResult tm1651DebugHandleDisplay(uint32_t transport, eTm1651MapType device, int argc, char *argv[])
{
    bool lIsDisplayOn = false;
    eTm1651Status lStatus;

    if ((argc != 4) || !tm1651DebugParseBool(argv[3], &lIsDisplayOn)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    lStatus = tm1651SetDisplayOn(device, lIsDisplayOn);
    if (consoleReply(transport,
        "%s display=%s status=%s\nOK",
        tm1651DebugGetDeviceName(device),
        lIsDisplayOn ? "on" : "off",
        tm1651DebugGetStatusText(lStatus)) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return (lStatus == TM1651_STATUS_OK) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
}

static eConsoleCommandResult tm1651DebugHandleDigits(uint32_t transport, eTm1651MapType device, int argc, char *argv[])
{
    uint8_t lSymbol[TM1651_DIGIT_MAX] = {0U};
    eTm1651Status lStatus;
    uint32_t lIndex;

    if (argc != 7) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    for (lIndex = 0U; lIndex < TM1651_DIGIT_MAX; lIndex++) {
        if (!tm1651DebugParseSymbol(argv[lIndex + 3U], &lSymbol[lIndex])) {
            return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
        }
    }

    lStatus = tm1651DisplayDigits(device, lSymbol[0], lSymbol[1], lSymbol[2], lSymbol[3]);
    if (consoleReply(transport,
        "%s digits=%u,%u,%u,%u status=%s\nOK",
        tm1651DebugGetDeviceName(device),
        (unsigned int)lSymbol[0],
        (unsigned int)lSymbol[1],
        (unsigned int)lSymbol[2],
        (unsigned int)lSymbol[3],
        tm1651DebugGetStatusText(lStatus)) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return (lStatus == TM1651_STATUS_OK) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
}

static eConsoleCommandResult tm1651DebugHandleRaw(uint32_t transport, eTm1651MapType device, int argc, char *argv[])
{
    uint8_t lBuffer[TM1651_DIGIT_MAX];
    uint8_t lLength = 0U;
    eTm1651Status lStatus;
    int lIndex;

    if ((argc < 4) || (argc > 7)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    for (lIndex = 3; lIndex < argc; lIndex++) {
        if (!tm1651DebugParseHexByte(argv[lIndex], &lBuffer[lLength])) {
            return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
        }

        lLength++;
    }

    lStatus = tm1651DisplayRaw(device, lBuffer, lLength);
    if (consoleReply(transport,
        "%s raw_len=%u status=%s\nOK",
        tm1651DebugGetDeviceName(device),
        (unsigned int)lLength,
        tm1651DebugGetStatusText(lStatus)) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return (lStatus == TM1651_STATUS_OK) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
}

static eConsoleCommandResult tm1651DebugHandleClear(uint32_t transport, eTm1651MapType device)
{
    eTm1651Status lStatus = tm1651ClearDisplay(device);

    if (consoleReply(transport, "%s clear=%s\nOK", tm1651DebugGetDeviceName(device), tm1651DebugGetStatusText(lStatus)) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return (lStatus == TM1651_STATUS_OK) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
}

static eConsoleCommandResult tm1651DebugHandleNone(uint32_t transport, eTm1651MapType device)
{
    eTm1651Status lStatus = tm1651ShowNone(device);

    if (consoleReply(transport, "%s none=%s\nOK", tm1651DebugGetDeviceName(device), tm1651DebugGetStatusText(lStatus)) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return (lStatus == TM1651_STATUS_OK) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
}

static eConsoleCommandResult tm1651DebugHandleNumber(uint32_t transport, eTm1651MapType device, int argc, char *argv[])
{
    uint32_t lValue = 0U;
    eTm1651Status lStatus;

    if ((argc != 4) || !tm1651DebugParseUint32(argv[3], &lValue) || (lValue > 999U)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    lStatus = tm1651ShowNumber3(device, (uint16_t)lValue);
    if (consoleReply(transport,
        "%s number=%u status=%s\nOK",
        tm1651DebugGetDeviceName(device),
        (unsigned int)lValue,
        tm1651DebugGetStatusText(lStatus)) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return (lStatus == TM1651_STATUS_OK) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
}

static eConsoleCommandResult tm1651DebugHandleError(uint32_t transport, eTm1651MapType device, int argc, char *argv[])
{
    uint32_t lValue = 0U;
    eTm1651Status lStatus;

    if ((argc != 4) || !tm1651DebugParseUint32(argv[3], &lValue) || (lValue > 99U)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    lStatus = tm1651ShowError(device, (uint16_t)lValue);
    if (consoleReply(transport,
        "%s error=%u status=%s\nOK",
        tm1651DebugGetDeviceName(device),
        (unsigned int)lValue,
        tm1651DebugGetStatusText(lStatus)) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return (lStatus == TM1651_STATUS_OK) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
}

static eConsoleCommandResult tm1651DebugReplyHelp(uint32_t transport, int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (consoleReply(transport,
        "tm1651 list\n"
        "tm1651 init <dev0|0|tm|tm1651>\n"
        "tm1651 info <dev0|0|tm|tm1651>\n"
        "tm1651 brightness <dev0|0|tm|tm1651> <0-7>\n"
        "tm1651 display <dev0|0|tm|tm1651> <on|off|1|0>\n"
        "tm1651 digits <dev0|0|tm|tm1651> <d1> <d2> <d3> <d4>\n"
        "tm1651 raw <dev0|0|tm|tm1651> <byte0> [byte1] [byte2] [byte3]\n"
        "tm1651 clear <dev0|0|tm|tm1651>\n"
        "tm1651 none <dev0|0|tm|tm1651>\n"
        "tm1651 number <dev0|0|tm|tm1651> <0-999>\n"
        "tm1651 error <dev0|0|tm|tm1651> <0-99>\n"
        "digits support: 0-9, blank, dash, e\n"
        "OK") <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult tm1651DebugConsoleHandler(uint32_t transport, int argc, char *argv[])
{
    eTm1651MapType lDevice = TM1651_DEV0;

    if ((argc < 2) || (argv == NULL)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (strcmp(argv[1], "list") == 0) {
        return (argc == 2) ? tm1651DebugReplyDeviceList(transport) : CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (strcmp(argv[1], "help") == 0) {
        return tm1651DebugReplyHelp(transport, argc, argv);
    }

    if ((argc < 3) || !tm1651DebugParseDevice(argv[2], &lDevice)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (strcmp(argv[1], "init") == 0) {
        return (argc == 3) ? tm1651DebugHandleInit(transport, lDevice) : CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (strcmp(argv[1], "info") == 0) {
        return (argc == 3) ? tm1651DebugHandleInfo(transport, lDevice) : CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (strcmp(argv[1], "brightness") == 0) {
        return tm1651DebugHandleBrightness(transport, lDevice, argc, argv);
    }

    if (strcmp(argv[1], "display") == 0) {
        return tm1651DebugHandleDisplay(transport, lDevice, argc, argv);
    }

    if (strcmp(argv[1], "digits") == 0) {
        return tm1651DebugHandleDigits(transport, lDevice, argc, argv);
    }

    if (strcmp(argv[1], "raw") == 0) {
        return tm1651DebugHandleRaw(transport, lDevice, argc, argv);
    }

    if (strcmp(argv[1], "clear") == 0) {
        return (argc == 3) ? tm1651DebugHandleClear(transport, lDevice) : CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (strcmp(argv[1], "none") == 0) {
        return (argc == 3) ? tm1651DebugHandleNone(transport, lDevice) : CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (strcmp(argv[1], "number") == 0) {
        return tm1651DebugHandleNumber(transport, lDevice, argc, argv);
    }

    if (strcmp(argv[1], "error") == 0) {
        return tm1651DebugHandleError(transport, lDevice, argc, argv);
    }

    return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
}

bool tm1651DebugConsoleRegister(void)
{
    return consoleRegisterCommand(&gTm1651ConsoleCommand);
}

#else

bool tm1651DebugConsoleRegister(void)
{
    return false;
}

#endif
/**************************End of file********************************/