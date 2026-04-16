/***********************************************************************************
* @file     : drvmcuflash.c
* @brief    : Generic MCU internal flash driver abstraction implementation.
* @details  : This module validates public parameters, serializes flash access,
*             protects the application region through logical areas, and forwards
*             erase/program actions to the project-specific BSP hook table.
* @author   : GitHub Copilot
* @date     : 2026-04-02
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
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

__attribute__((weak)) uint8_t drvMcuFlashGetPlatformAreaCount(void)
{
    return 0U;
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

static eDrvStatus drvMcuFlashResolveAbsoluteRange(uint32_t address, uint32_t length, uint32_t *endAddress)
{
    uint8_t lAreaCount;
    uint8_t lAreaIndex;
    stDrvMcuFlashAreaInfo lAreaInfo;
    uint32_t lAreaEndAddress;

    if ((endAddress == NULL) || (length == 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    lAreaCount = drvMcuFlashGetPlatformAreaCount();
    for (lAreaIndex = 0U; lAreaIndex < lAreaCount; ++lAreaIndex) {
        if (drvMcuFlashGetPlatformAreaInfo(lAreaIndex, &lAreaInfo) != DRV_STATUS_OK) {
            continue;
        }

        if ((lAreaInfo.size == 0U) || (lAreaInfo.startAddress > (UINT32_MAX - lAreaInfo.size))) {
            continue;
        }

        lAreaEndAddress = lAreaInfo.startAddress + lAreaInfo.size;
        if ((address < lAreaInfo.startAddress) || (address >= lAreaEndAddress)) {
            continue;
        }

        if (length > (lAreaEndAddress - address)) {
            return DRV_STATUS_INVALID_PARAM;
        }

        *endAddress = address + length;
        return DRV_STATUS_OK;
    }

    return DRV_STATUS_INVALID_PARAM;
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

bool drvMcuFlashInit(void)
{
    const stDrvMcuFlashBspInterface *lBspInterface;
    eDrvStatus lStatus;

    if (gDrvMcuFlashInitialized) {
        return true;
    }

    if (!drvMcuFlashHasValidBspInterface()) {
        #if (DRVMCUFLASH_LOG_SUPPORT == 1)
        LOG_E(DRVMCUFLASH_LOG_TAG, "Invalid BSP interface");
        #endif
        return false;
    }

    lBspInterface = drvMcuFlashGetPlatformBspInterface();
    if (lBspInterface == NULL) {
        return false;
    }

    lStatus = drvMcuFlashEnsureMutex();
    if (lStatus != DRV_STATUS_OK) {
        return false;
    }

    lStatus = lBspInterface->init();
    if (lStatus != DRV_STATUS_OK) {
        #if (DRVMCUFLASH_LOG_SUPPORT == 1)
        LOG_E(DRVMCUFLASH_LOG_TAG, "BSP init failed, status=%d", (int)lStatus);
        #endif
        return false;
    }

    gDrvMcuFlashInitialized = true;
    return true;
}

bool drvMcuFlashIsReady(void)
{
    return gDrvMcuFlashInitialized;
}

bool drvMcuFlashGetAreaInfo(uint8_t area, stDrvMcuFlashAreaInfo *info)
{
    if (info == NULL) {
        return false;
    }

    return drvMcuFlashGetPlatformAreaInfo(area, info) == DRV_STATUS_OK;
}

bool drvMcuFlashRead(uint32_t address, uint8_t *buffer, uint32_t length)
{
    const stDrvMcuFlashBspInterface *lBspInterface;
    uint32_t lEndAddress;
    eDrvStatus lStatus;

    if (!drvMcuFlashHasValidMutableBuffer(buffer, length) || (length == 0U)) {
        return false;
    }

    if (!drvMcuFlashIsReady()) {
        return false;
    }

    lBspInterface = drvMcuFlashGetPlatformBspInterface();
    if (lBspInterface == NULL) {
        return false;
    }

    lStatus = drvMcuFlashResolveAbsoluteRange(address, length, &lEndAddress);
    if (lStatus != DRV_STATUS_OK) {
        return false;
    }

    lStatus = drvMcuFlashLock();
    if (lStatus != DRV_STATUS_OK) {
        return false;
    }

    (void)lEndAddress;
    memcpy(buffer, (const void *)address, (size_t)length);

    drvMcuFlashUnlock();
    return true;
}

bool drvMcuFlashWrite(uint32_t address, const uint8_t *buffer, uint32_t length)
{
    const stDrvMcuFlashBspInterface *lBspInterface;
    uint32_t lEndAddress;
    eDrvStatus lStatus;
    eDrvStatus lLockStatus;

    if (!drvMcuFlashHasValidConstBuffer(buffer, length) || (length == 0U)) {
        return false;
    }

    if (!drvMcuFlashIsReady()) {
        return false;
    }

    lBspInterface = drvMcuFlashGetPlatformBspInterface();
    if (lBspInterface == NULL) {
        return false;
    }

    lStatus = drvMcuFlashResolveAbsoluteRange(address, length, &lEndAddress);
    if (lStatus != DRV_STATUS_OK) {
        return false;
    }

    lLockStatus = drvMcuFlashLock();
    if (lLockStatus != DRV_STATUS_OK) {
        return false;
    }

    (void)lEndAddress;
    lStatus = drvMcuFlashCheckWriteRequirement(address, buffer, length);
    if (lStatus == DRV_STATUS_OK) {
        lStatus = lBspInterface->unlock();
    }

    if (lStatus == DRV_STATUS_OK) {
        lStatus = lBspInterface->program(address, buffer, length);
    }

    lLockStatus = lBspInterface->lock();
    drvMcuFlashUnlock();

    if (lStatus != DRV_STATUS_OK) {
        return false;
    }

    return lLockStatus == DRV_STATUS_OK;
}

bool drvMcuFlashErase(uint32_t address, uint32_t length)
{
    const stDrvMcuFlashBspInterface *lBspInterface;
    uint32_t lEndAddress;
    uint32_t lAddress;
    uint32_t lSectorIndex;
    uint32_t lSectorStart;
    uint32_t lSectorSize;
    eDrvStatus lStatus;
    eDrvStatus lLockStatus;

    if (length == 0U) {
        return false;
    }

    if (!drvMcuFlashIsReady()) {
        return false;
    }

    lBspInterface = drvMcuFlashGetPlatformBspInterface();
    if (lBspInterface == NULL) {
        return false;
    }

    lStatus = drvMcuFlashResolveAbsoluteRange(address, length, &lEndAddress);
    if (lStatus != DRV_STATUS_OK) {
        return false;
    }

    lLockStatus = drvMcuFlashLock();
    if (lLockStatus != DRV_STATUS_OK) {
        return false;
    }

    lStatus = lBspInterface->unlock();
    if (lStatus != DRV_STATUS_OK) {
        lLockStatus = lBspInterface->lock();
        drvMcuFlashUnlock();
        return false;
    }

    lAddress = address;
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
        return false;
    }

    return lLockStatus == DRV_STATUS_OK;
}

bool drvMcuFlashIsRangeValid(uint32_t address, uint32_t length)
{
    uint32_t lEndAddress;

    return drvMcuFlashResolveAbsoluteRange(address, length, &lEndAddress) == DRV_STATUS_OK;
}

/**************************End of file********************************/
