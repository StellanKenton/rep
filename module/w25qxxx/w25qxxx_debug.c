/***********************************************************************************
* @file     : w25qxxx_debug.c
* @brief    : W25Qxxx debug and console command implementation.
* @details  : This file hosts optional console bindings for W25Qxxx operations.
**********************************************************************************/
#include "w25qxxx_debug.h"

#include "w25qxxx.h"
#include "w25qxxx_port.h"

#if (W25QXXX_CONSOLE_SUPPORT == 1)
#include <stdint.h>
#include <string.h>

#include "console.h"

#define W25QXXX_DEBUG_MAX_DATA_LENGTH    16U
#define W25QXXX_DEBUG_MAX_REPLY_LENGTH   96U

static bool w25qxxxDebugParseDevice(const char *name, eW25qxxxMapType *device);
static bool w25qxxxDebugParseHexNibble(char value, uint8_t *nibble);
static bool w25qxxxDebugParseUint32(const char *text, uint32_t *value);
static bool w25qxxxDebugParseHexByte(const char *text, uint8_t *value);
static const char *w25qxxxDebugGetDeviceName(eW25qxxxMapType device);
static const char *w25qxxxDebugGetStatusText(eW25qxxxStatus status);
static eConsoleCommandResult w25qxxxDebugBuildHexReply(const uint8_t *buffer, uint16_t length, char *output, uint32_t capacity);
static eConsoleCommandResult w25qxxxDebugReplyDeviceList(uint32_t transport);
static eConsoleCommandResult w25qxxxDebugHandleInit(uint32_t transport, eW25qxxxMapType device);
static eConsoleCommandResult w25qxxxDebugHandleJedec(uint32_t transport, eW25qxxxMapType device);
static eConsoleCommandResult w25qxxxDebugHandleStatus(uint32_t transport, eW25qxxxMapType device);
static eConsoleCommandResult w25qxxxDebugHandleInfo(uint32_t transport, eW25qxxxMapType device);
static eConsoleCommandResult w25qxxxDebugHandleRead(uint32_t transport, eW25qxxxMapType device, int argc, char *argv[]);
static eConsoleCommandResult w25qxxxDebugHandleWrite(uint32_t transport, eW25qxxxMapType device, int argc, char *argv[]);
static eConsoleCommandResult w25qxxxDebugHandleErase(uint32_t transport, eW25qxxxMapType device, int argc, char *argv[]);
static eConsoleCommandResult w25qxxxDebugReplyHelp(uint32_t transport, int argc, char *argv[]);
static eConsoleCommandResult w25qxxxDebugConsoleHandler(uint32_t transport, int argc, char *argv[]);

static const stConsoleCommand gW25qxxxConsoleCommand = {
    .commandName = "w25qxxx",
    .helpText = "w25qxxx <list|init|jedec|status|info|read|write|erase|help> ...",
    .ownerTag = "w25qxxx",
    .handler = w25qxxxDebugConsoleHandler,
};

static bool w25qxxxDebugParseDevice(const char *name, eW25qxxxMapType *device)
{
    if ((name == NULL) || (device == NULL)) {
        return false;
    }

    if ((strcmp(name, "dev0") == 0) || (strcmp(name, "0") == 0)) {
        *device = W25QXXX_DEV0;
        return true;
    }

    if ((strcmp(name, "dev1") == 0) || (strcmp(name, "1") == 0)) {
        *device = W25QXXX_DEV1;
        return true;
    }

    return false;
}

static bool w25qxxxDebugParseHexNibble(char value, uint8_t *nibble)
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

static bool w25qxxxDebugParseUint32(const char *text, uint32_t *value)
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
            if (!w25qxxxDebugParseHexNibble(*lCursor, &lNibble)) {
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

static bool w25qxxxDebugParseHexByte(const char *text, uint8_t *value)
{
    uint8_t lTemp = 0U;
    uint32_t lValue = 0U;

    if (!w25qxxxDebugParseUint32(text, &lValue) || (lValue > 0xFFU)) {
        return false;
    }

    lTemp = (uint8_t)lValue;
    *value = lTemp;
    return true;
}

static const char *w25qxxxDebugGetDeviceName(eW25qxxxMapType device)
{
    switch (device) {
        case W25QXXX_DEV0:
            return "dev0";
        case W25QXXX_DEV1:
            return "dev1";
        default:
            return NULL;
    }
}

static const char *w25qxxxDebugGetStatusText(eW25qxxxStatus status)
{
    switch (status) {
        case W25QXXX_STATUS_OK:
            return "ok";
        case W25QXXX_STATUS_INVALID_PARAM:
            return "invalid_param";
        case W25QXXX_STATUS_NOT_READY:
            return "not_ready";
        case W25QXXX_STATUS_BUSY:
            return "busy";
        case W25QXXX_STATUS_TIMEOUT:
            return "timeout";
        case W25QXXX_STATUS_NACK:
            return "nack";
        case W25QXXX_STATUS_UNSUPPORTED:
            return "unsupported";
        case W25QXXX_STATUS_DEVICE_ID_MISMATCH:
            return "id_notmatch";
        case W25QXXX_STATUS_OUT_OF_RANGE:
            return "out_of_range";
        default:
            return "error";
    }
}

static eConsoleCommandResult w25qxxxDebugBuildHexReply(const uint8_t *buffer, uint16_t length, char *output, uint32_t capacity)
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

static eConsoleCommandResult w25qxxxDebugReplyDeviceList(uint32_t transport)
{
    stW25qxxxCfg lCfg;
    uint32_t lIndex;

    for (lIndex = 0U; lIndex < (uint32_t)W25QXXX_DEV_MAX; lIndex++) {
        w25qxxxPortGetDefCfg((eW25qxxxMapType)lIndex, &lCfg);
        if (consoleReply(transport,
            "%s bus=%u ready=%s\n",
            w25qxxxDebugGetDeviceName((eW25qxxxMapType)lIndex),
            (unsigned int)lCfg.spi,
            w25qxxxIsReady((eW25qxxxMapType)lIndex) ? "yes" : "no") <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }
    }

    if (consoleReply(transport, "OK") <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult w25qxxxDebugHandleInit(uint32_t transport, eW25qxxxMapType device)
{
    eW25qxxxStatus lStatus = w25qxxxInit(device);

    if (consoleReply(transport, "%s init=%s\nOK", w25qxxxDebugGetDeviceName(device), w25qxxxDebugGetStatusText(lStatus)) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return (lStatus == W25QXXX_STATUS_OK) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
}

static eConsoleCommandResult w25qxxxDebugHandleJedec(uint32_t transport, eW25qxxxMapType device)
{
    uint8_t lManufacturerId = 0U;
    uint8_t lMemoryType = 0U;
    uint8_t lCapacityId = 0U;
    eW25qxxxStatus lStatus = w25qxxxReadJedecId(device, &lManufacturerId, &lMemoryType, &lCapacityId);

    if (lStatus != W25QXXX_STATUS_OK) {
        if (consoleReply(transport, "%s jedec=%s", w25qxxxDebugGetDeviceName(device), w25qxxxDebugGetStatusText(lStatus)) <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (consoleReply(transport,
        "%s jedec=%02X %02X %02X\nOK",
        w25qxxxDebugGetDeviceName(device),
        (unsigned int)lManufacturerId,
        (unsigned int)lMemoryType,
        (unsigned int)lCapacityId) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult w25qxxxDebugHandleStatus(uint32_t transport, eW25qxxxMapType device)
{
    uint8_t lStatusValue = 0U;
    eW25qxxxStatus lStatus = w25qxxxReadStatus1(device, &lStatusValue);

    if (lStatus != W25QXXX_STATUS_OK) {
        if (consoleReply(transport, "%s status=%s", w25qxxxDebugGetDeviceName(device), w25qxxxDebugGetStatusText(lStatus)) <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (consoleReply(transport, "%s sr1=%02X\nOK", w25qxxxDebugGetDeviceName(device), (unsigned int)lStatusValue) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult w25qxxxDebugHandleInfo(uint32_t transport, eW25qxxxMapType device)
{
    const stW25qxxxInfo *lInfo = w25qxxxGetInfo(device);

    if (lInfo == NULL) {
        if (consoleReply(transport, "%s info=not_ready", w25qxxxDebugGetDeviceName(device)) <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (consoleReply(transport,
        "%s size=%lu page=%u sector=%lu block=%lu addrw=%u\nOK",
        w25qxxxDebugGetDeviceName(device),
        (unsigned long)lInfo->totalSizeBytes,
        (unsigned int)lInfo->pageSizeBytes,
        (unsigned long)lInfo->sectorSizeBytes,
        (unsigned long)lInfo->blockSizeBytes,
        (unsigned int)lInfo->addressWidth) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult w25qxxxDebugHandleRead(uint32_t transport, eW25qxxxMapType device, int argc, char *argv[])
{
    uint32_t lAddress = 0U;
    uint32_t lLength = 0U;
    uint8_t lBuffer[W25QXXX_DEBUG_MAX_DATA_LENGTH];
    char lReply[W25QXXX_DEBUG_MAX_REPLY_LENGTH];
    eW25qxxxStatus lStatus;

    if ((argc != 5) || !w25qxxxDebugParseUint32(argv[3], &lAddress) || !w25qxxxDebugParseUint32(argv[4], &lLength) ||
        (lLength == 0U) || (lLength > W25QXXX_DEBUG_MAX_DATA_LENGTH)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    lStatus = w25qxxxRead(device, lAddress, lBuffer, lLength);
    if (lStatus != W25QXXX_STATUS_OK) {
        if (consoleReply(transport, "%s read=%s", w25qxxxDebugGetDeviceName(device), w25qxxxDebugGetStatusText(lStatus)) <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (w25qxxxDebugBuildHexReply(lBuffer, (uint16_t)lLength, lReply, sizeof(lReply)) != CONSOLE_COMMAND_RESULT_OK) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (consoleReply(transport, "%s addr=%08lX data=%s\nOK", w25qxxxDebugGetDeviceName(device), (unsigned long)lAddress, lReply) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult w25qxxxDebugHandleWrite(uint32_t transport, eW25qxxxMapType device, int argc, char *argv[])
{
    uint32_t lAddress = 0U;
    uint8_t lBuffer[W25QXXX_DEBUG_MAX_DATA_LENGTH];
    uint16_t lLength = 0U;
    eW25qxxxStatus lStatus;
    int lIndex;

    if ((argc < 5) || !w25qxxxDebugParseUint32(argv[3], &lAddress)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    for (lIndex = 4; lIndex < argc; lIndex++) {
        if (lLength >= W25QXXX_DEBUG_MAX_DATA_LENGTH) {
            return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
        }

        if (!w25qxxxDebugParseHexByte(argv[lIndex], &lBuffer[lLength])) {
            return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
        }

        lLength++;
    }

    lStatus = w25qxxxWrite(device, lAddress, lBuffer, lLength);
    if (consoleReply(transport,
        "%s addr=%08lX write=%u status=%s\nOK",
        w25qxxxDebugGetDeviceName(device),
        (unsigned long)lAddress,
        (unsigned int)lLength,
        w25qxxxDebugGetStatusText(lStatus)) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return (lStatus == W25QXXX_STATUS_OK) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
}

static eConsoleCommandResult w25qxxxDebugHandleErase(uint32_t transport, eW25qxxxMapType device, int argc, char *argv[])
{
    uint32_t lAddress = 0U;
    eW25qxxxStatus lStatus;

    if ((argc != 5) || (strcmp(argv[3], "sector") != 0) || !w25qxxxDebugParseUint32(argv[4], &lAddress)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    lStatus = w25qxxxEraseSector(device, lAddress);
    if (consoleReply(transport,
        "%s erase_sector=%08lX status=%s\nOK",
        w25qxxxDebugGetDeviceName(device),
        (unsigned long)lAddress,
        w25qxxxDebugGetStatusText(lStatus)) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return (lStatus == W25QXXX_STATUS_OK) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
}

static eConsoleCommandResult w25qxxxDebugReplyHelp(uint32_t transport, int argc, char *argv[])
{
    if (argc == 2) {
        if (consoleReply(transport,
            "w25qxxx <list|init|jedec|status|info|read|write|erase|help> ...\n"
            "  list\n"
            "  init <dev0|dev1>\n"
            "  jedec <dev0|dev1>\n"
            "  status <dev0|dev1>\n"
            "  info <dev0|dev1>\n"
            "  help <read|write|erase>\n"
            "    example: w25qxxx help read\n"
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
            "w25qxxx read <dev0|dev1> <addr> <len>\n"
            "  addr supports decimal or 0xhex, len=1..16\n"
            "  example: w25qxxx read dev1 0x1234 4\n"
            "OK") <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_OK;
    }

    if (strcmp(argv[2], "write") == 0) {
        if (consoleReply(transport,
            "w25qxxx write <dev0|dev1> <addr> <b0> [b1 ... b15]\n"
            "  each data byte supports decimal or 0xhex, max 16 bytes\n"
            "  example: w25qxxx write dev1 0x1234 00\n"
            "  example: w25qxxx write dev1 0x1234 0x48 0x65 0x6C 0x6C 0x6F\n"
            "OK") <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_OK;
    }

    if (strcmp(argv[2], "erase") == 0) {
        if (consoleReply(transport,
            "w25qxxx erase <dev0|dev1> sector <addr>\n"
            "  addr must be sector aligned\n"
            "  example: w25qxxx erase dev1 sector 0x1000\n"
            "OK") <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_OK;
    }

    return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
}

static eConsoleCommandResult w25qxxxDebugConsoleHandler(uint32_t transport, int argc, char *argv[])
{
    eW25qxxxMapType lDevice;

    if ((argc < 2) || (argv == NULL) || (argv[1] == NULL)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if ((strcmp(argv[1], "list") == 0) || (strcmp(argv[1], "stat") == 0)) {
        if (argc != 2) {
            return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
        }

        return w25qxxxDebugReplyDeviceList(transport);
    }

    if (strcmp(argv[1], "help") == 0) {
        return w25qxxxDebugReplyHelp(transport, argc, argv);
    }

    if ((argc < 3) || !w25qxxxDebugParseDevice(argv[2], &lDevice)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (strcmp(argv[1], "init") == 0) {
        return w25qxxxDebugHandleInit(transport, lDevice);
    }

    if (strcmp(argv[1], "jedec") == 0) {
        return w25qxxxDebugHandleJedec(transport, lDevice);
    }

    if (strcmp(argv[1], "status") == 0) {
        return w25qxxxDebugHandleStatus(transport, lDevice);
    }

    if (strcmp(argv[1], "info") == 0) {
        return w25qxxxDebugHandleInfo(transport, lDevice);
    }

    if (strcmp(argv[1], "read") == 0) {
        return w25qxxxDebugHandleRead(transport, lDevice, argc, argv);
    }

    if (strcmp(argv[1], "write") == 0) {
        return w25qxxxDebugHandleWrite(transport, lDevice, argc, argv);
    }

    if (strcmp(argv[1], "erase") == 0) {
        return w25qxxxDebugHandleErase(transport, lDevice, argc, argv);
    }

    return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
}
#endif

bool w25qxxxDebugConsoleRegister(void)
{
#if (W25QXXX_CONSOLE_SUPPORT == 1)
    return consoleRegisterCommand(&gW25qxxxConsoleCommand);
#else
    return true;
#endif
}

/**************************End of file********************************/
