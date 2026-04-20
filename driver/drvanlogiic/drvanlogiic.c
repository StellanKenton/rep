/***********************************************************************************
* @file     : drvanlogiic.c
* @brief    : Reusable software IIC driver abstraction implementation.
* @details  : This module implements stable bit-banged IIC transfers with bus
*             recovery, clock stretching support, and task-safe bus locking.
* @author   : GitHub Copilot
* @date     : 2026-04-01
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "drvanlogiic.h"

#include <stdbool.h>
#include <stddef.h>

#include "rep_config.h"
#include "../../service/rtos/rtos.h"

#if (DRVANLOGIIC_LOG_SUPPORT == 1)
#include "../../service/log/log.h"
#endif

#define DRVANLOGIIC_LOG_TAG                  "drvAnlogIic"

static bool gDrvAnlogIicInitialized[DRVANLOGIIC_MAX];
static stRepRtosMutex gDrvAnlogIicMutex[DRVANLOGIIC_MAX];

__attribute__((weak)) const stDrvAnlogIicBspInterface *drvAnlogIicGetPlatformBspInterfaces(void)
{
    return NULL;
}

static eDrvStatus drvAnlogIicMapRtosStatus(eRepRtosStatus status)
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

static bool drvAnlogIicIsValid(uint8_t iic)
{
    return iic < DRVANLOGIIC_MAX;
}

static bool drvAnlogIicIsValidAddress(uint8_t address)
{
    return address < 0x80U;
}

static bool drvAnlogIicIsValidWriteBuffer(const uint8_t *buffer, uint16_t length)
{
    return ((buffer != NULL) || (length == 0U));
}

static bool drvAnlogIicIsValidReadBuffer(uint8_t *buffer, uint16_t length)
{
    return ((buffer != NULL) || (length == 0U));
}

static stDrvAnlogIicBspInterface *drvAnlogIicGetBspInterface(uint8_t iic)
{
    const stDrvAnlogIicBspInterface *lInterfaces;

    if (!drvAnlogIicIsValid(iic)) {
        return NULL;
    }

    lInterfaces = drvAnlogIicGetPlatformBspInterfaces();
    if (lInterfaces == NULL) {
        return NULL;
    }

    return (stDrvAnlogIicBspInterface *)&lInterfaces[iic];
}

static bool drvAnlogIicHasValidBspInterface(uint8_t iic)
{
    stDrvAnlogIicBspInterface *lBspInterface = drvAnlogIicGetBspInterface(iic);

    return (lBspInterface != NULL) &&
           (lBspInterface->init != NULL) &&
           (lBspInterface->setScl != NULL) &&
           (lBspInterface->setSda != NULL) &&
           (lBspInterface->readScl != NULL) &&
           (lBspInterface->readSda != NULL) &&
           (lBspInterface->delayUs != NULL);
}

static uint16_t drvAnlogIicGetHalfPeriodUs(const stDrvAnlogIicBspInterface *bspInterface)
{
    if ((bspInterface == NULL) || (bspInterface->halfPeriodUs == 0U)) {
        return DRVANLOGIIC_DEFAULT_HALF_PERIOD_US;
    }

    return bspInterface->halfPeriodUs;
}

static uint8_t drvAnlogIicGetRecoveryClockCount(const stDrvAnlogIicBspInterface *bspInterface)
{
    if ((bspInterface == NULL) || (bspInterface->recoveryClockCount == 0U)) {
        return DRVANLOGIIC_DEFAULT_RECOVERY_CLOCKS;
    }

    return bspInterface->recoveryClockCount;
}

static void drvAnlogIicDelayHalfPeriod(const stDrvAnlogIicBspInterface *bspInterface)
{
    bspInterface->delayUs(drvAnlogIicGetHalfPeriodUs(bspInterface));
}

static eDrvStatus drvAnlogIicWaitSclHigh(uint8_t iic, stDrvAnlogIicBspInterface *bspInterface)
{
    if (bspInterface == NULL) {
        return DRV_STATUS_NOT_READY;
    }

    bspInterface->setScl(iic, true);
    return DRV_STATUS_OK;
}

static eDrvStatus drvAnlogIicWriteBit(uint8_t iic, stDrvAnlogIicBspInterface *bspInterface, bool isHigh)
{
    eDrvStatus lStatus;

    bspInterface->setSda(iic, isHigh);
    drvAnlogIicDelayHalfPeriod(bspInterface);

    lStatus = drvAnlogIicWaitSclHigh(iic, bspInterface);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    drvAnlogIicDelayHalfPeriod(bspInterface);
    bspInterface->setScl(iic, false);
    drvAnlogIicDelayHalfPeriod(bspInterface);
    return DRV_STATUS_OK;
}

static eDrvStatus drvAnlogIicReadBit(uint8_t iic, stDrvAnlogIicBspInterface *bspInterface, bool *bitValue)
{
    eDrvStatus lStatus;

    if (bitValue == NULL) {
        return DRV_STATUS_INVALID_PARAM;
    }

    bspInterface->setSda(iic, true);
    drvAnlogIicDelayHalfPeriod(bspInterface);

    lStatus = drvAnlogIicWaitSclHigh(iic, bspInterface);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    drvAnlogIicDelayHalfPeriod(bspInterface);
    *bitValue = bspInterface->readSda(iic);
    bspInterface->setScl(iic, false);
    drvAnlogIicDelayHalfPeriod(bspInterface);
    return DRV_STATUS_OK;
}

static eDrvStatus drvAnlogIicSendStart(uint8_t iic, stDrvAnlogIicBspInterface *bspInterface)
{
    eDrvStatus lStatus;

    bspInterface->setSda(iic, true);
    lStatus = drvAnlogIicWaitSclHigh(iic, bspInterface);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    drvAnlogIicDelayHalfPeriod(bspInterface);
    bspInterface->setSda(iic, false);
    drvAnlogIicDelayHalfPeriod(bspInterface);
    bspInterface->setScl(iic, false);
    drvAnlogIicDelayHalfPeriod(bspInterface);
    return DRV_STATUS_OK;
}

static eDrvStatus drvAnlogIicSendStop(uint8_t iic, stDrvAnlogIicBspInterface *bspInterface)
{
    eDrvStatus lStatus;

    bspInterface->setSda(iic, false);
    drvAnlogIicDelayHalfPeriod(bspInterface);

    lStatus = drvAnlogIicWaitSclHigh(iic, bspInterface);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    drvAnlogIicDelayHalfPeriod(bspInterface);
    bspInterface->setSda(iic, true);
    drvAnlogIicDelayHalfPeriod(bspInterface);
    return DRV_STATUS_OK;
}

static eDrvStatus drvAnlogIicWriteByte(uint8_t iic, stDrvAnlogIicBspInterface *bspInterface, uint8_t data)
{
    uint8_t lBitIndex;
    bool lAckLevel = true;
    eDrvStatus lStatus;

    for (lBitIndex = 0U; lBitIndex < 8U; ++lBitIndex) {
        lStatus = drvAnlogIicWriteBit(iic, bspInterface, ((data & 0x80U) != 0U));
        if (lStatus != DRV_STATUS_OK) {
            return lStatus;
        }

        data <<= 1U;
    }

    lStatus = drvAnlogIicReadBit(iic, bspInterface, &lAckLevel);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    return lAckLevel ? DRV_STATUS_NACK : DRV_STATUS_OK;
}

static eDrvStatus drvAnlogIicReadByte(uint8_t iic, stDrvAnlogIicBspInterface *bspInterface, bool acknowledge, uint8_t *data)
{
    uint8_t lBitIndex;
    bool lBitValue = false;
    uint8_t lData = 0U;
    eDrvStatus lStatus;

    if (data == NULL) {
        return DRV_STATUS_INVALID_PARAM;
    }

    for (lBitIndex = 0U; lBitIndex < 8U; ++lBitIndex) {
        lStatus = drvAnlogIicReadBit(iic, bspInterface, &lBitValue);
        if (lStatus != DRV_STATUS_OK) {
            return lStatus;
        }

        lData <<= 1U;
        if (lBitValue) {
            lData |= 0x01U;
        }
    }

    *data = lData;
    return drvAnlogIicWriteBit(iic, bspInterface, !acknowledge);
}

static eDrvStatus drvAnlogIicWriteBufferLocked(uint8_t iic, stDrvAnlogIicBspInterface *bspInterface, uint8_t address, const uint8_t *buffer, uint16_t length)
{
    uint16_t lIndex;
    eDrvStatus lStatus;

    lStatus = drvAnlogIicWriteByte(iic, bspInterface, (uint8_t)(address << 1U));
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    for (lIndex = 0U; lIndex < length; ++lIndex) {
        lStatus = drvAnlogIicWriteByte(iic, bspInterface, buffer[lIndex]);
        if (lStatus != DRV_STATUS_OK) {
            return lStatus;
        }
    }

    return DRV_STATUS_OK;
}

static eDrvStatus drvAnlogIicWriteRawLocked(uint8_t iic, stDrvAnlogIicBspInterface *bspInterface, const uint8_t *buffer, uint16_t length)
{
    uint16_t lIndex;
    eDrvStatus lStatus;

    for (lIndex = 0U; lIndex < length; ++lIndex) {
        lStatus = drvAnlogIicWriteByte(iic, bspInterface, buffer[lIndex]);
        if (lStatus != DRV_STATUS_OK) {
            return lStatus;
        }
    }

    return DRV_STATUS_OK;
}

static eDrvStatus drvAnlogIicReadBufferLocked(uint8_t iic, stDrvAnlogIicBspInterface *bspInterface, uint8_t address, uint8_t *buffer, uint16_t length)
{
    uint16_t lIndex;
    eDrvStatus lStatus;

    lStatus = drvAnlogIicWriteByte(iic, bspInterface, (uint8_t)((address << 1U) | 0x01U));
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    for (lIndex = 0U; lIndex < length; ++lIndex) {
        lStatus = drvAnlogIicReadByte(iic,
                                      bspInterface,
                                      (lIndex + 1U) < length,
                                      &buffer[lIndex]);
        if (lStatus != DRV_STATUS_OK) {
            return lStatus;
        }
    }

    return DRV_STATUS_OK;
}

static eDrvStatus drvAnlogIicRecoverBusLocked(uint8_t iic, stDrvAnlogIicBspInterface *bspInterface)
{
    uint8_t lClockIndex;
    uint8_t lRecoveryClockCount;
    eDrvStatus lStatus;

    if (bspInterface == NULL) {
        return DRV_STATUS_NOT_READY;
    }

    lRecoveryClockCount = drvAnlogIicGetRecoveryClockCount(bspInterface);

    bspInterface->setSda(iic, true);
    bspInterface->setScl(iic, true);
    drvAnlogIicDelayHalfPeriod(bspInterface);

    if (bspInterface->readSda(iic) && bspInterface->readScl(iic)) {
        return DRV_STATUS_OK;
    }

    for (lClockIndex = 0U; lClockIndex < lRecoveryClockCount; ++lClockIndex) {
        bspInterface->setScl(iic, false);
        drvAnlogIicDelayHalfPeriod(bspInterface);

        lStatus = drvAnlogIicWaitSclHigh(iic, bspInterface);
        if (lStatus != DRV_STATUS_OK) {
            return lStatus;
        }

        drvAnlogIicDelayHalfPeriod(bspInterface);
        if (bspInterface->readSda(iic)) {
            break;
        }
    }

    return drvAnlogIicSendStop(iic, bspInterface);
}

static eDrvStatus drvAnlogIicEnsureMutex(uint8_t iic)
{
    if (!drvAnlogIicIsValid(iic)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (!gDrvAnlogIicMutex[iic].isCreated) {
        return drvAnlogIicMapRtosStatus(repRtosMutexCreate(&gDrvAnlogIicMutex[iic]));
    }

    return DRV_STATUS_OK;
}

static eDrvStatus drvAnlogIicLockBus(uint8_t iic)
{
    if (drvAnlogIicEnsureMutex(iic) != DRV_STATUS_OK) {
        return DRV_STATUS_ERROR;
    }

    return drvAnlogIicMapRtosStatus(repRtosMutexTake(&gDrvAnlogIicMutex[iic], DRVANLOGIIC_LOCK_WAIT_MS));
}

static void drvAnlogIicUnlockBus(uint8_t iic)
{
    if (drvAnlogIicIsValid(iic)) {
        (void)repRtosMutexGive(&gDrvAnlogIicMutex[iic]);
    }
}

eDrvStatus drvAnlogIicInit(uint8_t iic)
{
    stDrvAnlogIicBspInterface *lBspInterface;
    eDrvStatus lStatus;

    if (!drvAnlogIicIsValid(iic)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (!drvAnlogIicHasValidBspInterface(iic)) {
        #if (DRVANLOGIIC_LOG_SUPPORT == 1)
        LOG_E(DRVANLOGIIC_LOG_TAG, "Invalid BSP interface for bus %u", (unsigned int)iic);
        #endif
        return DRV_STATUS_NOT_READY;
    }

    lBspInterface = drvAnlogIicGetBspInterface(iic);
    if (lBspInterface == NULL) {
        return DRV_STATUS_NOT_READY;
    }

    lBspInterface->init(iic);

    lStatus = drvAnlogIicEnsureMutex(iic);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    lStatus = drvAnlogIicLockBus(iic);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    lStatus = drvAnlogIicRecoverBusLocked(iic, lBspInterface);
    drvAnlogIicUnlockBus(iic);
    if (lStatus != DRV_STATUS_OK) {
        #if (DRVANLOGIIC_LOG_SUPPORT == 1)
        LOG_E(DRVANLOGIIC_LOG_TAG, "Software IIC bus %u init recover failed, status=%d", (unsigned int)iic, (int)lStatus);
        #endif
        return lStatus;
    }

    gDrvAnlogIicInitialized[iic] = true;
    return DRV_STATUS_OK;
}

eDrvStatus drvAnlogIicRecoverBus(uint8_t iic)
{
    stDrvAnlogIicBspInterface *lBspInterface;
    eDrvStatus lStatus;

    if (!drvAnlogIicIsValid(iic)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (!gDrvAnlogIicInitialized[iic]) {
        return DRV_STATUS_NOT_READY;
    }

    lStatus = drvAnlogIicLockBus(iic);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    lBspInterface = drvAnlogIicGetBspInterface(iic);
    lStatus = drvAnlogIicRecoverBusLocked(iic, lBspInterface);
    drvAnlogIicUnlockBus(iic);
    if (lStatus != DRV_STATUS_OK) {
        #if (DRVANLOGIIC_LOG_SUPPORT == 1)
        LOG_W(DRVANLOGIIC_LOG_TAG, "Software IIC bus %u recover failed, status=%d", (unsigned int)iic, (int)lStatus);
        #endif
    }
    return lStatus;
}

eDrvStatus drvAnlogIicBusAction(uint8_t iic, drvAnlogIicBusActionFunc action, void *context)
{
    stDrvAnlogIicBspInterface *lBspInterface;
    eDrvStatus lStatus;
    eDrvStatus lRecoverStatus;

    if (!drvAnlogIicIsValid(iic) || (action == NULL)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (!gDrvAnlogIicInitialized[iic]) {
        return DRV_STATUS_NOT_READY;
    }

    lStatus = drvAnlogIicLockBus(iic);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    lBspInterface = drvAnlogIicGetBspInterface(iic);
    if (lBspInterface == NULL) {
        drvAnlogIicUnlockBus(iic);
        return DRV_STATUS_NOT_READY;
    }

    lRecoverStatus = drvAnlogIicRecoverBusLocked(iic, lBspInterface);
    if (lRecoverStatus != DRV_STATUS_OK) {
        drvAnlogIicUnlockBus(iic);
        return lRecoverStatus;
    }

    lStatus = action(iic, lBspInterface, context);
    lRecoverStatus = drvAnlogIicRecoverBusLocked(iic, lBspInterface);
    drvAnlogIicUnlockBus(iic);

    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    return lRecoverStatus;
}

eDrvStatus drvAnlogIicTransfer(uint8_t iic, const stDrvAnlogIicTransfer *transfer)
{
    stDrvAnlogIicBspInterface *lBspInterface;
    eDrvStatus lStatus;
    eDrvStatus lStopStatus;

    if (!drvAnlogIicIsValid(iic) || (transfer == NULL)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (!drvAnlogIicIsValidAddress(transfer->address) ||
        !drvAnlogIicIsValidWriteBuffer(transfer->writeBuffer, transfer->writeLength) ||
        !drvAnlogIicIsValidWriteBuffer(transfer->secondWriteBuffer, transfer->secondWriteLength) ||
        !drvAnlogIicIsValidReadBuffer(transfer->readBuffer, transfer->readLength) ||
        ((transfer->writeLength == 0U) &&
         (transfer->secondWriteLength == 0U) &&
         (transfer->readLength == 0U))) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (!gDrvAnlogIicInitialized[iic]) {
        return DRV_STATUS_NOT_READY;
    }

    lStatus = drvAnlogIicLockBus(iic);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    lBspInterface = drvAnlogIicGetBspInterface(iic);
    if (lBspInterface == NULL) {
        drvAnlogIicUnlockBus(iic);
        return DRV_STATUS_NOT_READY;
    }

    lStatus = drvAnlogIicRecoverBusLocked(iic, lBspInterface);
    if (lStatus != DRV_STATUS_OK) {
        drvAnlogIicUnlockBus(iic);
        return lStatus;
    }

    lStatus = drvAnlogIicSendStart(iic, lBspInterface);
    if (lStatus == DRV_STATUS_OK) {
        if (transfer->writeLength > 0U) {
            lStatus = drvAnlogIicWriteBufferLocked(iic,
                                                   lBspInterface,
                                                   transfer->address,
                                                   transfer->writeBuffer,
                                                   transfer->writeLength);
        }

        if ((lStatus == DRV_STATUS_OK) && (transfer->secondWriteLength > 0U)) {
            if (transfer->writeLength == 0U) {
                lStatus = drvAnlogIicWriteBufferLocked(iic,
                                                       lBspInterface,
                                                       transfer->address,
                                                       transfer->secondWriteBuffer,
                                                       transfer->secondWriteLength);
            } else {
                lStatus = drvAnlogIicWriteRawLocked(iic,
                                                    lBspInterface,
                                                    transfer->secondWriteBuffer,
                                                    transfer->secondWriteLength);
            }
        }

        if ((lStatus == DRV_STATUS_OK) && (transfer->readLength > 0U)) {
            if ((transfer->writeLength > 0U) || (transfer->secondWriteLength > 0U)) {
                lStatus = drvAnlogIicSendStart(iic, lBspInterface);
            }

            if (lStatus == DRV_STATUS_OK) {
                lStatus = drvAnlogIicReadBufferLocked(iic,
                                                      lBspInterface,
                                                      transfer->address,
                                                      transfer->readBuffer,
                                                      transfer->readLength);
            }
        }
    }

    lStopStatus = drvAnlogIicSendStop(iic, lBspInterface);
    drvAnlogIicUnlockBus(iic);

    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    return lStopStatus;
}

eDrvStatus drvAnlogIicTransferTimeout(uint8_t iic, const stDrvAnlogIicTransfer *transfer, uint32_t timeoutMs)
{
    (void)timeoutMs;
    return drvAnlogIicTransfer(iic, transfer);
}

eDrvStatus drvAnlogIicWrite(uint8_t iic, uint8_t address, const uint8_t *buffer, uint16_t length)
{
    stDrvAnlogIicTransfer lTransfer = {
        .address = address,
        .writeBuffer = buffer,
        .writeLength = length,
        .readBuffer = NULL,
        .readLength = 0U,
        .secondWriteBuffer = NULL,
        .secondWriteLength = 0U,
    };

    return drvAnlogIicTransfer(iic, &lTransfer);
}

eDrvStatus drvAnlogIicWriteTimeout(uint8_t iic, uint8_t address, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs)
{
    (void)timeoutMs;
    return drvAnlogIicWrite(iic, address, buffer, length);
}

eDrvStatus drvAnlogIicRead(uint8_t iic, uint8_t address, uint8_t *buffer, uint16_t length)
{
    stDrvAnlogIicTransfer lTransfer = {
        .address = address,
        .writeBuffer = NULL,
        .writeLength = 0U,
        .readBuffer = buffer,
        .readLength = length,
        .secondWriteBuffer = NULL,
        .secondWriteLength = 0U,
    };

    return drvAnlogIicTransfer(iic, &lTransfer);
}

eDrvStatus drvAnlogIicReadTimeout(uint8_t iic, uint8_t address, uint8_t *buffer, uint16_t length, uint32_t timeoutMs)
{
    (void)timeoutMs;
    return drvAnlogIicRead(iic, address, buffer, length);
}

eDrvStatus drvAnlogIicWriteRegister(uint8_t iic, uint8_t address, const uint8_t *registerBuffer, uint16_t registerLength, const uint8_t *buffer, uint16_t length)
{
    stDrvAnlogIicTransfer lTransfer;

    if (!drvAnlogIicIsValid(iic)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (!drvAnlogIicIsValidWriteBuffer(registerBuffer, registerLength) ||
        ((registerLength == 0U) && (length == 0U))) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (!drvAnlogIicIsValidAddress(address) || !drvAnlogIicIsValidWriteBuffer(buffer, length)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    lTransfer.address = address;
    lTransfer.writeBuffer = registerBuffer;
    lTransfer.writeLength = registerLength;
    lTransfer.readBuffer = NULL;
    lTransfer.readLength = 0U;
    lTransfer.secondWriteBuffer = buffer;
    lTransfer.secondWriteLength = length;
    return drvAnlogIicTransfer(iic, &lTransfer);
}

eDrvStatus drvAnlogIicWriteRegisterTimeout(uint8_t iic, uint8_t address, const uint8_t *registerBuffer, uint16_t registerLength, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs)
{
    (void)timeoutMs;
    return drvAnlogIicWriteRegister(iic, address, registerBuffer, registerLength, buffer, length);
}

eDrvStatus drvAnlogIicReadRegister(uint8_t iic, uint8_t address, const uint8_t *registerBuffer, uint16_t registerLength, uint8_t *buffer, uint16_t length)
{
    stDrvAnlogIicTransfer lTransfer;

    if (!drvAnlogIicIsValidWriteBuffer(registerBuffer, registerLength) ||
        (registerLength == 0U) ||
        (length == 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    lTransfer.address = address;
    lTransfer.writeBuffer = registerBuffer;
    lTransfer.writeLength = registerLength;
    lTransfer.readBuffer = buffer;
    lTransfer.readLength = length;
    lTransfer.secondWriteBuffer = NULL;
    lTransfer.secondWriteLength = 0U;
    return drvAnlogIicTransfer(iic, &lTransfer);
}

eDrvStatus drvAnlogIicReadRegisterTimeout(uint8_t iic, uint8_t address, const uint8_t *registerBuffer, uint16_t registerLength, uint8_t *buffer, uint16_t length, uint32_t timeoutMs)
{
    (void)timeoutMs;
    return drvAnlogIicReadRegister(iic, address, registerBuffer, registerLength, buffer, length);
}

/**************************End of file********************************/
