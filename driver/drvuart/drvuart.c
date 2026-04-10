/***********************************************************************************
* @file     : drvuart.c
* @brief    : Generic MCU UART driver abstraction implementation.
* @details  : This module provides a small UART interface for project-level drivers.
* @author   : GitHub Copilot
* @date     : 2026-03-30
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "drvuart.h"

#if (DRVUART_LOG_SUPPORT == 1)
#include "../../Console/log.h"
#endif

#include <stdbool.h>
#include <stddef.h>

#define DRVUART_LOG_TAG                 "drvUart"

static bool gDrvUartInitialized[DRVUART_MAX];

__attribute__((weak)) const stDrvUartBspInterface *drvUartGetPlatformBspInterfaces(void)
{
    return NULL;
}

__attribute__((weak)) stRingBuffer *drvUartGetPlatformRingBuffer(uint8_t uart)
{
    (void)uart;
    return NULL;
}

__attribute__((weak)) eDrvStatus drvUartGetPlatformStorageConfig(uint8_t uart, uint8_t **storage, uint32_t *capacity)
{
    (void)uart;

    if (storage != NULL) {
        *storage = NULL;
    }

    if (capacity != NULL) {
        *capacity = 0U;
    }

    return DRV_STATUS_UNSUPPORTED;
}

/**
* @brief : Check if the provided logical UART mapping is valid.
* @param : uart UART mapping identifier.
* @return: true if the UART mapping is valid, false otherwise.
**/
static bool drvUartIsValid(uint8_t uart)
{
    return (uart < DRVUART_MAX);
}

/**
* @brief : Get the BSP interface entry for a logical UART.
* @param : uart UART mapping identifier.
* @return: Pointer to the BSP interface entry, or NULL when invalid.
**/
static stDrvUartBspInterface *drvUartGetBspInterface(uint8_t uart)
{
    const stDrvUartBspInterface *lInterfaces;

    if (!drvUartIsValid(uart)) {
        return NULL;
    }

    lInterfaces = drvUartGetPlatformBspInterfaces();
    if (lInterfaces == NULL) {
        return NULL;
    }

    return (stDrvUartBspInterface *)&lInterfaces[uart];
}

/**
* @brief : Check whether the BSP hook table is complete for basic UART usage.
* @param : None
* @return: true when required hooks are available.
**/
static bool drvUartHasValidBspInterface(uint8_t uart)
{
    stDrvUartBspInterface *lBspInterface = drvUartGetBspInterface(uart);

    return (lBspInterface != NULL) &&
           (lBspInterface->init != NULL) &&
           (lBspInterface->transmit != NULL) &&
           (lBspInterface->getDataLen != NULL) &&
           (lBspInterface->receive != NULL);
}

/**
* @brief : Check if the provided UART buffer is valid.
* @param : buffer UART data buffer pointer.
* @param : length UART data length.
* @return: true if the buffer is valid, false otherwise.
**/
static bool drvUartIsValidBuffer(const uint8_t *buffer, uint16_t length)
{
    return (buffer != NULL) && (length > 0U);
}

/**
* @brief : Check whether the logical UART has already been initialized.
* @param : uart UART mapping identifier.
* @return: true if the logical UART is initialized.
**/
static bool drvUartIsInitialized(uint8_t uart)
{
    return drvUartIsValid(uart) && gDrvUartInitialized[uart];
}

/**
* @brief : Pull pending bytes from BSP RX storage into the drv ring buffer.
* @param : uart UART mapping identifier.
* @return: UART operation status.
**/
static eDrvStatus drvUartSyncRxData(uint8_t uart)
{
    uint8_t lScratch[DRVUART_BSP_SYNC_CHUNK_SIZE];
    stRingBuffer *lRingBuffer = NULL;
    stDrvUartBspInterface *lBspInterface = NULL;
    uint32_t lRingFree = 0U;
    uint16_t lPending = 0U;

    if (!drvUartIsInitialized(uart)) {
        return DRV_STATUS_NOT_READY;
    }

    lBspInterface = drvUartGetBspInterface(uart);
    if ((lBspInterface == NULL) || (lBspInterface->getDataLen == NULL) || (lBspInterface->receive == NULL)) {
        return DRV_STATUS_NOT_READY;
    }

    lRingBuffer = drvUartGetPlatformRingBuffer(uart);
    if (lRingBuffer == NULL) {
        return DRV_STATUS_ERROR;
    }

    lRingFree = ringBufferGetFree(lRingBuffer);
    lPending = lBspInterface->getDataLen(uart);

    while ((lPending > 0U) && (lRingFree > 0U)) {
        uint16_t lChunkLength = lPending;

        if (lChunkLength > DRVUART_BSP_SYNC_CHUNK_SIZE) {
            lChunkLength = DRVUART_BSP_SYNC_CHUNK_SIZE;
        }

        if ((uint32_t)lChunkLength > lRingFree) {
            lChunkLength = (uint16_t)lRingFree;
        }

        if (lBspInterface->receive(uart, lScratch, lChunkLength) != DRV_STATUS_OK) {
            return DRV_STATUS_ERROR;
        }

        if (ringBufferWrite(lRingBuffer, lScratch, lChunkLength) != (uint32_t)lChunkLength) {
            return DRV_STATUS_ERROR;
        }

        lRingFree -= lChunkLength;
        lPending = lBspInterface->getDataLen(uart);
    }

    return DRV_STATUS_OK;
}

/**
* @brief : Initialize the UART driver and configure related resources.
* @param : uart UART mapping identifier.
* @return: UART operation status.
**/
eDrvStatus drvUartInit(uint8_t uart)
{
    uint8_t *lStorage = NULL;
    uint32_t lCapacity = 0U;
    stRingBuffer *lRingBuffer = NULL;
    stDrvUartBspInterface *lBspInterface = NULL;
    eDrvStatus lStatus;

    if (!drvUartIsValid(uart)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (!drvUartHasValidBspInterface(uart)) {
        #if (DRVUART_LOG_SUPPORT == 1)
        LOG_E(DRVUART_LOG_TAG, "Invalid BSP interface for uart %u", (unsigned int)uart);
        #endif
        return DRV_STATUS_NOT_READY;
    }

    lBspInterface = drvUartGetBspInterface(uart);
    if (lBspInterface == NULL) {
        return DRV_STATUS_NOT_READY;
    }

    lStatus = drvUartGetPlatformStorageConfig(uart, &lStorage, &lCapacity);
    if (lStatus != DRV_STATUS_OK) {
        #if (DRVUART_LOG_SUPPORT == 1)
        LOG_E(DRVUART_LOG_TAG, "Get storage config failed for uart %u, status=%d", (unsigned int)uart, (int)lStatus);
        #endif
        return lStatus;
    }

    if (lBspInterface->Buffer == NULL) {
        #if (DRVUART_LOG_SUPPORT == 1)
        LOG_E(DRVUART_LOG_TAG, "UART %u rx buffer is not configured", (unsigned int)uart);
        #endif
        return DRV_STATUS_NOT_READY;
    }

    lStorage = lBspInterface->Buffer;

    lRingBuffer = drvUartGetPlatformRingBuffer(uart);
    if (lRingBuffer == NULL) {
        #if (DRVUART_LOG_SUPPORT == 1)
        LOG_E(DRVUART_LOG_TAG, "UART %u ring buffer is not available", (unsigned int)uart);
        #endif
        return DRV_STATUS_ERROR;
    }

    if (ringBufferInit(lRingBuffer, lStorage, lCapacity) != RINGBUFFER_OK) {
        #if (DRVUART_LOG_SUPPORT == 1)
        LOG_E(DRVUART_LOG_TAG, "UART %u ring buffer init failed", (unsigned int)uart);
        #endif
        return DRV_STATUS_ERROR;
    }

    lStatus = lBspInterface->init(uart);
    if (lStatus != DRV_STATUS_OK) {
        #if (DRVUART_LOG_SUPPORT == 1)
        LOG_E(DRVUART_LOG_TAG, "UART %u init failed, status=%d", (unsigned int)uart, (int)lStatus);
        #endif
        return lStatus;
    }

    gDrvUartInitialized[uart] = true;
    return DRV_STATUS_OK;
}

/**
* @brief : Transmit data through UART in polling mode.
* @param : uart      UART mapping identifier.
* @param : buffer    Pointer to the transmit buffer.
* @param : length    Number of bytes to transmit.
* @param : timeoutMs Timeout in milliseconds.
* @return: UART operation status.
**/
eDrvStatus drvUartTransmit(uint8_t uart, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs)
{
    stDrvUartBspInterface *lBspInterface = NULL;

    if (!drvUartIsValid(uart) || !drvUartIsValidBuffer(buffer, length)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (!drvUartIsInitialized(uart)) {
        return DRV_STATUS_NOT_READY;
    }

    lBspInterface = drvUartGetBspInterface(uart);
    if ((lBspInterface == NULL) || (lBspInterface->transmit == NULL)) {
        return DRV_STATUS_NOT_READY;
    }

    return lBspInterface->transmit(uart, buffer, length, timeoutMs);
}

/**
* @brief : Start UART transmit in interrupt mode.
* @param : uart   UART mapping identifier.
* @param : buffer Pointer to the transmit buffer.
* @param : length Number of bytes to transmit.
* @return: UART operation status.
**/
eDrvStatus drvUartTransmitIt(uint8_t uart, const uint8_t *buffer, uint16_t length)
{
    stDrvUartBspInterface *lBspInterface = NULL;

    if (!drvUartIsValid(uart) || !drvUartIsValidBuffer(buffer, length)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (!drvUartIsInitialized(uart)) {
        return DRV_STATUS_NOT_READY;
    }

    lBspInterface = drvUartGetBspInterface(uart);
    if ((lBspInterface == NULL) || (lBspInterface->transmitIt == NULL)) {
        return DRV_STATUS_UNSUPPORTED;
    }

    return lBspInterface->transmitIt(uart, buffer, length);
}

/**
* @brief : Start UART transmit in DMA mode.
* @param : uart   UART mapping identifier.
* @param : buffer Pointer to the transmit buffer.
* @param : length Number of bytes to transmit.
* @return: UART operation status.
**/
eDrvStatus drvUartTransmitDma(uint8_t uart, const uint8_t *buffer, uint16_t length)
{
    stDrvUartBspInterface *lBspInterface = NULL;

    if (!drvUartIsValid(uart) || !drvUartIsValidBuffer(buffer, length)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (!drvUartIsInitialized(uart)) {
        return DRV_STATUS_NOT_READY;
    }

    lBspInterface = drvUartGetBspInterface(uart);
    if ((lBspInterface == NULL) || (lBspInterface->transmitDma == NULL)) {
        return DRV_STATUS_UNSUPPORTED;
    }

    return lBspInterface->transmitDma(uart, buffer, length);
}

/**
* @brief : Receive data from UART.
* @param : uart   UART mapping identifier.
* @param : buffer Pointer to the receive buffer.
* @param : length Number of bytes to receive.
* @return: UART operation status.
**/
eDrvStatus drvUartReceive(uint8_t uart, uint8_t *buffer, uint16_t length)
{
    stRingBuffer *lRingBuffer = NULL;
    eDrvStatus lStatus;

    if (!drvUartIsValid(uart) || !drvUartIsValidBuffer(buffer, length)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (!drvUartIsInitialized(uart)) {
        return DRV_STATUS_NOT_READY;
    }

    lStatus = drvUartSyncRxData(uart);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    lRingBuffer = drvUartGetPlatformRingBuffer(uart);
    if (lRingBuffer == NULL) {
        return DRV_STATUS_ERROR;
    }

    if (ringBufferGetUsed(lRingBuffer) < (uint32_t)length) {
        return DRV_STATUS_NOT_READY;
    }

    if (ringBufferRead(lRingBuffer, buffer, length) != (uint32_t)length) {
        return DRV_STATUS_ERROR;
    }

    return DRV_STATUS_OK;
}

/**
* @brief : Get the amount of data currently available in the UART receive path.
* @param : uart UART mapping identifier.
* @return: Available data length in bytes.
**/
uint16_t drvUartGetDataLen(uint8_t uart)
{
    uint32_t lUsed;

    if (!drvUartIsValid(uart)) {
        return 0U;
    }

    if (!drvUartIsInitialized(uart)) {
        return 0U;
    }

    if (drvUartSyncRxData(uart) != DRV_STATUS_OK) {
        return 0U;
    }

    lUsed = ringBufferGetUsed(drvUartGetPlatformRingBuffer(uart));
    if (lUsed > UINT16_MAX) {
        return UINT16_MAX;
    }

    return (uint16_t)lUsed;
}

/**
* @brief : Get the pointer to the UART receive ring buffer.
* @param : uart UART mapping identifier.
* @return: Pointer to the UART receive ring buffer, or NULL if invalid.
**/
stRingBuffer* drvUartGetRingBuffer(uint8_t uart)
{
    if (!drvUartIsValid(uart)) {
        return NULL;
    }

    if (!drvUartIsInitialized(uart)) {
        return NULL;
    }

    if (drvUartSyncRxData(uart) != DRV_STATUS_OK) {
        return NULL;
    }

    return drvUartGetPlatformRingBuffer(uart);
}

/**************************End of file********************************/
