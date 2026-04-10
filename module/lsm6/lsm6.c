/***********************************************************************************
* @file     : lsm6.c
* @brief    : LSM6 family IMU module implementation.
* @details  : The module provides a passive bring-up and raw sample read flow
*             for LSM6DS3/LSM6DSL/LSM6DSO class devices that share the common
*             control and data register layout.
**********************************************************************************/
#include "lsm6.h"

#include "lsm6_assembly.h"

#include <stddef.h>

static stLsm6Device gLsm6Devices[LSM6_DEV_MAX];
static bool gLsm6DefCfgDone[LSM6_DEV_MAX] = {false};

__attribute__((weak)) void lsm6LoadPlatformDefaultCfg(eLsm6MapType device, stLsm6Cfg *cfg)
{
    (void)device;

    if (cfg == NULL) {
        return;
    }

    cfg->address = LSM6_IIC_ADDRESS_LOW;
    cfg->accelDataRate = LSM6_ACCEL_ODR_104HZ;
    cfg->gyroDataRate = LSM6_GYRO_ODR_104HZ;
    cfg->accelRange = LSM6_ACCEL_RANGE_4G;
    cfg->gyroRange = LSM6_GYRO_RANGE_2000DPS;
    cfg->blockDataUpdate = true;
    cfg->autoIncrement = true;
}

__attribute__((weak)) const stLsm6IicInterface *lsm6GetPlatformIicInterface(eLsm6MapType device)
{
    (void)device;
    return NULL;
}

__attribute__((weak)) bool lsm6PlatformIsValidAssemble(eLsm6MapType device)
{
    (void)device;
    return false;
}

__attribute__((weak)) uint8_t lsm6PlatformGetLinkId(eLsm6MapType device)
{
    (void)device;
    return 0U;
}

__attribute__((weak)) uint32_t lsm6PlatformGetResetDelayMs(void)
{
    return 10U;
}

__attribute__((weak)) uint32_t lsm6PlatformGetResetPollDelayMs(void)
{
    return 2U;
}

__attribute__((weak)) void lsm6PlatformDelayMs(uint32_t delayMs)
{
    (void)delayMs;
}

static bool lsm6IsValidDevMap(eLsm6MapType device);
static stLsm6Device *lsm6GetDevCtx(eLsm6MapType device);
static eLsm6MapType lsm6GetDevMapByCtx(const stLsm6Device *device);
static void lsm6LoadDefCfg(eLsm6MapType device, stLsm6Cfg *cfg);
static bool lsm6IsValidCfg(const stLsm6Cfg *cfg);
static bool lsm6IsValidDev(const stLsm6Device *device);
static bool lsm6IsReadyXfer(const stLsm6Device *device);
static bool lsm6IsCompatDevId(uint8_t devId);
static const stLsm6IicInterface *lsm6GetIicIf(const stLsm6Device *device);
static eDrvStatus lsm6WriteRegInt(const stLsm6Device *device, uint8_t regAddr, uint8_t value);
static eDrvStatus lsm6ReadRegInt(const stLsm6Device *device, uint8_t regAddr, uint8_t *value);
static eDrvStatus lsm6ReadRegsInt(const stLsm6Device *device, uint8_t regAddr, uint8_t *buffer, uint16_t length);
static eDrvStatus lsm6ProbeDevice(stLsm6Device *device);
static eDrvStatus lsm6ResetDevice(stLsm6Device *device);
static eDrvStatus lsm6ApplyInitCfg(stLsm6Device *device);
static void lsm6ClrLastState(stLsm6Device *device);
static int16_t lsm6ParseLe16(const uint8_t *buffer);

eDrvStatus lsm6GetDefCfg(eLsm6MapType device, stLsm6Cfg *cfg)
{
    if ((cfg == NULL) || !lsm6IsValidDevMap(device)) {
        return LSM6_STATUS_INVALID_PARAM;
    }

    lsm6LoadDefCfg(device, cfg);
    return LSM6_STATUS_OK;
}

eDrvStatus lsm6GetCfg(eLsm6MapType device, stLsm6Cfg *cfg)
{
    stLsm6Device *lDeviceCtx;

    if (cfg == NULL) {
        return LSM6_STATUS_INVALID_PARAM;
    }

    lDeviceCtx = lsm6GetDevCtx(device);
    if (lDeviceCtx == NULL) {
        return LSM6_STATUS_INVALID_PARAM;
    }

    *cfg = lDeviceCtx->cfg;
    return LSM6_STATUS_OK;
}

eDrvStatus lsm6SetCfg(eLsm6MapType device, const stLsm6Cfg *cfg)
{
    stLsm6Device *lDeviceCtx;
    stLsm6Device lTmpDevice;

    if (cfg == NULL) {
        return LSM6_STATUS_INVALID_PARAM;
    }

    lTmpDevice.cfg = *cfg;
    if (!lsm6IsValidDev(&lTmpDevice)) {
        return LSM6_STATUS_INVALID_PARAM;
    }

    lDeviceCtx = lsm6GetDevCtx(device);
    if (lDeviceCtx == NULL) {
        return LSM6_STATUS_INVALID_PARAM;
    }

    lDeviceCtx->cfg = *cfg;
    lsm6ClrLastState(lDeviceCtx);
    gLsm6DefCfgDone[device] = true;
    return LSM6_STATUS_OK;
}

eDrvStatus lsm6Init(eLsm6MapType device)
{
    const stLsm6IicInterface *lIicIf;
    stLsm6Device *lDeviceCtx;
    eDrvStatus lStatus;

    lDeviceCtx = lsm6GetDevCtx(device);
    if (!lsm6IsValidDev(lDeviceCtx)) {
        return LSM6_STATUS_INVALID_PARAM;
    }

    if (lsm6GetIicIf(lDeviceCtx) == NULL) {
        return lsm6PlatformIsValidAssemble(device) ?
               LSM6_STATUS_NOT_READY :
               LSM6_STATUS_INVALID_PARAM;
    }

    lIicIf = lsm6GetIicIf(lDeviceCtx);
    lStatus = lIicIf->init(lsm6PlatformGetLinkId(device));
    if (lStatus != LSM6_STATUS_OK) {
        return lStatus;
    }

    lsm6ClrLastState(lDeviceCtx);

    lStatus = lsm6ProbeDevice(lDeviceCtx);
    if (lStatus != LSM6_STATUS_OK) {
        return lStatus;
    }

    lStatus = lsm6ResetDevice(lDeviceCtx);
    if (lStatus != LSM6_STATUS_OK) {
        return lStatus;
    }

    lStatus = lsm6ApplyInitCfg(lDeviceCtx);
    if (lStatus != LSM6_STATUS_OK) {
        return lStatus;
    }

    lDeviceCtx->isReady = true;
    return LSM6_STATUS_OK;
}

bool lsm6IsReady(eLsm6MapType device)
{
    return lsm6IsReadyXfer(lsm6GetDevCtx(device));
}

eDrvStatus lsm6ReadId(eLsm6MapType device, uint8_t *devId)
{
    const stLsm6IicInterface *lIicIf;
    stLsm6Device *lDeviceCtx;
    eDrvStatus lStatus;

    lDeviceCtx = lsm6GetDevCtx(device);
    if ((devId == NULL) || !lsm6IsValidDev(lDeviceCtx)) {
        return LSM6_STATUS_INVALID_PARAM;
    }

    lIicIf = lsm6GetIicIf(lDeviceCtx);
    if (lIicIf == NULL) {
        return LSM6_STATUS_NOT_READY;
    }

    lStatus = lIicIf->init(lsm6PlatformGetLinkId(device));
    if (lStatus != LSM6_STATUS_OK) {
        return lStatus;
    }

    return lsm6ReadRegInt(lDeviceCtx, LSM6_REG_WHO_AM_I, devId);
}

eDrvStatus lsm6ReadReg(eLsm6MapType device, uint8_t regAddr, uint8_t *value)
{
    stLsm6Device *lDeviceCtx;

    lDeviceCtx = lsm6GetDevCtx(device);
    if ((value == NULL) || !lsm6IsReadyXfer(lDeviceCtx)) {
        return (value == NULL) ? LSM6_STATUS_INVALID_PARAM : LSM6_STATUS_NOT_READY;
    }

    return lsm6ReadRegInt(lDeviceCtx, regAddr, value);
}

eDrvStatus lsm6WriteReg(eLsm6MapType device, uint8_t regAddr, uint8_t value)
{
    stLsm6Device *lDeviceCtx;

    lDeviceCtx = lsm6GetDevCtx(device);
    if (!lsm6IsReadyXfer(lDeviceCtx)) {
        return LSM6_STATUS_NOT_READY;
    }

    return lsm6WriteRegInt(lDeviceCtx, regAddr, value);
}

eDrvStatus lsm6ReadStatus(eLsm6MapType device, uint8_t *status)
{
    return lsm6ReadReg(device, LSM6_REG_STATUS, status);
}

eDrvStatus lsm6ReadRaw(eLsm6MapType device, stLsm6RawSample *sample)
{
    stLsm6Device *lDeviceCtx;
    uint8_t lBuffer[LSM6_SAMPLE_BYTES];
    eDrvStatus lStatus;

    if (sample == NULL) {
        return LSM6_STATUS_INVALID_PARAM;
    }

    lDeviceCtx = lsm6GetDevCtx(device);
    if (!lsm6IsReadyXfer(lDeviceCtx)) {
        return LSM6_STATUS_NOT_READY;
    }

    lStatus = lsm6ReadRegsInt(lDeviceCtx, LSM6_REG_OUT_TEMP_L, lBuffer, LSM6_SAMPLE_BYTES);
    if (lStatus != LSM6_STATUS_OK) {
        return lStatus;
    }

    lDeviceCtx->data.temperature = lsm6ParseLe16(&lBuffer[0]);
    lDeviceCtx->data.gyroX = lsm6ParseLe16(&lBuffer[2]);
    lDeviceCtx->data.gyroY = lsm6ParseLe16(&lBuffer[4]);
    lDeviceCtx->data.gyroZ = lsm6ParseLe16(&lBuffer[6]);
    lDeviceCtx->data.accelX = lsm6ParseLe16(&lBuffer[8]);
    lDeviceCtx->data.accelY = lsm6ParseLe16(&lBuffer[10]);
    lDeviceCtx->data.accelZ = lsm6ParseLe16(&lBuffer[12]);
    *sample = lDeviceCtx->data;
    return LSM6_STATUS_OK;
}

eDrvStatus lsm6ReadTempCdC(eLsm6MapType device, int32_t *tempCdC)
{
    stLsm6RawSample lSample;
    eDrvStatus lStatus;

    if (tempCdC == NULL) {
        return LSM6_STATUS_INVALID_PARAM;
    }

    lStatus = lsm6ReadRaw(device, &lSample);
    if (lStatus != LSM6_STATUS_OK) {
        return lStatus;
    }

    *tempCdC = (((int32_t)lSample.temperature * 100) / 256) + 2500;
    return LSM6_STATUS_OK;
}

static bool lsm6IsValidDevMap(eLsm6MapType device)
{
    return ((uint32_t)device < (uint32_t)LSM6_DEV_MAX);
}

static stLsm6Device *lsm6GetDevCtx(eLsm6MapType device)
{
    if (!lsm6IsValidDevMap(device)) {
        return NULL;
    }

    if (!gLsm6DefCfgDone[device]) {
        lsm6LoadDefCfg(device, &gLsm6Devices[device].cfg);
        lsm6ClrLastState(&gLsm6Devices[device]);
        gLsm6DefCfgDone[device] = true;
    }

    return &gLsm6Devices[device];
}

static eLsm6MapType lsm6GetDevMapByCtx(const stLsm6Device *device)
{
    ptrdiff_t lIndex;

    if ((device == NULL) ||
        (device < &gLsm6Devices[0]) ||
        (device >= &gLsm6Devices[LSM6_DEV_MAX])) {
        return LSM6_DEV_MAX;
    }

    lIndex = device - &gLsm6Devices[0];
    return (eLsm6MapType)lIndex;
}

static void lsm6LoadDefCfg(eLsm6MapType device, stLsm6Cfg *cfg)
{
    lsm6LoadPlatformDefaultCfg(device, cfg);

    if (!lsm6IsValidCfg(cfg)) {
        cfg->address = LSM6_IIC_ADDRESS_LOW;
        cfg->accelDataRate = LSM6_ACCEL_ODR_104HZ;
        cfg->gyroDataRate = LSM6_GYRO_ODR_104HZ;
        cfg->accelRange = LSM6_ACCEL_RANGE_4G;
        cfg->gyroRange = LSM6_GYRO_RANGE_2000DPS;
        cfg->blockDataUpdate = true;
        cfg->autoIncrement = true;
    }
}

static bool lsm6IsValidCfg(const stLsm6Cfg *cfg)
{
    if (cfg == NULL) {
        return false;
    }

    if ((cfg->address != LSM6_IIC_ADDRESS_LOW) && (cfg->address != LSM6_IIC_ADDRESS_HIGH)) {
        return false;
    }

    if ((uint32_t)cfg->accelDataRate > (uint32_t)LSM6_ACCEL_ODR_1660HZ) {
        return false;
    }

    if ((uint32_t)cfg->gyroDataRate > (uint32_t)LSM6_GYRO_ODR_1660HZ) {
        return false;
    }

    if ((uint32_t)cfg->accelRange > (uint32_t)LSM6_ACCEL_RANGE_8G) {
        return false;
    }

    if ((uint32_t)cfg->gyroRange > (uint32_t)LSM6_GYRO_RANGE_2000DPS) {
        return false;
    }

    return true;
}

static bool lsm6IsValidDev(const stLsm6Device *device)
{
    return (device != NULL) && lsm6IsValidCfg(&device->cfg);
}

static bool lsm6IsReadyXfer(const stLsm6Device *device)
{
    return lsm6IsValidDev(device) && device->isReady;
}

static bool lsm6IsCompatDevId(uint8_t devId)
{
    return (devId == LSM6_WHO_AM_I_LSM6DS3) ||
           (devId == LSM6_WHO_AM_I_LSM6DSL) ||
           (devId == LSM6_WHO_AM_I_LSM6DSM) ||
           (devId == LSM6_WHO_AM_I_LSM6DSO);
}

static const stLsm6IicInterface *lsm6GetIicIf(const stLsm6Device *device)
{
    eLsm6MapType lDevice;

    lDevice = lsm6GetDevMapByCtx(device);
    if (lDevice == LSM6_DEV_MAX) {
        return NULL;
    }

    return lsm6GetPlatformIicInterface(lDevice);
}

static eDrvStatus lsm6WriteRegInt(const stLsm6Device *device, uint8_t regAddr, uint8_t value)
{
    const stLsm6IicInterface *lIicIf = lsm6GetIicIf(device);

    if ((device == NULL) || (lIicIf == NULL)) {
        return LSM6_STATUS_NOT_READY;
    }

    return lIicIf->writeReg(lsm6PlatformGetLinkId(lsm6GetDevMapByCtx(device)),
                            device->cfg.address,
                            &regAddr,
                            1U,
                            &value,
                            1U);
}

static eDrvStatus lsm6ReadRegInt(const stLsm6Device *device, uint8_t regAddr, uint8_t *value)
{
    return lsm6ReadRegsInt(device, regAddr, value, 1U);
}

static eDrvStatus lsm6ReadRegsInt(const stLsm6Device *device, uint8_t regAddr, uint8_t *buffer, uint16_t length)
{
    const stLsm6IicInterface *lIicIf = lsm6GetIicIf(device);

    if ((device == NULL) || (buffer == NULL) || (length == 0U)) {
        return LSM6_STATUS_INVALID_PARAM;
    }

    if (lIicIf == NULL) {
        return LSM6_STATUS_NOT_READY;
    }

    return lIicIf->readReg(lsm6PlatformGetLinkId(lsm6GetDevMapByCtx(device)),
                           device->cfg.address,
                           &regAddr,
                           1U,
                           buffer,
                           length);
}

static eDrvStatus lsm6ProbeDevice(stLsm6Device *device)
{
    eDrvStatus lStatus;

    lStatus = lsm6ReadRegInt(device, LSM6_REG_WHO_AM_I, &device->lastWhoAmI);
    if (lStatus != LSM6_STATUS_OK) {
        return lStatus;
    }

    if (!lsm6IsCompatDevId(device->lastWhoAmI)) {
        return LSM6_STATUS_DEVICE_ID_MISMATCH;
    }

    return LSM6_STATUS_OK;
}

static eDrvStatus lsm6ResetDevice(stLsm6Device *device)
{
    uint8_t lCtrl3 = 0U;
    uint8_t lRetry;
    eDrvStatus lStatus;

    lStatus = lsm6WriteRegInt(device, LSM6_REG_CTRL3_C, LSM6_CTRL3_SW_RESET_BIT);
    if (lStatus != LSM6_STATUS_OK) {
        return lStatus;
    }

    lsm6PlatformDelayMs(lsm6PlatformGetResetDelayMs());

    for (lRetry = 0U; lRetry < 10U; ++lRetry) {
        lStatus = lsm6ReadRegInt(device, LSM6_REG_CTRL3_C, &lCtrl3);
        if (lStatus != LSM6_STATUS_OK) {
            return lStatus;
        }

        if ((lCtrl3 & LSM6_CTRL3_SW_RESET_BIT) == 0U) {
            return LSM6_STATUS_OK;
        }

        lsm6PlatformDelayMs(lsm6PlatformGetResetPollDelayMs());
    }

    return LSM6_STATUS_TIMEOUT;
}

static eDrvStatus lsm6ApplyInitCfg(stLsm6Device *device)
{
    uint8_t lCtrl3 = 0U;
    uint8_t lCtrl1Xl;
    uint8_t lCtrl2G;
    eDrvStatus lStatus;

    if (device->cfg.blockDataUpdate) {
        lCtrl3 |= LSM6_CTRL3_BDU_BIT;
    }

    if (device->cfg.autoIncrement) {
        lCtrl3 |= LSM6_CTRL3_IF_INC_BIT;
    }

    lCtrl1Xl = (uint8_t)(((uint8_t)device->cfg.accelDataRate << 4U) |
                         ((uint8_t)device->cfg.accelRange << 2U));
    lCtrl2G = (uint8_t)(((uint8_t)device->cfg.gyroDataRate << 4U) |
                        ((uint8_t)device->cfg.gyroRange << 2U));

    lStatus = lsm6WriteRegInt(device, LSM6_REG_CTRL3_C, lCtrl3);
    if (lStatus != LSM6_STATUS_OK) {
        return lStatus;
    }

    lStatus = lsm6WriteRegInt(device, LSM6_REG_CTRL1_XL, lCtrl1Xl);
    if (lStatus != LSM6_STATUS_OK) {
        return lStatus;
    }

    return lsm6WriteRegInt(device, LSM6_REG_CTRL2_G, lCtrl2G);
}

static void lsm6ClrLastState(stLsm6Device *device)
{
    if (device == NULL) {
        return;
    }

    device->data.temperature = 0;
    device->data.gyroX = 0;
    device->data.gyroY = 0;
    device->data.gyroZ = 0;
    device->data.accelX = 0;
    device->data.accelY = 0;
    device->data.accelZ = 0;
    device->lastWhoAmI = 0U;
    device->isReady = false;
}

static int16_t lsm6ParseLe16(const uint8_t *buffer)
{
    return (int16_t)((int16_t)((uint16_t)buffer[1] << 8U) | buffer[0]);
}
/**************************End of file********************************/