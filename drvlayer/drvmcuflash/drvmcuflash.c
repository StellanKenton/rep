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
#include "../../Console/log.h"
#endif

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#if (REP_RTOS_SYSTEM == REP_RTOS_FREERTOS)
#include "FreeRTOS.h"
#include "semphr.h"
#elif (REP_RTOS_SYSTEM == REP_RTOS_NONE) && (REP_MCU_PLATFORM == REP_MCU_PLATFORM_GD32)
#include "gd32f4xx.h"
#endif

#define DRVMCUFLASH_LOG_TAG                "drvMcuFlash"

static bool gDrvMcuFlashInitialized = false;

#if (REP_RTOS_SYSTEM == REP_RTOS_FREERTOS)
static SemaphoreHandle_t gDrvMcuFlashMutex = NULL;
#else
static volatile bool gDrvMcuFlashBusy = false;
#if (REP_RTOS_SYSTEM == REP_RTOS_NONE) && (REP_MCU_PLATFORM == REP_MCU_PLATFORM_GD32)
static uint32_t gDrvMcuFlashCriticalState = 0U;
static uint32_t gDrvMcuFlashCriticalDepth = 0U;
#endif
#endif

__attribute__((weak)) const stDrvMcuFlashBspInterface *drvMcuFlashGetPlatformBspInterface(void)
{
    return NULL;
}

__attribute__((weak)) eDrvStatus drvMcuFlashGetPlatformAreaInfo(eDrvMcuFlashAreaMap area, stDrvMcuFlashAreaInfo *info)
{
    (void)area;
    (void)info;
    return DRV_STATUS_UNSUPPORTED;
}

#if (REP_RTOS_SYSTEM == REP_RTOS_NONE) && (REP_MCU_PLATFORM == REP_MCU_PLATFORM_GD32)
static void drvMcuFlashEnterCritical(void)
{
    uint32_t lPrimask = __get_PRIMASK();

    __set_PRIMASK(1U);
    if (gDrvMcuFlashCriticalDepth == 0U) {
        gDrvMcuFlashCriticalState = lPrimask;
    }
    gDrvMcuFlashCriticalDepth++;
}

static void drvMcuFlashExitCritical(void)
{
    if (gDrvMcuFlashCriticalDepth > 0U) {
        gDrvMcuFlashCriticalDepth--;
        if (gDrvMcuFlashCriticalDepth == 0U) {
            __set_PRIMASK(gDrvMcuFlashCriticalState);
        }
    }
}
#endif

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

static eDrvStatus drvMcuFlashResolveRange(eDrvMcuFlashAreaMap area, uint32_t offset, uint32_t length, uint32_t *startAddress, uint32_t *endAddress)
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

#if (REP_RTOS_SYSTEM == REP_RTOS_FREERTOS)
static eDrvStatus drvMcuFlashEnsureMutex(void)
{
    if (gDrvMcuFlashMutex == NULL) {
        gDrvMcuFlashMutex = xSemaphoreCreateMutex();
        if (gDrvMcuFlashMutex == NULL) {
            return DRV_STATUS_ERROR;
        }
    }

    return DRV_STATUS_OK;
}
#endif

static eDrvStatus drvMcuFlashLock(void)
{
#if (REP_RTOS_SYSTEM == REP_RTOS_FREERTOS)
    if (drvMcuFlashEnsureMutex() != DRV_STATUS_OK) {
        return DRV_STATUS_ERROR;
    }

    if (xSemaphoreTake(gDrvMcuFlashMutex, pdMS_TO_TICKS(DRVMCUFLASH_LOCK_WAIT_MS)) != pdTRUE) {
        return DRV_STATUS_BUSY;
    }

    return DRV_STATUS_OK;
#else
#if (REP_RTOS_SYSTEM == REP_RTOS_NONE) && (REP_MCU_PLATFORM == REP_MCU_PLATFORM_GD32)
    drvMcuFlashEnterCritical();
#endif

    if (gDrvMcuFlashBusy) {
#if (REP_RTOS_SYSTEM == REP_RTOS_NONE) && (REP_MCU_PLATFORM == REP_MCU_PLATFORM_GD32)
        drvMcuFlashExitCritical();
#endif
        return DRV_STATUS_BUSY;
    }

    gDrvMcuFlashBusy = true;

#if (REP_RTOS_SYSTEM == REP_RTOS_NONE) && (REP_MCU_PLATFORM == REP_MCU_PLATFORM_GD32)
    drvMcuFlashExitCritical();
#endif
    return DRV_STATUS_OK;
#endif
}

static void drvMcuFlashUnlock(void)
{
#if (REP_RTOS_SYSTEM == REP_RTOS_FREERTOS)
    if (gDrvMcuFlashMutex != NULL) {
        (void)xSemaphoreGive(gDrvMcuFlashMutex);
    }
#else
#if (REP_RTOS_SYSTEM == REP_RTOS_NONE) && (REP_MCU_PLATFORM == REP_MCU_PLATFORM_GD32)
    drvMcuFlashEnterCritical();
#endif

    gDrvMcuFlashBusy = false;

#if (REP_RTOS_SYSTEM == REP_RTOS_NONE) && (REP_MCU_PLATFORM == REP_MCU_PLATFORM_GD32)
    drvMcuFlashExitCritical();
#endif
#endif
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

#if (REP_RTOS_SYSTEM == REP_RTOS_FREERTOS)
    lStatus = drvMcuFlashEnsureMutex();
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }
#endif

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

eDrvStatus drvMcuFlashGetAreaInfo(eDrvMcuFlashAreaMap area, stDrvMcuFlashAreaInfo *info)
{
    if (info == NULL) {
        return DRV_STATUS_INVALID_PARAM;
    }

    return drvMcuFlashGetPlatformAreaInfo(area, info);
}

eDrvStatus drvMcuFlashRead(eDrvMcuFlashAreaMap area, uint32_t offset, uint8_t *buffer, uint32_t length)
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

eDrvStatus drvMcuFlashWrite(eDrvMcuFlashAreaMap area, uint32_t offset, const uint8_t *buffer, uint32_t length)
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

eDrvStatus drvMcuFlashErase(eDrvMcuFlashAreaMap area, uint32_t offset, uint32_t length)
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
