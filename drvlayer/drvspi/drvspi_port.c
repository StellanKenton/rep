/***********************************************************************************
* @file     : drvspi_port.c
* @brief    : Hardware SPI port-layer BSP binding implementation.
* @details  : This file owns the project-level SPI map and default manual CS
*             binding for each enabled logical bus.
**********************************************************************************/
#include "drvspi.h"

#include "bspspi.h"

static stBspSpiCsPin gDrvSpiBus0CsPin = {
    .gpioRcu = RCU_GPIOE,
    .gpioPort = GPIOE,
    .gpioPin = GPIO_PIN_15,
    .isActiveLow = true,
};

static stBspSpiCsPin gDrvSpiBus1CsPin = {
    .gpioRcu = RCU_GPIOA,
    .gpioPort = GPIOA,
    .gpioPin = GPIO_PIN_4,
    .isActiveLow = true,
};

stDrvSpiBspInterface gDrvSpiBspInterface[DRVSPI_MAX] = {
    [DRVSPI_BUS0] = {
        .init = bspSpiInit,
        .transfer = bspSpiTransfer,
        .defaultTimeoutMs = DRVSPI_DEFAULT_TIMEOUT_MS,
        .csControl = {
            .init = bspSpiCsInit,
            .write = bspSpiCsWrite,
            .context = &gDrvSpiBus0CsPin,
        },
    },
    [DRVSPI_BUS1] = {
        .init = bspSpiInit,
        .transfer = bspSpiTransfer,
        .defaultTimeoutMs = DRVSPI_DEFAULT_TIMEOUT_MS,
        .csControl = {
            .init = bspSpiCsInit,
            .write = bspSpiCsWrite,
            .context = &gDrvSpiBus1CsPin,
        },
    },
};
/**************************End of file********************************/
