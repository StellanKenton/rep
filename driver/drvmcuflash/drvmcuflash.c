/***********************************************************************************
* @file     : drvmcuflash.c
* @brief    : Generic MCU internal flash driver abstraction implementation.
* @details  : This module validates public parameters, serializes flash access,
*             protects the application region through logical areas, and forwards
*             erase/program actions to the project-specific BSP hook table.
* @author   : GitHub Copilot
* @date     : 2026-04-02
* @version  : V1.0.0
**********************************************************************************/
#include "drvmcuflash.h"

#if (DRVMCUFLASH_LOG_SUPPORT == 1)
#include "../../service/console/log.h"
#endif

#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "../../service/rtos/rtos.h"

#define DRVMCUFLASH_LOG_TAG                "drvMcuFlash"

static bool gDrvMcuFlashInitialized = false;
static stRepRtosMutex gDrvMcuFlashMutex;

__attribute__((weak)) const stDrvMcuFlashBspInterface *drvMcuFlashGetPlatformBspInterface(void)
{
    return NULL;
}

__attribute__((weak)) eDrvStatus drvMcuFlashGetPlatformAreaInfo(uint8_t area, stDrvMcuFlashAreaInfo *info)
{
    (void)area;
    (void)info;
    return DRV_STATUS_UNSUPPORTED;
}

static eDrvStatus drvMcuFlashMapRtosStatus(eRepRtosStatus status)
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

static bool drvMcuFlashHasValidConstBuffer(const uint8_t *buffer, uint32_t length)
{
    return ((buffer != NULL) || (length == 0U));
}

static bool drvMcuFlashHasValidMutableBuffer(uint8_t *buffer, uint32_t length)
{
    return ((buffer != NULL) || (length == 0U));
}

static bool drvMcuFlashHasValidBspInterface(void)
{
    const stDrvMcuFlashBspInterface *lBspInterface = drvMcuFlashGetPlatformBspInterface();

    return (lBspInterface != NULL) &&
           (lBspInterface->init != NULL) &&
           (lBspInterface->unlock != NULL) &&
           (lBspInterface->lock != NULL) &&
           (lBspInterface->eraseSector != NULL) &&
           (lBspInterface->program != NULL) &&
           (lBspInterface->getSectorInfo != NULL);
}

static eDrvStatus drvMcuFlashResolveRange(uint8_t area, uint32_t offset, uint32_t length, uint32_t *startAddress, uint32_t *endAddress)
{
    stDrvMcuFlashAreaInfo lAreaInfo;

    if ((startAddress == NULL) || (endAddress == NULL) || (length == 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (drvMcuFlashGetPlatformAreaInfo(area, &lAreaInfo) != DRV_STATUS_OK) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if ((offset > lAreaInfo.size) || (length > (lAreaInfo.size - offset))) {
        return DRV_STATUS_INVALID_PARAM;
    }

    *startAddress = lAreaInfo.startAddress + offset;
    *endAddress = *startAddress + length;
    return DRV_STATUS_OK;
}

static eDrvStatus drvMcuFlashCheckWriteRequirement(uint32_t address, const uint8_t *buffer, uint32_t length)
{
    const uint8_t *lFlashData = (const uint8_t *)address;
    uint32_t lIndex;

    if (!drvMcuFlashHasValidConstBuffer(buffer, length) || (length == 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    for (lIndex = 0U; lIndex < length; ++lIndex) {
        if (((uint8_t)(~lFlashData[lIndex]) & buffer[lIndex]) != 0U) {
            return DRV_STATUS_ERROR;
        }
    }

    return DRV_STATUS_OK;
}

static eDrvStatus drvMcuFlashEnsureMutex(void)
{
    if (!gDrvMcuFlashMutex.isCreated) {
        return drvMcuFlashMapRtosStatus(repRtosMutexCreate(&gDrvMcuFlashMutex));
    }

    return DRV_STATUS_OK;
}

static eDrvStatus drvMcuFlashLock(void)
{
    if (drvMcuFlashEnsureMutex() != DRV_STATUS_OK) {
        return DRV_STATUS_ERROR;
    }

    return drvMcuFlashMapRtosStatus(repRtosMutexTake(&gDrvMcuFlashMutex, DRVMCUFLASH_LOCK_WAIT_MS));
}

static void drvMcuFlashUnlock(void)
{
    (void)repRtosMutexGive(&gDrvMcuFlashMutex);
}

eDrvStatus drvMcuFlashInit(void)
{
    const stDrvMcuFlashBspInterface *lBspInterface;
    eDrvStatus lStatus;

    if (gDrvMcuFlashInitialized) {
        return DRV_STATUS_OK;
    }

    if (!drvMcuFlashHasValidBspInterface()) {
        #if (DRVMCUFLASH_LOG_SUPPORT == 1)
        LOG_E(DRVMCUFLASH_LOG_TAG, "Invalid BSP interface");
        #endif
        return DRV_STATUS_NOT_READY;
    }

    lBspInterface = drvMcuFlashGetPlatformBspInterface();
    if (lBspInterface == NULL) {
        return DRV_STATUS_NOT_READY;
    }

    lStatus = drvMcuFlashEnsureMutex();
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    lStatus = lBspInterface->init();
    if (lStatus != DRV_STATUS_OK) {
        #if (DRVMCUFLASH_LOG_SUPPORT == 1)
        LOG_E(DRVMCUFLASH_LOG_TAG, "BSP init failed, status=%d", (int)lStatus);
        #endif
        return lStatus;
    }

    gDrvMcuFlashInitialized = true;
    return DRV_STATUS_OK;
}

bool drvMcuFlashIsReady(void)
{
    return gDrvMcuFlashInitialized;
}

eDrvStatus drvMcuFlashGetAreaInfo(uint8_t area, stDrvMcuFlashAreaInfo *info)
{
    if (info == NULL) {
        return DRV_STATUS_INVALID_PARAM;
    }

    return drvMcuFlashGetPlatformAreaInfo(area, info);
}

eDrvStatus drvMcuFlashRead(uint8_t area, uint32_t offset, uint8_t *buffer, uint32_t length)
{
    const stDrvMcuFlashBspInterface *lBspInterface;
    uint32_t lStartAddress;
    uint32_t lEndAddress;
    eDrvStatus lStatus;

    if (!drvMcuFlashHasValidMutableBuffer(buffer, length) || (length == 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (!drvMcuFlashIsReady()) {
        return DRV_STATUS_NOT_READY;
    }

    lBspInterface = drvMcuFlashGetPlatformBspInterface();
    if (lBspInterface == NULL) {
        return DRV_STATUS_NOT_READY;
    }

    lStatus = drvMcuFlashResolveRange(area, offset, length, &lStartAddress, &lEndAddress);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    lStatus = drvMcuFlashLock();
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    (void)lEndAddress;
    memcpy(buffer, (const void *)lStartAddress, (size_t)length);

    drvMcuFlashUnlock();
    return DRV_STATUS_OK;
}

eDrvStatus drvMcuFlashWrite(uint8_t area, uint32_t offset, const uint8_t *buffer, uint32_t length)
{
    const stDrvMcuFlashBspInterface *lBspInterface;
    uint32_t lStartAddress;
    uint32_t lEndAddress;
    eDrvStatus lStatus;
    eDrvStatus lLockStatus;

    if (!drvMcuFlashHasValidConstBuffer(buffer, length) || (length == 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (!drvMcuFlashIsReady()) {
        return DRV_STATUS_NOT_READY;
    }

    lBspInterface = drvMcuFlashGetPlatformBspInterface();
    if (lBspInterface == NULL) {
        return DRV_STATUS_NOT_READY;
    }

    lStatus = drvMcuFlashResolveRange(area, offset, length, &lStartAddress, &lEndAddress);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    lLockStatus = drvMcuFlashLock();
    if (lLockStatus != DRV_STATUS_OK) {
        return lLockStatus;
    }

    (void)lEndAddress;
    lStatus = drvMcuFlashCheckWriteRequirement(lStartAddress, buffer, length);
    if (lStatus == DRV_STATUS_OK) {
        lStatus = lBspInterface->unlock();
    }

    if (lStatus == DRV_STATUS_OK) {
        lStatus = lBspInterface->program(lStartAddress, buffer, length);
    }

    lLockStatus = lBspInterface->lock();
    drvMcuFlashUnlock();

    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    return lLockStatus;
}

eDrvStatus drvMcuFlashErase(uint8_t area, uint32_t offset, uint32_t length)
{
    const stDrvMcuFlashBspInterface *lBspInterface;
    uint32_t lStartAddress;
    uint32_t lEndAddress;
    uint32_t lAddress;
    uint32_t lSectorIndex;
    uint32_t lSectorStart;
    uint32_t lSectorSize;
    eDrvStatus lStatus;
    eDrvStatus lLockStatus;

    if (length == 0U) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (!drvMcuFlashIsReady()) {
        return DRV_STATUS_NOT_READY;
    }

    lBspInterface = drvMcuFlashGetPlatformBspInterface();
    if (lBspInterface == NULL) {
        return DRV_STATUS_NOT_READY;
    }

    lStatus = drvMcuFlashResolveRange(area, offset, length, &lStartAddress, &lEndAddress);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    lLockStatus = drvMcuFlashLock();
    if (lLockStatus != DRV_STATUS_OK) {
        return lLockStatus;
    }

    lStatus = lBspInterface->unlock();
    if (lStatus != DRV_STATUS_OK) {
        lLockStatus = lBspInterface->lock();
        drvMcuFlashUnlock();
        return (lLockStatus == DRV_STATUS_OK) ? lStatus : lLockStatus;
    }

    lAddress = lStartAddress;
    while ((lStatus == DRV_STATUS_OK) && (lAddress < lEndAddress)) {
        lStatus = lBspInterface->getSectorInfo(lAddress, &lSectorIndex, &lSectorStart, &lSectorSize);
        if (lStatus != DRV_STATUS_OK) {
            break;
        }

        if (lSectorSize == 0U) {
            lStatus = DRV_STATUS_ERROR;
            break;
        }

        lStatus = lBspInterface->eraseSector(lSectorIndex);
        if (lStatus != DRV_STATUS_OK) {
            break;
        }

        lAddress = lSectorStart + lSectorSize;
    }

    lLockStatus = lBspInterface->lock();
    drvMcuFlashUnlock();

    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    return lLockStatus;
}
/**************************End of file********************************/
