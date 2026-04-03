/***********************************************************************************
* @file     : drvanlogiic_debug.c
* @brief    : DrvAnlogIic debug and console command implementation.
* @details  : This file hosts optional console bindings for software IIC debug operations.
* @author   : GitHub Copilot
* @date     : 2026-04-02
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "drvanlogiic_debug.h"

#include "drvanlogiic.h"

#if (DRVANLOGIIC_CONSOLE_SUPPORT == 1)
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "console.h"

#define DRVANLOGIIC_DEBUG_MAX_DATA_LENGTH    16U
#define DRVANLOGIIC_DEBUG_MAX_REPLY_LENGTH   96U

extern stDrvAnlogIicBspInterface gDrvAnlogIicBspInterface[DRVANLOGIIC_MAX];

static bool drvAnlogIicDebugParseBus(const char *name, eDrvAnlogIicPortMap *iic);
static bool drvAnlogIicDebugParseUint32(const char *text, uint32_t *value);
static bool drvAnlogIicDebugParseHexNibble(char value, uint8_t *nibble);
static bool drvAnlogIicDebugParseHexByte(const char *text, uint8_t *value);
static const char *drvAnlogIicDebugGetBusName(eDrvAnlogIicPortMap iic);
static const char *drvAnlogIicDebugGetStatusText(eDrvStatus status);
static eConsoleCommandResult drvAnlogIicDebugBuildHexReply(const uint8_t *buffer, uint16_t length, char *output, uint32_t capacity);
static eConsoleCommandResult drvAnlogIicDebugReplyBusList(uint32_t transport);
static eConsoleCommandResult drvAnlogIicDebugHandleInit(uint32_t transport, eDrvAnlogIicPortMap iic);
static eConsoleCommandResult drvAnlogIicDebugHandleRecover(uint32_t transport, eDrvAnlogIicPortMap iic);
static eConsoleCommandResult drvAnlogIicDebugHandleWrite(uint32_t transport, eDrvAnlogIicPortMap iic, int argc, char *argv[]);
static eConsoleCommandResult drvAnlogIicDebugHandleRead(uint32_t transport, eDrvAnlogIicPortMap iic, int argc, char *argv[]);
static eConsoleCommandResult drvAnlogIicDebugHandleWriteReg(uint32_t transport, eDrvAnlogIicPortMap iic, int argc, char *argv[]);
static eConsoleCommandResult drvAnlogIicDebugHandleReadReg(uint32_t transport, eDrvAnlogIicPortMap iic, int argc, char *argv[]);
static eConsoleCommandResult drvAnlogIicDebugReplyHelp(uint32_t transport, int argc, char *argv[]);
static eConsoleCommandResult drvAnlogIicDebugConsoleHandler(uint32_t transport, int argc, char *argv[]);

static const stConsoleCommand gDrvAnlogIicConsoleCommand = {
    .commandName = "anlogiic",
    .helpText = "anlogiic <list|init|recover|write|read|writereg|readreg|help> ...",
    .ownerTag = "drvAnlogIic",
    .handler = drvAnlogIicDebugConsoleHandler,
};

static bool drvAnlogIicDebugParseBus(const char *name, eDrvAnlogIicPortMap *iic)
{
    if ((name == NULL) || (iic == NULL)) {
        return false;
    }

    if ((strcmp(name, "pca") == 0) ||
        (strcmp(name, "bus0") == 0) ||
        (strcmp(name, "0") == 0)) {
        *iic = DRVANLOGIIC_PCA;
        return true;
    }

    if ((strcmp(name, "tm") == 0) ||
        (strcmp(name, "bus1") == 0) ||
        (strcmp(name, "1") == 0)) {
        *iic = DRVANLOGIIC_TM;
        return true;
    }

    return false;
}

static bool drvAnlogIicDebugParseUint32(const char *text, uint32_t *value)
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

static bool drvAnlogIicDebugParseHexNibble(char value, uint8_t *nibble)
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

static bool drvAnlogIicDebugParseHexByte(const char *text, uint8_t *value)
{
    uint8_t lHigh = 0U;
    uint8_t lLow = 0U;
    const char *lCursor = text;

    if ((text == NULL) || (value == NULL) || (text[0] == '\0')) {
        return false;
    }

    if ((lCursor[0] == '0') && ((lCursor[1] == 'x') || (lCursor[1] == 'X'))) {
        lCursor += 2;
    }

    if (lCursor[0] == '\0') {
        return false;
    }

    if (lCursor[1] == '\0') {
        if (!drvAnlogIicDebugParseHexNibble(lCursor[0], &lLow)) {
            return false;
        }

        *value = lLow;
        return true;
    }

    if ((lCursor[2] != '\0') ||
        !drvAnlogIicDebugParseHexNibble(lCursor[0], &lHigh) ||
        !drvAnlogIicDebugParseHexNibble(lCursor[1], &lLow)) {
        return false;
    }

    *value = (uint8_t)((lHigh << 4) | lLow);
    return true;
}

static const char *drvAnlogIicDebugGetBusName(eDrvAnlogIicPortMap iic)
{
    switch (iic) {
        case DRVANLOGIIC_PCA:
            return "pca";
        case DRVANLOGIIC_TM:
            return "tm";
        default:
            return NULL;
    }
}

static const char *drvAnlogIicDebugGetStatusText(eDrvStatus status)
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

static eConsoleCommandResult drvAnlogIicDebugBuildHexReply(const uint8_t *buffer, uint16_t length, char *output, uint32_t capacity)
{
    static const char gHexDigits[] = "0123456789ABCDEF";
    uint32_t lIndex;
    uint32_t lOffset = 0U;

    if ((buffer == NULL) || (output == NULL) || (capacity == 0U)) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    output[0] = '\0';
    for (lIndex = 0U; lIndex < (uint32_t)length; lIndex++) {
        if ((lOffset + 3U) >= capacity) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        if (lIndex > 0U) {
            output[lOffset] = ' ';
            lOffset++;
        }

        output[lOffset] = gHexDigits[(buffer[lIndex] >> 4) & 0x0FU];
        output[lOffset + 1U] = gHexDigits[buffer[lIndex] & 0x0FU];
        lOffset += 2U;
        output[lOffset] = '\0';
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult drvAnlogIicDebugReplyBusList(uint32_t transport)
{
    const char *lPcaBusName = drvAnlogIicDebugGetBusName(DRVANLOGIIC_PCA);
    const char *lTmBusName = drvAnlogIicDebugGetBusName(DRVANLOGIIC_TM);

    if ((lPcaBusName == NULL) ||
        (lTmBusName == NULL) ||
        (consoleReply(transport,
            "%s half_period_us=%u recovery_clocks=%u\n%s half_period_us=%u recovery_clocks=%u\nOK",
            lPcaBusName,
            (unsigned int)gDrvAnlogIicBspInterface[DRVANLOGIIC_PCA].halfPeriodUs,
            (unsigned int)gDrvAnlogIicBspInterface[DRVANLOGIIC_PCA].recoveryClockCount,
            lTmBusName,
            (unsigned int)gDrvAnlogIicBspInterface[DRVANLOGIIC_TM].halfPeriodUs,
            (unsigned int)gDrvAnlogIicBspInterface[DRVANLOGIIC_TM].recoveryClockCount) <= 0)) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult drvAnlogIicDebugHandleInit(uint32_t transport, eDrvAnlogIicPortMap iic)
{
    eDrvStatus lStatus = drvAnlogIicInit(iic);

    if (consoleReply(transport, "%s init=%s\nOK", drvAnlogIicDebugGetBusName(iic), drvAnlogIicDebugGetStatusText(lStatus)) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return (lStatus == DRV_STATUS_OK) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
}

static eConsoleCommandResult drvAnlogIicDebugHandleRecover(uint32_t transport, eDrvAnlogIicPortMap iic)
{
    eDrvStatus lStatus = drvAnlogIicRecoverBus(iic);

    if (consoleReply(transport, "%s recover=%s\nOK", drvAnlogIicDebugGetBusName(iic), drvAnlogIicDebugGetStatusText(lStatus)) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return (lStatus == DRV_STATUS_OK) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
}

static eConsoleCommandResult drvAnlogIicDebugHandleWrite(uint32_t transport, eDrvAnlogIicPortMap iic, int argc, char *argv[])
{
    uint8_t lAddress = 0U;
    uint8_t lBuffer[DRVANLOGIIC_DEBUG_MAX_DATA_LENGTH];
    uint16_t lLength = 0U;
    eDrvStatus lStatus;
    int lIndex;

    if ((argc < 5) || (argv == NULL) || !drvAnlogIicDebugParseHexByte(argv[3], &lAddress)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    for (lIndex = 4; lIndex < argc; lIndex++) {
        if (lLength >= DRVANLOGIIC_DEBUG_MAX_DATA_LENGTH) {
            return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
        }

        if (!drvAnlogIicDebugParseHexByte(argv[lIndex], &lBuffer[lLength])) {
            return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
        }

        lLength++;
    }

    lStatus = drvAnlogIicWrite(iic, lAddress, lBuffer, lLength);
    if (consoleReply(transport,
        "%s addr=%02X write=%u status=%s\nOK",
        drvAnlogIicDebugGetBusName(iic),
        (unsigned int)lAddress,
        (unsigned int)lLength,
        drvAnlogIicDebugGetStatusText(lStatus)) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return (lStatus == DRV_STATUS_OK) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
}

static eConsoleCommandResult drvAnlogIicDebugHandleRead(uint32_t transport, eDrvAnlogIicPortMap iic, int argc, char *argv[])
{
    uint8_t lAddress = 0U;
    uint8_t lBuffer[DRVANLOGIIC_DEBUG_MAX_DATA_LENGTH];
    char lReply[DRVANLOGIIC_DEBUG_MAX_REPLY_LENGTH];
    uint32_t lLength = 0U;
    eDrvStatus lStatus;

    if ((argc != 5) || (argv == NULL) ||
        !drvAnlogIicDebugParseHexByte(argv[3], &lAddress) ||
        !drvAnlogIicDebugParseUint32(argv[4], &lLength) ||
        (lLength == 0U) || (lLength > DRVANLOGIIC_DEBUG_MAX_DATA_LENGTH)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    lStatus = drvAnlogIicRead(iic, lAddress, lBuffer, (uint16_t)lLength);
    if (lStatus != DRV_STATUS_OK) {
        if (consoleReply(transport,
            "%s addr=%02X read=%lu status=%s",
            drvAnlogIicDebugGetBusName(iic),
            (unsigned int)lAddress,
            (unsigned long)lLength,
            drvAnlogIicDebugGetStatusText(lStatus)) <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (drvAnlogIicDebugBuildHexReply(lBuffer, (uint16_t)lLength, lReply, sizeof(lReply)) != CONSOLE_COMMAND_RESULT_OK) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (consoleReply(transport,
        "%s addr=%02X data=%s\nOK",
        drvAnlogIicDebugGetBusName(iic),
        (unsigned int)lAddress,
        lReply) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult drvAnlogIicDebugHandleWriteReg(uint32_t transport, eDrvAnlogIicPortMap iic, int argc, char *argv[])
{
    uint8_t lAddress = 0U;
    uint8_t lRegister = 0U;
    uint8_t lBuffer[DRVANLOGIIC_DEBUG_MAX_DATA_LENGTH];
    uint16_t lLength = 0U;
    eDrvStatus lStatus;
    int lIndex;

    if ((argc < 6) || (argv == NULL) ||
        !drvAnlogIicDebugParseHexByte(argv[3], &lAddress) ||
        !drvAnlogIicDebugParseHexByte(argv[4], &lRegister)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    for (lIndex = 5; lIndex < argc; lIndex++) {
        if (lLength >= DRVANLOGIIC_DEBUG_MAX_DATA_LENGTH) {
            return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
        }

        if (!drvAnlogIicDebugParseHexByte(argv[lIndex], &lBuffer[lLength])) {
            return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
        }

        lLength++;
    }

    lStatus = drvAnlogIicWriteRegister(iic, lAddress, &lRegister, 1U, lBuffer, lLength);
    if (consoleReply(transport,
        "%s addr=%02X reg=%02X write=%u status=%s\nOK",
        drvAnlogIicDebugGetBusName(iic),
        (unsigned int)lAddress,
        (unsigned int)lRegister,
        (unsigned int)lLength,
        drvAnlogIicDebugGetStatusText(lStatus)) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return (lStatus == DRV_STATUS_OK) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
}

static eConsoleCommandResult drvAnlogIicDebugHandleReadReg(uint32_t transport, eDrvAnlogIicPortMap iic, int argc, char *argv[])
{
    uint8_t lAddress = 0U;
    uint8_t lRegister = 0U;
    uint8_t lBuffer[DRVANLOGIIC_DEBUG_MAX_DATA_LENGTH];
    char lReply[DRVANLOGIIC_DEBUG_MAX_REPLY_LENGTH];
    uint32_t lLength = 0U;
    eDrvStatus lStatus;

    if ((argc != 6) || (argv == NULL) ||
        !drvAnlogIicDebugParseHexByte(argv[3], &lAddress) ||
        !drvAnlogIicDebugParseHexByte(argv[4], &lRegister) ||
        !drvAnlogIicDebugParseUint32(argv[5], &lLength) ||
        (lLength == 0U) || (lLength > DRVANLOGIIC_DEBUG_MAX_DATA_LENGTH)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    lStatus = drvAnlogIicReadRegister(iic, lAddress, &lRegister, 1U, lBuffer, (uint16_t)lLength);
    if (lStatus != DRV_STATUS_OK) {
        if (consoleReply(transport,
            "%s addr=%02X reg=%02X read=%lu status=%s",
            drvAnlogIicDebugGetBusName(iic),
            (unsigned int)lAddress,
            (unsigned int)lRegister,
            (unsigned long)lLength,
            drvAnlogIicDebugGetStatusText(lStatus)) <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (drvAnlogIicDebugBuildHexReply(lBuffer, (uint16_t)lLength, lReply, sizeof(lReply)) != CONSOLE_COMMAND_RESULT_OK) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (consoleReply(transport,
        "%s addr=%02X reg=%02X data=%s\nOK",
        drvAnlogIicDebugGetBusName(iic),
        (unsigned int)lAddress,
        (unsigned int)lRegister,
        lReply) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult drvAnlogIicDebugConsoleHandler(uint32_t transport, int argc, char *argv[])
{
    eDrvAnlogIicPortMap lIic;

    if ((argc < 2) || (argv == NULL) || (argv[1] == NULL)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if ((strcmp(argv[1], "list") == 0) || (strcmp(argv[1], "stat") == 0)) {
        if (argc != 2) {
            return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
        }

        return drvAnlogIicDebugReplyBusList(transport);
    }

    if (strcmp(argv[1], "help") == 0) {
        return drvAnlogIicDebugReplyHelp(transport, argc, argv);
    }

    if ((argc < 3) || (argv[2] == NULL)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (!drvAnlogIicDebugParseBus(argv[2], &lIic)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (strcmp(argv[1], "init") == 0) {
        return drvAnlogIicDebugHandleInit(transport, lIic);
    }

    if (strcmp(argv[1], "recover") == 0) {
        return drvAnlogIicDebugHandleRecover(transport, lIic);
    }

    if (strcmp(argv[1], "write") == 0) {
        return drvAnlogIicDebugHandleWrite(transport, lIic, argc, argv);
    }

    if (strcmp(argv[1], "read") == 0) {
        return drvAnlogIicDebugHandleRead(transport, lIic, argc, argv);
    }

    if ((strcmp(argv[1], "writereg") == 0) || (strcmp(argv[1], "wr") == 0)) {
        return drvAnlogIicDebugHandleWriteReg(transport, lIic, argc, argv);
    }

    if ((strcmp(argv[1], "readreg") == 0) || (strcmp(argv[1], "rr") == 0)) {
        return drvAnlogIicDebugHandleReadReg(transport, lIic, argc, argv);
    }

    return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
}

static eConsoleCommandResult drvAnlogIicDebugReplyHelp(uint32_t transport, int argc, char *argv[])
{
    if (argc == 2) {
        if (consoleReply(transport,
            "anlogiic <list|init|recover|write|read|writereg|readreg|help> ...\n"
            "  list\n"
            "  init <bus0|0>\n"
            "  recover <bus0|0>\n"
            "  help <read|write|readreg|writereg>\n"
            "    example: anlogiic help read\n"
            "OK") <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_OK;
    }

    if (argc != 3) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (strcmp(argv[2], "read") == 0) {
        if (consoleReply(transport,
            "anlogiic read <bus0|0> <addr> <len>\n"
            "  addr supports hex byte, len is decimal 1..16\n"
            "  example: anlogiic read bus0 0x68 4\n"
            "OK") <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_OK;
    }

    if (strcmp(argv[2], "write") == 0) {
        if (consoleReply(transport,
            "anlogiic write <bus0|0> <addr> <b0> [b1 ... b15]\n"
            "  addr and data bytes support hex, max 16 bytes\n"
            "  example: anlogiic write bus0 0x68 0x75\n"
            "OK") <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_OK;
    }

    if ((strcmp(argv[2], "writereg") == 0) || (strcmp(argv[2], "wr") == 0)) {
        if (consoleReply(transport,
            "anlogiic writereg <bus0|0> <addr> <reg> <b0> [b1 ... b15]\n"
            "  alias: wr\n"
            "  addr, reg and data bytes support hex, max 16 bytes\n"
            "  example: anlogiic writereg bus0 0x68 0x75 0x00\n"
            "OK") <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_OK;
    }

    if ((strcmp(argv[2], "readreg") == 0) || (strcmp(argv[2], "rr") == 0)) {
        if (consoleReply(transport,
            "anlogiic readreg <bus0|0> <addr> <reg> <len>\n"
            "  alias: rr\n"
            "  addr and reg support hex, len is decimal 1..16\n"
            "  example: anlogiic readreg bus0 0x68 0x75 1\n"
            "OK") <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_OK;
    }

    return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
}
#endif

bool drvAnlogIicDebugConsoleRegister(void)
{
#if (DRVANLOGIIC_CONSOLE_SUPPORT == 1)
    return consoleRegisterCommand(&gDrvAnlogIicConsoleCommand);
#else
    return true;
#endif
}

/**************************End of file********************************/
