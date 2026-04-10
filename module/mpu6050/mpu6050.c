/***********************************************************************************
* @file     : mpu6050.c
* @brief    : MPU6050 sensor driver implementation.
* @details  : The driver configures and reads one explicitly selected MPU6050
*             device instance through the shared project port binding.
* @author   : GitHub Copilot
* @date     : 2026-04-01
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "mpu6050.h"

#include "mpu6050_assembly.h"

#include <stddef.h>

static stMpu6050Device gMpu6050Devices[MPU6050_DEV_MAX];
static bool gMpu6050DefCfgDone[MPU6050_DEV_MAX] = {false};

__attribute__((weak)) void mpu6050LoadPlatformDefaultCfg(eMPU6050MapType device, stMpu6050Cfg *cfg)
{
    (void)device;

    if (cfg == NULL) {
        return;
    }

    cfg->address = 0U;
    cfg->sampleRateDiv = 0U;
    cfg->dlpfCfg = 0U;
    cfg->accelRange = MPU6050_ACCEL_RANGE_2G;
    cfg->gyroRange = MPU6050_GYRO_RANGE_250DPS;
}

__attribute__((weak)) const stMpu6050IicInterface *mpu6050GetPlatformIicInterface(eMPU6050MapType device)
{
    (void)device;
    return NULL;
}

__attribute__((weak)) bool mpu6050PlatformIsValidAssemble(eMPU6050MapType device)
{
    (void)device;
    return false;
}

__attribute__((weak)) uint8_t mpu6050PlatformGetLinkId(eMPU6050MapType device)
{
    (void)device;
    return 0U;
}

__attribute__((weak)) uint32_t mpu6050PlatformGetResetDelayMs(void)
{
    return 0U;
}

__attribute__((weak)) uint32_t mpu6050PlatformGetWakeDelayMs(void)
{
    return 0U;
}

__attribute__((weak)) void mpu6050PlatformDelayMs(uint32_t delayMs)
{
    (void)delayMs;
}

static bool mpu6050IsValidDevMap(eMPU6050MapType device);
static stMpu6050Device *mpu6050GetDevCtx(eMPU6050MapType device);
static eMPU6050MapType mpu6050GetDevMapByCtx(const stMpu6050Device *device);
static void mpu6050LoadDefCfg(eMPU6050MapType device, stMpu6050Cfg *cfg);
static bool mpu6050IsValidCfg(const stMpu6050Cfg *cfg);
static bool mpu6050IsValidDev(const stMpu6050Device *device);
static bool mpu6050IsReadyXfer(const stMpu6050Device *device);
static bool mpu6050IsCompatDevId(uint8_t devId);
static const stMpu6050IicInterface *mpu6050GetIicIf(const stMpu6050Device *device);
static eDrvStatus mpu6050WriteRegInt(const stMpu6050Device *device, uint8_t regAddr, uint8_t value);
static eDrvStatus mpu6050ReadRegInt(const stMpu6050Device *device, uint8_t regAddr, uint8_t *value);
static eDrvStatus mpu6050ReadRegsInt(const stMpu6050Device *device, uint8_t regAddr, uint8_t *buffer, uint16_t length);
static void mpu6050ClrRawSample(stMpu6050RawSample *sample);
static int16_t mpu6050ParseBe16(const uint8_t *buffer);

eDrvStatus mpu6050GetDefCfg(eMPU6050MapType device, stMpu6050Cfg *cfg)
{
    if ((cfg == NULL) || !mpu6050IsValidDevMap(device)) {
        return MPU6050_STATUS_INVALID_PARAM;
    }

    mpu6050LoadDefCfg(device, cfg);
    return MPU6050_STATUS_OK;
}

eDrvStatus mpu6050GetCfg(eMPU6050MapType device, stMpu6050Cfg *cfg)
{
    stMpu6050Device *lDeviceCtx;

    if (cfg == NULL) {
        return MPU6050_STATUS_INVALID_PARAM;
    }

    lDeviceCtx = mpu6050GetDevCtx(device);
    if (lDeviceCtx == NULL) {
        return MPU6050_STATUS_INVALID_PARAM;
    }

    *cfg = lDeviceCtx->cfg;
    return MPU6050_STATUS_OK;
}

eDrvStatus mpu6050SetCfg(eMPU6050MapType device, const stMpu6050Cfg *cfg)
{
    stMpu6050Device *lDeviceCtx;
    stMpu6050Device lTmpDevice;

    if (cfg == NULL) {
        return MPU6050_STATUS_INVALID_PARAM;
    }

    lTmpDevice.cfg = *cfg;
    if (!mpu6050IsValidDev(&lTmpDevice)) {
        return MPU6050_STATUS_INVALID_PARAM;
    }

    lDeviceCtx = mpu6050GetDevCtx(device);
    if (lDeviceCtx == NULL) {
        return MPU6050_STATUS_INVALID_PARAM;
    }

    lDeviceCtx->cfg = *cfg;
    mpu6050ClrRawSample(&lDeviceCtx->data);
    lDeviceCtx->isReady = false;
    gMpu6050DefCfgDone[device] = true;
    return MPU6050_STATUS_OK;
}

eDrvStatus mpu6050Init(eMPU6050MapType device)
{
    const stMpu6050IicInterface *lIicIf;
    stMpu6050Device *lDeviceCtx;
    uint8_t lDevId;
    uint8_t lValue;
    eDrvStatus lStatus;

    lDeviceCtx = mpu6050GetDevCtx(device);
    if (!mpu6050IsValidDev(lDeviceCtx)) {
        return MPU6050_STATUS_INVALID_PARAM;
    }

    if (mpu6050GetIicIf(lDeviceCtx) == NULL) {
        return mpu6050PlatformIsValidAssemble(device) ?
               MPU6050_STATUS_NOT_READY :
               MPU6050_STATUS_INVALID_PARAM;
    }

    lIicIf = mpu6050GetIicIf(lDeviceCtx);
    lStatus = lIicIf->init(mpu6050PlatformGetLinkId(device));
    if (lStatus != MPU6050_STATUS_OK) {
        return lStatus;
    }

    lDeviceCtx->isReady = false;
    mpu6050ClrRawSample(&lDeviceCtx->data);

    lStatus = mpu6050ReadRegInt(lDeviceCtx, MPU6050_REG_WHO_AM_I, &lDevId);
    if (lStatus != MPU6050_STATUS_OK) {
        return lStatus;
    }

    if (!mpu6050IsCompatDevId(lDevId)) {
        return MPU6050_STATUS_DEVICE_ID_MISMATCH;
    }

    lStatus = mpu6050WriteRegInt(lDeviceCtx, MPU6050_REG_PWR_MGMT_1, MPU6050_PWR1_DEVICE_RESET_BIT);
    if (lStatus != MPU6050_STATUS_OK) {
        return lStatus;
    }

    mpu6050PlatformDelayMs(mpu6050PlatformGetResetDelayMs());

    lValue = MPU6050_PWR1_CLKSEL_PLL_XGYRO;
    lStatus = mpu6050WriteRegInt(lDeviceCtx, MPU6050_REG_PWR_MGMT_1, lValue);
    if (lStatus != MPU6050_STATUS_OK) {
        return lStatus;
    }

    mpu6050PlatformDelayMs(mpu6050PlatformGetWakeDelayMs());

    lStatus = mpu6050WriteRegInt(lDeviceCtx, MPU6050_REG_PWR_MGMT_2, 0U);
    if (lStatus != MPU6050_STATUS_OK) {
        return lStatus;
    }

    lStatus = mpu6050WriteRegInt(lDeviceCtx, MPU6050_REG_SMPLRT_DIV, lDeviceCtx->cfg.sampleRateDiv);
    if (lStatus != MPU6050_STATUS_OK) {
        return lStatus;
    }

    lStatus = mpu6050WriteRegInt(lDeviceCtx, MPU6050_REG_CONFIG, (uint8_t)(lDeviceCtx->cfg.dlpfCfg & 0x07U));
    if (lStatus != MPU6050_STATUS_OK) {
        return lStatus;
    }

    lStatus = mpu6050WriteRegInt(lDeviceCtx, MPU6050_REG_GYRO_CONFIG, (uint8_t)((uint8_t)lDeviceCtx->cfg.gyroRange << 3U));
    if (lStatus != MPU6050_STATUS_OK) {
        return lStatus;
    }

    lStatus = mpu6050WriteRegInt(lDeviceCtx, MPU6050_REG_ACCEL_CONFIG, (uint8_t)((uint8_t)lDeviceCtx->cfg.accelRange << 3U));
    if (lStatus != MPU6050_STATUS_OK) {
        return lStatus;
    }

    lDeviceCtx->isReady = true;
    return MPU6050_STATUS_OK;
}

bool mpu6050IsReady(eMPU6050MapType device)
{
    return mpu6050IsReadyXfer(mpu6050GetDevCtx(device));
}

eDrvStatus mpu6050ReadId(eMPU6050MapType device, uint8_t *devId)
{
    const stMpu6050IicInterface *lIicIf;
    stMpu6050Device *lDeviceCtx;
    eDrvStatus lStatus;

    lDeviceCtx = mpu6050GetDevCtx(device);
    if ((devId == NULL) || !mpu6050IsValidDev(lDeviceCtx)) {
        return MPU6050_STATUS_INVALID_PARAM;
    }

    lIicIf = mpu6050GetIicIf(lDeviceCtx);
    if (lIicIf == NULL) {
        return MPU6050_STATUS_NOT_READY;
    }

    lStatus = lIicIf->init(mpu6050PlatformGetLinkId(device));
    if (lStatus != MPU6050_STATUS_OK) {
        return lStatus;
    }

    return mpu6050ReadRegInt(lDeviceCtx, MPU6050_REG_WHO_AM_I, devId);
}

eDrvStatus mpu6050ReadReg(eMPU6050MapType device, uint8_t regAddr, uint8_t *value)
{
    stMpu6050Device *lDeviceCtx;

    lDeviceCtx = mpu6050GetDevCtx(device);
    if (!mpu6050IsReadyXfer(lDeviceCtx)) {
        return MPU6050_STATUS_NOT_READY;
    }

    return mpu6050ReadRegInt(lDeviceCtx, regAddr, value);
}

eDrvStatus mpu6050WriteReg(eMPU6050MapType device, uint8_t regAddr, uint8_t value)
{
    stMpu6050Device *lDeviceCtx;

    lDeviceCtx = mpu6050GetDevCtx(device);
    if (!mpu6050IsReadyXfer(lDeviceCtx)) {
        return MPU6050_STATUS_NOT_READY;
    }

    return mpu6050WriteRegInt(lDeviceCtx, regAddr, value);
}

eDrvStatus mpu6050SetSleep(eMPU6050MapType device, bool enable)
{
    stMpu6050Device *lDeviceCtx;
    uint8_t lValue;
    eDrvStatus lStatus;

    lDeviceCtx = mpu6050GetDevCtx(device);
    if (!mpu6050IsReadyXfer(lDeviceCtx)) {
        return MPU6050_STATUS_NOT_READY;
    }

    lStatus = mpu6050ReadRegInt(lDeviceCtx, MPU6050_REG_PWR_MGMT_1, &lValue);
    if (lStatus != MPU6050_STATUS_OK) {
        return lStatus;
    }

    if (enable) {
        lValue |= MPU6050_PWR1_SLEEP_BIT;
    } else {
        lValue &= (uint8_t)(~MPU6050_PWR1_SLEEP_BIT);
        lValue = (uint8_t)((lValue & (uint8_t)(~0x07U)) | MPU6050_PWR1_CLKSEL_PLL_XGYRO);
    }

    return mpu6050WriteRegInt(lDeviceCtx, MPU6050_REG_PWR_MGMT_1, lValue);
}

eDrvStatus mpu6050ReadRaw(eMPU6050MapType device, stMpu6050RawSample *sample)
{
    stMpu6050Device *lDeviceCtx;
    uint8_t lBuffer[MPU6050_SAMPLE_BYTES];
    eDrvStatus lStatus;

    if (sample == NULL) {
        return MPU6050_STATUS_INVALID_PARAM;
    }

    lDeviceCtx = mpu6050GetDevCtx(device);
    if (!mpu6050IsReadyXfer(lDeviceCtx)) {
        return MPU6050_STATUS_NOT_READY;
    }

    lStatus = mpu6050ReadRegsInt(lDeviceCtx, MPU6050_REG_ACCEL_XOUT_H, lBuffer, MPU6050_SAMPLE_BYTES);
    if (lStatus != MPU6050_STATUS_OK) {
        return lStatus;
    }

    lDeviceCtx->data.accelX = mpu6050ParseBe16(&lBuffer[0]);
    lDeviceCtx->data.accelY = mpu6050ParseBe16(&lBuffer[2]);
    lDeviceCtx->data.accelZ = mpu6050ParseBe16(&lBuffer[4]);
    lDeviceCtx->data.temperature = mpu6050ParseBe16(&lBuffer[6]);
    lDeviceCtx->data.gyroX = mpu6050ParseBe16(&lBuffer[8]);
    lDeviceCtx->data.gyroY = mpu6050ParseBe16(&lBuffer[10]);
    lDeviceCtx->data.gyroZ = mpu6050ParseBe16(&lBuffer[12]);
    *sample = lDeviceCtx->data;
    return MPU6050_STATUS_OK;
}

eDrvStatus mpu6050ReadTempCdC(eMPU6050MapType device, int32_t *tempCdC)
{
    stMpu6050Device *lDeviceCtx;
    uint8_t lBuffer[2];
    int16_t lRawTemp;
    eDrvStatus lStatus;

    if (tempCdC == NULL) {
        return MPU6050_STATUS_INVALID_PARAM;
    }

    lDeviceCtx = mpu6050GetDevCtx(device);
    if (!mpu6050IsReadyXfer(lDeviceCtx)) {
        return MPU6050_STATUS_NOT_READY;
    }

    lStatus = mpu6050ReadRegsInt(lDeviceCtx, MPU6050_REG_TEMP_OUT_H, lBuffer, 2U);
    if (lStatus != MPU6050_STATUS_OK) {
        return lStatus;
    }

    lRawTemp = mpu6050ParseBe16(lBuffer);
    *tempCdC = (((int32_t)lRawTemp * 100) / 340) + 3653;
    return MPU6050_STATUS_OK;
}

static bool mpu6050IsValidDevMap(eMPU6050MapType device)
{
    return ((uint32_t)device < (uint32_t)MPU6050_DEV_MAX);
}

static stMpu6050Device *mpu6050GetDevCtx(eMPU6050MapType device)
{
    if (!mpu6050IsValidDevMap(device)) {
        return NULL;
    }

    if (!gMpu6050DefCfgDone[device]) {
        mpu6050LoadDefCfg(device, &gMpu6050Devices[device].cfg);
        mpu6050ClrRawSample(&gMpu6050Devices[device].data);
        gMpu6050Devices[device].isReady = false;
        gMpu6050DefCfgDone[device] = true;
    }

    return &gMpu6050Devices[device];
}

static eMPU6050MapType mpu6050GetDevMapByCtx(const stMpu6050Device *device)
{
    ptrdiff_t lIndex;

    if ((device == NULL) ||
        (device < &gMpu6050Devices[0]) ||
        (device >= &gMpu6050Devices[MPU6050_DEV_MAX])) {
        return MPU6050_DEV_MAX;
    }

    lIndex = device - &gMpu6050Devices[0];
    return (eMPU6050MapType)lIndex;
}

static void mpu6050LoadDefCfg(eMPU6050MapType device, stMpu6050Cfg *cfg)
{
    if (cfg == NULL) {
        return;
    }

    mpu6050LoadPlatformDefaultCfg(device, cfg);
}

static bool mpu6050IsValidCfg(const stMpu6050Cfg *cfg)
{
    if (cfg == NULL) {
        return false;
    }

    if ((cfg->address != MPU6050_IIC_ADDRESS_LOW) &&
        (cfg->address != MPU6050_IIC_ADDRESS_HIGH)) {
        return false;
    }

    if (cfg->dlpfCfg > 6U) {
        return false;
    }

    if (cfg->accelRange >= MPU6050_ACCEL_RANGE_MAX) {
        return false;
    }

    if (cfg->gyroRange >= MPU6050_GYRO_RANGE_MAX) {
        return false;
    }

    return true;
}

static bool mpu6050IsValidDev(const stMpu6050Device *device)
{
    return (device != NULL) && mpu6050IsValidCfg(&device->cfg);
}

static bool mpu6050IsReadyXfer(const stMpu6050Device *device)
{
    return (device != NULL) &&
           device->isReady &&
           (mpu6050GetIicIf(device) != NULL);
}

static bool mpu6050IsCompatDevId(uint8_t devId)
{
    return (devId == MPU6050_WHO_AM_I_EXPECTED) ||
           (devId == MPU6050_WHO_AM_I_COMPATIBLE_6500);
}

static const stMpu6050IicInterface *mpu6050GetIicIf(const stMpu6050Device *device)
{
    eMPU6050MapType lDevice;

    if ((device == NULL) || !mpu6050IsValidCfg(&device->cfg)) {
        return NULL;
    }

    lDevice = mpu6050GetDevMapByCtx(device);
    if ((lDevice >= MPU6050_DEV_MAX) || !mpu6050PlatformIsValidAssemble(lDevice)) {
        return NULL;
    }

    return mpu6050GetPlatformIicInterface(lDevice);
}

static eDrvStatus mpu6050WriteRegInt(const stMpu6050Device *device, uint8_t regAddr, uint8_t value)
{
    const stMpu6050IicInterface *lIicIf;
    eMPU6050MapType lDevice;

    lIicIf = mpu6050GetIicIf(device);
    if ((lIicIf == NULL) || (lIicIf->writeReg == NULL)) {
        return MPU6050_STATUS_NOT_READY;
    }

    lDevice = mpu6050GetDevMapByCtx(device);
    if (lDevice >= MPU6050_DEV_MAX) {
        return MPU6050_STATUS_INVALID_PARAM;
    }

    return lIicIf->writeReg(mpu6050PlatformGetLinkId(lDevice), device->cfg.address, &regAddr, 1U, &value, 1U);
}

static eDrvStatus mpu6050ReadRegInt(const stMpu6050Device *device, uint8_t regAddr, uint8_t *value)
{
    const stMpu6050IicInterface *lIicIf;
    eMPU6050MapType lDevice;

    lIicIf = mpu6050GetIicIf(device);
    if ((lIicIf == NULL) || (lIicIf->readReg == NULL)) {
        return MPU6050_STATUS_NOT_READY;
    }

    if (value == NULL) {
        return MPU6050_STATUS_INVALID_PARAM;
    }

    lDevice = mpu6050GetDevMapByCtx(device);
    if (lDevice >= MPU6050_DEV_MAX) {
        return MPU6050_STATUS_INVALID_PARAM;
    }

    return lIicIf->readReg(mpu6050PlatformGetLinkId(lDevice), device->cfg.address, &regAddr, 1U, value, 1U);
}

static eDrvStatus mpu6050ReadRegsInt(const stMpu6050Device *device, uint8_t regAddr, uint8_t *buffer, uint16_t length)
{
    const stMpu6050IicInterface *lIicIf;
    eMPU6050MapType lDevice;

    lIicIf = mpu6050GetIicIf(device);
    if ((lIicIf == NULL) || (lIicIf->readReg == NULL)) {
        return MPU6050_STATUS_NOT_READY;
    }

    if ((buffer == NULL) || (length == 0U)) {
        return MPU6050_STATUS_INVALID_PARAM;
    }

    lDevice = mpu6050GetDevMapByCtx(device);
    if (lDevice >= MPU6050_DEV_MAX) {
        return MPU6050_STATUS_INVALID_PARAM;
    }

    return lIicIf->readReg(mpu6050PlatformGetLinkId(lDevice), device->cfg.address, &regAddr, 1U, buffer, length);
}

static void mpu6050ClrRawSample(stMpu6050RawSample *sample)
{
    if (sample == NULL) {
        return;
    }

    sample->accelX = 0;
    sample->accelY = 0;
    sample->accelZ = 0;
    sample->temperature = 0;
    sample->gyroX = 0;
    sample->gyroY = 0;
    sample->gyroZ = 0;
}

static int16_t mpu6050ParseBe16(const uint8_t *buffer)
{
    uint16_t lValue;

    lValue = ((uint16_t)buffer[0] << 8U) | (uint16_t)buffer[1];
    return (int16_t)lValue;
}

/**************************End of file********************************/
