#include "pca9535.h"

#include "pca9535_assembly.h"

#include <stddef.h>

static stPca9535Device gPca9535Devices[PCA9535_DEV_MAX];
static bool gPca9535DefCfgDone[PCA9535_DEV_MAX] = {false};

__attribute__((weak)) void pca9535LoadPlatformDefaultCfg(ePca9535MapType device, stPca9535Cfg *cfg)
{
    (void)device;

    if (cfg == NULL) {
        return;
    }

    cfg->address = 0U;
    cfg->outputValue = 0U;
    cfg->polarityMask = 0U;
    cfg->directionMask = 0U;
    cfg->resetBeforeInit = false;
}

__attribute__((weak)) const stPca9535IicInterface *pca9535GetPlatformIicInterface(ePca9535MapType device)
{
    (void)device;
    return NULL;
}

__attribute__((weak)) bool pca9535PlatformIsValidAssemble(ePca9535MapType device)
{
    (void)device;
    return false;
}

__attribute__((weak)) uint8_t pca9535PlatformGetLinkId(ePca9535MapType device)
{
    (void)device;
    return 0U;
}

__attribute__((weak)) void pca9535PlatformResetInit(void)
{
}

__attribute__((weak)) void pca9535PlatformResetWrite(bool assertReset)
{
    (void)assertReset;
}

__attribute__((weak)) void pca9535PlatformDelayMs(uint32_t delayMs)
{
    (void)delayMs;
}

__attribute__((weak)) uint32_t pca9535PlatformGetResetAssertDelayMs(void)
{
    return 0U;
}

__attribute__((weak)) uint32_t pca9535PlatformGetResetReleaseDelayMs(void)
{
    return 0U;
}

static bool pca9535IsValidDevMap(ePca9535MapType device);
static stPca9535Device *pca9535GetDevCtx(ePca9535MapType device);
static ePca9535MapType pca9535GetDevMapByCtx(const stPca9535Device *device);
static void pca9535LoadDefCfg(ePca9535MapType device, stPca9535Cfg *cfg);
static bool pca9535IsValidCfg(const stPca9535Cfg *cfg);
static bool pca9535IsValidDev(const stPca9535Device *device);
static bool pca9535IsReadyXfer(const stPca9535Device *device);
static const stPca9535IicInterface *pca9535GetIicIf(const stPca9535Device *device);
static eDrvStatus pca9535WriteRegInt(const stPca9535Device *device, uint8_t regAddr, uint8_t value);
static eDrvStatus pca9535ReadRegInt(const stPca9535Device *device, uint8_t regAddr, uint8_t *value);
static eDrvStatus pca9535WritePort16(const stPca9535Device *device, uint8_t regAddr, uint16_t value);
static eDrvStatus pca9535ReadPort16(const stPca9535Device *device, uint8_t regAddr, uint16_t *value);

eDrvStatus pca9535GetDefCfg(ePca9535MapType device, stPca9535Cfg *cfg)
{
    if ((cfg == NULL) || !pca9535IsValidDevMap(device)) {
        return PCA9535_STATUS_INVALID_PARAM;
    }

    pca9535LoadDefCfg(device, cfg);
    return PCA9535_STATUS_OK;
}

eDrvStatus pca9535GetCfg(ePca9535MapType device, stPca9535Cfg *cfg)
{
    stPca9535Device *lDeviceCtx;

    if (cfg == NULL) {
        return PCA9535_STATUS_INVALID_PARAM;
    }

    lDeviceCtx = pca9535GetDevCtx(device);
    if (lDeviceCtx == NULL) {
        return PCA9535_STATUS_INVALID_PARAM;
    }

    *cfg = lDeviceCtx->cfg;
    return PCA9535_STATUS_OK;
}

eDrvStatus pca9535SetCfg(ePca9535MapType device, const stPca9535Cfg *cfg)
{
    stPca9535Device *lDeviceCtx;
    stPca9535Device lTmpDevice;

    if (cfg == NULL) {
        return PCA9535_STATUS_INVALID_PARAM;
    }

    lTmpDevice.cfg = *cfg;
    if (!pca9535IsValidDev(&lTmpDevice)) {
        return PCA9535_STATUS_INVALID_PARAM;
    }

    lDeviceCtx = pca9535GetDevCtx(device);
    if (lDeviceCtx == NULL) {
        return PCA9535_STATUS_INVALID_PARAM;
    }

    lDeviceCtx->cfg = *cfg;
    lDeviceCtx->inputValue = 0U;
    lDeviceCtx->outputValue = cfg->outputValue;
    lDeviceCtx->polarityMask = cfg->polarityMask;
    lDeviceCtx->directionMask = cfg->directionMask;
    lDeviceCtx->isReady = false;
    gPca9535DefCfgDone[device] = true;
    return PCA9535_STATUS_OK;
}

eDrvStatus pca9535Init(ePca9535MapType device)
{
    const stPca9535IicInterface *lIicIf;
    stPca9535Device *lDeviceCtx;
    eDrvStatus lStatus;

    lDeviceCtx = pca9535GetDevCtx(device);
    if (!pca9535IsValidDev(lDeviceCtx)) {
        return PCA9535_STATUS_INVALID_PARAM;
    }

    if (pca9535GetIicIf(lDeviceCtx) == NULL) {
        return pca9535PlatformIsValidAssemble(device) ?
               PCA9535_STATUS_NOT_READY :
               PCA9535_STATUS_INVALID_PARAM;
    }

    lIicIf = pca9535GetIicIf(lDeviceCtx);
    lStatus = lIicIf->init(pca9535PlatformGetLinkId(device));
    if (lStatus != PCA9535_STATUS_OK) {
        return lStatus;
    }

    if (lDeviceCtx->cfg.resetBeforeInit) {
        pca9535PlatformResetInit();
        pca9535PlatformResetWrite(true);
        pca9535PlatformDelayMs(pca9535PlatformGetResetAssertDelayMs());
        pca9535PlatformResetWrite(false);
        pca9535PlatformDelayMs(pca9535PlatformGetResetReleaseDelayMs());
    }

    lDeviceCtx->isReady = false;
    lDeviceCtx->inputValue = 0U;

    lStatus = pca9535WritePort16(lDeviceCtx, PCA9535_REG_OUTPUT_PORT0, lDeviceCtx->cfg.outputValue);
    if (lStatus != PCA9535_STATUS_OK) {
        return lStatus;
    }

    lStatus = pca9535WritePort16(lDeviceCtx, PCA9535_REG_POLARITY_PORT0, lDeviceCtx->cfg.polarityMask);
    if (lStatus != PCA9535_STATUS_OK) {
        return lStatus;
    }

    lStatus = pca9535WritePort16(lDeviceCtx, PCA9535_REG_CONFIGURATION_PORT0, lDeviceCtx->cfg.directionMask);
    if (lStatus != PCA9535_STATUS_OK) {
        return lStatus;
    }

    lDeviceCtx->outputValue = lDeviceCtx->cfg.outputValue;
    lDeviceCtx->polarityMask = lDeviceCtx->cfg.polarityMask;
    lDeviceCtx->directionMask = lDeviceCtx->cfg.directionMask;
    lDeviceCtx->isReady = true;
    return PCA9535_STATUS_OK;
}

bool pca9535IsReady(ePca9535MapType device)
{
    return pca9535IsReadyXfer(pca9535GetDevCtx(device));
}

eDrvStatus pca9535ReadReg(ePca9535MapType device, uint8_t regAddr, uint8_t *value)
{
    stPca9535Device *lDeviceCtx;

    lDeviceCtx = pca9535GetDevCtx(device);
    if ((value == NULL) || !pca9535IsReadyXfer(lDeviceCtx)) {
        return (value == NULL) ? PCA9535_STATUS_INVALID_PARAM : PCA9535_STATUS_NOT_READY;
    }

    return pca9535ReadRegInt(lDeviceCtx, regAddr, value);
}

eDrvStatus pca9535WriteReg(ePca9535MapType device, uint8_t regAddr, uint8_t value)
{
    stPca9535Device *lDeviceCtx;

    lDeviceCtx = pca9535GetDevCtx(device);
    if (!pca9535IsReadyXfer(lDeviceCtx)) {
        return PCA9535_STATUS_NOT_READY;
    }

    return pca9535WriteRegInt(lDeviceCtx, regAddr, value);
}

eDrvStatus pca9535ReadInputPort(ePca9535MapType device, uint16_t *value)
{
    stPca9535Device *lDeviceCtx;
    eDrvStatus lStatus;

    lDeviceCtx = pca9535GetDevCtx(device);
    if ((value == NULL) || !pca9535IsReadyXfer(lDeviceCtx)) {
        return (value == NULL) ? PCA9535_STATUS_INVALID_PARAM : PCA9535_STATUS_NOT_READY;
    }

    lStatus = pca9535ReadPort16(lDeviceCtx, PCA9535_REG_INPUT_PORT0, value);
    if (lStatus == PCA9535_STATUS_OK) {
        lDeviceCtx->inputValue = *value;
    }
    return lStatus;
}

eDrvStatus pca9535GetOutputPort(ePca9535MapType device, uint16_t *value)
{
    stPca9535Device *lDeviceCtx;
    eDrvStatus lStatus;

    lDeviceCtx = pca9535GetDevCtx(device);
    if ((value == NULL) || !pca9535IsReadyXfer(lDeviceCtx)) {
        return (value == NULL) ? PCA9535_STATUS_INVALID_PARAM : PCA9535_STATUS_NOT_READY;
    }

    lStatus = pca9535ReadPort16(lDeviceCtx, PCA9535_REG_OUTPUT_PORT0, value);
    if (lStatus == PCA9535_STATUS_OK) {
        lDeviceCtx->outputValue = *value;
    }
    return lStatus;
}

eDrvStatus pca9535SetOutputPort(ePca9535MapType device, uint16_t value)
{
    stPca9535Device *lDeviceCtx;
    eDrvStatus lStatus;

    lDeviceCtx = pca9535GetDevCtx(device);
    if (!pca9535IsReadyXfer(lDeviceCtx)) {
        return PCA9535_STATUS_NOT_READY;
    }

    lStatus = pca9535WritePort16(lDeviceCtx, PCA9535_REG_OUTPUT_PORT0, value);
    if (lStatus == PCA9535_STATUS_OK) {
        lDeviceCtx->outputValue = value;
    }
    return lStatus;
}

eDrvStatus pca9535ModifyOutputPort(ePca9535MapType device, uint16_t mask, uint16_t value)
{
    stPca9535Device *lDeviceCtx;
    uint16_t lOutputValue;

    lDeviceCtx = pca9535GetDevCtx(device);
    if (!pca9535IsReadyXfer(lDeviceCtx)) {
        return PCA9535_STATUS_NOT_READY;
    }

    lOutputValue = (uint16_t)((lDeviceCtx->outputValue & (uint16_t)(~mask)) | (value & mask));
    return pca9535SetOutputPort(device, lOutputValue);
}

eDrvStatus pca9535GetPolarityPort(ePca9535MapType device, uint16_t *value)
{
    stPca9535Device *lDeviceCtx;
    eDrvStatus lStatus;

    lDeviceCtx = pca9535GetDevCtx(device);
    if ((value == NULL) || !pca9535IsReadyXfer(lDeviceCtx)) {
        return (value == NULL) ? PCA9535_STATUS_INVALID_PARAM : PCA9535_STATUS_NOT_READY;
    }

    lStatus = pca9535ReadPort16(lDeviceCtx, PCA9535_REG_POLARITY_PORT0, value);
    if (lStatus == PCA9535_STATUS_OK) {
        lDeviceCtx->polarityMask = *value;
    }
    return lStatus;
}

eDrvStatus pca9535SetPolarityPort(ePca9535MapType device, uint16_t value)
{
    stPca9535Device *lDeviceCtx;
    eDrvStatus lStatus;

    lDeviceCtx = pca9535GetDevCtx(device);
    if (!pca9535IsReadyXfer(lDeviceCtx)) {
        return PCA9535_STATUS_NOT_READY;
    }

    lStatus = pca9535WritePort16(lDeviceCtx, PCA9535_REG_POLARITY_PORT0, value);
    if (lStatus == PCA9535_STATUS_OK) {
        lDeviceCtx->polarityMask = value;
    }
    return lStatus;
}

eDrvStatus pca9535GetDirectionPort(ePca9535MapType device, uint16_t *value)
{
    stPca9535Device *lDeviceCtx;
    eDrvStatus lStatus;

    lDeviceCtx = pca9535GetDevCtx(device);
    if ((value == NULL) || !pca9535IsReadyXfer(lDeviceCtx)) {
        return (value == NULL) ? PCA9535_STATUS_INVALID_PARAM : PCA9535_STATUS_NOT_READY;
    }

    lStatus = pca9535ReadPort16(lDeviceCtx, PCA9535_REG_CONFIGURATION_PORT0, value);
    if (lStatus == PCA9535_STATUS_OK) {
        lDeviceCtx->directionMask = *value;
    }
    return lStatus;
}

eDrvStatus pca9535SetDirectionPort(ePca9535MapType device, uint16_t value)
{
    stPca9535Device *lDeviceCtx;
    eDrvStatus lStatus;

    lDeviceCtx = pca9535GetDevCtx(device);
    if (!pca9535IsReadyXfer(lDeviceCtx)) {
        return PCA9535_STATUS_NOT_READY;
    }

    lStatus = pca9535WritePort16(lDeviceCtx, PCA9535_REG_CONFIGURATION_PORT0, value);
    if (lStatus == PCA9535_STATUS_OK) {
        lDeviceCtx->directionMask = value;
    }
    return lStatus;
}

static bool pca9535IsValidDevMap(ePca9535MapType device)
{
    return ((uint32_t)device < (uint32_t)PCA9535_DEV_MAX);
}

static stPca9535Device *pca9535GetDevCtx(ePca9535MapType device)
{
    if (!pca9535IsValidDevMap(device)) {
        return NULL;
    }

    if (!gPca9535DefCfgDone[device]) {
        pca9535LoadDefCfg(device, &gPca9535Devices[device].cfg);
        gPca9535Devices[device].inputValue = 0U;
        gPca9535Devices[device].outputValue = gPca9535Devices[device].cfg.outputValue;
        gPca9535Devices[device].polarityMask = gPca9535Devices[device].cfg.polarityMask;
        gPca9535Devices[device].directionMask = gPca9535Devices[device].cfg.directionMask;
        gPca9535Devices[device].isReady = false;
        gPca9535DefCfgDone[device] = true;
    }

    return &gPca9535Devices[device];
}

static ePca9535MapType pca9535GetDevMapByCtx(const stPca9535Device *device)
{
    ptrdiff_t lIndex;

    if ((device == NULL) ||
        (device < &gPca9535Devices[0]) ||
        (device >= &gPca9535Devices[PCA9535_DEV_MAX])) {
        return PCA9535_DEV_MAX;
    }

    lIndex = device - &gPca9535Devices[0];
    return (ePca9535MapType)lIndex;
}

static void pca9535LoadDefCfg(ePca9535MapType device, stPca9535Cfg *cfg)
{
    if (cfg == NULL) {
        return;
    }

    pca9535LoadPlatformDefaultCfg(device, cfg);
}

static bool pca9535IsValidCfg(const stPca9535Cfg *cfg)
{
    return (cfg != NULL) &&
           (cfg->address >= PCA9535_IIC_ADDRESS_LLL) &&
           (cfg->address <= PCA9535_IIC_ADDRESS_HHH);
}

static bool pca9535IsValidDev(const stPca9535Device *device)
{
    if (device == NULL) {
        return false;
    }

    return pca9535IsValidCfg(&device->cfg);
}

static bool pca9535IsReadyXfer(const stPca9535Device *device)
{
    return (device != NULL) && device->isReady && (pca9535GetIicIf(device) != NULL);
}

static const stPca9535IicInterface *pca9535GetIicIf(const stPca9535Device *device)
{
    ePca9535MapType lDevice;

    if (device == NULL) {
        return NULL;
    }

    if (!pca9535IsValidCfg(&device->cfg)) {
        return NULL;
    }

    lDevice = pca9535GetDevMapByCtx(device);
    if ((lDevice >= PCA9535_DEV_MAX) || !pca9535PlatformIsValidAssemble(lDevice)) {
        return NULL;
    }

    return pca9535GetPlatformIicInterface(lDevice);
}

static eDrvStatus pca9535WriteRegInt(const stPca9535Device *device, uint8_t regAddr, uint8_t value)
{
    const stPca9535IicInterface *lIicIf;
    ePca9535MapType lDevice;

    lIicIf = pca9535GetIicIf(device);
    if (lIicIf == NULL) {
        return PCA9535_STATUS_NOT_READY;
    }

    lDevice = pca9535GetDevMapByCtx(device);
    if (lDevice >= PCA9535_DEV_MAX) {
        return PCA9535_STATUS_INVALID_PARAM;
    }

    return lIicIf->writeReg(pca9535PlatformGetLinkId(lDevice), device->cfg.address, &regAddr, 1U, &value, 1U);
}

static eDrvStatus pca9535ReadRegInt(const stPca9535Device *device, uint8_t regAddr, uint8_t *value)
{
    const stPca9535IicInterface *lIicIf;
    ePca9535MapType lDevice;

    if (value == NULL) {
        return PCA9535_STATUS_INVALID_PARAM;
    }

    lIicIf = pca9535GetIicIf(device);
    if (lIicIf == NULL) {
        return PCA9535_STATUS_NOT_READY;
    }

    lDevice = pca9535GetDevMapByCtx(device);
    if (lDevice >= PCA9535_DEV_MAX) {
        return PCA9535_STATUS_INVALID_PARAM;
    }

    return lIicIf->readReg(pca9535PlatformGetLinkId(lDevice), device->cfg.address, &regAddr, 1U, value, 1U);
}

static eDrvStatus pca9535WritePort16(const stPca9535Device *device, uint8_t regAddr, uint16_t value)
{
    const stPca9535IicInterface *lIicIf;
    ePca9535MapType lDevice;
    uint8_t lBuffer[2];

    lIicIf = pca9535GetIicIf(device);
    if (lIicIf == NULL) {
        return PCA9535_STATUS_NOT_READY;
    }

    lBuffer[0] = (uint8_t)(value & 0x00FFU);
    lBuffer[1] = (uint8_t)((value >> 8U) & 0x00FFU);
    lDevice = pca9535GetDevMapByCtx(device);
    if (lDevice >= PCA9535_DEV_MAX) {
        return PCA9535_STATUS_INVALID_PARAM;
    }

    return lIicIf->writeReg(pca9535PlatformGetLinkId(lDevice), device->cfg.address, &regAddr, 1U, lBuffer, 2U);
}

static eDrvStatus pca9535ReadPort16(const stPca9535Device *device, uint8_t regAddr, uint16_t *value)
{
    const stPca9535IicInterface *lIicIf;
    ePca9535MapType lDevice;
    uint8_t lBuffer[2];
    eDrvStatus lStatus;

    if (value == NULL) {
        return PCA9535_STATUS_INVALID_PARAM;
    }

    lIicIf = pca9535GetIicIf(device);
    if (lIicIf == NULL) {
        return PCA9535_STATUS_NOT_READY;
    }

    lDevice = pca9535GetDevMapByCtx(device);
    if (lDevice >= PCA9535_DEV_MAX) {
        return PCA9535_STATUS_INVALID_PARAM;
    }

    lStatus = lIicIf->readReg(pca9535PlatformGetLinkId(lDevice), device->cfg.address, &regAddr, 1U, lBuffer, 2U);
    if (lStatus != PCA9535_STATUS_OK) {
        return lStatus;
    }

    *value = (uint16_t)((uint16_t)lBuffer[0] | ((uint16_t)lBuffer[1] << 8U));
    return PCA9535_STATUS_OK;
}
