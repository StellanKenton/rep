/************************************************************************************
* @file     : w25qxxx_port.c
* @brief    : W25Qxxx project port-layer implementation.
* @details  : This file binds each logical W25Qxxx device to the project drvspi
*             layer and provides an RTOS-aware millisecond delay hook.
***********************************************************************************/
#include "w25qxxx_port.h"

#include <stdbool.h>

#include "Rep/rep_config.h"
#include "Rep/drvlayer/drvspi/drvspi_port.h"

#if (REP_RTOS_SYSTEM == REP_RTOS_FREERTOS)
#include "FreeRTOS.h"
#include "task.h"
#endif

#if (REP_MCU_PLATFORM == REP_MCU_PLATFORM_GD32)
#include "gd32f4xx.h"
#endif

static bool gW25qxxxPortCycleCntReady = false;

static void w25qxxxPortEnableCycleCnt(void);
static eDrvStatus w25qxxxPortHardSpiInitAdpt(uint8_t bus);
static eDrvStatus w25qxxxPortHardSpiTransferAdpt(uint8_t bus, const uint8_t *writeBuffer, uint16_t writeLength, const uint8_t *secondWriteBuffer, uint16_t secondWriteLength, uint8_t *readBuffer, uint16_t readLength, uint8_t readFillData);

static const stW25qxxxPortSpiInterface gW25qxxxPortHardSpiInterface = {
    .init = w25qxxxPortHardSpiInitAdpt,
    .transfer = w25qxxxPortHardSpiTransferAdpt,
};

static const stW25qxxxCfg gW25qxxxPortDefCfg[W25QXXX_DEV_MAX] = {
    [W25QXXX_DEV0] = {
        .linkId = DRVSPI_BUS0,
    },
    [W25QXXX_DEV1] = {
        .linkId = DRVSPI_BUS1,
    },
};

void w25qxxxLoadPlatformDefaultCfg(eW25qxxxMapType device, stW25qxxxCfg *cfg)
{
    if ((cfg == NULL) || ((uint32_t)device >= (uint32_t)W25QXXX_DEV_MAX)) {
        return;
    }

    *cfg = gW25qxxxPortDefCfg[device];
}

const stW25qxxxSpiInterface *w25qxxxGetPlatformSpiInterface(const stW25qxxxCfg *cfg)
{
    if (!w25qxxxPortIsValidCfg(cfg)) {
        return NULL;
    }

    return &gW25qxxxPortHardSpiInterface;
}

bool w25qxxxPlatformIsValidCfg(const stW25qxxxCfg *cfg)
{
    return w25qxxxPortIsValidCfg(cfg);
}

void w25qxxxPlatformDelayMs(uint32_t delayMs)
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

    w25qxxxPortEnableCycleCnt();
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
        for (lInner = 0U; lInner < 2000U; ++lInner) {
        }
    }
#endif
}

void w25qxxxPortGetDefCfg(eW25qxxxMapType device, stW25qxxxCfg *cfg)
{
    w25qxxxLoadPlatformDefaultCfg(device, cfg);
}

eDrvStatus w25qxxxPortAssembleHardSpi(stW25qxxxCfg *cfg, uint8_t spi)
{
    if ((cfg == NULL) || ((uint8_t)spi >= (uint8_t)DRVSPI_MAX)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    cfg->linkId = spi;
    return DRV_STATUS_OK;
}

bool w25qxxxPortIsValidCfg(const stW25qxxxCfg *cfg)
{
    return (cfg != NULL) && ((uint8_t)cfg->linkId < (uint8_t)DRVSPI_MAX);
}

bool w25qxxxPortHasValidSpiIf(const stW25qxxxCfg *cfg)
{
    const stW25qxxxSpiInterface *lInterface;

    lInterface = w25qxxxGetPlatformSpiInterface(cfg);
    return (lInterface != NULL) &&
           (lInterface->init != NULL) &&
           (lInterface->transfer != NULL);
}

const stW25qxxxPortSpiInterface *w25qxxxPortGetSpiIf(const stW25qxxxCfg *cfg)
{
    return (const stW25qxxxPortSpiInterface *)w25qxxxGetPlatformSpiInterface(cfg);
}

void w25qxxxPortDelayMs(uint32_t delayMs)
{
    w25qxxxPlatformDelayMs(delayMs);
}

static void w25qxxxPortEnableCycleCnt(void)
{
#if (REP_MCU_PLATFORM == REP_MCU_PLATFORM_GD32)
    if (gW25qxxxPortCycleCntReady) {
        return;
    }

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    gW25qxxxPortCycleCntReady = true;
#endif
}

static eDrvStatus w25qxxxPortHardSpiInitAdpt(uint8_t bus)
{
    if (bus >= (uint8_t)DRVSPI_MAX) {
        return DRV_STATUS_INVALID_PARAM;
    }

    return drvSpiInit(bus);
}

static eDrvStatus w25qxxxPortHardSpiTransferAdpt(uint8_t bus, const uint8_t *writeBuffer, uint16_t writeLength, const uint8_t *secondWriteBuffer, uint16_t secondWriteLength, uint8_t *readBuffer, uint16_t readLength, uint8_t readFillData)
{
    stDrvSpiTransfer lTransfer;

    if (bus >= (uint8_t)DRVSPI_MAX) {
        return DRV_STATUS_INVALID_PARAM;
    }

    lTransfer.writeBuffer = writeBuffer;
    lTransfer.writeLength = writeLength;
    lTransfer.secondWriteBuffer = secondWriteBuffer;
    lTransfer.secondWriteLength = secondWriteLength;
    lTransfer.readBuffer = readBuffer;
    lTransfer.readLength = readLength;
    lTransfer.readFillData = readFillData;
    return drvSpiTransfer(bus, &lTransfer);
}
/**************************End of file********************************/
