#include "tm1651.h"

#include <stddef.h>

static stTm1651Device gTm1651Devices[TM1651_DEV_MAX];
static bool gTm1651DefCfgDone[TM1651_DEV_MAX] = {false};

__attribute__((weak)) void tm1651LoadPlatformDefaultCfg(eTm1651MapType device, stTm1651Cfg *cfg)
{
    (void)device;

    if (cfg == NULL) {
        return;
    }

    cfg->linkId = 0U;
    cfg->brightness = 0U;
    cfg->digitCount = TM1651_DEFAULT_DIGIT_COUNT;
    cfg->isDisplayOn = false;
}

__attribute__((weak)) const stTm1651IicInterface *tm1651GetPlatformIicInterface(const stTm1651Cfg *cfg)
{
    (void)cfg;
    return NULL;
}

__attribute__((weak)) bool tm1651PlatformIsValidCfg(const stTm1651Cfg *cfg)
{
    (void)cfg;
    return false;
}

static bool tm1651IsValidDevMap(eTm1651MapType device);
static stTm1651Device *tm1651GetDevCtx(eTm1651MapType device);
static void tm1651LoadDefCfg(eTm1651MapType device, stTm1651Cfg *cfg);
static bool tm1651IsValidCfg(const stTm1651Cfg *cfg);
static bool tm1651IsReadyXfer(const stTm1651Device *device);
static const stTm1651IicInterface *tm1651GetIicIf(const stTm1651Device *device);
static uint8_t tm1651EncodeSymbol(uint8_t symbol);
static void tm1651FillBlank(uint8_t *segData, uint8_t length);
static eTm1651Status tm1651WriteFrame(const stTm1651Device *device, const uint8_t *buffer, uint8_t length);
static eTm1651Status tm1651ApplyDisplayCtrl(const stTm1651Device *device);
static eTm1651Status tm1651RefreshDisplay(stTm1651Device *device);

eTm1651Status tm1651GetDefCfg(eTm1651MapType device, stTm1651Cfg *cfg)
{
    if ((cfg == NULL) || !tm1651IsValidDevMap(device)) {
        return TM1651_STATUS_INVALID_PARAM;
    }

    tm1651LoadDefCfg(device, cfg);
    return TM1651_STATUS_OK;
}

eTm1651Status tm1651GetCfg(eTm1651MapType device, stTm1651Cfg *cfg)
{
    stTm1651Device *lDeviceCtx;

    if (cfg == NULL) {
        return TM1651_STATUS_INVALID_PARAM;
    }

    lDeviceCtx = tm1651GetDevCtx(device);
    if (lDeviceCtx == NULL) {
        return TM1651_STATUS_INVALID_PARAM;
    }

    *cfg = lDeviceCtx->cfg;
    return TM1651_STATUS_OK;
}

eTm1651Status tm1651SetCfg(eTm1651MapType device, const stTm1651Cfg *cfg)
{
    stTm1651Device *lDeviceCtx;

    if ((cfg == NULL) || !tm1651IsValidCfg(cfg)) {
        return TM1651_STATUS_INVALID_PARAM;
    }

    lDeviceCtx = tm1651GetDevCtx(device);
    if (lDeviceCtx == NULL) {
        return TM1651_STATUS_INVALID_PARAM;
    }

    lDeviceCtx->cfg = *cfg;
    lDeviceCtx->isReady = false;
    gTm1651DefCfgDone[device] = true;
    return TM1651_STATUS_OK;
}

eTm1651Status tm1651Init(eTm1651MapType device)
{
    const stTm1651IicInterface *lIicIf;
    stTm1651Device *lDeviceCtx;
    eTm1651Status lStatus;

    lDeviceCtx = tm1651GetDevCtx(device);
    if ((lDeviceCtx == NULL) || !tm1651IsValidCfg(&lDeviceCtx->cfg)) {
        return TM1651_STATUS_INVALID_PARAM;
    }

    if (tm1651GetIicIf(lDeviceCtx) == NULL) {
        return tm1651IsValidCfg(&lDeviceCtx->cfg) ?
               TM1651_STATUS_NOT_READY :
               TM1651_STATUS_INVALID_PARAM;
    }

    lIicIf = tm1651GetIicIf(lDeviceCtx);
    lStatus = lIicIf->init((uint8_t)lDeviceCtx->cfg.linkId);
    if (lStatus != TM1651_STATUS_OK) {
        return lStatus;
    }

    tm1651FillBlank(lDeviceCtx->segData, TM1651_DIGIT_MAX);
    lDeviceCtx->isReady = false;
    lStatus = tm1651RefreshDisplay(lDeviceCtx);
    if (lStatus != TM1651_STATUS_OK) {
        return lStatus;
    }

    lDeviceCtx->isReady = true;
    return TM1651_STATUS_OK;
}

bool tm1651IsReady(eTm1651MapType device)
{
    return tm1651IsReadyXfer(tm1651GetDevCtx(device));
}

eTm1651Status tm1651SetBrightness(eTm1651MapType device, uint8_t brightness)
{
    stTm1651Device *lDeviceCtx;

    if (brightness > 7U) {
        return TM1651_STATUS_INVALID_PARAM;
    }

    lDeviceCtx = tm1651GetDevCtx(device);
    if (!tm1651IsReadyXfer(lDeviceCtx)) {
        return TM1651_STATUS_NOT_READY;
    }

    lDeviceCtx->cfg.brightness = brightness;
    return tm1651ApplyDisplayCtrl(lDeviceCtx);
}

eTm1651Status tm1651SetDisplayOn(eTm1651MapType device, bool isDisplayOn)
{
    stTm1651Device *lDeviceCtx;

    lDeviceCtx = tm1651GetDevCtx(device);
    if (!tm1651IsReadyXfer(lDeviceCtx)) {
        return TM1651_STATUS_NOT_READY;
    }

    lDeviceCtx->cfg.isDisplayOn = isDisplayOn;
    return tm1651ApplyDisplayCtrl(lDeviceCtx);
}

eTm1651Status tm1651DisplayRaw(eTm1651MapType device, const uint8_t *segData, uint8_t length)
{
    stTm1651Device *lDeviceCtx;
    uint8_t lIndex;

    lDeviceCtx = tm1651GetDevCtx(device);
    if ((segData == NULL) || (length == 0U) || (length > TM1651_DIGIT_MAX) || !tm1651IsReadyXfer(lDeviceCtx)) {
        return (segData == NULL) || (length == 0U) || (length > TM1651_DIGIT_MAX) ?
               TM1651_STATUS_INVALID_PARAM :
               TM1651_STATUS_NOT_READY;
    }

    tm1651FillBlank(lDeviceCtx->segData, TM1651_DIGIT_MAX);
    for (lIndex = 0U; lIndex < length; ++lIndex) {
        lDeviceCtx->segData[lIndex] = segData[lIndex];
    }

    return tm1651RefreshDisplay(lDeviceCtx);
}

eTm1651Status tm1651DisplayDigits(eTm1651MapType device, uint8_t dig1, uint8_t dig2, uint8_t dig3, uint8_t dig4)
{
    stTm1651Device *lDeviceCtx;
    uint8_t lSymbols[TM1651_DIGIT_MAX] = {dig1, dig2, dig3, dig4};
    uint8_t lIndex;
    uint8_t lDigitCount;

    lDeviceCtx = tm1651GetDevCtx(device);
    if (!tm1651IsReadyXfer(lDeviceCtx)) {
        return TM1651_STATUS_NOT_READY;
    }

    lDigitCount = lDeviceCtx->cfg.digitCount;
    tm1651FillBlank(lDeviceCtx->segData, TM1651_DIGIT_MAX);
    for (lIndex = 0U; (lIndex < lDigitCount) && (lIndex < TM1651_DIGIT_MAX); ++lIndex) {
        lDeviceCtx->segData[lIndex] = tm1651EncodeSymbol(lSymbols[lIndex]);
    }

    return tm1651RefreshDisplay(lDeviceCtx);
}

eTm1651Status tm1651ClearDisplay(eTm1651MapType device)
{
    return tm1651DisplayDigits(device,
                               TM1651_SYMBOL_BLANK,
                               TM1651_SYMBOL_BLANK,
                               TM1651_SYMBOL_BLANK,
                               TM1651_SYMBOL_BLANK);
}

eTm1651Status tm1651ShowNone(eTm1651MapType device)
{
    return tm1651DisplayDigits(device,
                               TM1651_SYMBOL_DASH,
                               TM1651_SYMBOL_DASH,
                               TM1651_SYMBOL_DASH,
                               TM1651_SYMBOL_0);
}

eTm1651Status tm1651ShowNumber3(eTm1651MapType device, uint16_t value)
{
    if (value > 999U) {
        return TM1651_STATUS_INVALID_PARAM;
    }

    return tm1651DisplayDigits(device,
                               (uint8_t)((value / 100U) % 10U),
                               (uint8_t)((value / 10U) % 10U),
                               (uint8_t)(value % 10U),
                               TM1651_SYMBOL_0);
}

eTm1651Status tm1651ShowError(eTm1651MapType device, uint16_t value)
{
    if (value > 99U) {
        return TM1651_STATUS_INVALID_PARAM;
    }

    return tm1651DisplayDigits(device,
                               TM1651_SYMBOL_E,
                               (uint8_t)((value / 10U) % 10U),
                               (uint8_t)(value % 10U),
                               TM1651_SYMBOL_0);
}

static bool tm1651IsValidDevMap(eTm1651MapType device)
{
    return ((uint32_t)device < (uint32_t)TM1651_DEV_MAX);
}

static stTm1651Device *tm1651GetDevCtx(eTm1651MapType device)
{
    if (!tm1651IsValidDevMap(device)) {
        return NULL;
    }

    if (!gTm1651DefCfgDone[device]) {
        tm1651LoadDefCfg(device, &gTm1651Devices[device].cfg);
        tm1651FillBlank(gTm1651Devices[device].segData, TM1651_DIGIT_MAX);
        gTm1651Devices[device].isReady = false;
        gTm1651DefCfgDone[device] = true;
    }

    return &gTm1651Devices[device];
}

static void tm1651LoadDefCfg(eTm1651MapType device, stTm1651Cfg *cfg)
{
    if (cfg == NULL) {
        return;
    }

    tm1651LoadPlatformDefaultCfg(device, cfg);
}

static bool tm1651IsValidCfg(const stTm1651Cfg *cfg)
{
    if (cfg == NULL) {
        return false;
    }

    if ((cfg->brightness > 7U) ||
        (cfg->digitCount == 0U) ||
        (cfg->digitCount > TM1651_DIGIT_MAX) ||
        !tm1651PlatformIsValidCfg(cfg)) {
        return false;
    }

    return true;
}

static bool tm1651IsReadyXfer(const stTm1651Device *device)
{
    return (device != NULL) && device->isReady && (tm1651GetIicIf(device) != NULL);
}

static const stTm1651IicInterface *tm1651GetIicIf(const stTm1651Device *device)
{
    if (device == NULL) {
        return NULL;
    }

    if (!tm1651PlatformIsValidCfg(&device->cfg)) {
        return NULL;
    }

    return tm1651GetPlatformIicInterface(&device->cfg);
}

static uint8_t tm1651EncodeSymbol(uint8_t symbol)
{
    static const uint8_t gTm1651SegMap[] = {
        0x3FU,
        0x06U,
        0x5BU,
        0x4FU,
        0x66U,
        0x6DU,
        0x7DU,
        0x07U,
        0x7FU,
        0x6FU,
        0x00U,
        0x40U,
        0x79U,
    };

    if (symbol >= (uint8_t)(sizeof(gTm1651SegMap) / sizeof(gTm1651SegMap[0]))) {
        return 0x00U;
    }

    return gTm1651SegMap[symbol];
}

static void tm1651FillBlank(uint8_t *segData, uint8_t length)
{
    uint8_t lIndex;

    if (segData == NULL) {
        return;
    }

    for (lIndex = 0U; lIndex < length; ++lIndex) {
        segData[lIndex] = 0x00U;
    }
}

static eTm1651Status tm1651WriteFrame(const stTm1651Device *device, const uint8_t *buffer, uint8_t length)
{
    const stTm1651IicInterface *lIicIf;

    if ((device == NULL) || (buffer == NULL) || (length == 0U)) {
        return TM1651_STATUS_INVALID_PARAM;
    }

    lIicIf = tm1651GetIicIf(device);
    if (lIicIf == NULL) {
        return TM1651_STATUS_NOT_READY;
    }

    return lIicIf->writeFrame((uint8_t)device->cfg.linkId, buffer, length);
}

static eTm1651Status tm1651ApplyDisplayCtrl(const stTm1651Device *device)
{
    uint8_t lCommand;

    if (device == NULL) {
        return TM1651_STATUS_INVALID_PARAM;
    }

    lCommand = device->cfg.isDisplayOn ?
               (uint8_t)(TM1651_DISPLAY_ON_CMD | (device->cfg.brightness & 0x07U)) :
               TM1651_DISPLAY_OFF_CMD;
    return tm1651WriteFrame(device, &lCommand, 1U);
}

static eTm1651Status tm1651RefreshDisplay(stTm1651Device *device)
{
    uint8_t lDataCmd = TM1651_DATA_CMD_AUTO_ADDR;
    uint8_t lFrame[TM1651_DIGIT_MAX + 1U];
    uint8_t lIndex;
    eTm1651Status lStatus;

    if (device == NULL) {
        return TM1651_STATUS_INVALID_PARAM;
    }

    lStatus = tm1651WriteFrame(device, &lDataCmd, 1U);
    if (lStatus != TM1651_STATUS_OK) {
        return lStatus;
    }

    lFrame[0] = TM1651_ADDR_CMD_BASE;
    tm1651FillBlank(&lFrame[1], TM1651_DIGIT_MAX);
    for (lIndex = 0U; (lIndex < device->cfg.digitCount) && (lIndex < TM1651_DIGIT_MAX); ++lIndex) {
        lFrame[lIndex + 1U] = device->segData[lIndex];
    }

    lStatus = tm1651WriteFrame(device, lFrame, (uint8_t)(device->cfg.digitCount + 1U));
    if (lStatus != TM1651_STATUS_OK) {
        return lStatus;
    }

    return tm1651ApplyDisplayCtrl(device);
}
/**************************End of file********************************/
