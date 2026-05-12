/***********************************************************************************
* @file     : pca9535.c
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "pca9535.h"

#include "pca9535_assembly.h"

#include <stddef.h>

static stPca9535Device gPca9535Devices[PCA9535_DEV_MAX];
static bool gPca9535DefCfgDone[PCA9535_DEV_MAX] = {false};
static const stPca9535Ops *pca9535GetOps(void);
static bool pca9535IsAssembleReady(ePca9535MapType device);
static void pca9535LoadDefaultCfgFromOps(ePca9535MapType device, stPca9535Cfg *cfg);
static uint8_t pca9535GetLinkIdByDevice(ePca9535MapType device);
static void pca9535ResetInit(void);
static void pca9535ResetWrite(bool assertReset);
static uint32_t pca9535GetResetAssertDelayMs(void);
static uint32_t pca9535GetResetReleaseDelayMs(void);
static void pca9535DelayMs(uint32_t delayMs);

static const stPca9535Ops *pca9535GetOps(void)
{
    return pca9535PortGetOps();
}

static bool pca9535IsAssembleReady(ePca9535MapType device)
{
    const stPca9535Ops *lOps = pca9535GetOps();

    if ((lOps == NULL) ||
        (lOps->loadDefaultCfg == NULL) ||
        (lOps->getIicInterface == NULL) ||
        (lOps->isValidAssemble == NULL) ||
        (lOps->getLinkId == NULL) ||
        (lOps->delayMs == NULL)) {
        return false;
    }

    return lOps->isValidAssemble(device);
}

static void pca9535LoadDefaultCfgFromOps(ePca9535MapType device, stPca9535Cfg *cfg)
{
    const stPca9535Ops *lOps = pca9535GetOps();

    if ((cfg == NULL) || (lOps == NULL) || (lOps->loadDefaultCfg == NULL)) {
        return;
    }

    lOps->loadDefaultCfg(device, cfg);
}

static uint8_t pca9535GetLinkIdByDevice(ePca9535MapType device)
{
    const stPca9535Ops *lOps = pca9535GetOps();

    if ((lOps == NULL) || (lOps->getLinkId == NULL)) {
        return 0U;
    }

    return lOps->getLinkId(device);
}

static void pca9535ResetInit(void)
{
    const stPca9535Ops *lOps = pca9535GetOps();

    if ((lOps != NULL) && (lOps->resetInit != NULL)) {
        lOps->resetInit();
    }
}

static void pca9535ResetWrite(bool assertReset)
{
    const stPca9535Ops *lOps = pca9535GetOps();

    if ((lOps != NULL) && (lOps->resetWrite != NULL)) {
        lOps->resetWrite(assertReset);
    }
}

static uint32_t pca9535GetResetAssertDelayMs(void)
{
    const stPca9535Ops *lOps = pca9535GetOps();

    if ((lOps == NULL) || (lOps->getResetAssertDelayMs == NULL)) {
        return 0U;
    }

    return lOps->getResetAssertDelayMs();
}

static uint32_t pca9535GetResetReleaseDelayMs(void)
{
    const stPca9535Ops *lOps = pca9535GetOps();

    if ((lOps == NULL) || (lOps->getResetReleaseDelayMs == NULL)) {
        return 0U;
    }

    return lOps->getResetReleaseDelayMs();
}

static void pca9535DelayMs(uint32_t delayMs)
{
    const stPca9535Ops *lOps = pca9535GetOps();

    if ((lOps != NULL) && (lOps->delayMs != NULL)) {
        lOps->delayMs(delayMs);
    }
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
        return pca9535IsAssembleReady(device) ?
               PCA9535_STATUS_NOT_READY :
               PCA9535_STATUS_INVALID_PARAM;
    }

    lIicIf = pca9535GetIicIf(lDeviceCtx);
    lStatus = lIicIf->init(pca9535GetLinkIdByDevice(device));
    if (lStatus != PCA9535_STATUS_OK) {
        return lStatus;
    }

    if (lDeviceCtx->cfg.resetBeforeInit) {
        pca9535ResetInit();
        pca9535ResetWrite(true);
        pca9535DelayMs(pca9535GetResetAssertDelayMs());
        pca9535ResetWrite(false);
        pca9535DelayMs(pca9535GetResetReleaseDelayMs());
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

    pca9535LoadDefaultCfgFromOps(device, cfg);
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
    const stPca9535Ops *lOps = pca9535GetOps();

    if ((lDevice >= PCA9535_DEV_MAX) || !pca9535IsAssembleReady(lDevice)) {
        return NULL;
    }

    return ((lOps != NULL) && (lOps->getIicInterface != NULL)) ? lOps->getIicInterface(lDevice) : NULL;
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

    return lIicIf->writeReg(pca9535GetLinkIdByDevice(lDevice), device->cfg.address, &regAddr, 1U, &value, 1U);
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

    return lIicIf->readReg(pca9535GetLinkIdByDevice(lDevice), device->cfg.address, &regAddr, 1U, value, 1U);
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

    return lIicIf->writeReg(pca9535GetLinkIdByDevice(lDevice), device->cfg.address, &regAddr, 1U, lBuffer, 2U);
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

    lStatus = lIicIf->readReg(pca9535GetLinkIdByDevice(lDevice), device->cfg.address, &regAddr, 1U, lBuffer, 2U);
    if (lStatus != PCA9535_STATUS_OK) {
        return lStatus;
    }

    *value = (uint16_t)((uint16_t)lBuffer[0] | ((uint16_t)lBuffer[1] << 8U));
    return PCA9535_STATUS_OK;
}

/**************************End of file********************************/
