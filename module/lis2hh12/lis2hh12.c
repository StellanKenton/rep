/***********************************************************************************
* @file     : lis2hh12.c
* @brief    : LIS2HH12 accelerometer module implementation.
* @details  : The module provides a passive bring-up and FIFO acquisition flow
*             for a LIS2HH12 accelerometer instance selected through assembly
*             hooks.
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "lis2hh12.h"

#include "lis2hh12_assembly.h"

#include <stddef.h>
#include <string.h>

static stLis2hh12Device gLis2hh12Devices[LIS2HH12_DEV_MAX];
static bool gLis2hh12DefCfgDone[LIS2HH12_DEV_MAX] = {false};
static const uint32_t gLis2hh12RetryDelayMsDefault = 10U;
static const uint32_t gLis2hh12ResetPollDelayMsDefault = 1U;

static const stLis2hh12Ops *lis2hh12GetOps(void);
static bool lis2hh12IsAssembleReady(eLis2hh12MapType device);
static void lis2hh12LoadDefaultCfgFromOps(eLis2hh12MapType device, stLis2hh12Cfg *cfg);
static uint8_t lis2hh12GetLinkIdByDevice(eLis2hh12MapType device);
static uint32_t lis2hh12GetRetryDelayMs(void);
static uint32_t lis2hh12GetResetPollDelayMs(void);
static void lis2hh12DelayMs(uint32_t delayMs);

static const stLis2hh12Ops *lis2hh12GetOps(void)
{
    return lis2hh12PortGetOps();
}

static bool lis2hh12IsAssembleReady(eLis2hh12MapType device)
{
    const stLis2hh12Ops *lOps = lis2hh12GetOps();

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

static void lis2hh12LoadDefaultCfgFromOps(eLis2hh12MapType device, stLis2hh12Cfg *cfg)
{
    const stLis2hh12Ops *lOps = lis2hh12GetOps();

    if (cfg == NULL) {
        return;
    }

    (void)memset(cfg, 0, sizeof(*cfg));
    if ((lOps == NULL) || (lOps->loadDefaultCfg == NULL)) {
        return;
    }

    lOps->loadDefaultCfg(device, cfg);
}

static uint8_t lis2hh12GetLinkIdByDevice(eLis2hh12MapType device)
{
    const stLis2hh12Ops *lOps = lis2hh12GetOps();

    if ((lOps == NULL) || (lOps->getLinkId == NULL)) {
        return 0U;
    }

    return lOps->getLinkId(device);
}

static uint32_t lis2hh12GetRetryDelayMs(void)
{
    const stLis2hh12Ops *lOps = lis2hh12GetOps();

    if ((lOps == NULL) || (lOps->getRetryDelayMs == NULL)) {
        return gLis2hh12RetryDelayMsDefault;
    }

    return lOps->getRetryDelayMs();
}

static uint32_t lis2hh12GetResetPollDelayMs(void)
{
    const stLis2hh12Ops *lOps = lis2hh12GetOps();

    if ((lOps == NULL) || (lOps->getResetPollDelayMs == NULL)) {
        return gLis2hh12ResetPollDelayMsDefault;
    }

    return lOps->getResetPollDelayMs();
}

static void lis2hh12DelayMs(uint32_t delayMs)
{
    const stLis2hh12Ops *lOps = lis2hh12GetOps();

    if ((lOps == NULL) || (lOps->delayMs == NULL)) {
        return;
    }

    lOps->delayMs(delayMs);
}

static bool lis2hh12IsValidDevMap(eLis2hh12MapType device);
static stLis2hh12Device *lis2hh12GetDevCtx(eLis2hh12MapType device);
static eLis2hh12MapType lis2hh12GetDevMapByCtx(const stLis2hh12Device *device);
static void lis2hh12LoadDefCfg(eLis2hh12MapType device, stLis2hh12Cfg *cfg);
static bool lis2hh12IsValidCfg(const stLis2hh12Cfg *cfg);
static bool lis2hh12IsValidDev(const stLis2hh12Device *device);
static bool lis2hh12IsReadyXfer(const stLis2hh12Device *device);
static const stLis2hh12IicInterface *lis2hh12GetIicIf(const stLis2hh12Device *device);
static eDrvStatus lis2hh12WriteRegInt(const stLis2hh12Device *device, uint8_t regAddr, uint8_t value);
static eDrvStatus lis2hh12ReadRegInt(const stLis2hh12Device *device, uint8_t regAddr, uint8_t *value);
static eDrvStatus lis2hh12ReadRegsInt(const stLis2hh12Device *device, uint8_t regAddr, uint8_t *buffer, uint16_t length);
static eDrvStatus lis2hh12UpdateRegInt(const stLis2hh12Device *device, uint8_t regAddr, uint8_t mask, uint8_t value);
static eDrvStatus lis2hh12ApplyInitCfg(stLis2hh12Device *device);
static eDrvStatus lis2hh12ProbeDevice(stLis2hh12Device *device);
static eDrvStatus lis2hh12ResetDevice(stLis2hh12Device *device);
static eDrvStatus lis2hh12ReadFifoStatusInt(const stLis2hh12Device *device, stLis2hh12FifoStatus *status);
static eDrvStatus lis2hh12DrainFifoInt(stLis2hh12Device *device, stLis2hh12Sample *samples, uint8_t readCount, uint8_t maxSamples, uint8_t *sampleCount);
static void lis2hh12ClrLastState(stLis2hh12Device *device);
static void lis2hh12ParseFifoStatus(uint8_t rawValue, stLis2hh12FifoStatus *status);
static int16_t lis2hh12ParseLe16(const uint8_t *buffer);

eDrvStatus lis2hh12GetDefCfg(eLis2hh12MapType device, stLis2hh12Cfg *cfg)
{
    if ((cfg == NULL) || !lis2hh12IsValidDevMap(device)) {
        return LIS2HH12_STATUS_INVALID_PARAM;
    }

    lis2hh12LoadDefCfg(device, cfg);
    return lis2hh12IsValidCfg(cfg) ? LIS2HH12_STATUS_OK : LIS2HH12_STATUS_NOT_READY;
}

eDrvStatus lis2hh12GetCfg(eLis2hh12MapType device, stLis2hh12Cfg *cfg)
{
    stLis2hh12Device *lDeviceCtx;

    if (cfg == NULL) {
        return LIS2HH12_STATUS_INVALID_PARAM;
    }

    lDeviceCtx = lis2hh12GetDevCtx(device);
    if (lDeviceCtx == NULL) {
        return LIS2HH12_STATUS_INVALID_PARAM;
    }

    *cfg = lDeviceCtx->cfg;
    return LIS2HH12_STATUS_OK;
}

eDrvStatus lis2hh12SetCfg(eLis2hh12MapType device, const stLis2hh12Cfg *cfg)
{
    stLis2hh12Device *lDeviceCtx;
    stLis2hh12Device lTmpDevice;

    if (cfg == NULL) {
        return LIS2HH12_STATUS_INVALID_PARAM;
    }

    lTmpDevice.cfg = *cfg;
    if (!lis2hh12IsValidDev(&lTmpDevice)) {
        return LIS2HH12_STATUS_INVALID_PARAM;
    }

    lDeviceCtx = lis2hh12GetDevCtx(device);
    if (lDeviceCtx == NULL) {
        return LIS2HH12_STATUS_INVALID_PARAM;
    }

    lDeviceCtx->cfg = *cfg;
    lis2hh12ClrLastState(lDeviceCtx);
    gLis2hh12DefCfgDone[device] = true;
    return LIS2HH12_STATUS_OK;
}

eDrvStatus lis2hh12Init(eLis2hh12MapType device)
{
    const stLis2hh12IicInterface *lIicIf;
    stLis2hh12Device *lDeviceCtx;
    eDrvStatus lStatus;

    lDeviceCtx = lis2hh12GetDevCtx(device);
    if (!lis2hh12IsValidDev(lDeviceCtx)) {
        return LIS2HH12_STATUS_INVALID_PARAM;
    }

    if (lis2hh12GetIicIf(lDeviceCtx) == NULL) {
        return lis2hh12IsAssembleReady(device) ?
               LIS2HH12_STATUS_NOT_READY :
               LIS2HH12_STATUS_INVALID_PARAM;
    }

    lIicIf = lis2hh12GetIicIf(lDeviceCtx);
    lStatus = lIicIf->init(lis2hh12GetLinkIdByDevice(device));
    if (lStatus != LIS2HH12_STATUS_OK) {
        return lStatus;
    }

    lis2hh12ClrLastState(lDeviceCtx);

    lStatus = lis2hh12ProbeDevice(lDeviceCtx);
    if (lStatus != LIS2HH12_STATUS_OK) {
        return lStatus;
    }

    lStatus = lis2hh12ResetDevice(lDeviceCtx);
    if (lStatus != LIS2HH12_STATUS_OK) {
        return lStatus;
    }

    lStatus = lis2hh12ApplyInitCfg(lDeviceCtx);
    if (lStatus != LIS2HH12_STATUS_OK) {
        return lStatus;
    }

    lDeviceCtx->isReady = true;
    return LIS2HH12_STATUS_OK;
}

bool lis2hh12IsReady(eLis2hh12MapType device)
{
    return lis2hh12IsReadyXfer(lis2hh12GetDevCtx(device));
}

eDrvStatus lis2hh12ReadId(eLis2hh12MapType device, uint8_t *devId)
{
    const stLis2hh12IicInterface *lIicIf;
    stLis2hh12Device *lDeviceCtx;
    eDrvStatus lStatus;

    lDeviceCtx = lis2hh12GetDevCtx(device);
    if ((devId == NULL) || !lis2hh12IsValidDev(lDeviceCtx)) {
        return LIS2HH12_STATUS_INVALID_PARAM;
    }

    lIicIf = lis2hh12GetIicIf(lDeviceCtx);
    if (lIicIf == NULL) {
        return LIS2HH12_STATUS_NOT_READY;
    }

    lStatus = lIicIf->init(lis2hh12GetLinkIdByDevice(device));
    if (lStatus != LIS2HH12_STATUS_OK) {
        return lStatus;
    }

    return lis2hh12ReadRegInt(lDeviceCtx, LIS2HH12_REG_WHO_AM_I, devId);
}

eDrvStatus lis2hh12ReadReg(eLis2hh12MapType device, uint8_t regAddr, uint8_t *value)
{
    stLis2hh12Device *lDeviceCtx;

    lDeviceCtx = lis2hh12GetDevCtx(device);
    if ((value == NULL) || !lis2hh12IsReadyXfer(lDeviceCtx)) {
        return (value == NULL) ? LIS2HH12_STATUS_INVALID_PARAM : LIS2HH12_STATUS_NOT_READY;
    }

    return lis2hh12ReadRegInt(lDeviceCtx, regAddr, value);
}

eDrvStatus lis2hh12WriteReg(eLis2hh12MapType device, uint8_t regAddr, uint8_t value)
{
    stLis2hh12Device *lDeviceCtx;

    lDeviceCtx = lis2hh12GetDevCtx(device);
    if (!lis2hh12IsReadyXfer(lDeviceCtx)) {
        return LIS2HH12_STATUS_NOT_READY;
    }

    return lis2hh12WriteRegInt(lDeviceCtx, regAddr, value);
}

eDrvStatus lis2hh12ReadRaw(eLis2hh12MapType device, stLis2hh12Sample *sample)
{
    stLis2hh12Device *lDeviceCtx;
    uint8_t lBuffer[LIS2HH12_SAMPLE_BYTES];
    eDrvStatus lStatus;

    if (sample == NULL) {
        return LIS2HH12_STATUS_INVALID_PARAM;
    }

    lDeviceCtx = lis2hh12GetDevCtx(device);
    if (!lis2hh12IsReadyXfer(lDeviceCtx)) {
        return LIS2HH12_STATUS_NOT_READY;
    }

    lStatus = lis2hh12ReadRegsInt(lDeviceCtx, LIS2HH12_REG_OUT_X_L, lBuffer, LIS2HH12_SAMPLE_BYTES);
    if (lStatus != LIS2HH12_STATUS_OK) {
        return lStatus;
    }

    lDeviceCtx->lastSample.x = lis2hh12ParseLe16(&lBuffer[0]);
    lDeviceCtx->lastSample.y = lis2hh12ParseLe16(&lBuffer[2]);
    lDeviceCtx->lastSample.z = lis2hh12ParseLe16(&lBuffer[4]);
    *sample = lDeviceCtx->lastSample;
    return LIS2HH12_STATUS_OK;
}

eDrvStatus lis2hh12ReadFifoStatus(eLis2hh12MapType device, stLis2hh12FifoStatus *status)
{
    stLis2hh12Device *lDeviceCtx;
    eDrvStatus lStatus;

    if (status == NULL) {
        return LIS2HH12_STATUS_INVALID_PARAM;
    }

    lDeviceCtx = lis2hh12GetDevCtx(device);
    if (!lis2hh12IsReadyXfer(lDeviceCtx)) {
        return LIS2HH12_STATUS_NOT_READY;
    }

    lStatus = lis2hh12ReadFifoStatusInt(lDeviceCtx, status);
    if (lStatus == LIS2HH12_STATUS_OK) {
        lDeviceCtx->lastFifoStatus = *status;
    }

    return lStatus;
}

eDrvStatus lis2hh12ReadFifoSamples(eLis2hh12MapType device, stLis2hh12Sample *samples, uint8_t maxSamples, uint8_t *sampleCount)
{
    stLis2hh12Device *lDeviceCtx;
    stLis2hh12FifoStatus lStatusInfo;
    eDrvStatus lStatus;

    if ((sampleCount == NULL) || (samples == NULL) || (maxSamples == 0U)) {
        return LIS2HH12_STATUS_INVALID_PARAM;
    }

    *sampleCount = 0U;
    lDeviceCtx = lis2hh12GetDevCtx(device);
    if (!lis2hh12IsReadyXfer(lDeviceCtx)) {
        return LIS2HH12_STATUS_NOT_READY;
    }

    lStatus = lis2hh12ReadFifoStatusInt(lDeviceCtx, &lStatusInfo);
    if (lStatus != LIS2HH12_STATUS_OK) {
        return lStatus;
    }

    lDeviceCtx->lastFifoStatus = lStatusInfo;
    if (lStatusInfo.isEmpty || (lStatusInfo.sampleCount == 0U)) {
        return LIS2HH12_STATUS_DATA_INVALID;
    }

    if (lStatusInfo.sampleCount > lDeviceCtx->cfg.fifoWatermark) {
        (void)lis2hh12DrainFifoInt(lDeviceCtx, NULL, lStatusInfo.sampleCount, 0U, sampleCount);
        return LIS2HH12_STATUS_DATA_INVALID;
    }

    if (lStatusInfo.sampleCount > maxSamples) {
        (void)lis2hh12DrainFifoInt(lDeviceCtx, NULL, lStatusInfo.sampleCount, 0U, sampleCount);
        return LIS2HH12_STATUS_BUFFER_TOO_SMALL;
    }

    return lis2hh12DrainFifoInt(lDeviceCtx, samples, lStatusInfo.sampleCount, maxSamples, sampleCount);
}

static bool lis2hh12IsValidDevMap(eLis2hh12MapType device)
{
    return ((uint32_t)device < (uint32_t)LIS2HH12_DEV_MAX);
}

static stLis2hh12Device *lis2hh12GetDevCtx(eLis2hh12MapType device)
{
    if (!lis2hh12IsValidDevMap(device)) {
        return NULL;
    }

    if (!gLis2hh12DefCfgDone[device]) {
        lis2hh12LoadDefCfg(device, &gLis2hh12Devices[device].cfg);
        lis2hh12ClrLastState(&gLis2hh12Devices[device]);
        gLis2hh12DefCfgDone[device] = true;
    }

    return &gLis2hh12Devices[device];
}

static eLis2hh12MapType lis2hh12GetDevMapByCtx(const stLis2hh12Device *device)
{
    ptrdiff_t lIndex;

    if ((device == NULL) ||
        (device < &gLis2hh12Devices[0]) ||
        (device >= &gLis2hh12Devices[LIS2HH12_DEV_MAX])) {
        return LIS2HH12_DEV_MAX;
    }

    lIndex = device - &gLis2hh12Devices[0];
    return (eLis2hh12MapType)lIndex;
}

static void lis2hh12LoadDefCfg(eLis2hh12MapType device, stLis2hh12Cfg *cfg)
{
    lis2hh12LoadDefaultCfgFromOps(device, cfg);
}

static bool lis2hh12IsValidCfg(const stLis2hh12Cfg *cfg)
{
    if (cfg == NULL) {
        return false;
    }

    if ((cfg->address != LIS2HH12_IIC_ADDRESS_LOW) &&
        (cfg->address != LIS2HH12_IIC_ADDRESS_HIGH)) {
        return false;
    }

    if ((cfg->fifoWatermark == 0U) || (cfg->fifoWatermark >= LIS2HH12_FIFO_CAPACITY)) {
        return false;
    }

    if (cfg->retryMax == 0U) {
        return false;
    }

    if ((uint32_t)cfg->dataRate > (uint32_t)LIS2HH12_DATA_RATE_800HZ) {
        return false;
    }

    if ((uint32_t)cfg->fullScale > (uint32_t)LIS2HH12_FULL_SCALE_8G) {
        return false;
    }

    if ((uint32_t)cfg->filterIntPath > (uint32_t)LIS2HH12_FILTER_INT_BOTH) {
        return false;
    }

    if ((uint32_t)cfg->filterOutPath > (uint32_t)LIS2HH12_FILTER_OUT_HP) {
        return false;
    }

    if ((uint32_t)cfg->filterLowBandwidth > (uint32_t)LIS2HH12_FILTER_LOW_BW_ODR_DIV_400) {
        return false;
    }

    if (((uint32_t)cfg->filterAntiAliasBandwidth < (uint32_t)LIS2HH12_FILTER_AA_AUTO) ||
        ((uint32_t)cfg->filterAntiAliasBandwidth > (uint32_t)LIS2HH12_FILTER_AA_50HZ)) {
        return false;
    }

    switch (cfg->fifoMode) {
        case LIS2HH12_FIFO_MODE_OFF:
        case LIS2HH12_FIFO_MODE_BYPASS:
        case LIS2HH12_FIFO_MODE_FIFO:
        case LIS2HH12_FIFO_MODE_STREAM:
        case LIS2HH12_FIFO_MODE_STREAM_TO_FIFO:
        case LIS2HH12_FIFO_MODE_BYPASS_TO_STREAM:
        case LIS2HH12_FIFO_MODE_BYPASS_TO_FIFO:
            break;
        default:
            return false;
    }

    return true;
}

static bool lis2hh12IsValidDev(const stLis2hh12Device *device)
{
    return lis2hh12IsValidCfg((device == NULL) ? NULL : &device->cfg);
}

static bool lis2hh12IsReadyXfer(const stLis2hh12Device *device)
{
    return (lis2hh12IsValidDev(device) && device->isReady);
}

static const stLis2hh12IicInterface *lis2hh12GetIicIf(const stLis2hh12Device *device)
{
    const stLis2hh12Ops *lOps;
    const stLis2hh12IicInterface *lIicIf;
    eLis2hh12MapType lDeviceMap;

    lDeviceMap = lis2hh12GetDevMapByCtx(device);
    if ((lDeviceMap >= LIS2HH12_DEV_MAX) || !lis2hh12IsAssembleReady(lDeviceMap)) {
        return NULL;
    }

    lOps = lis2hh12GetOps();
    lIicIf = lOps->getIicInterface(lDeviceMap);
    if ((lIicIf == NULL) || (lIicIf->init == NULL) || (lIicIf->writeReg == NULL) || (lIicIf->readReg == NULL)) {
        return NULL;
    }

    return lIicIf;
}

static eDrvStatus lis2hh12WriteRegInt(const stLis2hh12Device *device, uint8_t regAddr, uint8_t value)
{
    const stLis2hh12IicInterface *lIicIf;

    lIicIf = lis2hh12GetIicIf(device);
    if (lIicIf == NULL) {
        return LIS2HH12_STATUS_NOT_READY;
    }

    return lIicIf->writeReg(lis2hh12GetLinkIdByDevice(lis2hh12GetDevMapByCtx(device)), device->cfg.address, &regAddr, 1U, &value, 1U);
}

static eDrvStatus lis2hh12ReadRegInt(const stLis2hh12Device *device, uint8_t regAddr, uint8_t *value)
{
    const stLis2hh12IicInterface *lIicIf;

    if (value == NULL) {
        return LIS2HH12_STATUS_INVALID_PARAM;
    }

    lIicIf = lis2hh12GetIicIf(device);
    if (lIicIf == NULL) {
        return LIS2HH12_STATUS_NOT_READY;
    }

    return lIicIf->readReg(lis2hh12GetLinkIdByDevice(lis2hh12GetDevMapByCtx(device)), device->cfg.address, &regAddr, 1U, value, 1U);
}

static eDrvStatus lis2hh12ReadRegsInt(const stLis2hh12Device *device, uint8_t regAddr, uint8_t *buffer, uint16_t length)
{
    const stLis2hh12IicInterface *lIicIf;

    if ((buffer == NULL) || (length == 0U)) {
        return LIS2HH12_STATUS_INVALID_PARAM;
    }

    lIicIf = lis2hh12GetIicIf(device);
    if (lIicIf == NULL) {
        return LIS2HH12_STATUS_NOT_READY;
    }

    return lIicIf->readReg(lis2hh12GetLinkIdByDevice(lis2hh12GetDevMapByCtx(device)), device->cfg.address, &regAddr, 1U, buffer, length);
}

static eDrvStatus lis2hh12UpdateRegInt(const stLis2hh12Device *device, uint8_t regAddr, uint8_t mask, uint8_t value)
{
    uint8_t lRegValue;
    eDrvStatus lStatus;

    lStatus = lis2hh12ReadRegInt(device, regAddr, &lRegValue);
    if (lStatus != LIS2HH12_STATUS_OK) {
        return lStatus;
    }

    lRegValue = (uint8_t)((lRegValue & (uint8_t)(~mask)) | (value & mask));
    return lis2hh12WriteRegInt(device, regAddr, lRegValue);
}

static eDrvStatus lis2hh12ApplyInitCfg(stLis2hh12Device *device)
{
    eDrvStatus lStatus;
    uint8_t lCtrl1Value;
    uint8_t lCtrl4Value;

    lCtrl1Value = (uint8_t)(LIS2HH12_CTRL1_XEN_BIT |
                            LIS2HH12_CTRL1_YEN_BIT |
                            LIS2HH12_CTRL1_ZEN_BIT |
                            (((uint8_t)device->cfg.dataRate << 4U) & LIS2HH12_CTRL1_ODR_MASK) |
                            (device->cfg.blockDataUpdate ? LIS2HH12_CTRL1_BDU_BIT : 0U));
    lStatus = lis2hh12WriteRegInt(device, LIS2HH12_REG_CTRL1, lCtrl1Value);
    if (lStatus != LIS2HH12_STATUS_OK) {
        return lStatus;
    }

    lStatus = lis2hh12UpdateRegInt(device, LIS2HH12_REG_CTRL2, LIS2HH12_CTRL2_HPM_MASK, (uint8_t)device->cfg.filterIntPath);
    if (lStatus != LIS2HH12_STATUS_OK) {
        return lStatus;
    }

    lStatus = lis2hh12UpdateRegInt(device, LIS2HH12_REG_CTRL2, LIS2HH12_CTRL2_FDS_MASK, (device->cfg.filterOutPath == LIS2HH12_FILTER_OUT_BYPASS) ? 0U : LIS2HH12_CTRL2_FDS_MASK);
    if (lStatus != LIS2HH12_STATUS_OK) {
        return lStatus;
    }

    lStatus = lis2hh12UpdateRegInt(device, LIS2HH12_REG_CTRL2, LIS2HH12_CTRL2_DFC_MASK, (uint8_t)((uint8_t)device->cfg.filterOutPath << 5U));
    if (lStatus != LIS2HH12_STATUS_OK) {
        return lStatus;
    }

    lCtrl4Value = (uint8_t)(((uint8_t)device->cfg.fullScale << 4U) & LIS2HH12_CTRL4_FS_MASK);
    lCtrl4Value |= (uint8_t)((uint8_t)device->cfg.filterAntiAliasBandwidth & 0x03U);
    lCtrl4Value |= (uint8_t)(((uint8_t)device->cfg.filterLowBandwidth << 6U) & LIS2HH12_CTRL4_BW_MASK);
    if ((uint8_t)device->cfg.filterAntiAliasBandwidth != (uint8_t)LIS2HH12_FILTER_AA_AUTO) {
        lCtrl4Value |= LIS2HH12_CTRL4_BW_SCALE_ODR_BIT;
    }
    if (device->cfg.autoIncrement) {
        lCtrl4Value |= LIS2HH12_CTRL4_IF_ADD_INC_BIT;
    }

    lStatus = lis2hh12WriteRegInt(device, LIS2HH12_REG_CTRL4, lCtrl4Value);
    if (lStatus != LIS2HH12_STATUS_OK) {
        return lStatus;
    }

    lStatus = lis2hh12UpdateRegInt(device, LIS2HH12_REG_CTRL3, LIS2HH12_CTRL3_FIFO_EN_BIT, ((uint8_t)device->cfg.fifoMode & 0x10U) ? LIS2HH12_CTRL3_FIFO_EN_BIT : 0U);
    if (lStatus != LIS2HH12_STATUS_OK) {
        return lStatus;
    }

    lStatus = lis2hh12UpdateRegInt(device, LIS2HH12_REG_CTRL3, LIS2HH12_CTRL3_STOP_FTH_BIT, (device->cfg.fifoWatermark == 0U) ? 0U : LIS2HH12_CTRL3_STOP_FTH_BIT);
    if (lStatus != LIS2HH12_STATUS_OK) {
        return lStatus;
    }

    lStatus = lis2hh12UpdateRegInt(device, LIS2HH12_REG_FIFO_CTRL, LIS2HH12_FIFO_CTRL_MODE_MASK, (uint8_t)(((uint8_t)device->cfg.fifoMode & 0x0FU) << 5U));
    if (lStatus != LIS2HH12_STATUS_OK) {
        return lStatus;
    }

    return lis2hh12UpdateRegInt(device, LIS2HH12_REG_FIFO_CTRL, LIS2HH12_FIFO_CTRL_FTH_MASK, device->cfg.fifoWatermark & LIS2HH12_FIFO_CTRL_FTH_MASK);
}

static eDrvStatus lis2hh12ProbeDevice(stLis2hh12Device *device)
{
    uint8_t lAttempt;
    uint8_t lDevId = 0U;
    eDrvStatus lStatus = LIS2HH12_STATUS_ERROR;

    for (lAttempt = 0U; lAttempt < device->cfg.retryMax; lAttempt++) {
        lStatus = lis2hh12ReadRegInt(device, LIS2HH12_REG_WHO_AM_I, &lDevId);
        if ((lStatus == LIS2HH12_STATUS_OK) && (lDevId == LIS2HH12_WHO_AM_I_EXPECTED)) {
            return LIS2HH12_STATUS_OK;
        }

        lis2hh12DelayMs(lis2hh12GetRetryDelayMs());
    }

    if (lStatus != LIS2HH12_STATUS_OK) {
        return lStatus;
    }

    return LIS2HH12_STATUS_DEVICE_ID_MISMATCH;
}

static eDrvStatus lis2hh12ResetDevice(stLis2hh12Device *device)
{
    uint8_t lAttempt;
    uint8_t lValue = 0U;
    eDrvStatus lStatus;

    lStatus = lis2hh12UpdateRegInt(device, LIS2HH12_REG_CTRL5, LIS2HH12_CTRL5_SOFT_RESET_BIT, LIS2HH12_CTRL5_SOFT_RESET_BIT);
    if (lStatus != LIS2HH12_STATUS_OK) {
        return lStatus;
    }

    for (lAttempt = 0U; lAttempt < device->cfg.retryMax; lAttempt++) {
        lStatus = lis2hh12ReadRegInt(device, LIS2HH12_REG_CTRL5, &lValue);
        if (lStatus != LIS2HH12_STATUS_OK) {
            return lStatus;
        }

        if ((lValue & LIS2HH12_CTRL5_SOFT_RESET_BIT) == 0U) {
            return LIS2HH12_STATUS_OK;
        }

        lis2hh12DelayMs(lis2hh12GetResetPollDelayMs());
    }

    return LIS2HH12_STATUS_TIMEOUT;
}

static eDrvStatus lis2hh12ReadFifoStatusInt(const stLis2hh12Device *device, stLis2hh12FifoStatus *status)
{
    uint8_t lRawStatus;
    eDrvStatus lStatus;

    if (status == NULL) {
        return LIS2HH12_STATUS_INVALID_PARAM;
    }

    lStatus = lis2hh12ReadRegInt(device, LIS2HH12_REG_FIFO_SRC, &lRawStatus);
    if (lStatus != LIS2HH12_STATUS_OK) {
        return lStatus;
    }

    lis2hh12ParseFifoStatus(lRawStatus, status);
    return LIS2HH12_STATUS_OK;
}

static eDrvStatus lis2hh12DrainFifoInt(stLis2hh12Device *device, stLis2hh12Sample *samples, uint8_t readCount, uint8_t maxSamples, uint8_t *sampleCount)
{
    uint8_t lIndex;
    uint8_t lBuffer[LIS2HH12_SAMPLE_BYTES];
    eDrvStatus lStatus;

    if (sampleCount != NULL) {
        *sampleCount = 0U;
    }

    for (lIndex = 0U; lIndex < readCount; lIndex++) {
        lStatus = lis2hh12ReadRegsInt(device, LIS2HH12_REG_OUT_X_L, lBuffer, LIS2HH12_SAMPLE_BYTES);
        if (lStatus != LIS2HH12_STATUS_OK) {
            return lStatus;
        }

        if ((samples != NULL) && (lIndex < maxSamples)) {
            samples[lIndex].x = lis2hh12ParseLe16(&lBuffer[0]);
            samples[lIndex].y = lis2hh12ParseLe16(&lBuffer[2]);
            samples[lIndex].z = lis2hh12ParseLe16(&lBuffer[4]);
            device->lastSample = samples[lIndex];
        }

        if (sampleCount != NULL) {
            *sampleCount = (uint8_t)(*sampleCount + 1U);
        }
    }

    return LIS2HH12_STATUS_OK;
}

static void lis2hh12ClrLastState(stLis2hh12Device *device)
{
    if (device == NULL) {
        return;
    }

    device->lastSample.x = 0;
    device->lastSample.y = 0;
    device->lastSample.z = 0;
    device->lastFifoStatus.sampleCount = 0U;
    device->lastFifoStatus.isEmpty = true;
    device->lastFifoStatus.isOverrun = false;
    device->lastFifoStatus.isThresholdReached = false;
    device->isReady = false;
}

static void lis2hh12ParseFifoStatus(uint8_t rawValue, stLis2hh12FifoStatus *status)
{
    if (status == NULL) {
        return;
    }

    status->sampleCount = (uint8_t)(rawValue & LIS2HH12_FIFO_SRC_FSS_MASK);
    status->isEmpty = ((rawValue & LIS2HH12_FIFO_SRC_EMPTY_BIT) != 0U);
    status->isOverrun = ((rawValue & LIS2HH12_FIFO_SRC_OVR_BIT) != 0U);
    status->isThresholdReached = ((rawValue & LIS2HH12_FIFO_SRC_FTH_BIT) != 0U);
}

static int16_t lis2hh12ParseLe16(const uint8_t *buffer)
{
    return (int16_t)((((uint16_t)buffer[1]) << 8U) | (uint16_t)buffer[0]);
}

/**************************End of file********************************/
