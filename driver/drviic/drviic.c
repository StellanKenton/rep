/***********************************************************************************
* @file     : drviic.c
* @brief    : Reusable hardware IIC driver abstraction implementation.
* @details  : This module validates public parameters, serializes bus access,
*             and forwards transactions to the project-specific BSP hook table.
* @author   : GitHub Copilot
* @date     : 2026-04-01
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "drviic.h"

#if (DRVIIC_LOG_SUPPORT == 1)
#include "../../service/log/log.h"
#endif

#include <stdbool.h>
#include <stddef.h>

#include "rep_config.h"
#include "../../service/rtos/rtos.h"

#define DRVIIC_LOG_TAG                   "drvIic"

static bool gDrvIicInitialized[DRVIIC_MAX];
static stRepRtosMutex gDrvIicMutex[DRVIIC_MAX];

__attribute__((weak)) const stDrvIicBspInterface *drvIicGetPlatformBspInterfaces(void)
{
    return NULL;
}

static eDrvStatus drvIicMapRtosStatus(eRepRtosStatus status)
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

static bool drvIicIsValid(uint8_t iic)
{
    return iic < DRVIIC_MAX;
}

static bool drvIicIsValidAddress(uint8_t address)
{
    return address < 0x80U;
}

static bool drvIicIsValidConstBuffer(const uint8_t *buffer, uint16_t length)
{
    return ((buffer != NULL) || (length == 0U));
}

static bool drvIicIsValidMutableBuffer(uint8_t *buffer, uint16_t length)
{
    return ((buffer != NULL) || (length == 0U));
}

static bool drvIicHasWritePhase(const stDrvIicTransfer *transfer)
{
    return (transfer != NULL) &&
           ((transfer->writeLength > 0U) || (transfer->secondWriteLength > 0U));
}

static bool drvIicHasReadPhase(const stDrvIicTransfer *transfer)
{
    return (transfer != NULL) && (transfer->readLength > 0U);
}

static bool drvIicIsValidTransfer(const stDrvIicTransfer *transfer)
{
    if (transfer == NULL) {
        return false;
    }

    if (!drvIicIsValidAddress(transfer->address)) {
        return false;
    }

    if (!drvIicIsValidConstBuffer(transfer->writeBuffer, transfer->writeLength)) {
        return false;
    }

    if (!drvIicIsValidConstBuffer(transfer->secondWriteBuffer, transfer->secondWriteLength)) {
        return false;
    }

    if (!drvIicIsValidMutableBuffer(transfer->readBuffer, transfer->readLength)) {
        return false;
    }

    return drvIicHasWritePhase(transfer) || drvIicHasReadPhase(transfer);
}

static bool drvIicIsInitialized(uint8_t iic)
{
    return drvIicIsValid(iic) && gDrvIicInitialized[iic];
}

static stDrvIicBspInterface *drvIicGetBspInterface(uint8_t iic)
{
    const stDrvIicBspInterface *lInterfaces;

    if (!drvIicIsValid(iic)) {
        return NULL;
    }

    lInterfaces = drvIicGetPlatformBspInterfaces();
    if (lInterfaces == NULL) {
        return NULL;
    }

    return (stDrvIicBspInterface *)&lInterfaces[iic];
}

static bool drvIicHasValidBspInterface(uint8_t iic)
{
    stDrvIicBspInterface *lBspInterface = drvIicGetBspInterface(iic);

    return (lBspInterface != NULL) &&
           (lBspInterface->init != NULL) &&
           (lBspInterface->transfer != NULL);
}

static uint32_t drvIicGetTimeoutMs(const stDrvIicBspInterface *bspInterface, uint32_t timeoutMs)
{
    if (timeoutMs > 0U) {
        return timeoutMs;
    }

    if ((bspInterface != NULL) && (bspInterface->defaultTimeoutMs > 0U)) {
        return bspInterface->defaultTimeoutMs;
    }

    return DRVIIC_DEFAULT_TIMEOUT_MS;
}

static eDrvStatus drvIicEnsureMutex(uint8_t iic)
{
    if (!drvIicIsValid(iic)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (!gDrvIicMutex[iic].isCreated) {
        return drvIicMapRtosStatus(repRtosMutexCreate(&gDrvIicMutex[iic]));
    }

    return DRV_STATUS_OK;
}

static eDrvStatus drvIicLockBus(uint8_t iic)
{
    if (drvIicEnsureMutex(iic) != DRV_STATUS_OK) {
        return DRV_STATUS_ERROR;
    }

    return drvIicMapRtosStatus(repRtosMutexTake(&gDrvIicMutex[iic], DRVIIC_LOCK_WAIT_MS));
}

static void drvIicUnlockBus(uint8_t iic)
{
    if (drvIicIsValid(iic)) {
        (void)repRtosMutexGive(&gDrvIicMutex[iic]);
    }
}

static eDrvStatus drvIicTransferLocked(uint8_t iic, const stDrvIicTransfer *transfer, uint32_t timeoutMs)
{
    stDrvIicBspInterface *lBspInterface = drvIicGetBspInterface(iic);

    if ((lBspInterface == NULL) || (lBspInterface->transfer == NULL)) {
        return DRV_STATUS_NOT_READY;
    }

    return lBspInterface->transfer(iic, transfer, drvIicGetTimeoutMs(lBspInterface, timeoutMs));
}

eDrvStatus drvIicInit(uint8_t iic)
{
    stDrvIicBspInterface *lBspInterface = NULL;
    eDrvStatus lStatus;

    if (!drvIicIsValid(iic)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (drvIicIsInitialized(iic)) {
        return DRV_STATUS_OK;
    }

    if (!drvIicHasValidBspInterface(iic)) {
        #if (DRVIIC_LOG_SUPPORT == 1)
        LOG_E(DRVIIC_LOG_TAG, "Invalid BSP interface for bus %u", (unsigned int)iic);
        #endif
        return DRV_STATUS_NOT_READY;
    }

    lBspInterface = drvIicGetBspInterface(iic);
    if (lBspInterface == NULL) {
        return DRV_STATUS_NOT_READY;
    }

    lStatus = drvIicEnsureMutex(iic);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    lStatus = lBspInterface->init(iic);
    if (lStatus != DRV_STATUS_OK) {
        #if (DRVIIC_LOG_SUPPORT == 1)
        LOG_E(DRVIIC_LOG_TAG, "IIC bus %u init failed, status=%d", (unsigned int)iic, (int)lStatus);
        #endif
        return lStatus;
    }

    gDrvIicInitialized[iic] = true;
    return DRV_STATUS_OK;
}

eDrvStatus drvIicRecoverBus(uint8_t iic)
{
    stDrvIicBspInterface *lBspInterface = NULL;
    eDrvStatus lStatus;

    if (!drvIicIsValid(iic)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (!drvIicIsInitialized(iic)) {
        return DRV_STATUS_NOT_READY;
    }

    lBspInterface = drvIicGetBspInterface(iic);
    if ((lBspInterface == NULL) || (lBspInterface->recoverBus == NULL)) {
        return DRV_STATUS_UNSUPPORTED;
    }

    lStatus = drvIicLockBus(iic);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    lStatus = lBspInterface->recoverBus(iic);
    drvIicUnlockBus(iic);
    if (lStatus != DRV_STATUS_OK) {
        #if (DRVIIC_LOG_SUPPORT == 1)
        LOG_W(DRVIIC_LOG_TAG, "IIC bus %u recover failed, status=%d", (unsigned int)iic, (int)lStatus);
        #endif
    }
    return lStatus;
}

eDrvStatus drvIicTransferTimeout(uint8_t iic, const stDrvIicTransfer *transfer, uint32_t timeoutMs)
{
    eDrvStatus lStatus;

    if (!drvIicIsValid(iic) || !drvIicIsValidTransfer(transfer)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (!drvIicIsInitialized(iic)) {
        return DRV_STATUS_NOT_READY;
    }

    lStatus = drvIicLockBus(iic);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    lStatus = drvIicTransferLocked(iic, transfer, timeoutMs);
    drvIicUnlockBus(iic);
    return lStatus;
}

eDrvStatus drvIicTransfer(uint8_t iic, const stDrvIicTransfer *transfer)
{
    return drvIicTransferTimeout(iic, transfer, 0U);
}

eDrvStatus drvIicWriteTimeout(uint8_t iic, uint8_t address, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs)
{
    stDrvIicTransfer lTransfer;

    lTransfer.address = address;
    lTransfer.writeBuffer = buffer;
    lTransfer.writeLength = length;
    lTransfer.readBuffer = NULL;
    lTransfer.readLength = 0U;
    lTransfer.secondWriteBuffer = NULL;
    lTransfer.secondWriteLength = 0U;

    return drvIicTransferTimeout(iic, &lTransfer, timeoutMs);
}

eDrvStatus drvIicWrite(uint8_t iic, uint8_t address, const uint8_t *buffer, uint16_t length)
{
    return drvIicWriteTimeout(iic, address, buffer, length, 0U);
}

eDrvStatus drvIicReadTimeout(uint8_t iic, uint8_t address, uint8_t *buffer, uint16_t length, uint32_t timeoutMs)
{
    stDrvIicTransfer lTransfer;

    lTransfer.address = address;
    lTransfer.writeBuffer = NULL;
    lTransfer.writeLength = 0U;
    lTransfer.readBuffer = buffer;
    lTransfer.readLength = length;
    lTransfer.secondWriteBuffer = NULL;
    lTransfer.secondWriteLength = 0U;

    return drvIicTransferTimeout(iic, &lTransfer, timeoutMs);
}

eDrvStatus drvIicRead(uint8_t iic, uint8_t address, uint8_t *buffer, uint16_t length)
{
    return drvIicReadTimeout(iic, address, buffer, length, 0U);
}

eDrvStatus drvIicWriteRegisterTimeout(uint8_t iic, uint8_t address, const uint8_t *registerBuffer, uint16_t registerLength, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs)
{
    stDrvIicTransfer lTransfer;

    lTransfer.address = address;
    lTransfer.writeBuffer = registerBuffer;
    lTransfer.writeLength = registerLength;
    lTransfer.readBuffer = NULL;
    lTransfer.readLength = 0U;
    lTransfer.secondWriteBuffer = buffer;
    lTransfer.secondWriteLength = length;

    return drvIicTransferTimeout(iic, &lTransfer, timeoutMs);
}

eDrvStatus drvIicWriteRegister(uint8_t iic, uint8_t address, const uint8_t *registerBuffer, uint16_t registerLength, const uint8_t *buffer, uint16_t length)
{
    return drvIicWriteRegisterTimeout(iic, address, registerBuffer, registerLength, buffer, length, 0U);
}

eDrvStatus drvIicReadRegisterTimeout(uint8_t iic, uint8_t address, const uint8_t *registerBuffer, uint16_t registerLength, uint8_t *buffer, uint16_t length, uint32_t timeoutMs)
{
    stDrvIicTransfer lTransfer;

    lTransfer.address = address;
    lTransfer.writeBuffer = registerBuffer;
    lTransfer.writeLength = registerLength;
    lTransfer.readBuffer = buffer;
    lTransfer.readLength = length;
    lTransfer.secondWriteBuffer = NULL;
    lTransfer.secondWriteLength = 0U;

    return drvIicTransferTimeout(iic, &lTransfer, timeoutMs);
}

eDrvStatus drvIicReadRegister(uint8_t iic, uint8_t address, const uint8_t *registerBuffer, uint16_t registerLength, uint8_t *buffer, uint16_t length)
{
    return drvIicReadRegisterTimeout(iic, address, registerBuffer, registerLength, buffer, length, 0U);
}

/**************************End of file********************************/
