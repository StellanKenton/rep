/***********************************************************************************
* @file     : gd25qxxx_debug.c
* @brief    : GD25Qxxx debug and console command implementation.
* @details  : This file hosts optional console bindings for GD25Qxxx operations.
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "gd25qxxx_debug.h"

#include "gd25qxxx.h"
#include "gd25qxxx_port.h"

#if (GD25QXXX_CONSOLE_SUPPORT == 1)
#include <stdint.h>
#include <string.h>

#include "../../service/log/console.h"

#define GD25QXXX_DEBUG_MAX_DATA_LENGTH    16U
#define GD25QXXX_DEBUG_MAX_REPLY_LENGTH   96U

static bool gd25qxxxDebugParseDevice(const char *name, eGd25qxxxMapType *device);
static bool gd25qxxxDebugParseHexNibble(char value, uint8_t *nibble);
static bool gd25qxxxDebugParseUint32(const char *text, uint32_t *value);
static bool gd25qxxxDebugParseHexByte(const char *text, uint8_t *value);
static const char *gd25qxxxDebugGetDeviceName(eGd25qxxxMapType device);
static const char *gd25qxxxDebugGetStatusText(eGd25qxxxStatus status);
static eConsoleCommandResult gd25qxxxDebugBuildHexReply(const uint8_t *buffer, uint16_t length, char *output, uint32_t capacity);
static eConsoleCommandResult gd25qxxxDebugReplyDeviceList(uint32_t transport);
static eConsoleCommandResult gd25qxxxDebugHandleInit(uint32_t transport, eGd25qxxxMapType device);
static eConsoleCommandResult gd25qxxxDebugHandleJedec(uint32_t transport, eGd25qxxxMapType device);
static eConsoleCommandResult gd25qxxxDebugHandleStatus(uint32_t transport, eGd25qxxxMapType device);
static eConsoleCommandResult gd25qxxxDebugHandleInfo(uint32_t transport, eGd25qxxxMapType device);
static eConsoleCommandResult gd25qxxxDebugHandleRead(uint32_t transport, eGd25qxxxMapType device, int argc, char *argv[]);
static eConsoleCommandResult gd25qxxxDebugHandleWrite(uint32_t transport, eGd25qxxxMapType device, int argc, char *argv[]);
static eConsoleCommandResult gd25qxxxDebugHandleErase(uint32_t transport, eGd25qxxxMapType device, int argc, char *argv[]);
static eConsoleCommandResult gd25qxxxDebugReplyHelp(uint32_t transport, int argc, char *argv[]);
static eConsoleCommandResult gd25qxxxDebugConsoleHandler(uint32_t transport, int argc, char *argv[]);

static const stConsoleCommand gGd25qxxxConsoleCommand = {
    .commandName = "gd25qxxx",
    .helpText = "gd25qxxx <list|init|jedec|status|info|read|write|erase|help> ...",
    .ownerTag = "gd25qxxx",
    .handler = gd25qxxxDebugConsoleHandler,
};

static bool gd25qxxxDebugParseDevice(const char *name, eGd25qxxxMapType *device)
{
    if ((name == NULL) || (device == NULL)) {
        return false;
    }

    if ((strcmp(name, "gd25q32_mem") == 0) ||
        (strcmp(name, "gd25q32") == 0) ||
        (strcmp(name, "mem") == 0) ||
        (strcmp(name, "0") == 0)) {
        *device = GD25Q32_MEM;
        return true;
    }

    return false;
}

static bool gd25qxxxDebugParseHexNibble(char value, uint8_t *nibble)
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

static bool gd25qxxxDebugParseUint32(const char *text, uint32_t *value)
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
            if (!gd25qxxxDebugParseHexNibble(*lCursor, &lNibble)) {
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

static bool gd25qxxxDebugParseHexByte(const char *text, uint8_t *value)
{
    uint8_t lTemp = 0U;
    uint32_t lValue = 0U;

    if (!gd25qxxxDebugParseUint32(text, &lValue) || (lValue > 0xFFU)) {
        return false;
    }

    lTemp = (uint8_t)lValue;
    *value = lTemp;
    return true;
}

static const char *gd25qxxxDebugGetDeviceName(eGd25qxxxMapType device)
{
    switch (device) {
        case GD25Q32_MEM:
            return "gd25q32_mem";
        default:
            return NULL;
    }
}

static const char *gd25qxxxDebugGetStatusText(eGd25qxxxStatus status)
{
    switch (status) {
        case GD25QXXX_STATUS_OK:
            return "ok";
        case GD25QXXX_STATUS_INVALID_PARAM:
            return "invalid_param";
        case GD25QXXX_STATUS_NOT_READY:
            return "not_ready";
        case GD25QXXX_STATUS_BUSY:
            return "busy";
        case GD25QXXX_STATUS_TIMEOUT:
            return "timeout";
        case GD25QXXX_STATUS_NACK:
            return "nack";
        case GD25QXXX_STATUS_UNSUPPORTED:
            return "unsupported";
        case GD25QXXX_STATUS_DEVICE_ID_MISMATCH:
            return "id_notmatch";
        case GD25QXXX_STATUS_OUT_OF_RANGE:
            return "out_of_range";
        default:
            return "error";
    }
}

static eConsoleCommandResult gd25qxxxDebugBuildHexReply(const uint8_t *buffer, uint16_t length, char *output, uint32_t capacity)
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

static eConsoleCommandResult gd25qxxxDebugReplyDeviceList(uint32_t transport)
{
    stGd25qxxxCfg lCfg;
    uint32_t lIndex;

    for (lIndex = 0U; lIndex < (uint32_t)GD25QXXX_DEV_MAX; lIndex++) {
        gd25qxxxPortGetDefCfg((eGd25qxxxMapType)lIndex, &lCfg);
        if (consoleReply(transport,
            "%s bus=%u ready=%s\n",
            gd25qxxxDebugGetDeviceName((eGd25qxxxMapType)lIndex),
            (unsigned int)lCfg.linkId,
            gd25qxxxIsReady((eGd25qxxxMapType)lIndex) ? "yes" : "no") <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }
    }

    if (consoleReply(transport, "OK") <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult gd25qxxxDebugHandleInit(uint32_t transport, eGd25qxxxMapType device)
{
    eGd25qxxxStatus lStatus = gd25qxxxInit(device);

    if (consoleReply(transport, "%s init=%s\nOK", gd25qxxxDebugGetDeviceName(device), gd25qxxxDebugGetStatusText(lStatus)) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return (lStatus == GD25QXXX_STATUS_OK) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
}

static eConsoleCommandResult gd25qxxxDebugHandleJedec(uint32_t transport, eGd25qxxxMapType device)
{
    uint8_t lManufacturerId = 0U;
    uint8_t lMemoryType = 0U;
    uint8_t lCapacityId = 0U;
    eGd25qxxxStatus lStatus = gd25qxxxReadJedecId(device, &lManufacturerId, &lMemoryType, &lCapacityId);

    if (lStatus != GD25QXXX_STATUS_OK) {
        if (consoleReply(transport, "%s jedec=%s", gd25qxxxDebugGetDeviceName(device), gd25qxxxDebugGetStatusText(lStatus)) <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (consoleReply(transport,
        "%s jedec=%02X %02X %02X\nOK",
        gd25qxxxDebugGetDeviceName(device),
        (unsigned int)lManufacturerId,
        (unsigned int)lMemoryType,
        (unsigned int)lCapacityId) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult gd25qxxxDebugHandleStatus(uint32_t transport, eGd25qxxxMapType device)
{
    uint8_t lStatusValue = 0U;
    eGd25qxxxStatus lStatus = gd25qxxxReadStatus1(device, &lStatusValue);

    if (lStatus != GD25QXXX_STATUS_OK) {
        if (consoleReply(transport, "%s status=%s", gd25qxxxDebugGetDeviceName(device), gd25qxxxDebugGetStatusText(lStatus)) <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (consoleReply(transport, "%s sr1=%02X\nOK", gd25qxxxDebugGetDeviceName(device), (unsigned int)lStatusValue) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult gd25qxxxDebugHandleInfo(uint32_t transport, eGd25qxxxMapType device)
{
    const stGd25qxxxInfo *lInfo = gd25qxxxGetInfo(device);

    if (lInfo == NULL) {
        if (consoleReply(transport, "%s info=not_ready", gd25qxxxDebugGetDeviceName(device)) <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (consoleReply(transport,
        "%s size=%lu page=%u sector=%lu block=%lu addrw=%u\nOK",
        gd25qxxxDebugGetDeviceName(device),
        (unsigned long)lInfo->totalSizeBytes,
        (unsigned int)lInfo->pageSizeBytes,
        (unsigned long)lInfo->sectorSizeBytes,
        (unsigned long)lInfo->blockSizeBytes,
        (unsigned int)lInfo->addressWidth) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult gd25qxxxDebugHandleRead(uint32_t transport, eGd25qxxxMapType device, int argc, char *argv[])
{
    uint32_t lAddress = 0U;
    uint32_t lLength = 0U;
    uint8_t lBuffer[GD25QXXX_DEBUG_MAX_DATA_LENGTH];
    char lReply[GD25QXXX_DEBUG_MAX_REPLY_LENGTH];
    eGd25qxxxStatus lStatus;

    if ((argc != 5) || !gd25qxxxDebugParseUint32(argv[3], &lAddress) || !gd25qxxxDebugParseUint32(argv[4], &lLength) ||
        (lLength == 0U) || (lLength > GD25QXXX_DEBUG_MAX_DATA_LENGTH)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    lStatus = gd25qxxxRead(device, lAddress, lBuffer, lLength);
    if (lStatus != GD25QXXX_STATUS_OK) {
        if (consoleReply(transport, "%s read=%s", gd25qxxxDebugGetDeviceName(device), gd25qxxxDebugGetStatusText(lStatus)) <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (gd25qxxxDebugBuildHexReply(lBuffer, (uint16_t)lLength, lReply, sizeof(lReply)) != CONSOLE_COMMAND_RESULT_OK) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (consoleReply(transport, "%s data=%s\nOK", gd25qxxxDebugGetDeviceName(device), lReply) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult gd25qxxxDebugHandleWrite(uint32_t transport, eGd25qxxxMapType device, int argc, char *argv[])
{
    uint32_t lAddress = 0U;
    uint8_t lBuffer[GD25QXXX_DEBUG_MAX_DATA_LENGTH];
    uint32_t lIndex;
    eGd25qxxxStatus lStatus;

    if ((argc < 5) || (argc > (int)(4U + GD25QXXX_DEBUG_MAX_DATA_LENGTH)) || !gd25qxxxDebugParseUint32(argv[3], &lAddress)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    for (lIndex = 4U; lIndex < (uint32_t)argc; lIndex++) {
        if (!gd25qxxxDebugParseHexByte(argv[lIndex], &lBuffer[lIndex - 4U])) {
            return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
        }
    }

    lStatus = gd25qxxxWrite(device, lAddress, lBuffer, (uint32_t)argc - 4U);
    if (consoleReply(transport, "%s write=%s\nOK", gd25qxxxDebugGetDeviceName(device), gd25qxxxDebugGetStatusText(lStatus)) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return (lStatus == GD25QXXX_STATUS_OK) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
}

static eConsoleCommandResult gd25qxxxDebugHandleErase(uint32_t transport, eGd25qxxxMapType device, int argc, char *argv[])
{
    uint32_t lAddress = 0U;
    eGd25qxxxStatus lStatus;

    if ((argc != 4) || !gd25qxxxDebugParseUint32(argv[3], &lAddress)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    lStatus = gd25qxxxEraseSector(device, lAddress);
    if (consoleReply(transport, "%s erase=%s\nOK", gd25qxxxDebugGetDeviceName(device), gd25qxxxDebugGetStatusText(lStatus)) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return (lStatus == GD25QXXX_STATUS_OK) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
}

static eConsoleCommandResult gd25qxxxDebugReplyHelp(uint32_t transport, int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (consoleReply(transport,
        "gd25qxxx list\n"
        "gd25qxxx init <gd25q32_mem|gd25q32|mem|0>\n"
        "gd25qxxx jedec <gd25q32_mem|gd25q32|mem|0>\n"
        "gd25qxxx status <gd25q32_mem|gd25q32|mem|0>\n"
        "gd25qxxx info <gd25q32_mem|gd25q32|mem|0>\n"
        "gd25qxxx read <gd25q32_mem|gd25q32|mem|0> <addr> <len>\n"
        "gd25qxxx write <gd25q32_mem|gd25q32|mem|0> <addr> <b0> [b1 ...]\n"
        "gd25qxxx erase <gd25q32_mem|gd25q32|mem|0> <addr>\n"
        "OK") <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult gd25qxxxDebugConsoleHandler(uint32_t transport, int argc, char *argv[])
{
    eGd25qxxxMapType lDevice;

    if ((argc < 2) || (argv == NULL) || (argv[1] == NULL)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (strcmp(argv[1], "list") == 0) {
        return gd25qxxxDebugReplyDeviceList(transport);
    }

    if (strcmp(argv[1], "help") == 0) {
        return gd25qxxxDebugReplyHelp(transport, argc, argv);
    }

    if ((argc < 3) || !gd25qxxxDebugParseDevice(argv[2], &lDevice)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (strcmp(argv[1], "init") == 0) {
        return gd25qxxxDebugHandleInit(transport, lDevice);
    }

    if (strcmp(argv[1], "jedec") == 0) {
        return gd25qxxxDebugHandleJedec(transport, lDevice);
    }

    if (strcmp(argv[1], "status") == 0) {
        return gd25qxxxDebugHandleStatus(transport, lDevice);
    }

    if (strcmp(argv[1], "info") == 0) {
        return gd25qxxxDebugHandleInfo(transport, lDevice);
    }

    if (strcmp(argv[1], "read") == 0) {
        return gd25qxxxDebugHandleRead(transport, lDevice, argc, argv);
    }

    if (strcmp(argv[1], "write") == 0) {
        return gd25qxxxDebugHandleWrite(transport, lDevice, argc, argv);
    }

    if (strcmp(argv[1], "erase") == 0) {
        return gd25qxxxDebugHandleErase(transport, lDevice, argc, argv);
    }

    return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
}

bool gd25qxxxDebugConsoleRegister(void)
{
    return consoleRegisterCommand(&gGd25qxxxConsoleCommand);
}

#else

bool gd25qxxxDebugConsoleRegister(void)
{
    return true;
}

#endif

/**************************End of file********************************/
