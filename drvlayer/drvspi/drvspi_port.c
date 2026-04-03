/***********************************************************************************
* @file     : drvspi_port.c
* @brief    : Hardware SPI port-layer BSP binding implementation.
* @details  : This file owns the project-level SPI map and default manual CS
*             binding for each enabled logical bus.
**********************************************************************************/
#include "drvspi.h"

#include "bspspi.h"

static stBspSpiCsPin gDrvSpiBus0CsPin = {
    .gpioPort = SPI_CS_GPIO_Port,
    .gpioPin = SPI_CS_Pin,
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
};
/**************************End of file********************************/
