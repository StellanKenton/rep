/***********************************************************************************
* @file     : mpu6050_debug.c
* @brief    : MPU6050 debug and console command implementation.
* @details  : This file hosts optional console bindings for MPU6050 operations.
**********************************************************************************/
#include "mpu6050_debug.h"

#include "mpu6050.h"
#include "mpu6050_port.h"

#if (MPU6050_CONSOLE_SUPPORT == 1)
#include <stdint.h>
#include <string.h>

#include "console.h"

static bool mpu6050DebugParseDevice(const char *name, eMPU6050MapType *device);
static bool mpu6050DebugParseHexNibble(char value, uint8_t *nibble);
static bool mpu6050DebugParseHexByte(const char *text, uint8_t *value);
static const char *mpu6050DebugGetDeviceName(eMPU6050MapType device);
static const char *mpu6050DebugGetStatusText(eDrvStatus status);
static const char *mpu6050DebugGetBindTypeText(eMpu6050TransportType type);
static eConsoleCommandResult mpu6050DebugReplyDeviceList(uint32_t transport);
static eConsoleCommandResult mpu6050DebugHandleInit(uint32_t transport, eMPU6050MapType device);
static eConsoleCommandResult mpu6050DebugHandleId(uint32_t transport, eMPU6050MapType device);
static eConsoleCommandResult mpu6050DebugHandleRegGet(uint32_t transport, eMPU6050MapType device, int argc, char *argv[]);
static eConsoleCommandResult mpu6050DebugHandleRegSet(uint32_t transport, eMPU6050MapType device, int argc, char *argv[]);
static eConsoleCommandResult mpu6050DebugHandleRaw(uint32_t transport, eMPU6050MapType device);
static eConsoleCommandResult mpu6050DebugHandleTemp(uint32_t transport, eMPU6050MapType device);
static eConsoleCommandResult mpu6050DebugReplyHelp(uint32_t transport, int argc, char *argv[]);
static eConsoleCommandResult mpu6050DebugConsoleHandler(uint32_t transport, int argc, char *argv[]);

static const stConsoleCommand gMpu6050ConsoleCommand = {
    .commandName = "mpu6050",
    .helpText = "mpu6050 <list|init|id|regget|regset|raw|temp|help> ...",
    .ownerTag = "mpu6050",
    .handler = mpu6050DebugConsoleHandler,
};

static bool mpu6050DebugParseDevice(const char *name, eMPU6050MapType *device)
{
    if ((name == NULL) || (device == NULL)) {
        return false;
    }

    if ((strcmp(name, "dev0") == 0) || (strcmp(name, "0") == 0)) {
        *device = MPU6050_DEV0;
        return true;
    }

    if ((strcmp(name, "dev1") == 0) || (strcmp(name, "1") == 0)) {
        *device = MPU6050_DEV1;
        return true;
    }

    return false;
}

static bool mpu6050DebugParseHexNibble(char value, uint8_t *nibble)
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

static bool mpu6050DebugParseHexByte(const char *text, uint8_t *value)
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
        if (!mpu6050DebugParseHexNibble(lCursor[0], &lLow)) {
            return false;
        }

        *value = lLow;
        return true;
    }

    if ((lCursor[2] != '\0') ||
        !mpu6050DebugParseHexNibble(lCursor[0], &lHigh) ||
        !mpu6050DebugParseHexNibble(lCursor[1], &lLow)) {
        return false;
    }

    *value = (uint8_t)((lHigh << 4) | lLow);
    return true;
}

static const char *mpu6050DebugGetDeviceName(eMPU6050MapType device)
{
    switch (device) {
        case MPU6050_DEV0:
            return "dev0";
        case MPU6050_DEV1:
            return "dev1";
        default:
            return NULL;
    }
}

static const char *mpu6050DebugGetStatusText(eDrvStatus status)
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
        case DRV_STATUS_ID_NOTMATCH:
            return "id_notmatch";
        default:
            return "error";
    }
}

static const char *mpu6050DebugGetBindTypeText(eMpu6050TransportType type)
{
    switch (type) {
        case MPU6050_TRANSPORT_TYPE_SOFTWARE:
            return "soft_iic";
        case MPU6050_TRANSPORT_TYPE_HARDWARE:
            return "hard_iic";
        default:
            return "none";
    }
}

static eConsoleCommandResult mpu6050DebugReplyDeviceList(uint32_t transport)
{
    stMpu6050Cfg lCfg;
    stMpu6050PortAssembleCfg lAssembleCfg;
    uint32_t lIndex;

    for (lIndex = 0U; lIndex < (uint32_t)MPU6050_DEV_MAX; lIndex++) {
        mpu6050GetDefCfg((eMPU6050MapType)lIndex, &lCfg);
        mpu6050PortGetAssembleCfg((eMPU6050MapType)lIndex, &lAssembleCfg);
        if (consoleReply(transport,
            "%s bind=%s bus=%u addr=%02X ready=%s\n",
            mpu6050DebugGetDeviceName((eMPU6050MapType)lIndex),
            mpu6050DebugGetBindTypeText(lAssembleCfg.transportType),
            (unsigned int)lAssembleCfg.linkId,
            (unsigned int)lCfg.address,
            mpu6050IsReady((eMPU6050MapType)lIndex) ? "yes" : "no") <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }
    }

    if (consoleReply(transport, "OK") <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult mpu6050DebugHandleInit(uint32_t transport, eMPU6050MapType device)
{
    eDrvStatus lStatus = mpu6050Init(device);

    if (consoleReply(transport, "%s init=%s\nOK", mpu6050DebugGetDeviceName(device), mpu6050DebugGetStatusText(lStatus)) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return (lStatus == DRV_STATUS_OK) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
}

static eConsoleCommandResult mpu6050DebugHandleId(uint32_t transport, eMPU6050MapType device)
{
    uint8_t lDevId = 0U;
    eDrvStatus lStatus = mpu6050ReadId(device, &lDevId);

    if (lStatus != DRV_STATUS_OK) {
        if (consoleReply(transport, "%s id=%s", mpu6050DebugGetDeviceName(device), mpu6050DebugGetStatusText(lStatus)) <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (consoleReply(transport, "%s whoami=%02X\nOK", mpu6050DebugGetDeviceName(device), (unsigned int)lDevId) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult mpu6050DebugHandleRegGet(uint32_t transport, eMPU6050MapType device, int argc, char *argv[])
{
    uint8_t lReg = 0U;
    uint8_t lValue = 0U;
    eDrvStatus lStatus;

    if ((argc != 4) || !mpu6050DebugParseHexByte(argv[3], &lReg)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    lStatus = mpu6050ReadReg(device, lReg, &lValue);
    if (lStatus != DRV_STATUS_OK) {
        if (consoleReply(transport, "%s reg=%02X status=%s", mpu6050DebugGetDeviceName(device), (unsigned int)lReg, mpu6050DebugGetStatusText(lStatus)) <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (consoleReply(transport, "%s reg=%02X value=%02X\nOK", mpu6050DebugGetDeviceName(device), (unsigned int)lReg, (unsigned int)lValue) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult mpu6050DebugHandleRegSet(uint32_t transport, eMPU6050MapType device, int argc, char *argv[])
{
    uint8_t lReg = 0U;
    uint8_t lValue = 0U;
    eDrvStatus lStatus;

    if ((argc != 5) || !mpu6050DebugParseHexByte(argv[3], &lReg) || !mpu6050DebugParseHexByte(argv[4], &lValue)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    lStatus = mpu6050WriteReg(device, lReg, lValue);
    if (consoleReply(transport,
        "%s reg=%02X write=%02X status=%s\nOK",
        mpu6050DebugGetDeviceName(device),
        (unsigned int)lReg,
        (unsigned int)lValue,
        mpu6050DebugGetStatusText(lStatus)) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return (lStatus == DRV_STATUS_OK) ? CONSOLE_COMMAND_RESULT_OK : CONSOLE_COMMAND_RESULT_ERROR;
}

static eConsoleCommandResult mpu6050DebugHandleRaw(uint32_t transport, eMPU6050MapType device)
{
    stMpu6050RawSample lSample;
    eDrvStatus lStatus = mpu6050ReadRaw(device, &lSample);

    if (lStatus != DRV_STATUS_OK) {
        if (consoleReply(transport, "%s raw=%s", mpu6050DebugGetDeviceName(device), mpu6050DebugGetStatusText(lStatus)) <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (consoleReply(transport,
        "%s ax=%d ay=%d az=%d temp=%d gx=%d gy=%d gz=%d\nOK",
        mpu6050DebugGetDeviceName(device),
        (int)lSample.accelX,
        (int)lSample.accelY,
        (int)lSample.accelZ,
        (int)lSample.temperature,
        (int)lSample.gyroX,
        (int)lSample.gyroY,
        (int)lSample.gyroZ) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult mpu6050DebugHandleTemp(uint32_t transport, eMPU6050MapType device)
{
    int32_t lTempCdC = 0;
    eDrvStatus lStatus = mpu6050ReadTempCdC(device, &lTempCdC);

    if (lStatus != DRV_STATUS_OK) {
        if (consoleReply(transport, "%s temp=%s", mpu6050DebugGetDeviceName(device), mpu6050DebugGetStatusText(lStatus)) <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    if (consoleReply(transport, "%s temp_cdc=%ld\nOK", mpu6050DebugGetDeviceName(device), (long)lTempCdC) <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult mpu6050DebugConsoleHandler(uint32_t transport, int argc, char *argv[])
{
    eMPU6050MapType lDevice;

    if ((argc < 2) || (argv == NULL) || (argv[1] == NULL)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if ((strcmp(argv[1], "list") == 0) || (strcmp(argv[1], "stat") == 0)) {
        if (argc != 2) {
            return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
        }

        return mpu6050DebugReplyDeviceList(transport);
    }

    if (strcmp(argv[1], "help") == 0) {
        return mpu6050DebugReplyHelp(transport, argc, argv);
    }

    if ((argc < 3) || (argv[2] == NULL) || !mpu6050DebugParseDevice(argv[2], &lDevice)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (strcmp(argv[1], "init") == 0) {
        return mpu6050DebugHandleInit(transport, lDevice);
    }

    if (strcmp(argv[1], "id") == 0) {
        return mpu6050DebugHandleId(transport, lDevice);
    }

    if ((strcmp(argv[1], "regget") == 0) || (strcmp(argv[1], "rg") == 0)) {
        return mpu6050DebugHandleRegGet(transport, lDevice, argc, argv);
    }

    if ((strcmp(argv[1], "regset") == 0) || (strcmp(argv[1], "rs") == 0)) {
        return mpu6050DebugHandleRegSet(transport, lDevice, argc, argv);
    }

    if (strcmp(argv[1], "raw") == 0) {
        return mpu6050DebugHandleRaw(transport, lDevice);
    }

    if (strcmp(argv[1], "temp") == 0) {
        return mpu6050DebugHandleTemp(transport, lDevice);
    }

    return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
}

static eConsoleCommandResult mpu6050DebugReplyHelp(uint32_t transport, int argc, char *argv[])
{
    if (argc == 2) {
        if (consoleReply(transport,
            "mpu6050 <list|init|id|regget|regset|raw|temp|help> ...\n"
            "  list\n"
            "  init <dev0|dev1>\n"
            "  id <dev0|dev1>\n"
            "  raw <dev0|dev1>\n"
            "  temp <dev0|dev1>\n"
            "  help <regget|regset>\n"
            "    example: mpu6050 help regget\n"
            "OK") <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_OK;
    }

    if (argc != 3) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if ((strcmp(argv[2], "regget") == 0) || (strcmp(argv[2], "rg") == 0)) {
        if (consoleReply(transport,
            "mpu6050 regget <dev0|dev1> <reg>\n"
            "  alias: rg\n"
            "  reg supports hex byte\n"
            "  example: mpu6050 regget dev0 0x75\n"
            "OK") <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_OK;
    }

    if ((strcmp(argv[2], "regset") == 0) || (strcmp(argv[2], "rs") == 0)) {
        if (consoleReply(transport,
            "mpu6050 regset <dev0|dev1> <reg> <value>\n"
            "  alias: rs\n"
            "  reg and value support hex byte\n"
            "  example: mpu6050 regset dev0 0x6B 0x00\n"
            "OK") <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }

        return CONSOLE_COMMAND_RESULT_OK;
    }

    return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
}
#endif

bool mpu6050DebugConsoleRegister(void)
{
#if (MPU6050_CONSOLE_SUPPORT == 1)
    return consoleRegisterCommand(&gMpu6050ConsoleCommand);
#else
    return true;
#endif
}

/**************************End of file********************************/
