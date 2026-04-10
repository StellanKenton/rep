/***********************************************************************************
* @file     : drvusb.c
* @brief    : Reusable USB controller driver abstraction implementation.
* @details  : This module validates public parameters, serializes controller and
*             endpoint access, and forwards USB operations to project-specific BSP
*             hook tables.
* @author   : GitHub Copilot
* @date     : 2026-04-10
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "drvusb.h"

#if (DRVUSB_LOG_SUPPORT == 1)
#include "../../service/console/log.h"
#endif

#include <stdbool.h>

#include "../../service/rtos/rtos.h"

#define DRVUSB_LOG_TAG                   "drvUsb"

static bool gDrvUsbInitialized[DRVUSB_MAX];
static stRepRtosMutex gDrvUsbMutex[DRVUSB_MAX];

__attribute__((weak)) const stDrvUsbBspInterface *drvUsbGetPlatformBspInterfaces(void)
{
    return NULL;
}

static eDrvStatus drvUsbMapRtosStatus(eRepRtosStatus status)
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

static bool drvUsbIsValid(uint8_t usb)
{
    return usb < DRVUSB_MAX;
}

static bool drvUsbIsValidEndpointAddress(uint8_t endpointAddress)
{
    return (endpointAddress & 0x70U) == 0U;
}

static bool drvUsbIsValidConstBuffer(const uint8_t *buffer, uint16_t length)
{
    return ((buffer != NULL) || (length == 0U));
}

static bool drvUsbIsValidMutableBuffer(uint8_t *buffer, uint16_t length)
{
    return ((buffer != NULL) || (length == 0U));
}

static bool drvUsbIsValidEndpointConfig(const stDrvUsbEndpointConfig *config)
{
    if (config == NULL) {
        return false;
    }

    if (!drvUsbIsValidEndpointAddress(config->endpointAddress)) {
        return false;
    }

    if (config->maxPacketSize == 0U) {
        return false;
    }

    return config->type <= DRVUSB_ENDPOINT_TYPE_INTERRUPT;
}

static bool drvUsbIsInitialized(uint8_t usb)
{
    return drvUsbIsValid(usb) && gDrvUsbInitialized[usb];
}

static stDrvUsbBspInterface *drvUsbGetBspInterface(uint8_t usb)
{
    const stDrvUsbBspInterface *lInterfaces;

    if (!drvUsbIsValid(usb)) {
        return NULL;
    }

    lInterfaces = drvUsbGetPlatformBspInterfaces();
    if (lInterfaces == NULL) {
        return NULL;
    }

    return (stDrvUsbBspInterface *)&lInterfaces[usb];
}

static bool drvUsbHasValidBspInterface(uint8_t usb)
{
    stDrvUsbBspInterface *lBspInterface = drvUsbGetBspInterface(usb);

    return (lBspInterface != NULL) &&
           (lBspInterface->init != NULL) &&
           (lBspInterface->start != NULL) &&
           (lBspInterface->stop != NULL) &&
           (lBspInterface->openEndpoint != NULL) &&
           (lBspInterface->closeEndpoint != NULL) &&
           (lBspInterface->transmit != NULL) &&
           (lBspInterface->receive != NULL);
}

static uint32_t drvUsbGetTimeoutMs(const stDrvUsbBspInterface *bspInterface, uint32_t timeoutMs)
{
    if (timeoutMs > 0U) {
        return timeoutMs;
    }

    if ((bspInterface != NULL) && (bspInterface->defaultTimeoutMs > 0U)) {
        return bspInterface->defaultTimeoutMs;
    }

    return DRVUSB_DEFAULT_TIMEOUT_MS;
}

static eDrvStatus drvUsbEnsureMutex(uint8_t usb)
{
    if (!drvUsbIsValid(usb)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (!gDrvUsbMutex[usb].isCreated) {
        return drvUsbMapRtosStatus(repRtosMutexCreate(&gDrvUsbMutex[usb]));
    }

    return DRV_STATUS_OK;
}

static eDrvStatus drvUsbLock(uint8_t usb)
{
    if (drvUsbEnsureMutex(usb) != DRV_STATUS_OK) {
        return DRV_STATUS_ERROR;
    }

    return drvUsbMapRtosStatus(repRtosMutexTake(&gDrvUsbMutex[usb], DRVUSB_LOCK_WAIT_MS));
}

static void drvUsbUnlock(uint8_t usb)
{
    if (drvUsbIsValid(usb)) {
        (void)repRtosMutexGive(&gDrvUsbMutex[usb]);
    }
}

static eDrvStatus drvUsbSetConnectionState(uint8_t usb, bool isConnect)
{
    stDrvUsbBspInterface *lBspInterface = NULL;
    eDrvStatus lStatus;

    if (!drvUsbIsValid(usb)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (!drvUsbIsInitialized(usb)) {
        return DRV_STATUS_NOT_READY;
    }

    lBspInterface = drvUsbGetBspInterface(usb);
    if ((lBspInterface == NULL) || (lBspInterface->setConnect == NULL)) {
        return DRV_STATUS_UNSUPPORTED;
    }

    lStatus = drvUsbLock(usb);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    lStatus = lBspInterface->setConnect(usb, isConnect);
    drvUsbUnlock(usb);
    return lStatus;
}

eDrvStatus drvUsbInit(uint8_t usb)
{
    stDrvUsbBspInterface *lBspInterface = NULL;
    eDrvStatus lStatus;

    if (!drvUsbIsValid(usb)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (drvUsbIsInitialized(usb)) {
        return DRV_STATUS_OK;
    }

    if (!drvUsbHasValidBspInterface(usb)) {
        #if (DRVUSB_LOG_SUPPORT == 1)
        LOG_E(DRVUSB_LOG_TAG, "Invalid BSP interface for usb %u", (unsigned int)usb);
        #endif
        return DRV_STATUS_NOT_READY;
    }

    lBspInterface = drvUsbGetBspInterface(usb);
    if (lBspInterface == NULL) {
        return DRV_STATUS_NOT_READY;
    }

    lStatus = drvUsbEnsureMutex(usb);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    lStatus = lBspInterface->init(usb);
    if (lStatus != DRV_STATUS_OK) {
        #if (DRVUSB_LOG_SUPPORT == 1)
        LOG_E(DRVUSB_LOG_TAG, "USB %u init failed, status=%d", (unsigned int)usb, (int)lStatus);
        #endif
        return lStatus;
    }

    gDrvUsbInitialized[usb] = true;
    return DRV_STATUS_OK;
}

eDrvStatus drvUsbStart(uint8_t usb)
{
    stDrvUsbBspInterface *lBspInterface = NULL;
    eDrvStatus lStatus;

    if (!drvUsbIsValid(usb)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (!drvUsbIsInitialized(usb)) {
        return DRV_STATUS_NOT_READY;
    }

    lBspInterface = drvUsbGetBspInterface(usb);
    if ((lBspInterface == NULL) || (lBspInterface->start == NULL)) {
        return DRV_STATUS_NOT_READY;
    }

    lStatus = drvUsbLock(usb);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    lStatus = lBspInterface->start(usb);
    drvUsbUnlock(usb);
    return lStatus;
}

eDrvStatus drvUsbStop(uint8_t usb)
{
    stDrvUsbBspInterface *lBspInterface = NULL;
    eDrvStatus lStatus;

    if (!drvUsbIsValid(usb)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (!drvUsbIsInitialized(usb)) {
        return DRV_STATUS_NOT_READY;
    }

    lBspInterface = drvUsbGetBspInterface(usb);
    if ((lBspInterface == NULL) || (lBspInterface->stop == NULL)) {
        return DRV_STATUS_NOT_READY;
    }

    lStatus = drvUsbLock(usb);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    lStatus = lBspInterface->stop(usb);
    drvUsbUnlock(usb);
    return lStatus;
}

eDrvStatus drvUsbConnect(uint8_t usb)
{
    return drvUsbSetConnectionState(usb, true);
}

eDrvStatus drvUsbDisconnect(uint8_t usb)
{
    return drvUsbSetConnectionState(usb, false);
}

eDrvStatus drvUsbOpenEndpoint(uint8_t usb, const stDrvUsbEndpointConfig *config)
{
    stDrvUsbBspInterface *lBspInterface = NULL;
    eDrvStatus lStatus;

    if (!drvUsbIsValid(usb) || !drvUsbIsValidEndpointConfig(config)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (!drvUsbIsInitialized(usb)) {
        return DRV_STATUS_NOT_READY;
    }

    lBspInterface = drvUsbGetBspInterface(usb);
    if ((lBspInterface == NULL) || (lBspInterface->openEndpoint == NULL)) {
        return DRV_STATUS_NOT_READY;
    }

    lStatus = drvUsbLock(usb);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    lStatus = lBspInterface->openEndpoint(usb, config);
    drvUsbUnlock(usb);
    return lStatus;
}

eDrvStatus drvUsbCloseEndpoint(uint8_t usb, uint8_t endpointAddress)
{
    stDrvUsbBspInterface *lBspInterface = NULL;
    eDrvStatus lStatus;

    if (!drvUsbIsValid(usb) || !drvUsbIsValidEndpointAddress(endpointAddress)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (!drvUsbIsInitialized(usb)) {
        return DRV_STATUS_NOT_READY;
    }

    lBspInterface = drvUsbGetBspInterface(usb);
    if ((lBspInterface == NULL) || (lBspInterface->closeEndpoint == NULL)) {
        return DRV_STATUS_NOT_READY;
    }

    lStatus = drvUsbLock(usb);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    lStatus = lBspInterface->closeEndpoint(usb, endpointAddress);
    drvUsbUnlock(usb);
    return lStatus;
}

eDrvStatus drvUsbFlushEndpoint(uint8_t usb, uint8_t endpointAddress)
{
    stDrvUsbBspInterface *lBspInterface = NULL;
    eDrvStatus lStatus;

    if (!drvUsbIsValid(usb) || !drvUsbIsValidEndpointAddress(endpointAddress)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (!drvUsbIsInitialized(usb)) {
        return DRV_STATUS_NOT_READY;
    }

    lBspInterface = drvUsbGetBspInterface(usb);
    if ((lBspInterface == NULL) || (lBspInterface->flushEndpoint == NULL)) {
        return DRV_STATUS_UNSUPPORTED;
    }

    lStatus = drvUsbLock(usb);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    lStatus = lBspInterface->flushEndpoint(usb, endpointAddress);
    drvUsbUnlock(usb);
    return lStatus;
}

eDrvStatus drvUsbTransmitTimeout(uint8_t usb, uint8_t endpointAddress, const uint8_t *buffer, uint16_t length, uint32_t timeoutMs)
{
    stDrvUsbBspInterface *lBspInterface = NULL;
    eDrvStatus lStatus;

    if (!drvUsbIsValid(usb) || !drvUsbIsValidEndpointAddress(endpointAddress) ||
        !drvUsbIsValidConstBuffer(buffer, length) || (length == 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (!drvUsbIsInitialized(usb)) {
        return DRV_STATUS_NOT_READY;
    }

    lBspInterface = drvUsbGetBspInterface(usb);
    if ((lBspInterface == NULL) || (lBspInterface->transmit == NULL)) {
        return DRV_STATUS_NOT_READY;
    }

    lStatus = drvUsbLock(usb);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    lStatus = lBspInterface->transmit(usb,
                                      endpointAddress,
                                      buffer,
                                      length,
                                      drvUsbGetTimeoutMs(lBspInterface, timeoutMs));
    drvUsbUnlock(usb);
    return lStatus;
}

eDrvStatus drvUsbTransmit(uint8_t usb, uint8_t endpointAddress, const uint8_t *buffer, uint16_t length)
{
    return drvUsbTransmitTimeout(usb, endpointAddress, buffer, length, 0U);
}

eDrvStatus drvUsbReceiveTimeout(uint8_t usb, uint8_t endpointAddress, uint8_t *buffer, uint16_t length, uint16_t *actualLength, uint32_t timeoutMs)
{
    stDrvUsbBspInterface *lBspInterface = NULL;
    eDrvStatus lStatus;

    if (actualLength != NULL) {
        *actualLength = 0U;
    }

    if (!drvUsbIsValid(usb) || !drvUsbIsValidEndpointAddress(endpointAddress) ||
        !drvUsbIsValidMutableBuffer(buffer, length) || (length == 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    if (!drvUsbIsInitialized(usb)) {
        return DRV_STATUS_NOT_READY;
    }

    lBspInterface = drvUsbGetBspInterface(usb);
    if ((lBspInterface == NULL) || (lBspInterface->receive == NULL)) {
        return DRV_STATUS_NOT_READY;
    }

    lStatus = drvUsbLock(usb);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    lStatus = lBspInterface->receive(usb,
                                     endpointAddress,
                                     buffer,
                                     length,
                                     actualLength,
                                     drvUsbGetTimeoutMs(lBspInterface, timeoutMs));
    drvUsbUnlock(usb);
    return lStatus;
}

eDrvStatus drvUsbReceive(uint8_t usb, uint8_t endpointAddress, uint8_t *buffer, uint16_t length, uint16_t *actualLength)
{
    return drvUsbReceiveTimeout(usb, endpointAddress, buffer, length, actualLength, 0U);
}

bool drvUsbIsConnected(uint8_t usb)
{
    stDrvUsbBspInterface *lBspInterface = drvUsbGetBspInterface(usb);

    return drvUsbIsInitialized(usb) &&
           (lBspInterface != NULL) &&
           (lBspInterface->isConnected != NULL) &&
           lBspInterface->isConnected(usb);
}

bool drvUsbIsConfigured(uint8_t usb)
{
    stDrvUsbBspInterface *lBspInterface = drvUsbGetBspInterface(usb);

    return drvUsbIsInitialized(usb) &&
           (lBspInterface != NULL) &&
           (lBspInterface->isConfigured != NULL) &&
           lBspInterface->isConfigured(usb);
}

eDrvUsbSpeed drvUsbGetSpeed(uint8_t usb)
{
    stDrvUsbBspInterface *lBspInterface = drvUsbGetBspInterface(usb);

    if (!drvUsbIsInitialized(usb) || (lBspInterface == NULL) || (lBspInterface->getSpeed == NULL)) {
        return DRVUSB_SPEED_UNKNOWN;
    }

    return lBspInterface->getSpeed(usb);
}

eDrvUsbRole drvUsbGetRole(uint8_t usb)
{
    stDrvUsbBspInterface *lBspInterface = drvUsbGetBspInterface(usb);

    if (!drvUsbIsValid(usb) || (lBspInterface == NULL)) {
        return DRVUSB_ROLE_UNKNOWN;
    }

    return lBspInterface->role;
}

/**************************End of file********************************/
