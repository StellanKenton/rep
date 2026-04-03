/***********************************************************************************
* @file     : drvadc.c
* @brief    : Reusable ADC driver abstraction implementation.
* @details  : This module validates public parameters, serializes channel access,
*             and forwards sampling requests to the project-specific BSP hooks.
* @author   : GitHub Copilot
* @date     : 2026-04-03
* @version  : V1.0.0
**********************************************************************************/
#include "drvadc.h"

#if (DRVADC_LOG_SUPPORT == 1)
#include "../../Console/log.h"
#endif

#include <stdbool.h>
#include <stddef.h>

#include "rep_config.h"

#if (REP_RTOS_SYSTEM == REP_RTOS_FREERTOS)
#include "FreeRTOS.h"
#include "semphr.h"
#elif (REP_RTOS_SYSTEM == REP_RTOS_NONE) && (REP_MCU_PLATFORM == REP_MCU_PLATFORM_GD32)
#include "gd32f4xx.h"
#endif

#define DRVADC_LOG_TAG                    "drvAdc"

static bool gDrvAdcInitialized[DRVADC_MAX];

#if (REP_RTOS_SYSTEM == REP_RTOS_FREERTOS)
static SemaphoreHandle_t gDrvAdcMutex[DRVADC_MAX];
#else
static volatile bool gDrvAdcBusy[DRVADC_MAX];
#if (REP_RTOS_SYSTEM == REP_RTOS_NONE) && (REP_MCU_PLATFORM == REP_MCU_PLATFORM_GD32)
static uint32_t gDrvAdcCriticalState = 0U;
static uint32_t gDrvAdcCriticalDepth = 0U;
#endif
#endif

extern stDrvAdcBspInterface gDrvAdcBspInterface;
extern stDrvAdcData gDrvAdcData[DRVADC_MAX];

#if (REP_RTOS_SYSTEM == REP_RTOS_NONE) && (REP_MCU_PLATFORM == REP_MCU_PLATFORM_GD32)
static void drvAdcEnterCritical(void)
{
    uint32_t lPrimask = __get_PRIMASK();

    __set_PRIMASK(1U);
    if (gDrvAdcCriticalDepth == 0U) {
        gDrvAdcCriticalState = lPrimask;
    }
    gDrvAdcCriticalDepth++;
}

static void drvAdcExitCritical(void)
{
    if (gDrvAdcCriticalDepth > 0U) {
        gDrvAdcCriticalDepth--;
        if (gDrvAdcCriticalDepth == 0U) {
            __set_PRIMASK(gDrvAdcCriticalState);
        }
    }
}
#endif

static bool drvAdcIsValid(eDrvAdcPortMap adc)
{
    return adc < DRVADC_MAX;
}

static bool drvAdcIsValidValuePointer(const uint16_t *value)
{
    return value != NULL;
}

static bool drvAdcIsValidResolutionBits(uint8_t resolutionBits)
{
    return (resolutionBits > 0U) && (resolutionBits <= 16U);
}

static bool drvAdcIsInitialized(eDrvAdcPortMap adc)
{
    return drvAdcIsValid(adc) && gDrvAdcInitialized[adc];
}

static stDrvAdcBspInterface *drvAdcGetBspInterface(void)
{
    return &gDrvAdcBspInterface;
}

static bool drvAdcHasValidBspInterface(void)
{
    stDrvAdcBspInterface *lBspInterface = drvAdcGetBspInterface();

    return (lBspInterface != NULL) &&
           (lBspInterface->init != NULL) &&
           (lBspInterface->readRaw != NULL);
}

static uint32_t drvAdcGetTimeoutMs(uint32_t timeoutMs)
{
    const stDrvAdcBspInterface *lBspInterface = drvAdcGetBspInterface();

    if (timeoutMs > 0U) {
        return timeoutMs;
    }

    if ((lBspInterface != NULL) && (lBspInterface->defaultTimeoutMs > 0U)) {
        return lBspInterface->defaultTimeoutMs;
    }

    return DRVADC_DEFAULT_TIMEOUT_MS;
}

static uint16_t drvAdcGetReferenceMv(void)
{
    const stDrvAdcBspInterface *lBspInterface = drvAdcGetBspInterface();

    if ((lBspInterface != NULL) && (lBspInterface->referenceMv > 0U)) {
        return lBspInterface->referenceMv;
    }

    return DRVADC_DEFAULT_REFERENCE_MV;
}

static uint8_t drvAdcGetResolutionBits(void)
{
    const stDrvAdcBspInterface *lBspInterface = drvAdcGetBspInterface();

    if ((lBspInterface != NULL) && drvAdcIsValidResolutionBits(lBspInterface->resolutionBits)) {
        return lBspInterface->resolutionBits;
    }

    return DRVADC_DEFAULT_RESOLUTION_BITS;
}

static uint32_t drvAdcGetMaxRawValue(void)
{
    uint8_t lResolutionBits = drvAdcGetResolutionBits();

    if (!drvAdcIsValidResolutionBits(lResolutionBits)) {
        return 0U;
    }

    return ((uint32_t)1U << lResolutionBits) - 1U;
}

#if (REP_RTOS_SYSTEM == REP_RTOS_FREERTOS)
static eDrvStatus drvAdcEnsureMutex(eDrvAdcPortMap adc)
{
    if (!drvAdcIsValid(adc)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (gDrvAdcMutex[adc] == NULL) {
        gDrvAdcMutex[adc] = xSemaphoreCreateMutex();
        if (gDrvAdcMutex[adc] == NULL) {
            return DRV_STATUS_ERROR;
        }
    }

    return DRV_STATUS_OK;
}
#endif

static eDrvStatus drvAdcLock(eDrvAdcPortMap adc)
{
#if (REP_RTOS_SYSTEM == REP_RTOS_FREERTOS)
    if (drvAdcEnsureMutex(adc) != DRV_STATUS_OK) {
        return DRV_STATUS_ERROR;
    }

    if (xSemaphoreTake(gDrvAdcMutex[adc], pdMS_TO_TICKS(DRVADC_LOCK_WAIT_MS)) != pdTRUE) {
        return DRV_STATUS_BUSY;
    }

    return DRV_STATUS_OK;
#else
    if (!drvAdcIsValid(adc)) {
        return DRV_STATUS_INVALID_PARAM;
    }

#if (REP_RTOS_SYSTEM == REP_RTOS_NONE) && (REP_MCU_PLATFORM == REP_MCU_PLATFORM_GD32)
    drvAdcEnterCritical();
#endif

    if (gDrvAdcBusy[adc]) {
#if (REP_RTOS_SYSTEM == REP_RTOS_NONE) && (REP_MCU_PLATFORM == REP_MCU_PLATFORM_GD32)
        drvAdcExitCritical();
#endif
        return DRV_STATUS_BUSY;
    }

    gDrvAdcBusy[adc] = true;

#if (REP_RTOS_SYSTEM == REP_RTOS_NONE) && (REP_MCU_PLATFORM == REP_MCU_PLATFORM_GD32)
    drvAdcExitCritical();
#endif
    return DRV_STATUS_OK;
#endif
}

static void drvAdcUnlock(eDrvAdcPortMap adc)
{
#if (REP_RTOS_SYSTEM == REP_RTOS_FREERTOS)
    if (drvAdcIsValid(adc) && (gDrvAdcMutex[adc] != NULL)) {
        (void)xSemaphoreGive(gDrvAdcMutex[adc]);
    }
#else
    if (!drvAdcIsValid(adc)) {
        return;
    }

#if (REP_RTOS_SYSTEM == REP_RTOS_NONE) && (REP_MCU_PLATFORM == REP_MCU_PLATFORM_GD32)
    drvAdcEnterCritical();
#endif

    gDrvAdcBusy[adc] = false;

#if (REP_RTOS_SYSTEM == REP_RTOS_NONE) && (REP_MCU_PLATFORM == REP_MCU_PLATFORM_GD32)
    drvAdcExitCritical();
#endif
#endif
}

static void drvAdcUpdateDataRaw(eDrvAdcPortMap adc, uint16_t rawValue)
{
    if (!drvAdcIsValid(adc)) {
        return;
    }

    gDrvAdcData[adc].raw = rawValue;
}

static void drvAdcUpdateDataMv(eDrvAdcPortMap adc, uint16_t valueMv)
{
    if (!drvAdcIsValid(adc)) {
        return;
    }

    gDrvAdcData[adc].mv = valueMv;
}

static eDrvStatus drvAdcReadRawLocked(eDrvAdcPortMap adc, uint16_t *value, uint32_t timeoutMs)
{
    stDrvAdcBspInterface *lBspInterface = drvAdcGetBspInterface();

    if ((lBspInterface == NULL) || (lBspInterface->readRaw == NULL)) {
        return DRV_STATUS_NOT_READY;
    }

    return lBspInterface->readRaw(adc, value, drvAdcGetTimeoutMs(timeoutMs));
}

static eDrvStatus drvAdcConvertRawToMv(eDrvAdcPortMap adc, uint16_t rawValue, uint16_t *valueMv)
{
    uint32_t lReferenceMv;
    uint32_t lMaxRawValue;
    uint32_t lScaledValue;

    if (!drvAdcIsValidValuePointer(valueMv)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    lReferenceMv = drvAdcGetReferenceMv();
    lMaxRawValue = drvAdcGetMaxRawValue();
    if ((lReferenceMv == 0U) || (lMaxRawValue == 0U)) {
        return DRV_STATUS_UNSUPPORTED;
    }

    lScaledValue = ((uint32_t)rawValue * lReferenceMv) / lMaxRawValue;
    *valueMv = (uint16_t)lScaledValue;
    return DRV_STATUS_OK;
}

eDrvStatus drvAdcInit(eDrvAdcPortMap adc)
{
    stDrvAdcBspInterface *lBspInterface = NULL;
    eDrvStatus lStatus;

    if (!drvAdcIsValid(adc)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (drvAdcIsInitialized(adc)) {
        return DRV_STATUS_OK;
    }

    if (!drvAdcHasValidBspInterface()) {
        #if (DRVADC_LOG_SUPPORT == 1)
        LOG_E(DRVADC_LOG_TAG, "Invalid BSP interface for channel %u", (unsigned int)adc);
        #endif
        return DRV_STATUS_NOT_READY;
    }

    lBspInterface = drvAdcGetBspInterface();
    if (lBspInterface == NULL) {
        return DRV_STATUS_NOT_READY;
    }

#if (REP_RTOS_SYSTEM == REP_RTOS_FREERTOS)
    lStatus = drvAdcEnsureMutex(adc);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }
#endif

    lStatus = lBspInterface->init(adc);
    if (lStatus != DRV_STATUS_OK) {
        #if (DRVADC_LOG_SUPPORT == 1)
        LOG_E(DRVADC_LOG_TAG, "ADC channel %u init failed, status=%d", (unsigned int)adc, (int)lStatus);
        #endif
        return lStatus;
    }

    gDrvAdcInitialized[adc] = true;
    return DRV_STATUS_OK;
}

eDrvStatus drvAdcReadRaw(eDrvAdcPortMap adc, uint16_t *value)
{
    return drvAdcReadRawTimeout(adc, value, 0U);
}

eDrvStatus drvAdcReadRawTimeout(eDrvAdcPortMap adc, uint16_t *value, uint32_t timeoutMs)
{
    eDrvStatus lStatus;

    if (!drvAdcIsValid(adc) || !drvAdcIsValidValuePointer(value)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (!drvAdcIsInitialized(adc)) {
        return DRV_STATUS_NOT_READY;
    }

    lStatus = drvAdcLock(adc);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    lStatus = drvAdcReadRawLocked(adc, value, timeoutMs);
    if (lStatus == DRV_STATUS_OK) {
        drvAdcUpdateDataRaw(adc, *value);
        if (drvAdcConvertRawToMv(adc, *value, &gDrvAdcData[adc].mv) != DRV_STATUS_OK) {
            gDrvAdcData[adc].mv = 0U;
        }
    }
    drvAdcUnlock(adc);
    return lStatus;
}

eDrvStatus drvAdcReadMv(eDrvAdcPortMap adc, uint16_t *valueMv)
{
    return drvAdcReadMvTimeout(adc, valueMv, 0U);
}

eDrvStatus drvAdcReadMvTimeout(eDrvAdcPortMap adc, uint16_t *valueMv, uint32_t timeoutMs)
{
    eDrvStatus lStatus;
    uint16_t lRawValue = 0U;

    if (!drvAdcIsValid(adc) || !drvAdcIsValidValuePointer(valueMv)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (!drvAdcIsInitialized(adc)) {
        return DRV_STATUS_NOT_READY;
    }

    lStatus = drvAdcLock(adc);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    lStatus = drvAdcReadRawLocked(adc, &lRawValue, timeoutMs);
    if (lStatus == DRV_STATUS_OK) {
        drvAdcUpdateDataRaw(adc, lRawValue);
        lStatus = drvAdcConvertRawToMv(adc, lRawValue, valueMv);
        if (lStatus == DRV_STATUS_OK) {
            drvAdcUpdateDataMv(adc, *valueMv);
        }
    }

    drvAdcUnlock(adc);
    return lStatus;
}

/**************************End of file********************************/
