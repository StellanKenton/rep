/************************************************************************************
* @file     : gd25qxxx_port.c
* @brief    : GD25Qxxx project port-layer implementation.
* @details  : This file binds each logical GD25Qxxx device to the project drvspi
*             layer and provides an RTOS-aware millisecond delay hook.
***********************************************************************************/
#include "gd25qxxx_port.h"

#include <stdbool.h>
#include <stddef.h>

#include "Rep/rep_config.h"

#if (REP_RTOS_SYSTEM == REP_RTOS_FREERTOS)
#include "FreeRTOS.h"
#include "task.h"
#endif

#if (REP_MCU_PLATFORM == REP_MCU_PLATFORM_GD32)
#include "gd32f4xx.h"
#endif

#if (REP_MCU_PLATFORM == REP_MCU_PLATFORM_GD32)
static bool gGd25qxxxPortCycleCntReady = false;

static void gd25qxxxPortEnableCycleCnt(void);
#endif

static eDrvStatus gd25qxxxPortHardSpiInitAdpt(uint8_t bus);
static eDrvStatus gd25qxxxPortHardSpiTransferAdpt(uint8_t bus, const uint8_t *writeBuffer, uint16_t writeLength, const uint8_t *secondWriteBuffer, uint16_t secondWriteLength, uint8_t *readBuffer, uint16_t readLength, uint8_t readFillData);
static const stGd25qxxxPortSpiInterface *gd25qxxxPortGetBindSpiIf(eGd25qxxxPortSpiType type);

static const stGd25qxxxPortSpiInterface gGd25qxxxPortSpiInterfaces[GD25QXXX_PORT_SPI_TYPE_MAX] = {
    [GD25QXXX_PORT_SPI_TYPE_HARDWARE] = {
        .init = gd25qxxxPortHardSpiInitAdpt,
        .transfer = gd25qxxxPortHardSpiTransferAdpt,
    },
};

static const stGd25qxxxCfg gGd25qxxxPortDefCfg[GD25QXXX_DEV_MAX] = {
    [GD25Q32_MEM] = {
        .spiBind = {
            .type = GD25QXXX_PORT_SPI_TYPE_HARDWARE,
            .bus = (uint8_t)DRVSPI_BUS0,
            .spiIf = &gGd25qxxxPortSpiInterfaces[GD25QXXX_PORT_SPI_TYPE_HARDWARE],
        },
    },
};

void gd25qxxxPortGetDefBind(stGd25qxxxPortSpiBinding *bind)
{
    if (bind == NULL) {
        return;
    }

    bind->type = GD25QXXX_PORT_SPI_TYPE_HARDWARE;
    bind->bus = (uint8_t)DRVSPI_BUS0;
    bind->spiIf = gd25qxxxPortGetBindSpiIf(bind->type);
}

void gd25qxxxPortGetDefCfg(eGd25qxxxMapType device, stGd25qxxxCfg *cfg)
{
    if ((cfg == NULL) || ((uint32_t)device >= (uint32_t)GD25QXXX_DEV_MAX)) {
        return;
    }

    *cfg = gGd25qxxxPortDefCfg[device];
}

eDrvStatus gd25qxxxPortSetHardSpi(stGd25qxxxPortSpiBinding *bind, eDrvSpiPortMap spi)
{
    if ((bind == NULL) || ((uint8_t)spi >= (uint8_t)DRVSPI_MAX)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    bind->type = GD25QXXX_PORT_SPI_TYPE_HARDWARE;
    bind->bus = (uint8_t)spi;
    bind->spiIf = gd25qxxxPortGetBindSpiIf(bind->type);
    return DRV_STATUS_OK;
}

bool gd25qxxxPortIsValidBind(const stGd25qxxxPortSpiBinding *bind)
{
    if (bind == NULL) {
        return false;
    }

    switch (bind->type) {
        case GD25QXXX_PORT_SPI_TYPE_HARDWARE:
            return (bind->bus < (uint8_t)DRVSPI_MAX) &&
                   (bind->spiIf == &gGd25qxxxPortSpiInterfaces[GD25QXXX_PORT_SPI_TYPE_HARDWARE]);
        default:
            return false;
    }
}

bool gd25qxxxPortHasValidSpiIf(const stGd25qxxxPortSpiBinding *bind)
{
    const stGd25qxxxPortSpiInterface *lInterface;

    lInterface = gd25qxxxPortGetSpiIf(bind);
    return (lInterface != NULL) &&
           (lInterface->init != NULL) &&
           (lInterface->transfer != NULL);
}

const stGd25qxxxPortSpiInterface *gd25qxxxPortGetSpiIf(const stGd25qxxxPortSpiBinding *bind)
{
    if (!gd25qxxxPortIsValidBind(bind)) {
        return NULL;
    }

    return bind->spiIf;
}

void gd25qxxxPortDelayMs(uint32_t delayMs)
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

    gd25qxxxPortEnableCycleCnt();
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

#if (REP_MCU_PLATFORM == REP_MCU_PLATFORM_GD32)
static void gd25qxxxPortEnableCycleCnt(void)
{
    if (gGd25qxxxPortCycleCntReady) {
        return;
    }

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    gGd25qxxxPortCycleCntReady = true;
}
#endif

static const stGd25qxxxPortSpiInterface *gd25qxxxPortGetBindSpiIf(eGd25qxxxPortSpiType type)
{
    if ((uint32_t)type >= (uint32_t)GD25QXXX_PORT_SPI_TYPE_MAX) {
        return NULL;
    }

    if (type == GD25QXXX_PORT_SPI_TYPE_NONE) {
        return NULL;
    }

    return &gGd25qxxxPortSpiInterfaces[type];
}

static eDrvStatus gd25qxxxPortHardSpiInitAdpt(uint8_t bus)
{
    if (bus >= (uint8_t)DRVSPI_MAX) {
        return DRV_STATUS_INVALID_PARAM;
    }

    return drvSpiInit((eDrvSpiPortMap)bus);
}

static eDrvStatus gd25qxxxPortHardSpiTransferAdpt(uint8_t bus, const uint8_t *writeBuffer, uint16_t writeLength, const uint8_t *secondWriteBuffer, uint16_t secondWriteLength, uint8_t *readBuffer, uint16_t readLength, uint8_t readFillData)
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
    return drvSpiTransfer((eDrvSpiPortMap)bus, &lTransfer);
}
/**************************End of file********************************/
