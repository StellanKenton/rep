/***********************************************************************************
* @file     : drvadc.c
* @brief    : Reusable ADC driver abstraction implementation.
* @details  : This module validates public parameters, serializes channel access,
*             and forwards sampling requests to the project-specific BSP hooks.
* @author   : GitHub Copilot
* @date     : 2026-04-03
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "drvadc.h"

#if (DRVADC_LOG_SUPPORT == 1)
#include "../../service/console/log.h"
#endif

#include <stdbool.h>
#include <stddef.h>

#include "rep_config.h"
#include "../../service/rtos/rtos.h"

#define DRVADC_LOG_TAG                    "drvAdc"

static bool gDrvAdcInitialized[DRVADC_MAX];
static stRepRtosMutex gDrvAdcMutex[DRVADC_MAX];
static uint32_t gDrvAdcBackgroundTickMs;
static bool gDrvAdcBackgroundTickValid;
static uint16_t gDrvAdcFilterBuffer[DRVADC_MAX][DRVADC_FILTER_WINDOW_SIZE];
static uint32_t gDrvAdcFilterSum[DRVADC_MAX];
static uint8_t gDrvAdcFilterIndex[DRVADC_MAX];
static uint8_t gDrvAdcFilterCount[DRVADC_MAX];

static eDrvStatus drvAdcReadRawLocked(uint8_t adc, uint16_t *value, uint32_t timeoutMs);
static eDrvStatus drvAdcConvertRawToMv(uint8_t adc, uint16_t rawValue, uint16_t *valueMv);

__attribute__((weak)) const stDrvAdcBspInterface *drvAdcGetPlatformBspInterface(void)
{
    return NULL;
}

__attribute__((weak)) stDrvAdcData *drvAdcGetPlatformData(void)
{
    return NULL;
}

static eDrvStatus drvAdcMapRtosStatus(eRepRtosStatus status)
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

static bool drvAdcIsValid(uint8_t adc)
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

static bool drvAdcIsInitialized(uint8_t adc)
{
    return drvAdcIsValid(adc) && gDrvAdcInitialized[adc];
}

static stDrvAdcData *drvAdcGetDataCache(void)
{
    return drvAdcGetPlatformData();
}

static stDrvAdcBspInterface *drvAdcGetBspInterface(void)
{
    return (stDrvAdcBspInterface *)drvAdcGetPlatformBspInterface();
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

static eDrvStatus drvAdcEnsureMutex(uint8_t adc)
{
    if (!drvAdcIsValid(adc)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (!gDrvAdcMutex[adc].isCreated) {
        return drvAdcMapRtosStatus(repRtosMutexCreate(&gDrvAdcMutex[adc]));
    }

    return DRV_STATUS_OK;
}

static eDrvStatus drvAdcLock(uint8_t adc)
{
    if (drvAdcEnsureMutex(adc) != DRV_STATUS_OK) {
        return DRV_STATUS_ERROR;
    }

    return drvAdcMapRtosStatus(repRtosMutexTake(&gDrvAdcMutex[adc], DRVADC_LOCK_WAIT_MS));
}

static void drvAdcUnlock(uint8_t adc)
{
    if (drvAdcIsValid(adc)) {
        (void)repRtosMutexGive(&gDrvAdcMutex[adc]);
    }
}

static void drvAdcUpdateDataRaw(uint8_t adc, uint16_t rawValue)
{
    stDrvAdcData *lDataCache;

    if (!drvAdcIsValid(adc)) {
        return;
    }

    lDataCache = drvAdcGetDataCache();
    if (lDataCache != NULL) {
        lDataCache[adc].raw = rawValue;
    }
}

static void drvAdcUpdateDataMv(uint8_t adc, uint16_t valueMv)
{
    stDrvAdcData *lDataCache;

    if (!drvAdcIsValid(adc)) {
        return;
    }

    lDataCache = drvAdcGetDataCache();
    if (lDataCache != NULL) {
        lDataCache[adc].mv = valueMv;
    }
}

static void drvAdcUpdateDataRawFiltered(uint8_t adc, uint16_t rawFiltered)
{
    stDrvAdcData *lDataCache;

    if (!drvAdcIsValid(adc)) {
        return;
    }

    lDataCache = drvAdcGetDataCache();
    if (lDataCache != NULL) {
        lDataCache[adc].rawFiltered = rawFiltered;
    }
}

static void drvAdcUpdateDataMvFiltered(uint8_t adc, uint16_t mvFiltered)
{
    stDrvAdcData *lDataCache;

    if (!drvAdcIsValid(adc)) {
        return;
    }

    lDataCache = drvAdcGetDataCache();
    if (lDataCache != NULL) {
        lDataCache[adc].mvFiltered = mvFiltered;
    }
}

static void drvAdcUpdateFilteredData(uint8_t adc, uint16_t rawValue)
{
    uint8_t lIndex;
    uint8_t lCount;
    uint16_t lRawFiltered;
    uint16_t lMvFiltered = 0U;

    if (!drvAdcIsValid(adc) || (DRVADC_FILTER_WINDOW_SIZE == 0U)) {
        return;
    }

    lIndex = gDrvAdcFilterIndex[adc];
    lCount = gDrvAdcFilterCount[adc];

    if (lCount >= DRVADC_FILTER_WINDOW_SIZE) {
        gDrvAdcFilterSum[adc] -= gDrvAdcFilterBuffer[adc][lIndex];
    } else {
        gDrvAdcFilterCount[adc] = (uint8_t)(lCount + 1U);
        lCount = gDrvAdcFilterCount[adc];
    }

    gDrvAdcFilterBuffer[adc][lIndex] = rawValue;
    gDrvAdcFilterSum[adc] += rawValue;
    gDrvAdcFilterIndex[adc] = (uint8_t)((lIndex + 1U) % DRVADC_FILTER_WINDOW_SIZE);

    lRawFiltered = (uint16_t)(gDrvAdcFilterSum[adc] / lCount);
    drvAdcUpdateDataRawFiltered(adc, lRawFiltered);
    if (drvAdcConvertRawToMv(adc, lRawFiltered, &lMvFiltered) != DRV_STATUS_OK) {
        lMvFiltered = 0U;
    }
    drvAdcUpdateDataMvFiltered(adc, lMvFiltered);
}

static void drvAdcUpdateSampleData(uint8_t adc, uint16_t rawValue)
{
    uint16_t lValueMv = 0U;

    drvAdcUpdateDataRaw(adc, rawValue);
    if (drvAdcConvertRawToMv(adc, rawValue, &lValueMv) != DRV_STATUS_OK) {
        lValueMv = 0U;
    }
    drvAdcUpdateDataMv(adc, lValueMv);
    drvAdcUpdateFilteredData(adc, rawValue);
}

static eDrvStatus drvAdcSampleChannel(uint8_t adc, uint32_t timeoutMs)
{
    eDrvStatus lStatus;
    uint16_t lRawValue = 0U;

    if (!drvAdcIsValid(adc)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (!drvAdcIsInitialized(adc)) {
        lStatus = drvAdcInit(adc);
        if (lStatus != DRV_STATUS_OK) {
            return lStatus;
        }
    }

    lStatus = drvAdcLock(adc);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    lStatus = drvAdcReadRawLocked(adc, &lRawValue, timeoutMs);
    if (lStatus == DRV_STATUS_OK) {
        drvAdcUpdateSampleData(adc, lRawValue);
    }

    drvAdcUnlock(adc);
    return lStatus;
}

static bool drvAdcBackgroundIsDue(uint32_t currentTickMs)
{
    if (!gDrvAdcBackgroundTickValid) {
        gDrvAdcBackgroundTickMs = currentTickMs;
        gDrvAdcBackgroundTickValid = true;
        return true;
    }

    if ((uint32_t)(currentTickMs - gDrvAdcBackgroundTickMs) < DRVADC_BACKGROUND_PERIOD_MS) {
        return false;
    }

    gDrvAdcBackgroundTickMs = currentTickMs;
    return true;
}

static eDrvStatus drvAdcReadRawLocked(uint8_t adc, uint16_t *value, uint32_t timeoutMs)
{
    stDrvAdcBspInterface *lBspInterface = drvAdcGetBspInterface();

    if ((lBspInterface == NULL) || (lBspInterface->readRaw == NULL)) {
        return DRV_STATUS_NOT_READY;
    }

    return lBspInterface->readRaw(adc, value, drvAdcGetTimeoutMs(timeoutMs));
}

static eDrvStatus drvAdcConvertRawToMv(uint8_t adc, uint16_t rawValue, uint16_t *valueMv)
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

eDrvStatus drvAdcInit(uint8_t adc)
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

    lStatus = drvAdcEnsureMutex(adc);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

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

eDrvStatus drvAdcReadRaw(uint8_t adc, uint16_t *value)
{
    return drvAdcReadRawTimeout(adc, value, 0U);
}

eDrvStatus drvAdcReadRawTimeout(uint8_t adc, uint16_t *value, uint32_t timeoutMs)
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
        drvAdcUpdateSampleData(adc, *value);
    }
    drvAdcUnlock(adc);
    return lStatus;
}

eDrvStatus drvAdcReadMv(uint8_t adc, uint16_t *valueMv)
{
    return drvAdcReadMvTimeout(adc, valueMv, 0U);
}

eDrvStatus drvAdcReadMvTimeout(uint8_t adc, uint16_t *valueMv, uint32_t timeoutMs)
{
    eDrvStatus lStatus;
    uint16_t lRawValue = 0U;
    uint16_t lValueMv = 0U;

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
        lStatus = drvAdcConvertRawToMv(adc, lRawValue, &lValueMv);
        if (lStatus == DRV_STATUS_OK) {
            *valueMv = lValueMv;
            drvAdcUpdateSampleData(adc, lRawValue);
        }
    }

    drvAdcUnlock(adc);
    return lStatus;
}

void drvAdcBackground(void)
{
    uint8_t lAdc;
    uint32_t lTickMs;

    lTickMs = repRtosGetTickMs();
    if (!drvAdcBackgroundIsDue(lTickMs)) {
        return;
    }

    for (lAdc = 0U; lAdc < DRVADC_MAX; lAdc++) {
        (void)drvAdcSampleChannel(lAdc, 0U);
    }
}

/**************************End of file********************************/
