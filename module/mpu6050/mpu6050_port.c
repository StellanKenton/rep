/***********************************************************************************
* @file     : mpu6050_port.c
* @brief    : MPU6050 project port-layer implementation.
* @details  : This file binds each MPU6050 device instance to either the
*             hardware or software IIC drv implementation at runtime.
* @author   : GitHub Copilot
* @date     : 2026-04-01
* @version  : V1.0.0
**********************************************************************************/
#include "mpu6050_port.h"

#include "mpu6050.h"

#include <stdbool.h>

#include "rep_config.h"
#include "drvanlogiic_port.h"
#include "drviic_port.h"

#if (REP_RTOS_SYSTEM == REP_RTOS_FREERTOS)
#include "FreeRTOS.h"
#include "task.h"
#endif

#if (REP_MCU_PLATFORM == REP_MCU_PLATFORM_GD32)
#include "gd32f4xx.h"
#endif

static bool gMpu6050PortCycleCntReady = false;
static bool gMpu6050PortAssembleCfgDone[MPU6050_DEV_MAX] = {false};

static bool mpu6050PortIsValidDevMap(eMPU6050MapType device);
static stMpu6050PortAssembleCfg *mpu6050PortGetAssembleCfgCtx(eMPU6050MapType device);

static void mpu6050PortEnableCycleCnt(void);
static eDrvStatus mpu6050PortSoftIicInitAdpt(uint8_t bus);
static eDrvStatus mpu6050PortSoftIicWriteRegAdpt(uint8_t bus, uint8_t address, const uint8_t *regBuf, uint16_t regLen, const uint8_t *buffer, uint16_t length);
static eDrvStatus mpu6050PortSoftIicReadRegAdpt(uint8_t bus, uint8_t address, const uint8_t *regBuf, uint16_t regLen, uint8_t *buffer, uint16_t length);
static eDrvStatus mpu6050PortHardIicInitAdpt(uint8_t bus);
static eDrvStatus mpu6050PortHardIicWriteRegAdpt(uint8_t bus, uint8_t address, const uint8_t *regBuf, uint16_t regLen, const uint8_t *buffer, uint16_t length);
static eDrvStatus mpu6050PortHardIicReadRegAdpt(uint8_t bus, uint8_t address, const uint8_t *regBuf, uint16_t regLen, uint8_t *buffer, uint16_t length);
static const stMpu6050PortIicInterface *mpu6050PortGetBindIicIf(eMpu6050TransportType type);

static const stMpu6050PortIicInterface gMpu6050PortIicInterfaces[MPU6050_TRANSPORT_TYPE_MAX] = {
    [MPU6050_TRANSPORT_TYPE_SOFTWARE] = {
        .init = mpu6050PortSoftIicInitAdpt,
        .writeReg = mpu6050PortSoftIicWriteRegAdpt,
        .readReg = mpu6050PortSoftIicReadRegAdpt,
    },
    [MPU6050_TRANSPORT_TYPE_HARDWARE] = {
        .init = mpu6050PortHardIicInitAdpt,
        .writeReg = mpu6050PortHardIicWriteRegAdpt,
        .readReg = mpu6050PortHardIicReadRegAdpt,
    },
};

static const stMpu6050Cfg gMpu6050PortDefCfg[MPU6050_DEV_MAX] = {
    [MPU6050_DEV0] = {
        .address = MPU6050_IIC_ADDRESS_LOW,
        .sampleRateDiv = 0U,
        .dlpfCfg = 3U,
        .accelRange = MPU6050_ACCEL_RANGE_4G,
        .gyroRange = MPU6050_GYRO_RANGE_2000DPS,
    },
    [MPU6050_DEV1] = {
        .address = MPU6050_IIC_ADDRESS_HIGH,
        .sampleRateDiv = 0U,
        .dlpfCfg = 3U,
        .accelRange = MPU6050_ACCEL_RANGE_4G,
        .gyroRange = MPU6050_GYRO_RANGE_2000DPS,
    },
};

static const stMpu6050PortAssembleCfg gMpu6050PortDefAssembleCfg[MPU6050_DEV_MAX] = {
    [MPU6050_DEV0] = {
        .transportType = MPU6050_TRANSPORT_TYPE_HARDWARE,
        .linkId = (uint8_t)DRVIIC_BUS0,
    },
    [MPU6050_DEV1] = {
        .transportType = MPU6050_TRANSPORT_TYPE_HARDWARE,
        .linkId = (uint8_t)DRVIIC_BUS0,
    },
};

static stMpu6050PortAssembleCfg gMpu6050PortAssembleCfg[MPU6050_DEV_MAX];

void mpu6050LoadPlatformDefaultCfg(eMPU6050MapType device, stMpu6050Cfg *cfg)
{
    if ((cfg == NULL) || ((uint32_t)device >= (uint32_t)MPU6050_DEV_MAX)) {
        return;
    }

    *cfg = gMpu6050PortDefCfg[device];
}

const stMpu6050IicInterface *mpu6050GetPlatformIicInterface(eMPU6050MapType device)
{
    stMpu6050PortAssembleCfg *lCfg;

    lCfg = mpu6050PortGetAssembleCfgCtx(device);
    if ((lCfg == NULL) || !mpu6050PortIsValidAssembleCfg(lCfg)) {
        return NULL;
    }

    return mpu6050PortGetBindIicIf(lCfg->transportType);
}

bool mpu6050PlatformIsValidAssemble(eMPU6050MapType device)
{
    stMpu6050PortAssembleCfg *lCfg;

    lCfg = mpu6050PortGetAssembleCfgCtx(device);
    return (lCfg != NULL) && mpu6050PortIsValidAssembleCfg(lCfg);
}

uint8_t mpu6050PlatformGetLinkId(eMPU6050MapType device)
{
    stMpu6050PortAssembleCfg *lCfg;

    lCfg = mpu6050PortGetAssembleCfgCtx(device);
    return (lCfg != NULL) ? lCfg->linkId : 0U;
}

uint32_t mpu6050PlatformGetResetDelayMs(void)
{
    return MPU6050_PORT_RESET_DELAY_MS;
}

uint32_t mpu6050PlatformGetWakeDelayMs(void)
{
    return MPU6050_PORT_WAKE_DELAY_MS;
}

void mpu6050PlatformDelayMs(uint32_t delayMs)
{
#if (REP_RTOS_SYSTEM == REP_RTOS_FREERTOS)
    TickType_t lDelayTicks;

    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        lDelayTicks = pdMS_TO_TICKS(delayMs);
        if ((delayMs > 0U) && (lDelayTicks == 0U)) {
            lDelayTicks = 1U;
        }
        vTaskDelay(lDelayTicks);
        return;
    }
#endif

#if (REP_MCU_PLATFORM == REP_MCU_PLATFORM_GD32)
    uint32_t lCyclesPerMs;
    uint32_t lStartCycles;
    uint32_t lWaitCycles;

    if (delayMs == 0U) {
        return;
    }

    mpu6050PortEnableCycleCnt();
    lCyclesPerMs = SystemCoreClock / 1000U;
    if (lCyclesPerMs == 0U) {
        lCyclesPerMs = 1U;
    }

    lWaitCycles = lCyclesPerMs * delayMs;
    lStartCycles = DWT->CYCCNT;
    while ((DWT->CYCCNT - lStartCycles) < lWaitCycles) {
        __NOP();
    }
#else
    volatile uint32_t lOuter;
    volatile uint32_t lInner;

    for (lOuter = 0U; lOuter < delayMs; ++lOuter) {
        for (lInner = 0U; lInner < 5000U; ++lInner) {
        }
    }
#endif
}

eDrvStatus mpu6050PortGetDefAssembleCfg(eMPU6050MapType device, stMpu6050PortAssembleCfg *cfg)
{
    if ((cfg == NULL) || !mpu6050PortIsValidDevMap(device)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    *cfg = gMpu6050PortDefAssembleCfg[device];
    return DRV_STATUS_OK;
}

eDrvStatus mpu6050PortGetAssembleCfg(eMPU6050MapType device, stMpu6050PortAssembleCfg *cfg)
{
    stMpu6050PortAssembleCfg *lCfg;

    if (cfg == NULL) {
        return DRV_STATUS_INVALID_PARAM;
    }

    lCfg = mpu6050PortGetAssembleCfgCtx(device);
    if (lCfg == NULL) {
        return DRV_STATUS_INVALID_PARAM;
    }

    *cfg = *lCfg;
    return DRV_STATUS_OK;
}

eDrvStatus mpu6050PortSetAssembleCfg(eMPU6050MapType device, const stMpu6050PortAssembleCfg *cfg)
{
    stMpu6050PortAssembleCfg *lCfg;

    if ((cfg == NULL) || !mpu6050PortIsValidAssembleCfg(cfg)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    lCfg = mpu6050PortGetAssembleCfgCtx(device);
    if (lCfg == NULL) {
        return DRV_STATUS_INVALID_PARAM;
    }

    *lCfg = *cfg;
    gMpu6050PortAssembleCfgDone[device] = true;
    return DRV_STATUS_OK;
}

eDrvStatus mpu6050PortAssembleSoftIic(stMpu6050PortAssembleCfg *cfg, uint8_t iic)
{
    if ((cfg == NULL) || ((uint8_t)iic >= (uint8_t)DRVANLOGIIC_MAX)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    cfg->transportType = MPU6050_TRANSPORT_TYPE_SOFTWARE;
    cfg->linkId = (uint8_t)iic;
    return DRV_STATUS_OK;
}

eDrvStatus mpu6050PortAssembleHardIic(stMpu6050PortAssembleCfg *cfg, uint8_t iic)
{
    if ((cfg == NULL) || ((uint8_t)iic >= (uint8_t)DRVIIC_MAX)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    cfg->transportType = MPU6050_TRANSPORT_TYPE_HARDWARE;
    cfg->linkId = (uint8_t)iic;
    return DRV_STATUS_OK;
}

void mpu6050PortDelayMs(uint32_t delayMs)
{
    mpu6050PlatformDelayMs(delayMs);
}

bool mpu6050PortIsValidAssembleCfg(const stMpu6050PortAssembleCfg *cfg)
{
    if (cfg == NULL) {
        return false;
    }

    switch (cfg->transportType) {
        case MPU6050_TRANSPORT_TYPE_SOFTWARE:
            return (cfg->linkId < (uint8_t)DRVANLOGIIC_MAX);
        case MPU6050_TRANSPORT_TYPE_HARDWARE:
            return (cfg->linkId < (uint8_t)DRVIIC_MAX);
        default:
            return false;
    }
}

bool mpu6050PortHasValidIicIf(const stMpu6050PortAssembleCfg *cfg)
{
    const stMpu6050IicInterface *lInterface;

    lInterface = mpu6050PortGetIicIf(cfg);
    return (lInterface != NULL) &&
           (lInterface->init != NULL) &&
           (lInterface->writeReg != NULL) &&
           (lInterface->readReg != NULL);
}

const stMpu6050PortIicInterface *mpu6050PortGetIicIf(const stMpu6050PortAssembleCfg *cfg)
{
    if (!mpu6050PortIsValidAssembleCfg(cfg)) {
        return NULL;
    }

    return mpu6050PortGetBindIicIf(cfg->transportType);
}

static bool mpu6050PortIsValidDevMap(eMPU6050MapType device)
{
    return ((uint32_t)device < (uint32_t)MPU6050_DEV_MAX);
}

static stMpu6050PortAssembleCfg *mpu6050PortGetAssembleCfgCtx(eMPU6050MapType device)
{
    if (!mpu6050PortIsValidDevMap(device)) {
        return NULL;
    }

    if (!gMpu6050PortAssembleCfgDone[device]) {
        gMpu6050PortAssembleCfg[device] = gMpu6050PortDefAssembleCfg[device];
        gMpu6050PortAssembleCfgDone[device] = true;
    }

    return &gMpu6050PortAssembleCfg[device];
}

static const stMpu6050PortIicInterface *mpu6050PortGetBindIicIf(eMpu6050TransportType type)
{
    if ((uint32_t)type >= (uint32_t)MPU6050_TRANSPORT_TYPE_MAX) {
        return NULL;
    }

    if (type == MPU6050_TRANSPORT_TYPE_NONE) {
        return NULL;
    }

    return &gMpu6050PortIicInterfaces[type];
}

static eDrvStatus mpu6050PortSoftIicInitAdpt(uint8_t bus)
{
    if (bus >= (uint8_t)DRVANLOGIIC_MAX) {
        return DRV_STATUS_INVALID_PARAM;
    }

    return drvAnlogIicInit(bus);
}

static eDrvStatus mpu6050PortSoftIicWriteRegAdpt(uint8_t bus, uint8_t address, const uint8_t *regBuf, uint16_t regLen, const uint8_t *buffer, uint16_t length)
{
    if (bus >= (uint8_t)DRVANLOGIIC_MAX) {
        return DRV_STATUS_INVALID_PARAM;
    }

    return drvAnlogIicWriteRegister(bus, address, regBuf, regLen, buffer, length);
}

static eDrvStatus mpu6050PortSoftIicReadRegAdpt(uint8_t bus, uint8_t address, const uint8_t *regBuf, uint16_t regLen, uint8_t *buffer, uint16_t length)
{
    if (bus >= (uint8_t)DRVANLOGIIC_MAX) {
        return DRV_STATUS_INVALID_PARAM;
    }

    return drvAnlogIicReadRegister(bus, address, regBuf, regLen, buffer, length);
}

static eDrvStatus mpu6050PortHardIicInitAdpt(uint8_t bus)
{
    if (bus >= (uint8_t)DRVIIC_MAX) {
        return DRV_STATUS_INVALID_PARAM;
    }

    return drvIicInit(bus);
}

static eDrvStatus mpu6050PortHardIicWriteRegAdpt(uint8_t bus, uint8_t address, const uint8_t *regBuf, uint16_t regLen, const uint8_t *buffer, uint16_t length)
{
    if (bus >= (uint8_t)DRVIIC_MAX) {
        return DRV_STATUS_INVALID_PARAM;
    }

    return drvIicWriteRegister(bus, address, regBuf, regLen, buffer, length);
}

static eDrvStatus mpu6050PortHardIicReadRegAdpt(uint8_t bus, uint8_t address, const uint8_t *regBuf, uint16_t regLen, uint8_t *buffer, uint16_t length)
{
    if (bus >= (uint8_t)DRVIIC_MAX) {
        return DRV_STATUS_INVALID_PARAM;
    }

    return drvIicReadRegister(bus, address, regBuf, regLen, buffer, length);
}

static void mpu6050PortEnableCycleCnt(void)
{
#if (REP_MCU_PLATFORM == REP_MCU_PLATFORM_GD32)
    if (gMpu6050PortCycleCntReady) {
        return;
    }

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    gMpu6050PortCycleCntReady = true;
#endif
}

/**************************End of file********************************/

