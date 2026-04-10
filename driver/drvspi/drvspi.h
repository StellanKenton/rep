/************************************************************************************
* @file     : drvspi.h
* @brief    : Reusable hardware SPI driver abstraction.
* @details  : This module exposes a stable master-mode SPI interface for upper
*             modules while hiding controller and chip-select details behind hooks.
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef DRVSPI_H
#define DRVSPI_H

#include <stdbool.h>
#include <stdint.h>

#include "rep_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DRVSPI_LOG_SUPPORT
#define DRVSPI_LOG_SUPPORT                    1
#endif

#ifndef DRVSPI_CONSOLE_SUPPORT
#define DRVSPI_CONSOLE_SUPPORT                1
#endif

#ifndef DRVSPI_MAX
#define DRVSPI_MAX                            1U
#endif

#ifndef DRVSPI_LOCK_WAIT_MS
#define DRVSPI_LOCK_WAIT_MS                   5U
#endif

#ifndef DRVSPI_DEFAULT_TIMEOUT_MS
#define DRVSPI_DEFAULT_TIMEOUT_MS             100U
#endif

#ifndef DRVSPI_DEFAULT_READ_FILL_DATA
#define DRVSPI_DEFAULT_READ_FILL_DATA         0xFFU
#endif


typedef struct stDrvSpiTransfer {
    const uint8_t *writeBuffer;
    uint16_t writeLength;
    const uint8_t *secondWriteBuffer;
    uint16_t secondWriteLength;
    uint8_t *readBuffer;
    uint16_t readLength;
    uint8_t readFillData;
} stDrvSpiTransfer;

typedef eDrvStatus (*drvSpiBspInitFunc)(uint8_t spi);
typedef eDrvStatus (*drvSpiBspTransferFunc)(uint8_t spi, const uint8_t *txBuffer, uint8_t *rxBuffer, uint16_t length, uint8_t fillData, uint32_t timeoutMs);
typedef void (*drvSpiCsInitFunc)(void *context);
typedef void (*drvSpiCsWriteFunc)(void *context, bool isActive);

typedef struct stDrvSpiCsControl {
    drvSpiCsInitFunc init;
    drvSpiCsWriteFunc write;
    void *context;
} stDrvSpiCsControl;

typedef struct stDrvSpiBspInterface {
    drvSpiBspInitFunc init;
    drvSpiBspTransferFunc transfer;
    uint32_t defaultTimeoutMs;
    stDrvSpiCsControl csControl;
} stDrvSpiBspInterface;

eDrvStatus drvSpiInit(uint8_t spi);
eDrvStatus drvSpiSetCsControl(uint8_t spi, const stDrvSpiCsControl *control);
eDrvStatus drvSpiTransfer(uint8_t spi, const stDrvSpiTransfer *transfer);
eDrvStatus drvSpiTransferTimeout(uint8_t spi, const stDrvSpiTransfer *transfer, uint32_t timeoutMs);
eDrvStatus drvSpiWrite(uint8_t spi, const uint8_t *buffer, uint16_t length);
eDrvStatus drvSpiWriteTimeout(uint8_t spi, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs);
eDrvStatus drvSpiRead(uint8_t spi, uint8_t *buffer, uint16_t length);
eDrvStatus drvSpiReadTimeout(uint8_t spi, uint8_t *buffer, uint16_t length, uint32_t timeoutMs);
eDrvStatus drvSpiWriteRead(uint8_t spi, const uint8_t *writeBuffer, uint16_t writeLength, uint8_t *readBuffer, uint16_t readLength);
eDrvStatus drvSpiWriteReadTimeout(uint8_t spi, const uint8_t *writeBuffer, uint16_t writeLength, uint8_t *readBuffer, uint16_t readLength, uint32_t timeoutMs);
eDrvStatus drvSpiExchange(uint8_t spi, const uint8_t *txBuffer, uint8_t *rxBuffer, uint16_t length);
eDrvStatus drvSpiExchangeTimeout(uint8_t spi, const uint8_t *txBuffer, uint8_t *rxBuffer, uint16_t length, uint32_t timeoutMs);

const stDrvSpiBspInterface *drvSpiGetPlatformBspInterfaces(void);

#ifdef __cplusplus
}
#endif

#endif  // DRVSPI_H
/**************************End of file********************************/
