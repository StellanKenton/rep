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

#if (REP_RTOS_SYSTEM == REP_RTOS_FREERTOS)
#include "FreeRTOS.h"
#include "task.h"
#endif

#if (REP_MCU_PLATFORM == REP_MCU_PLATFORM_GD32)
#include "gd32f4xx.h"
#endif

static bool gMpu6050PortCycleCntReady = false;

static void mpu6050PortEnableCycleCnt(void);
static eDrvStatus mpu6050PortSoftIicInitAdpt(uint8_t bus);
static eDrvStatus mpu6050PortSoftIicWriteRegAdpt(uint8_t bus, uint8_t address, const uint8_t *regBuf, uint16_t regLen, const uint8_t *buffer, uint16_t length);
static eDrvStatus mpu6050PortSoftIicReadRegAdpt(uint8_t bus, uint8_t address, const uint8_t *regBuf, uint16_t regLen, uint8_t *buffer, uint16_t length);
static eDrvStatus mpu6050PortHardIicInitAdpt(uint8_t bus);
static eDrvStatus mpu6050PortHardIicWriteRegAdpt(uint8_t bus, uint8_t address, const uint8_t *regBuf, uint16_t regLen, const uint8_t *buffer, uint16_t length);
static eDrvStatus mpu6050PortHardIicReadRegAdpt(uint8_t bus, uint8_t address, const uint8_t *regBuf, uint16_t regLen, uint8_t *buffer, uint16_t length);
static const stMpu6050PortIicInterface *mpu6050PortGetBindIicIf(eMpu6050PortIicType type);

static const stMpu6050PortIicInterface gMpu6050PortIicInterfaces[MPU6050_PORT_IIC_TYPE_MAX] = {
    [MPU6050_PORT_IIC_TYPE_SOFTWARE] = {
        .init = mpu6050PortSoftIicInitAdpt,
        .writeReg = mpu6050PortSoftIicWriteRegAdpt,
        .readReg = mpu6050PortSoftIicReadRegAdpt,
    },
    [MPU6050_PORT_IIC_TYPE_HARDWARE] = {
        .init = mpu6050PortHardIicInitAdpt,
        .writeReg = mpu6050PortHardIicWriteRegAdpt,
        .readReg = mpu6050PortHardIicReadRegAdpt,
    },
};

static const stMpu6050Cfg gMpu6050PortDefCfg[MPU6050_DEV_MAX] = {
    [MPU6050_DEV0] = {
        .iicBind = {
            .type = MPU6050_PORT_IIC_TYPE_HARDWARE,
            .bus = (uint8_t)DRVIIC_BUS0,
            .iicIf = &gMpu6050PortIicInterfaces[MPU6050_PORT_IIC_TYPE_HARDWARE],
        },
        .address = MPU6050_IIC_ADDRESS_LOW,
        .sampleRateDiv = 0U,
        .dlpfCfg = 3U,
        .accelRange = MPU6050_ACCEL_RANGE_4G,
        .gyroRange = MPU6050_GYRO_RANGE_2000DPS,
    },
    [MPU6050_DEV1] = {
        .iicBind = {
            .type = MPU6050_PORT_IIC_TYPE_HARDWARE,
            .bus = (uint8_t)DRVIIC_BUS0,
            .iicIf = &gMpu6050PortIicInterfaces[MPU6050_PORT_IIC_TYPE_HARDWARE],
        },
        .address = MPU6050_IIC_ADDRESS_HIGH,
        .sampleRateDiv = 0U,
        .dlpfCfg = 3U,
        .accelRange = MPU6050_ACCEL_RANGE_4G,
        .gyroRange = MPU6050_GYRO_RANGE_2000DPS,
    },
};


void mpu6050PortGetDefBind(stMpu6050PortIicBinding *bind)
{
    if (bind == NULL) {
        return;
    }

    bind->type = MPU6050_PORT_IIC_TYPE_HARDWARE;
    bind->bus = (uint8_t)DRVIIC_BUS0;
    bind->iicIf = mpu6050PortGetBindIicIf(bind->type);
}

void mpu6050PortGetDefCfg(eMPU6050MapType device, stMpu6050Cfg *cfg)
{
    if ((cfg == NULL) || ((uint32_t)device >= (uint32_t)MPU6050_DEV_MAX)) {
        return;
    }

    *cfg = gMpu6050PortDefCfg[device];
}

eDrvStatus mpu6050PortSetSoftIic(stMpu6050PortIicBinding *bind, eDrvAnlogIicPortMap iic)
{
    if ((bind == NULL) || ((uint8_t)iic >= (uint8_t)DRVANLOGIIC_MAX)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    bind->type = MPU6050_PORT_IIC_TYPE_SOFTWARE;
    bind->bus = (uint8_t)iic;
    bind->iicIf = mpu6050PortGetBindIicIf(bind->type);
    return DRV_STATUS_OK;
}

eDrvStatus mpu6050PortSetHardIic(stMpu6050PortIicBinding *bind, eDrvIicPortMap iic)
{
    if ((bind == NULL) || ((uint8_t)iic >= (uint8_t)DRVIIC_MAX)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    bind->type = MPU6050_PORT_IIC_TYPE_HARDWARE;
    bind->bus = (uint8_t)iic;
    bind->iicIf = mpu6050PortGetBindIicIf(bind->type);
    return DRV_STATUS_OK;
}

void mpu6050PortDelayMs(uint32_t delayMs)
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

bool mpu6050PortIsValidBind(const stMpu6050PortIicBinding *bind)
{
    if (bind == NULL) {
        return false;
    }

    switch (bind->type) {
        case MPU6050_PORT_IIC_TYPE_SOFTWARE:
            return (bind->bus < (uint8_t)DRVANLOGIIC_MAX) &&
                   (bind->iicIf == &gMpu6050PortIicInterfaces[MPU6050_PORT_IIC_TYPE_SOFTWARE]);
        case MPU6050_PORT_IIC_TYPE_HARDWARE:
            return (bind->bus < (uint8_t)DRVIIC_MAX) &&
                   (bind->iicIf == &gMpu6050PortIicInterfaces[MPU6050_PORT_IIC_TYPE_HARDWARE]);
        default:
            return false;
    }
}

bool mpu6050PortHasValidIicIf(const stMpu6050PortIicBinding *bind)
{
    const stMpu6050PortIicInterface *lInterface;

    lInterface = mpu6050PortGetIicIf(bind);
    return (lInterface != NULL) &&
           (lInterface->init != NULL) &&
           (lInterface->writeReg != NULL) &&
           (lInterface->readReg != NULL);
}

const stMpu6050PortIicInterface *mpu6050PortGetIicIf(const stMpu6050PortIicBinding *bind)
{
    if (!mpu6050PortIsValidBind(bind)) {
        return NULL;
    }

    return bind->iicIf;
}

static const stMpu6050PortIicInterface *mpu6050PortGetBindIicIf(eMpu6050PortIicType type)
{
    if ((uint32_t)type >= (uint32_t)MPU6050_PORT_IIC_TYPE_MAX) {
        return NULL;
    }

    if (type == MPU6050_PORT_IIC_TYPE_NONE) {
        return NULL;
    }

    return &gMpu6050PortIicInterfaces[type];
}

static eDrvStatus mpu6050PortSoftIicInitAdpt(uint8_t bus)
{
    if (bus >= (uint8_t)DRVANLOGIIC_MAX) {
        return DRV_STATUS_INVALID_PARAM;
    }

    return drvAnlogIicInit((eDrvAnlogIicPortMap)bus);
}

static eDrvStatus mpu6050PortSoftIicWriteRegAdpt(uint8_t bus, uint8_t address, const uint8_t *regBuf, uint16_t regLen, const uint8_t *buffer, uint16_t length)
{
    if (bus >= (uint8_t)DRVANLOGIIC_MAX) {
        return DRV_STATUS_INVALID_PARAM;
    }

    return drvAnlogIicWriteRegister((eDrvAnlogIicPortMap)bus, address, regBuf, regLen, buffer, length);
}

static eDrvStatus mpu6050PortSoftIicReadRegAdpt(uint8_t bus, uint8_t address, const uint8_t *regBuf, uint16_t regLen, uint8_t *buffer, uint16_t length)
{
    if (bus >= (uint8_t)DRVANLOGIIC_MAX) {
        return DRV_STATUS_INVALID_PARAM;
    }

    return drvAnlogIicReadRegister((eDrvAnlogIicPortMap)bus, address, regBuf, regLen, buffer, length);
}

static eDrvStatus mpu6050PortHardIicInitAdpt(uint8_t bus)
{
    if (bus >= (uint8_t)DRVIIC_MAX) {
        return DRV_STATUS_INVALID_PARAM;
    }

    return drvIicInit((eDrvIicPortMap)bus);
}

static eDrvStatus mpu6050PortHardIicWriteRegAdpt(uint8_t bus, uint8_t address, const uint8_t *regBuf, uint16_t regLen, const uint8_t *buffer, uint16_t length)
{
    if (bus >= (uint8_t)DRVIIC_MAX) {
        return DRV_STATUS_INVALID_PARAM;
    }

    return drvIicWriteRegister((eDrvIicPortMap)bus, address, regBuf, regLen, buffer, length);
}

static eDrvStatus mpu6050PortHardIicReadRegAdpt(uint8_t bus, uint8_t address, const uint8_t *regBuf, uint16_t regLen, uint8_t *buffer, uint16_t length)
{
    if (bus >= (uint8_t)DRVIIC_MAX) {
        return DRV_STATUS_INVALID_PARAM;
    }

    return drvIicReadRegister((eDrvIicPortMap)bus, address, regBuf, regLen, buffer, length);
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

