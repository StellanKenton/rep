/***********************************************************************************
* @file     : drvuart_debug.c
* @brief    : DrvUart debug and console command implementation.
* @details  : This file hosts optional console bindings for UART debug operations.
* @author   : GitHub Copilot
* @date     : 2026-04-01
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "drvuart_debug.h"
#include "drvuart.h"

#if (DRVUART_CONSOLE_SUPPORT == 1)
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "console.h"
#include "ringbuffer.h"

#define DRVUART_DEBUG_READ_MAX_LENGTH       24U
#define DRVUART_DEBUG_SEND_TEXT_MAX_LENGTH  64U
#define DRVUART_DEBUG_SEND_HEX_MAX_LENGTH   24U

typedef enum eDrvUartDebugReadMode {
    DRVUART_DEBUG_READ_MODE_HEX = 0,
    DRVUART_DEBUG_READ_MODE_TEXT,
} eDrvUartDebugReadMode;

typedef struct stDrvUartDebugPortDescriptor {
    eDrvUartPortMap uart;
    const char *portName;
    const char *txMode;
    bool isReadable;
    bool isWritable;
} stDrvUartDebugPortDescriptor;

static const stDrvUartDebugPortDescriptor *drvUartDebugGetDefaultPort(void);
static bool drvUartDebugParseUint32(const char *text, uint32_t *value);
static bool drvUartDebugParseHexByte(const char *text, uint8_t *value);
static eConsoleCommandResult drvUartDebugReplyPortList(uint32_t transport);
static eConsoleCommandResult drvUartDebugReplyPortStatus(uint32_t transport);
static eConsoleCommandResult drvUartDebugReplyRxLength(uint32_t transport);
static eConsoleCommandResult drvUartDebugFlushRxBuffer(uint32_t transport);
static eConsoleCommandResult drvUartDebugReadRxData(uint32_t transport, uint32_t length, eDrvUartDebugReadMode mode);
static eConsoleCommandResult drvUartDebugSendText(uint32_t transport, int argc, char *argv[]);
static eConsoleCommandResult drvUartDebugSendHex(uint32_t transport, int argc, char *argv[]);
static eConsoleCommandResult drvUartDebugReplyHelp(uint32_t transport, int argc, char *argv[]);
static eConsoleCommandResult drvUartDebugConsoleHandler(uint32_t transport, int argc, char *argv[]);

static const stDrvUartDebugPortDescriptor gDrvUartDebugPorts[] = {
    {
        .uart = DRVUART_DEBUG,
        .portName = "debug",
        .txMode = "polling+dma",
        .isReadable = true,
        .isWritable = true,
    },
};

static const stConsoleCommand gDrvUartConsoleCommand = {
    .commandName = "uart",
    .helpText = "uart <list|stat|rxlen|read|flush|send|sendhex|help> ...",
    .ownerTag = "drvUart",
    .handler = drvUartDebugConsoleHandler,
};

static const stDrvUartDebugPortDescriptor *drvUartDebugGetDefaultPort(void)
{
    return &gDrvUartDebugPorts[0];
}

static bool drvUartDebugParseUint32(const char *text, uint32_t *value)
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

static bool drvUartDebugParseHexNibble(char value, uint8_t *nibble)
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

static bool drvUartDebugParseHexByte(const char *text, uint8_t *value)
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
        if (!drvUartDebugParseHexNibble(lCursor[0], &lLow)) {
            return false;
        }

        *value = lLow;
        return true;
    }

    if ((lCursor[2] != '\0') ||
        !drvUartDebugParseHexNibble(lCursor[0], &lHigh) ||
        !drvUartDebugParseHexNibble(lCursor[1], &lLow)) {
        return false;
    }

    *value = (uint8_t)((lHigh << 4) | lLow);
    return true;
}

static eConsoleCommandResult drvUartDebugReplyPortList(uint32_t transport)
{
    uint32_t lIndex;

    for (lIndex = 0U; lIndex < (uint32_t)(sizeof(gDrvUartDebugPorts) / sizeof(gDrvUartDebugPorts[0])); lIndex++) {
        if (consoleReply(transport,
            "%s read=%s write=%s tx=%s",
            gDrvUartDebugPorts[lIndex].portName,
            gDrvUartDebugPorts[lIndex].isReadable ? "yes" : "no",
            gDrvUartDebugPorts[lIndex].isWritable ? "yes" : "no",
            gDrvUartDebugPorts[lIndex].txMode) <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }
    }

    if (consoleReply(transport, "OK") <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult drvUartDebugReplyPortStatus(uint32_t transport)
{
    const stDrvUartDebugPortDescriptor *lPort = drvUartDebugGetDefaultPort();
    stRingBuffer *lRingBuffer = drvUartGetRingBuffer(lPort->uart);
    uint32_t lCapacity = 0U;
    uint32_t lFree = 0U;
    uint32_t lUsed = 0U;
    const char *lReadyText = "no";

    if (lRingBuffer != NULL) {
        lCapacity = ringBufferGetCapacity(lRingBuffer);
        lFree = ringBufferGetFree(lRingBuffer);
        lUsed = ringBufferGetUsed(lRingBuffer);
        lReadyText = "yes";
    }

    if (consoleReply(transport,
        "port=%s ready=%s rx_used=%lu rx_free=%lu rx_capacity=%lu tx=%s\nOK",
        lPort->portName,
        lReadyText,
        (unsigned long)lUsed,
        (unsigned long)lFree,
        (unsigned long)lCapacity,
        lPort->txMode) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult drvUartDebugReplyRxLength(uint32_t transport)
{
    const stDrvUartDebugPortDescriptor *lPort = drvUartDebugGetDefaultPort();
    uint16_t lLength = drvUartGetDataLen(lPort->uart);

    if (consoleReply(transport,
        "port=%s rx_pending=%u\nOK",
        lPort->portName,
        (unsigned int)lLength) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult drvUartDebugFlushRxBuffer(uint32_t transport)
{
    const stDrvUartDebugPortDescriptor *lPort = drvUartDebugGetDefaultPort();
    uint8_t lScratch[DRVUART_DEBUG_READ_MAX_LENGTH];
    uint32_t lDiscarded = 0U;

    while (drvUartGetDataLen(lPort->uart) > 0U) {
        uint16_t lChunk = drvUartGetDataLen(lPort->uart);

        if (lChunk > DRVUART_DEBUG_READ_MAX_LENGTH) {
            lChunk = DRVUART_DEBUG_READ_MAX_LENGTH;
        }

        if (drvUartReceive(lPort->uart, lScratch, lChunk) != DRV_STATUS_OK) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        lDiscarded += lChunk;
    }

    if (consoleReply(transport,
        "port=%s flushed=%lu\nOK",
        lPort->portName,
        (unsigned long)lDiscarded) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult drvUartDebugBuildHexReply(const uint8_t *buffer, uint16_t length, char *output, uint32_t capacity)
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

static eConsoleCommandResult drvUartDebugBuildTextReply(const uint8_t *buffer, uint16_t length, char *output, uint32_t capacity)
{
    uint32_t lIndex;
    uint32_t lOffset = 0U;

    if ((buffer == NULL) || (output == NULL) || (capacity == 0U)) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    output[0] = '\0';
    for (lIndex = 0U; lIndex < (uint32_t)length; lIndex++) {
        uint8_t lValue = buffer[lIndex];

        if ((lValue >= 32U) && (lValue <= 126U) && (lValue != '\\')) {
            if ((lOffset + 1U) >= capacity) {
                return CONSOLE_COMMAND_RESULT_ERROR;
            }

            output[lOffset] = (char)lValue;
            lOffset++;
            output[lOffset] = '\0';
            continue;
        }

        if ((lOffset + 4U) >= capacity) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        switch (lValue) {
            case '\r':
                output[lOffset] = '\\';
                output[lOffset + 1U] = 'r';
                lOffset += 2U;
                break;
            case '\n':
                output[lOffset] = '\\';
                output[lOffset + 1U] = 'n';
                lOffset += 2U;
                break;
            case '\t':
                output[lOffset] = '\\';
                output[lOffset + 1U] = 't';
                lOffset += 2U;
                break;
            case '\\':
                output[lOffset] = '\\';
                output[lOffset + 1U] = '\\';
                lOffset += 2U;
                break;
            default:
                output[lOffset] = '\\';
                output[lOffset + 1U] = 'x';
                output[lOffset + 2U] = "0123456789ABCDEF"[(lValue >> 4) & 0x0FU];
                output[lOffset + 3U] = "0123456789ABCDEF"[lValue & 0x0FU];
                lOffset += 4U;
                break;
        }

        output[lOffset] = '\0';
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult drvUartDebugReadRxData(uint32_t transport, uint32_t length, eDrvUartDebugReadMode mode)
{
    const stDrvUartDebugPortDescriptor *lPort = drvUartDebugGetDefaultPort();
    uint8_t lData[DRVUART_DEBUG_READ_MAX_LENGTH];
    char lReplyBuffer[CONSOLE_REPLY_BUFFER_SIZE - 32U];
    uint16_t lReadLength;
    uint16_t lPendingLength;
    eConsoleCommandResult lResult;

    if ((length == 0U) || (length > DRVUART_DEBUG_READ_MAX_LENGTH)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    lPendingLength = drvUartGetDataLen(lPort->uart);
    if ((lPendingLength == 0U) || (length > lPendingLength)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    lReadLength = (uint16_t)length;
    if (drvUartReceive(lPort->uart, lData, lReadLength) != DRV_STATUS_OK) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (mode == DRVUART_DEBUG_READ_MODE_TEXT) {
        lResult = drvUartDebugBuildTextReply(lData, lReadLength, lReplyBuffer, sizeof(lReplyBuffer));
    } else {
        lResult = drvUartDebugBuildHexReply(lData, lReadLength, lReplyBuffer, sizeof(lReplyBuffer));
    }

    if (lResult != CONSOLE_COMMAND_RESULT_OK) {
        return lResult;
    }

    if (consoleReply(transport,
        "port=%s read=%u mode=%s data=%s\nOK",
        lPort->portName,
        (unsigned int)lReadLength,
        (mode == DRVUART_DEBUG_READ_MODE_TEXT) ? "text" : "hex",
        lReplyBuffer) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult drvUartDebugSendText(uint32_t transport, int argc, char *argv[])
{
    const stDrvUartDebugPortDescriptor *lPort = drvUartDebugGetDefaultPort();
    char lBuffer[DRVUART_DEBUG_SEND_TEXT_MAX_LENGTH + 1U];
    uint16_t lLength = 0U;
    int lIndex;

    if ((argc < 3) || (argv == NULL)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    lBuffer[0] = '\0';
    for (lIndex = 2; lIndex < argc; lIndex++) {
        size_t lPartLength;

        if (argv[lIndex] == NULL) {
            return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
        }

        if (lLength > 0U) {
            if (lLength >= DRVUART_DEBUG_SEND_TEXT_MAX_LENGTH) {
                return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
            }

            lBuffer[lLength] = ' ';
            lLength++;
            lBuffer[lLength] = '\0';
        }

        lPartLength = strlen(argv[lIndex]);
        if ((lPartLength == 0U) || ((size_t)lLength + lPartLength > DRVUART_DEBUG_SEND_TEXT_MAX_LENGTH)) {
            return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
        }

        (void)memcpy(&lBuffer[lLength], argv[lIndex], lPartLength);
        lLength = (uint16_t)(lLength + (uint16_t)lPartLength);
        lBuffer[lLength] = '\0';
    }

    if (drvUartTransmitDma(lPort->uart, (const uint8_t *)lBuffer, lLength) != DRV_STATUS_OK) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (consoleReply(transport,
        "port=%s sent=%u mode=text\nOK",
        lPort->portName,
        (unsigned int)lLength) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult drvUartDebugSendHex(uint32_t transport, int argc, char *argv[])
{
    const stDrvUartDebugPortDescriptor *lPort = drvUartDebugGetDefaultPort();
    uint8_t lBuffer[DRVUART_DEBUG_SEND_HEX_MAX_LENGTH];
    uint16_t lLength = 0U;
    int lIndex;

    if ((argc < 3) || (argv == NULL)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    for (lIndex = 2; lIndex < argc; lIndex++) {
        if (lLength >= DRVUART_DEBUG_SEND_HEX_MAX_LENGTH) {
            return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
        }

        if (!drvUartDebugParseHexByte(argv[lIndex], &lBuffer[lLength])) {
            return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
        }

        lLength++;
    }

    if (drvUartTransmitDma(lPort->uart, lBuffer, lLength) != DRV_STATUS_OK) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (consoleReply(transport,
        "port=%s sent=%u mode=hex\nOK",
        lPort->portName,
        (unsigned int)lLength) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult drvUartDebugConsoleHandler(uint32_t transport, int argc, char *argv[])
{
    uint32_t lLength = 0U;

    if ((argc < 2) || (argv == NULL) || (argv[1] == NULL)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (strcmp(argv[1], "list") == 0) {
        if (argc != 2) {
            return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
        }

        return drvUartDebugReplyPortList(transport);
    }

    if (strcmp(argv[1], "stat") == 0) {
        if (argc != 2) {
            return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
        }

        return drvUartDebugReplyPortStatus(transport);
    }

    if ((strcmp(argv[1], "rxlen") == 0) || (strcmp(argv[1], "avail") == 0)) {
        if (argc != 2) {
            return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
        }

        return drvUartDebugReplyRxLength(transport);
    }

    if (strcmp(argv[1], "flush") == 0) {
        if (argc != 2) {
            return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
        }

        return drvUartDebugFlushRxBuffer(transport);
    }

    if (strcmp(argv[1], "help") == 0) {
        return drvUartDebugReplyHelp(transport, argc, argv);
    }

    if ((strcmp(argv[1], "read") == 0) || (strcmp(argv[1], "recv") == 0)) {
        if ((argc != 3) && (argc != 4)) {
            return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
        }

        if (!drvUartDebugParseUint32(argv[2], &lLength)) {
            return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
        }

        if (argc == 4) {
            if (strcmp(argv[3], "text") == 0) {
                return drvUartDebugReadRxData(transport, lLength, DRVUART_DEBUG_READ_MODE_TEXT);
            }

            if (strcmp(argv[3], "hex") != 0) {
                return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
            }
        }

        return drvUartDebugReadRxData(transport, lLength, DRVUART_DEBUG_READ_MODE_HEX);
    }

    if ((strcmp(argv[1], "send") == 0) || (strcmp(argv[1], "tx") == 0)) {
        return drvUartDebugSendText(transport, argc, argv);
    }

    if ((strcmp(argv[1], "sendhex") == 0) || (strcmp(argv[1], "txhex") == 0)) {
        return drvUartDebugSendHex(transport, argc, argv);
    }

    return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
}

static eConsoleCommandResult drvUartDebugReplyHelp(uint32_t transport, int argc, char *argv[])
{
    if (argc == 2) {
        if (consoleReply(transport,
            "uart <list|stat|rxlen|read|flush|send|sendhex|help> ...\n"
            "  list\n"
            "  stat\n"
            "  rxlen\n"
            "  flush\n"
            "  help <read|send|sendhex>\n"
            "    example: uart help read\n"
            "OK") <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_OK;
    }

    if (argc != 3) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if ((strcmp(argv[2], "read") == 0) || (strcmp(argv[2], "recv") == 0)) {
        if (consoleReply(transport,
            "uart read <len> [text|hex]\n"
            "  alias: recv\n"
            "  len is decimal 1..24, mode defaults to hex\n"
            "  example: uart read 8 hex\n"
            "OK") <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_OK;
    }

    if ((strcmp(argv[2], "send") == 0) || (strcmp(argv[2], "tx") == 0)) {
        if (consoleReply(transport,
            "uart send <word0> [word1 ...]\n"
            "  alias: tx\n"
            "  words are joined with spaces, total text length max 64 chars\n"
            "  example: uart send hello world\n"
            "OK") <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_OK;
    }

    if ((strcmp(argv[2], "sendhex") == 0) || (strcmp(argv[2], "txhex") == 0)) {
        if (consoleReply(transport,
            "uart sendhex <b0> [b1 ... b23]\n"
            "  alias: txhex\n"
            "  data bytes support hex, max 24 bytes\n"
            "  example: uart sendhex 0x48 0x69\n"
            "OK") <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_OK;
    }

    return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
}
#endif

bool drvUartDebugConsoleRegister(void)
{
#if (DRVUART_CONSOLE_SUPPORT == 1)
    return consoleRegisterCommand(&gDrvUartConsoleCommand);
#else
    return true;
#endif
}
/**************************End of file********************************/
