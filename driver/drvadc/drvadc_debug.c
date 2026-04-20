/***********************************************************************************
* @file     : drvadc_debug.c
* @brief    : DrvAdc debug and console command implementation.
* @details  : This file hosts optional console bindings for ADC debug operations.
* @author   : GitHub Copilot
* @date     : 2026-04-13
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "drvadc_debug.h"

#include "drvadc.h"
#include "drvadc_port.h"

#if (DRVADC_CONSOLE_SUPPORT == 1)
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "../../service/log/console.h"

static const char *drvAdcDebugGetChannelName(uint8_t adc);
static const char *drvAdcDebugGetStatusText(eDrvStatus status);
static eDrvStatus drvAdcDebugRefreshChannel(uint8_t adc);
static eConsoleCommandResult drvAdcDebugReplyAllChannels(uint32_t transport);
static eConsoleCommandResult drvAdcDebugReplyHelp(uint32_t transport, int argc, char *argv[]);
static eConsoleCommandResult drvAdcDebugConsoleHandler(uint32_t transport, int argc, char *argv[]);

static const stConsoleCommand gDrvAdcConsoleCommand = {
    .commandName = "adc",
    .helpText = "adc [help]",
    .ownerTag = "drvAdc",
    .handler = drvAdcDebugConsoleHandler,
};

static const char *drvAdcDebugGetChannelName(uint8_t adc)
{
    switch ((eDrvAdcPortMap)adc) {
        case DRVADC_BAT:
            return "bat";
        case DRVADC_FORCE:
            return "force";
        case DRVADC_DC:
            return "dc";
        case DRVADC_5V0:
            return "5v0";
        case DRVADC_3V3:
            return "3v3";
        default:
            return "unknown";
    }
}

static const char *drvAdcDebugGetStatusText(eDrvStatus status)
{
    switch (status) {
        case DRV_STATUS_OK:
            return "ok";
        case DRV_STATUS_INVALID_PARAM:
            return "invalid";
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

static eDrvStatus drvAdcDebugRefreshChannel(uint8_t adc)
{
    eDrvStatus lStatus;
    uint16_t lRawValue = 0U;

    lStatus = drvAdcInit(adc);
    if ((lStatus != DRV_STATUS_OK) && (lStatus != DRV_STATUS_NOT_READY)) {
        return lStatus;
    }

    if (lStatus == DRV_STATUS_NOT_READY) {
        return lStatus;
    }

    return drvAdcReadRaw(adc, &lRawValue);
}

static eConsoleCommandResult drvAdcDebugReplyAllChannels(uint32_t transport)
{
    stDrvAdcData *lDataCache = drvAdcGetPlatformData();
    uint8_t lAdc;

    if (lDataCache == NULL) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    for (lAdc = 0U; lAdc < DRVADC_MAX; lAdc++) {
        eDrvStatus lStatus = drvAdcDebugRefreshChannel(lAdc);

        if (consoleReply(transport,
            "%s status=%s raw=%u mv=%u rawf=%u mvf=%u",
            drvAdcDebugGetChannelName(lAdc),
            drvAdcDebugGetStatusText(lStatus),
            (unsigned int)lDataCache[lAdc].raw,
            (unsigned int)lDataCache[lAdc].mv,
            (unsigned int)lDataCache[lAdc].rawFiltered,
            (unsigned int)lDataCache[lAdc].mvFiltered) <= 0) {
            return CONSOLE_COMMAND_RESULT_ERROR;
        }
    }

    if (consoleReply(transport, "OK") <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult drvAdcDebugReplyHelp(uint32_t transport, int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (consoleReply(transport,
        "adc\n"
        "  sample all adc channels once and print raw/mv/rawf/mvf\n"
        "adc help\n"
        "  show this help\n"
        "OK") <= 0) {
        return CONSOLE_COMMAND_RESULT_ERROR;
    }

    return CONSOLE_COMMAND_RESULT_OK;
}

static eConsoleCommandResult drvAdcDebugConsoleHandler(uint32_t transport, int argc, char *argv[])
{
    if ((argc < 1) || (argv == NULL) || (argv[0] == NULL)) {
        return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
    }

    if (argc == 1) {
        return drvAdcDebugReplyAllChannels(transport);
    }

    if ((argc == 2) && (argv[1] != NULL) && (strcmp(argv[1], "help") == 0)) {
        return drvAdcDebugReplyHelp(transport, argc, argv);
    }

    return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
}
#endif

bool drvAdcDebugConsoleRegister(void)
{
#if (DRVADC_CONSOLE_SUPPORT == 1)
    return consoleRegisterCommand(&gDrvAdcConsoleCommand);
#else
    return true;
#endif
}

/**************************End of file********************************/
