/************************************************************************************
* @file     : drvspi.h
* @brief    : Reusable hardware SPI driver abstraction.
* @details  : This module exposes a stable master-mode SPI interface for upper
*             modules while hiding controller and chip-select details behind hooks.
***********************************************************************************/
#ifndef DRVSPI_H
#define DRVSPI_H

#include <stdbool.h>
#include <stdint.h>

#include "rep_config.h"
#include "drvspi_types.h"

#ifdef __cplusplus
extern "C" {
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

typedef eDrvStatus (*drvSpiBspInitFunc)(eDrvSpiPortMap spi);
typedef eDrvStatus (*drvSpiBspTransferFunc)(eDrvSpiPortMap spi, const uint8_t *txBuffer, uint8_t *rxBuffer, uint16_t length, uint8_t fillData, uint32_t timeoutMs);
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

eDrvStatus drvSpiInit(eDrvSpiPortMap spi);
eDrvStatus drvSpiSetCsControl(eDrvSpiPortMap spi, const stDrvSpiCsControl *control);
eDrvStatus drvSpiTransfer(eDrvSpiPortMap spi, const stDrvSpiTransfer *transfer);
eDrvStatus drvSpiTransferTimeout(eDrvSpiPortMap spi, const stDrvSpiTransfer *transfer, uint32_t timeoutMs);
eDrvStatus drvSpiWrite(eDrvSpiPortMap spi, const uint8_t *buffer, uint16_t length);
eDrvStatus drvSpiWriteTimeout(eDrvSpiPortMap spi, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs);
eDrvStatus drvSpiRead(eDrvSpiPortMap spi, uint8_t *buffer, uint16_t length);
eDrvStatus drvSpiReadTimeout(eDrvSpiPortMap spi, uint8_t *buffer, uint16_t length, uint32_t timeoutMs);
eDrvStatus drvSpiWriteRead(eDrvSpiPortMap spi, const uint8_t *writeBuffer, uint16_t writeLength, uint8_t *readBuffer, uint16_t readLength);
eDrvStatus drvSpiWriteReadTimeout(eDrvSpiPortMap spi, const uint8_t *writeBuffer, uint16_t writeLength, uint8_t *readBuffer, uint16_t readLength, uint32_t timeoutMs);
eDrvStatus drvSpiExchange(eDrvSpiPortMap spi, const uint8_t *txBuffer, uint8_t *rxBuffer, uint16_t length);
eDrvStatus drvSpiExchangeTimeout(eDrvSpiPortMap spi, const uint8_t *txBuffer, uint8_t *rxBuffer, uint16_t length, uint32_t timeoutMs);

#ifdef __cplusplus
}
#endif

#endif  // DRVSPI_H
/**************************End of file********************************/

