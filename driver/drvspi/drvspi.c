/***********************************************************************************
* @file     : drvspi.c
* @brief    : Reusable hardware SPI driver abstraction implementation.
* @details  : This module validates public parameters, serializes bus access,
*             manages manual chip-select control, and forwards transfers to the
*             project-specific BSP hook table.
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "drvspi.h"

#if (DRVSPI_LOG_SUPPORT == 1)
#include "../../service/log/log.h"
#endif

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "rep_config.h"
#include "../../service/rtos/rtos.h"

#define DRVSPI_LOG_TAG                   "drvSpi"

static bool gDrvSpiInitialized[DRVSPI_MAX];
static stRepRtosMutex gDrvSpiMutex[DRVSPI_MAX];

__attribute__((weak)) const stDrvSpiBspInterface *drvSpiGetPlatformBspInterfaces(void)
{
    return NULL;
}

static eDrvStatus drvSpiMapRtosStatus(eRepRtosStatus status)
{
    switch (status) {
        case REP_RTOS_STATUS_OK:
            return DRV_STATUS_OK;
        case REP_RTOS_STATUS_INVALID_PARAM:
            return DRV_STATUS_INVALID_PARAM;
        case REP_RTOS_STATUS_NOT_READY:
            return DRV_STATUS_NOT_READY;
        case REP_RTOS_STATUS_BUSY:
            return DRV_STATUS_BUSY;
        case REP_RTOS_STATUS_TIMEOUT:
            return DRV_STATUS_TIMEOUT;
        case REP_RTOS_STATUS_UNSUPPORTED:
            return DRV_STATUS_UNSUPPORTED;
        default:
            return DRV_STATUS_ERROR;
    }
}

static bool drvSpiIsValid(uint8_t spi)
{
    return spi < DRVSPI_MAX;
}

static bool drvSpiIsValidConstBuffer(const uint8_t *buffer, uint16_t length)
{
    return ((buffer != NULL) || (length == 0U));
}

static bool drvSpiIsValidMutableBuffer(uint8_t *buffer, uint16_t length)
{
    return ((buffer != NULL) || (length == 0U));
}

static bool drvSpiHasTransferPhase(const stDrvSpiTransfer *transfer)
{
    return (transfer != NULL) &&
           ((transfer->writeLength > 0U) ||
            (transfer->secondWriteLength > 0U) ||
            (transfer->readLength > 0U));
}

static bool drvSpiIsValidTransfer(const stDrvSpiTransfer *transfer)
{
    if (transfer == NULL) {
        return false;
    }

    if (!drvSpiIsValidConstBuffer(transfer->writeBuffer, transfer->writeLength)) {
        return false;
    }

    if (!drvSpiIsValidConstBuffer(transfer->secondWriteBuffer, transfer->secondWriteLength)) {
        return false;
    }

    if (!drvSpiIsValidMutableBuffer(transfer->readBuffer, transfer->readLength)) {
        return false;
    }

    return drvSpiHasTransferPhase(transfer);
}

static bool drvSpiIsInitialized(uint8_t spi)
{
    return drvSpiIsValid(spi) && gDrvSpiInitialized[spi];
}

static stDrvSpiBspInterface *drvSpiGetBspInterface(uint8_t spi)
{
    const stDrvSpiBspInterface *lInterfaces;

    if (!drvSpiIsValid(spi)) {
        return NULL;
    }

    lInterfaces = drvSpiGetPlatformBspInterfaces();
    if (lInterfaces == NULL) {
        return NULL;
    }

    return (stDrvSpiBspInterface *)&lInterfaces[spi];
}

static bool drvSpiHasValidBspInterface(uint8_t spi)
{
    stDrvSpiBspInterface *lBspInterface = drvSpiGetBspInterface(spi);

    return (lBspInterface != NULL) &&
           (lBspInterface->init != NULL) &&
           (lBspInterface->transfer != NULL);
}

static uint32_t drvSpiGetTimeoutMs(const stDrvSpiBspInterface *bspInterface, uint32_t timeoutMs)
{
    if (timeoutMs > 0U) {
        return timeoutMs;
    }

    if ((bspInterface != NULL) && (bspInterface->defaultTimeoutMs > 0U)) {
        return bspInterface->defaultTimeoutMs;
    }

    return DRVSPI_DEFAULT_TIMEOUT_MS;
}

static void drvSpiInitCsControl(stDrvSpiBspInterface *bspInterface)
{
    if (bspInterface == NULL) {
        return;
    }

    if (bspInterface->csControl.init != NULL) {
        bspInterface->csControl.init(bspInterface->csControl.context);
    }

    if (bspInterface->csControl.write != NULL) {
        bspInterface->csControl.write(bspInterface->csControl.context, false);
    }
}

static void drvSpiSetCsActive(stDrvSpiBspInterface *bspInterface, bool isActive)
{
    if ((bspInterface == NULL) || (bspInterface->csControl.write == NULL)) {
        return;
    }

    bspInterface->csControl.write(bspInterface->csControl.context, isActive);
}

static eDrvStatus drvSpiEnsureMutex(uint8_t spi)
{
    if (!drvSpiIsValid(spi)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (!gDrvSpiMutex[spi].isCreated) {
        return drvSpiMapRtosStatus(repRtosMutexCreate(&gDrvSpiMutex[spi]));
    }

    return DRV_STATUS_OK;
}

static eDrvStatus drvSpiLockBus(uint8_t spi)
{
    if (drvSpiEnsureMutex(spi) != DRV_STATUS_OK) {
        return DRV_STATUS_ERROR;
    }

    return drvSpiMapRtosStatus(repRtosMutexTake(&gDrvSpiMutex[spi], DRVSPI_LOCK_WAIT_MS));
}

static void drvSpiUnlockBus(uint8_t spi)
{
    if (drvSpiIsValid(spi)) {
        (void)repRtosMutexGive(&gDrvSpiMutex[spi]);
    }
}

static eDrvStatus drvSpiRawTransferLocked(uint8_t spi, const uint8_t *txBuffer, uint8_t *rxBuffer, uint16_t length, uint8_t fillData, uint32_t timeoutMs)
{
    stDrvSpiBspInterface *lBspInterface = drvSpiGetBspInterface(spi);

    if ((lBspInterface == NULL) || (lBspInterface->transfer == NULL)) {
        return DRV_STATUS_NOT_READY;
    }

    return lBspInterface->transfer(spi,
                                   txBuffer,
                                   rxBuffer,
                                   length,
                                   fillData,
                                   drvSpiGetTimeoutMs(lBspInterface, timeoutMs));
}

static eDrvStatus drvSpiTransferLocked(uint8_t spi, const stDrvSpiTransfer *transfer, uint32_t timeoutMs)
{
    stDrvSpiBspInterface *lBspInterface = drvSpiGetBspInterface(spi);
    eDrvStatus lStatus;

    if ((lBspInterface == NULL) || (lBspInterface->transfer == NULL)) {
        return DRV_STATUS_NOT_READY;
    }

    drvSpiSetCsActive(lBspInterface, true);

    if (transfer->writeLength > 0U) {
        lStatus = drvSpiRawTransferLocked(spi,
                                          transfer->writeBuffer,
                                          NULL,
                                          transfer->writeLength,
                                          transfer->readFillData,
                                          timeoutMs);
        if (lStatus != DRV_STATUS_OK) {
            drvSpiSetCsActive(lBspInterface, false);
            return lStatus;
        }
    }

    if (transfer->secondWriteLength > 0U) {
        lStatus = drvSpiRawTransferLocked(spi,
                                          transfer->secondWriteBuffer,
                                          NULL,
                                          transfer->secondWriteLength,
                                          transfer->readFillData,
                                          timeoutMs);
        if (lStatus != DRV_STATUS_OK) {
            drvSpiSetCsActive(lBspInterface, false);
            return lStatus;
        }
    }

    if (transfer->readLength > 0U) {
        lStatus = drvSpiRawTransferLocked(spi,
                                          NULL,
                                          transfer->readBuffer,
                                          transfer->readLength,
                                          transfer->readFillData,
                                          timeoutMs);
        drvSpiSetCsActive(lBspInterface, false);
        return lStatus;
    }

    drvSpiSetCsActive(lBspInterface, false);
    return DRV_STATUS_OK;
}

eDrvStatus drvSpiInit(uint8_t spi)
{
    stDrvSpiBspInterface *lBspInterface = NULL;
    eDrvStatus lStatus;

    if (!drvSpiIsValid(spi)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (drvSpiIsInitialized(spi)) {
        return DRV_STATUS_OK;
    }

    if (!drvSpiHasValidBspInterface(spi)) {
        #if (DRVSPI_LOG_SUPPORT == 1)
        LOG_E(DRVSPI_LOG_TAG, "Invalid BSP interface for bus %u", (unsigned int)spi);
        #endif
        return DRV_STATUS_NOT_READY;
    }

    lBspInterface = drvSpiGetBspInterface(spi);
    if (lBspInterface == NULL) {
        return DRV_STATUS_NOT_READY;
    }

    lStatus = drvSpiEnsureMutex(spi);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    lStatus = lBspInterface->init(spi);
    if (lStatus != DRV_STATUS_OK) {
        #if (DRVSPI_LOG_SUPPORT == 1)
        LOG_E(DRVSPI_LOG_TAG, "SPI bus %u init failed, status=%d", (unsigned int)spi, (int)lStatus);
        #endif
        return lStatus;
    }

    drvSpiInitCsControl(lBspInterface);
    gDrvSpiInitialized[spi] = true;
    return DRV_STATUS_OK;
}

eDrvStatus drvSpiSetCsControl(uint8_t spi, const stDrvSpiCsControl *control)
{
    stDrvSpiBspInterface *lBspInterface = drvSpiGetBspInterface(spi);

    if (!drvSpiIsValid(spi) || (lBspInterface == NULL)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (control == NULL) {
        (void)memset(&lBspInterface->csControl, 0, sizeof(lBspInterface->csControl));
        return DRV_STATUS_OK;
    }

    lBspInterface->csControl = *control;
    if (drvSpiIsInitialized(spi)) {
        drvSpiInitCsControl(lBspInterface);
    }

    return DRV_STATUS_OK;
}

eDrvStatus drvSpiTransferTimeout(uint8_t spi, const stDrvSpiTransfer *transfer, uint32_t timeoutMs)
{
    eDrvStatus lStatus;

    if (!drvSpiIsValid(spi) || !drvSpiIsValidTransfer(transfer)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (!drvSpiIsInitialized(spi)) {
        return DRV_STATUS_NOT_READY;
    }

    lStatus = drvSpiLockBus(spi);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    lStatus = drvSpiTransferLocked(spi, transfer, timeoutMs);
    drvSpiUnlockBus(spi);
    return lStatus;
}

eDrvStatus drvSpiTransfer(uint8_t spi, const stDrvSpiTransfer *transfer)
{
    return drvSpiTransferTimeout(spi, transfer, 0U);
}

eDrvStatus drvSpiWriteTimeout(uint8_t spi, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs)
{
    stDrvSpiTransfer lTransfer = {
        .writeBuffer = buffer,
        .writeLength = length,
        .secondWriteBuffer = NULL,
        .secondWriteLength = 0U,
        .readBuffer = NULL,
        .readLength = 0U,
        .readFillData = DRVSPI_DEFAULT_READ_FILL_DATA,
    };

    return drvSpiTransferTimeout(spi, &lTransfer, timeoutMs);
}

eDrvStatus drvSpiWrite(uint8_t spi, const uint8_t *buffer, uint16_t length)
{
    return drvSpiWriteTimeout(spi, buffer, length, 0U);
}

eDrvStatus drvSpiReadTimeout(uint8_t spi, uint8_t *buffer, uint16_t length, uint32_t timeoutMs)
{
    stDrvSpiTransfer lTransfer = {
        .writeBuffer = NULL,
        .writeLength = 0U,
        .secondWriteBuffer = NULL,
        .secondWriteLength = 0U,
        .readBuffer = buffer,
        .readLength = length,
        .readFillData = DRVSPI_DEFAULT_READ_FILL_DATA,
    };

    return drvSpiTransferTimeout(spi, &lTransfer, timeoutMs);
}

eDrvStatus drvSpiRead(uint8_t spi, uint8_t *buffer, uint16_t length)
{
    return drvSpiReadTimeout(spi, buffer, length, 0U);
}

eDrvStatus drvSpiWriteReadTimeout(uint8_t spi, const uint8_t *writeBuffer, uint16_t writeLength, uint8_t *readBuffer, uint16_t readLength, uint32_t timeoutMs)
{
    stDrvSpiTransfer lTransfer = {
        .writeBuffer = writeBuffer,
        .writeLength = writeLength,
        .secondWriteBuffer = NULL,
        .secondWriteLength = 0U,
        .readBuffer = readBuffer,
        .readLength = readLength,
        .readFillData = DRVSPI_DEFAULT_READ_FILL_DATA,
    };

    return drvSpiTransferTimeout(spi, &lTransfer, timeoutMs);
}

eDrvStatus drvSpiWriteRead(uint8_t spi, const uint8_t *writeBuffer, uint16_t writeLength, uint8_t *readBuffer, uint16_t readLength)
{
    return drvSpiWriteReadTimeout(spi, writeBuffer, writeLength, readBuffer, readLength, 0U);
}

eDrvStatus drvSpiExchangeTimeout(uint8_t spi, const uint8_t *txBuffer, uint8_t *rxBuffer, uint16_t length, uint32_t timeoutMs)
{
    stDrvSpiBspInterface *lBspInterface;
    eDrvStatus lStatus;

    if (!drvSpiIsValid(spi) ||
        !drvSpiIsValidConstBuffer(txBuffer, length) ||
        !drvSpiIsValidMutableBuffer(rxBuffer, length) ||
        (length == 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (!drvSpiIsInitialized(spi)) {
        return DRV_STATUS_NOT_READY;
    }

    lStatus = drvSpiLockBus(spi);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    lBspInterface = drvSpiGetBspInterface(spi);
    drvSpiSetCsActive(lBspInterface, true);
    lStatus = drvSpiRawTransferLocked(spi,txBuffer,rxBuffer,length,DRVSPI_DEFAULT_READ_FILL_DATA,timeoutMs);
    drvSpiSetCsActive(lBspInterface, false);
    drvSpiUnlockBus(spi);
    return lStatus;
}

eDrvStatus drvSpiExchange(uint8_t spi, const uint8_t *txBuffer, uint8_t *rxBuffer, uint16_t length)
{
    return drvSpiExchangeTimeout(spi, txBuffer, rxBuffer, length, 0U);
}

/**************************End of file********************************/
