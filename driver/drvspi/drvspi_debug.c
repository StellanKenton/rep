/***********************************************************************************
* @file     : drvspi_debug.c
* @brief    : DrvSpi debug and console command implementation.
* @details  : This file hosts optional console bindings for SPI debug operations.
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "drvspi_debug.h"

#include "drvspi.h"
#include "drvspi_port.h"

#if (DRVSPI_CONSOLE_SUPPORT == 1)
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "../../service/log/console.h"

#define DRVSPI_DEBUG_MAX_DATA_LENGTH     16U
#define DRVSPI_DEBUG_MAX_REPLY_LENGTH    96U

extern stDrvSpiBspInterface gDrvSpiBspInterface[DRVSPI_MAX];

static bool drvSpiDebugParseBus(const char *name, eDrvSpiPortMap *spi);
static bool drvSpiDebugParseUint32(const char *text, uint32_t *value);
static bool drvSpiDebugParseHexNibble(char value, uint8_t *nibble);
static bool drvSpiDebugParseHexByte(const char *text, uint8_t *value);
static const char *drvSpiDebugGetBusName(eDrvSpiPortMap spi);
static const char *drvSpiDebugGetStatusText(eDrvStatus status);
static eConsoleCommandResult drvSpiDebugBuildHexReply(const uint8_t *buffer, uint16_t length, char *output, uint32_t capacity);
static eConsoleCommandResult drvSpiDebugReplyBusList(uint32_t transport);
static eConsoleCommandResult drvSpiDebugHandleInit(uint32_t transport, eDrvSpiPortMap spi);
static eConsoleCommandResult drvSpiDebugHandleWrite(uint32_t transport, eDrvSpiPortMap spi, int argc, char *argv[]);
static eConsoleCommandResult drvSpiDebugHandleRead(uint32_t transport, eDrvSpiPortMap spi, int argc, char *argv[]);
static eConsoleCommandResult drvSpiDebugHandleWriteRead(uint32_t transport, eDrvSpiPortMap spi, int argc, char *argv[]);
static eConsoleCommandResult drvSpiDebugHandleExchange(uint32_t transport, eDrvSpiPortMap spi, int argc, char *argv[]);
static eConsoleCommandResult drvSpiDebugReplyHelp(uint32_t transport, int argc, char *argv[]);
static eConsoleCommandResult drvSpiDebugConsoleHandler(uint32_t transport, int argc, char *argv[]);

static const stConsoleCommand gDrvSpiConsoleCommand = {
    .commandName = "spi",
    .helpText = "spi <list|init|write|read|writeread|exchange|help> ...",
    .ownerTag = "drvSpi",
    .handler = drvSpiDebugConsoleHandler,
};

static bool drvSpiDebugParseBus(const char *name, eDrvSpiPortMap *spi)
{
    if ((name == NULL) || (spi == NULL)) {
        return false;
    }

    if ((strcmp(name, "bus0") == 0) || (strcmp(name, "0") == 0)) {
        *spi = DRVSPI_BUS0;
        return true;
    }

    return false;
}

static bool drvSpiDebugParseUint32(const char *text, uint32_t *value)
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

static bool drvSpiDebugParseHexNibble(char value, uint8_t *nibble)
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

static bool drvSpiDebugParseHexByte(const char *text, uint8_t *value)
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
        if (!drvSpiDebugParseHexNibble(lCursor[0], &lLow)) {
            return false;
        }

        *value = lLow;
        return true;
    }

    if ((lCursor[2] != '\0') ||
        !drvSpiDebugParseHexNibble(lCursor[0], &lHigh) ||
        !drvSpiDebugParseHexNibble(lCursor[1], &lLow)) {
        return false;
    }

    *value = (uint8_t)((lHigh << 4) | lLow);
    return true;
}

static const char *drvSpiDebugGetBusName(eDrvSpiPortMap spi)
{
    switch (spi) {
        case DRVSPI_BUS0:
            return "bus0";
        default:
            return NULL;
    }
}

static const char *drvSpiDebugGetStatusText(eDrvStatus status)
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
        case DRV_STATUS_UNSUPPORTED:
            return "unsupported";
        default:
            return "error";
    }
}

static eConsoleCommandResult drvSpiDebugBuildHexReply(const uint8_t *buffer, uint16_t length, char *output, uint32_t capacity)
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

static eConsoleCommandResult drvSpiDebugReplyBusList(uint32_t transport)
{
    uint32_t lIndex;

    for (lIndex = 0U; lIndex < (uint32_t)DRVSPI_MAX; ++lIndex) {
        if (consoleReply(transport,
            "%s cs=%s timeout_ms=%lu\n",
            drvSpiDebugGetBusName((eDrvSpiPortMap)lIndex),
            (gDrvSpiBspInterface[lIndex].csControl.write != NULL) ? "yes" : "no",
            (unsigned long)gDrvSpiBspInterface[lIndex].defaultTimeoutMs) <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }
    }

    if (consoleReply(transport, "OK") <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult drvSpiDebugHandleInit(uint32_t transport, eDrvSpiPortMap spi)
{
    eDrvStatus lStatus = drvSpiInit(spi);

    if (consoleReply(transport, "%s init=%s\nOK", drvSpiDebugGetBusName(spi), drvSpiDebugGetStatusText(lStatus)) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return (lStatus == DRV_STATUS_OK) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
}

static eConsoleCommandResult drvSpiDebugHandleWrite(uint32_t transport, eDrvSpiPortMap spi, int argc, char *argv[])
{
    uint8_t lBuffer[DRVSPI_DEBUG_MAX_DATA_LENGTH];
    uint16_t lLength = 0U;
    eDrvStatus lStatus;
    int lIndex;

    if ((argc < 4) || (argv == NULL)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    for (lIndex = 3; lIndex < argc; lIndex++) {
        if (lLength >= DRVSPI_DEBUG_MAX_DATA_LENGTH) {
            return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
        }

        if (!drvSpiDebugParseHexByte(argv[lIndex], &lBuffer[lLength])) {
            return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
        }

        lLength++;
    }

    lStatus = drvSpiWrite(spi, lBuffer, lLength);
    if (consoleReply(transport,
        "%s write=%u status=%s\nOK",
        drvSpiDebugGetBusName(spi),
        (unsigned int)lLength,
        drvSpiDebugGetStatusText(lStatus)) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return (lStatus == DRV_STATUS_OK) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
}

static eConsoleCommandResult drvSpiDebugHandleRead(uint32_t transport, eDrvSpiPortMap spi, int argc, char *argv[])
{
    uint8_t lBuffer[DRVSPI_DEBUG_MAX_DATA_LENGTH];
    char lReply[DRVSPI_DEBUG_MAX_REPLY_LENGTH];
    uint32_t lLength = 0U;
    eDrvStatus lStatus;

    if ((argc != 4) || (argv == NULL) ||
        !drvSpiDebugParseUint32(argv[3], &lLength) ||
        (lLength == 0U) || (lLength > DRVSPI_DEBUG_MAX_DATA_LENGTH)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    lStatus = drvSpiRead(spi, lBuffer, (uint16_t)lLength);
    if (lStatus != DRV_STATUS_OK) {
        if (consoleReply(transport,
            "%s read=%lu status=%s",
            drvSpiDebugGetBusName(spi),
            (unsigned long)lLength,
            drvSpiDebugGetStatusText(lStatus)) <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (drvSpiDebugBuildHexReply(lBuffer, (uint16_t)lLength, lReply, sizeof(lReply)) != CONSOLE_COMMAND_RESULT_OK) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (consoleReply(transport,
        "%s data=%s\nOK",
        drvSpiDebugGetBusName(spi),
        lReply) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult drvSpiDebugHandleWriteRead(uint32_t transport, eDrvSpiPortMap spi, int argc, char *argv[])
{
    uint8_t lWriteBuffer[DRVSPI_DEBUG_MAX_DATA_LENGTH];
    uint8_t lReadBuffer[DRVSPI_DEBUG_MAX_DATA_LENGTH];
    char lReply[DRVSPI_DEBUG_MAX_REPLY_LENGTH];
    uint16_t lWriteLength = 0U;
    uint32_t lReadLength = 0U;
    eDrvStatus lStatus;
    int lIndex;

    if ((argc < 5) || (argv == NULL) ||
        !drvSpiDebugParseUint32(argv[argc - 1], &lReadLength) ||
        (lReadLength == 0U) || (lReadLength > DRVSPI_DEBUG_MAX_DATA_LENGTH)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    for (lIndex = 3; lIndex < (argc - 1); lIndex++) {
        if (lWriteLength >= DRVSPI_DEBUG_MAX_DATA_LENGTH) {
            return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
        }

        if (!drvSpiDebugParseHexByte(argv[lIndex], &lWriteBuffer[lWriteLength])) {
            return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
        }

        lWriteLength++;
    }

    lStatus = drvSpiWriteRead(spi, lWriteBuffer, lWriteLength, lReadBuffer, (uint16_t)lReadLength);
    if (lStatus != DRV_STATUS_OK) {
        if (consoleReply(transport,
            "%s writeread status=%s",
            drvSpiDebugGetBusName(spi),
            drvSpiDebugGetStatusText(lStatus)) <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (drvSpiDebugBuildHexReply(lReadBuffer, (uint16_t)lReadLength, lReply, sizeof(lReply)) != CONSOLE_COMMAND_RESULT_OK) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (consoleReply(transport,
        "%s data=%s\nOK",
        drvSpiDebugGetBusName(spi),
        lReply) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult drvSpiDebugHandleExchange(uint32_t transport, eDrvSpiPortMap spi, int argc, char *argv[])
{
    uint8_t lWriteBuffer[DRVSPI_DEBUG_MAX_DATA_LENGTH];
    uint8_t lReadBuffer[DRVSPI_DEBUG_MAX_DATA_LENGTH];
    char lReply[DRVSPI_DEBUG_MAX_REPLY_LENGTH];
    uint16_t lLength = 0U;
    eDrvStatus lStatus;
    int lIndex;

    if ((argc < 4) || (argv == NULL)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    for (lIndex = 3; lIndex < argc; lIndex++) {
        if (lLength >= DRVSPI_DEBUG_MAX_DATA_LENGTH) {
            return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
        }

        if (!drvSpiDebugParseHexByte(argv[lIndex], &lWriteBuffer[lLength])) {
            return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
        }

        lLength++;
    }

    lStatus = drvSpiExchange(spi, lWriteBuffer, lReadBuffer, lLength);
    if (lStatus != DRV_STATUS_OK) {
        if (consoleReply(transport,
            "%s exchange status=%s",
            drvSpiDebugGetBusName(spi),
            drvSpiDebugGetStatusText(lStatus)) <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (drvSpiDebugBuildHexReply(lReadBuffer, lLength, lReply, sizeof(lReply)) != CONSOLE_COMMAND_RESULT_OK) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (consoleReply(transport,
        "%s data=%s\nOK",
        drvSpiDebugGetBusName(spi),
        lReply) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult drvSpiDebugConsoleHandler(uint32_t transport, int argc, char *argv[])
{
    eDrvSpiPortMap lSpi;

    if ((argc < 2) || (argv == NULL) || (argv[1] == NULL)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if ((strcmp(argv[1], "list") == 0) || (strcmp(argv[1], "stat") == 0)) {
        if (argc != 2) {
            return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
        }

        return drvSpiDebugReplyBusList(transport);
    }

    if (strcmp(argv[1], "help") == 0) {
        return drvSpiDebugReplyHelp(transport, argc, argv);
    }

    if ((argc < 3) || (argv[2] == NULL)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (!drvSpiDebugParseBus(argv[2], &lSpi)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (strcmp(argv[1], "init") == 0) {
        return drvSpiDebugHandleInit(transport, lSpi);
    }

    if (strcmp(argv[1], "write") == 0) {
        return drvSpiDebugHandleWrite(transport, lSpi, argc, argv);
    }

    if (strcmp(argv[1], "read") == 0) {
        return drvSpiDebugHandleRead(transport, lSpi, argc, argv);
    }

    if ((strcmp(argv[1], "writeread") == 0) || (strcmp(argv[1], "wr") == 0)) {
        return drvSpiDebugHandleWriteRead(transport, lSpi, argc, argv);
    }

    if ((strcmp(argv[1], "exchange") == 0) || (strcmp(argv[1], "xfer") == 0)) {
        return drvSpiDebugHandleExchange(transport, lSpi, argc, argv);
    }

    return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
}

static eConsoleCommandResult drvSpiDebugReplyHelp(uint32_t transport, int argc, char *argv[])
{
    if (argc == 2) {
        if (consoleReply(transport,
            "spi <list|init|write|read|writeread|exchange|help> ...\n"
            "  list\n"
            "  init <bus0|0>\n"
            "  help <read|write|writeread|exchange>\n"
            "    example: spi help writeread\n"
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
            "spi read <bus0|0> <len>\n"
            "  len is decimal 1..16\n"
            "  example: spi read bus0 4\n"
            "OK") <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_OK;
    }

    if (strcmp(argv[2], "write") == 0) {
        if (consoleReply(transport,
            "spi write <bus0|0> <b0> [b1 ... b15]\n"
            "  data bytes support hex, max 16 bytes\n"
            "  example: spi write bus0 0x9F\n"
            "OK") <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_OK;
    }

    if ((strcmp(argv[2], "writeread") == 0) || (strcmp(argv[2], "wr") == 0)) {
        if (consoleReply(transport,
            "spi writeread <bus0|0> <b0> [b1 ... b15] <len>\n"
            "  alias: wr\n"
            "  write bytes support hex, read len is decimal 1..16\n"
            "  example: spi writeread bus0 0x9F 3\n"
            "OK") <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_OK;
    }

    if ((strcmp(argv[2], "exchange") == 0) || (strcmp(argv[2], "xfer") == 0)) {
        if (consoleReply(transport,
            "spi exchange <bus0|0> <b0> [b1 ... b15]\n"
            "  alias: xfer\n"
            "  data bytes support hex, max 16 bytes\n"
            "  example: spi exchange bus0 0x9F 0x00 0x00 0x00\n"
            "OK") <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_OK;
    }

    return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
}
#endif

bool drvSpiDebugConsoleRegister(void)
{
#if (DRVSPI_CONSOLE_SUPPORT == 1)
    return consoleRegisterCommand(&gDrvSpiConsoleCommand);
#else
    return true;
#endif
}

/**************************End of file********************************/
