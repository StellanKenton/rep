/************************************************************************************
* @file     : w25qxxx.c
* @brief    : W25Qxxx SPI NOR flash module implementation.
* @details  : This file manages logical device instances, JEDEC probing, busy
*             polling, paged programming, and erase flows through port hooks.
***********************************************************************************/
#include "w25qxxx.h"

#include <stddef.h>


static stW25qxxxDevice gW25qxxxDevices[W25QXXX_DEV_MAX];
static bool gW25qxxxDefCfgDone[W25QXXX_DEV_MAX] = {false};

__attribute__((weak)) void w25qxxxLoadPlatformDefaultCfg(eW25qxxxMapType device, stW25qxxxCfg *cfg)
{
    (void)device;

    if (cfg == NULL) {
        return;
    }

    cfg->linkId = 0U;
}

__attribute__((weak)) const stW25qxxxSpiInterface *w25qxxxGetPlatformSpiInterface(const stW25qxxxCfg *cfg)
{
    (void)cfg;
    return NULL;
}

__attribute__((weak)) bool w25qxxxPlatformIsValidCfg(const stW25qxxxCfg *cfg)
{
    (void)cfg;
    return false;
}

__attribute__((weak)) void w25qxxxPlatformDelayMs(uint32_t delayMs)
{
    (void)delayMs;
}

static bool w25qxxxIsValidDevMap(eW25qxxxMapType device);
static stW25qxxxDevice *w25qxxxGetDevCtx(eW25qxxxMapType device);
static void w25qxxxLoadDefCfg(eW25qxxxMapType device, stW25qxxxCfg *cfg);
static void w25qxxxClrInfo(stW25qxxxInfo *info);
static bool w25qxxxIsValidDev(const stW25qxxxDevice *device);
static bool w25qxxxIsReadyXfer(const stW25qxxxDevice *device);
static bool w25qxxxIsValidCapacityId(uint8_t capacityId);
static bool w25qxxxIsRangeValid(const stW25qxxxDevice *device, uint32_t address, uint32_t length);
static void w25qxxxFillInfo(stW25qxxxInfo *info);
static eW25qxxxStatus w25qxxxMapPortStatus(eDrvStatus status);
static const stW25qxxxSpiInterface *w25qxxxGetSpiIf(const stW25qxxxDevice *device);
static eW25qxxxStatus w25qxxxTransferInt(const stW25qxxxDevice *device, const uint8_t *writeBuffer, uint16_t writeLength, const uint8_t *secondWriteBuffer, uint16_t secondWriteLength, uint8_t *readBuffer, uint16_t readLength);
static void w25qxxxBuildAddrCmd(uint8_t *header, uint8_t command, uint32_t address, uint8_t addressWidth);
static eW25qxxxStatus w25qxxxReadStatus1Int(const stW25qxxxDevice *device, uint8_t *statusValue);
static eW25qxxxStatus w25qxxxReadJedecIdInt(const stW25qxxxDevice *device, uint8_t *manufacturerId, uint8_t *memoryType, uint8_t *capacityId);
static eW25qxxxStatus w25qxxxWriteEnInt(const stW25qxxxDevice *device);
static eW25qxxxStatus w25qxxxWaitReadyInt(const stW25qxxxDevice *device, uint32_t timeoutMs);
static uint8_t w25qxxxGetReadCmd(const stW25qxxxDevice *device);
static uint8_t w25qxxxGetProgCmd(const stW25qxxxDevice *device);
static uint8_t w25qxxxGetSectEraseCmd(const stW25qxxxDevice *device);
static uint8_t w25qxxxGetBlkEraseCmd(const stW25qxxxDevice *device);

eW25qxxxStatus w25qxxxGetDefCfg(eW25qxxxMapType device, stW25qxxxCfg *cfg)
{
    if ((cfg == NULL) || !w25qxxxIsValidDevMap(device)) {
        return W25QXXX_STATUS_INVALID_PARAM;
    }

    w25qxxxLoadDefCfg(device, cfg);
    return W25QXXX_STATUS_OK;
}

eW25qxxxStatus w25qxxxGetCfg(eW25qxxxMapType device, stW25qxxxCfg *cfg)
{
    stW25qxxxDevice *lDeviceCtx;

    if (cfg == NULL) {
        return W25QXXX_STATUS_INVALID_PARAM;
    }

    lDeviceCtx = w25qxxxGetDevCtx(device);
    if (lDeviceCtx == NULL) {
        return W25QXXX_STATUS_INVALID_PARAM;
    }

    *cfg = lDeviceCtx->cfg;
    return W25QXXX_STATUS_OK;
}

eW25qxxxStatus w25qxxxSetCfg(eW25qxxxMapType device, const stW25qxxxCfg *cfg)
{
    stW25qxxxDevice *lDeviceCtx;

    if ((cfg == NULL) || !w25qxxxPlatformIsValidCfg(cfg)) {
        return W25QXXX_STATUS_INVALID_PARAM;
    }

    lDeviceCtx = w25qxxxGetDevCtx(device);
    if (lDeviceCtx == NULL) {
        return W25QXXX_STATUS_INVALID_PARAM;
    }

    lDeviceCtx->cfg = *cfg;
    w25qxxxClrInfo(&lDeviceCtx->info);
    lDeviceCtx->isReady = false;
    gW25qxxxDefCfgDone[device] = true;
    return W25QXXX_STATUS_OK;
}

eW25qxxxStatus w25qxxxInit(eW25qxxxMapType device)
{
    const stW25qxxxSpiInterface *lSpiIf;
    stW25qxxxDevice *lDeviceCtx;
    eW25qxxxStatus lStatus;

    lDeviceCtx = w25qxxxGetDevCtx(device);
    if (!w25qxxxIsValidDev(lDeviceCtx)) {
        return W25QXXX_STATUS_INVALID_PARAM;
    }

    if (w25qxxxGetSpiIf(lDeviceCtx) == NULL) {
        return w25qxxxPlatformIsValidCfg(&lDeviceCtx->cfg) ?
               W25QXXX_STATUS_NOT_READY :
               W25QXXX_STATUS_INVALID_PARAM;
    }

    lSpiIf = w25qxxxGetSpiIf(lDeviceCtx);
    lStatus = w25qxxxMapPortStatus(lSpiIf->init((uint8_t)lDeviceCtx->cfg.linkId));
    if (lStatus != W25QXXX_STATUS_OK) {
        return lStatus;
    }

    lDeviceCtx->isReady = false;
    w25qxxxClrInfo(&lDeviceCtx->info);

    lStatus = w25qxxxReadJedecIdInt(lDeviceCtx,
                                    &lDeviceCtx->info.manufacturerId,
                                    &lDeviceCtx->info.memoryType,
                                    &lDeviceCtx->info.capacityId);
    if (lStatus != W25QXXX_STATUS_OK) {
        return lStatus;
    }

    if (lDeviceCtx->info.manufacturerId != W25QXXX_MANUFACTURER_ID) {
        return W25QXXX_STATUS_DEVICE_ID_MISMATCH;
    }

    if (!w25qxxxIsValidCapacityId(lDeviceCtx->info.capacityId)) {
        return W25QXXX_STATUS_UNSUPPORTED;
    }

    w25qxxxFillInfo(&lDeviceCtx->info);
    lDeviceCtx->isReady = true;
    return W25QXXX_STATUS_OK;
}

bool w25qxxxIsReady(eW25qxxxMapType device)
{
    return w25qxxxIsReadyXfer(w25qxxxGetDevCtx(device));
}

const stW25qxxxInfo *w25qxxxGetInfo(eW25qxxxMapType device)
{
    stW25qxxxDevice *lDeviceCtx;

    lDeviceCtx = w25qxxxGetDevCtx(device);
    if (!w25qxxxIsReadyXfer(lDeviceCtx)) {
        return NULL;
    }

    return &lDeviceCtx->info;
}

eW25qxxxStatus w25qxxxReadJedecId(eW25qxxxMapType device, uint8_t *manufacturerId, uint8_t *memoryType, uint8_t *capacityId)
{
    const stW25qxxxSpiInterface *lSpiIf;
    stW25qxxxDevice *lDeviceCtx;
    eW25qxxxStatus lStatus;

    if ((manufacturerId == NULL) || (memoryType == NULL) || (capacityId == NULL)) {
        return W25QXXX_STATUS_INVALID_PARAM;
    }

    lDeviceCtx = w25qxxxGetDevCtx(device);
    if (!w25qxxxIsValidDev(lDeviceCtx)) {
        return W25QXXX_STATUS_INVALID_PARAM;
    }

    lSpiIf = w25qxxxGetSpiIf(lDeviceCtx);
    if (lSpiIf == NULL) {
        return W25QXXX_STATUS_NOT_READY;
    }

    lStatus = w25qxxxMapPortStatus(lSpiIf->init((uint8_t)lDeviceCtx->cfg.linkId));
    if (lStatus != W25QXXX_STATUS_OK) {
        return lStatus;
    }

    return w25qxxxReadJedecIdInt(lDeviceCtx, manufacturerId, memoryType, capacityId);
}

eW25qxxxStatus w25qxxxReadStatus1(eW25qxxxMapType device, uint8_t *statusValue)
{
    stW25qxxxDevice *lDeviceCtx;

    lDeviceCtx = w25qxxxGetDevCtx(device);
    if (!w25qxxxIsReadyXfer(lDeviceCtx)) {
        return W25QXXX_STATUS_NOT_READY;
    }

    return w25qxxxReadStatus1Int(lDeviceCtx, statusValue);
}

eW25qxxxStatus w25qxxxWaitReady(eW25qxxxMapType device, uint32_t timeoutMs)
{
    stW25qxxxDevice *lDeviceCtx;

    lDeviceCtx = w25qxxxGetDevCtx(device);
    if (!w25qxxxIsReadyXfer(lDeviceCtx)) {
        return W25QXXX_STATUS_NOT_READY;
    }

    return w25qxxxWaitReadyInt(lDeviceCtx, timeoutMs);
}

eW25qxxxStatus w25qxxxRead(eW25qxxxMapType device, uint32_t address, uint8_t *buffer, uint32_t length)
{
    stW25qxxxDevice *lDeviceCtx;
    uint8_t lHeader[5];
    uint32_t lOffset;
    uint32_t lChunkLength;
    uint8_t lHeaderLength;
    eW25qxxxStatus lStatus;

    lDeviceCtx = w25qxxxGetDevCtx(device);
    if (!w25qxxxIsReadyXfer(lDeviceCtx)) {
        return W25QXXX_STATUS_NOT_READY;
    }

    if ((buffer == NULL) && (length > 0U)) {
        return W25QXXX_STATUS_INVALID_PARAM;
    }

    if (length == 0U) {
        return W25QXXX_STATUS_OK;
    }

    if (!w25qxxxIsRangeValid(lDeviceCtx, address, length)) {
        return W25QXXX_STATUS_OUT_OF_RANGE;
    }

    lHeaderLength = (lDeviceCtx->info.addressWidth == 4U) ? 5U : 4U;
    lOffset = 0U;
    while (lOffset < length) {
        lChunkLength = length - lOffset;
        if (lChunkLength > W25QXXX_MAX_TRANSFER_LENGTH) {
            lChunkLength = W25QXXX_MAX_TRANSFER_LENGTH;
        }

        w25qxxxBuildAddrCmd(lHeader, w25qxxxGetReadCmd(lDeviceCtx), address + lOffset, lDeviceCtx->info.addressWidth);
        lStatus = w25qxxxTransferInt(lDeviceCtx, lHeader, lHeaderLength, NULL, 0U, &buffer[lOffset], (uint16_t)lChunkLength);
        if (lStatus != W25QXXX_STATUS_OK) {
            return lStatus;
        }

        lOffset += lChunkLength;
    }

    return W25QXXX_STATUS_OK;
}

eW25qxxxStatus w25qxxxWrite(eW25qxxxMapType device, uint32_t address, const uint8_t *buffer, uint32_t length)
{
    stW25qxxxDevice *lDeviceCtx;
    uint8_t lHeader[5];
    uint32_t lOffset;
    uint32_t lChunkLength;
    uint32_t lPageOffset;
    uint32_t lPageRemain;
    uint8_t lHeaderLength;
    eW25qxxxStatus lStatus;

    lDeviceCtx = w25qxxxGetDevCtx(device);
    if (!w25qxxxIsReadyXfer(lDeviceCtx)) {
        return W25QXXX_STATUS_NOT_READY;
    }

    if ((buffer == NULL) && (length > 0U)) {
        return W25QXXX_STATUS_INVALID_PARAM;
    }

    if (length == 0U) {
        return W25QXXX_STATUS_OK;
    }

    if (!w25qxxxIsRangeValid(lDeviceCtx, address, length)) {
        return W25QXXX_STATUS_OUT_OF_RANGE;
    }

    lHeaderLength = (lDeviceCtx->info.addressWidth == 4U) ? 5U : 4U;
    lOffset = 0U;
    while (lOffset < length) {
        lPageOffset = (address + lOffset) % lDeviceCtx->info.pageSizeBytes;
        lPageRemain = lDeviceCtx->info.pageSizeBytes - lPageOffset;
        lChunkLength = length - lOffset;
        if (lChunkLength > lPageRemain) {
            lChunkLength = lPageRemain;
        }

        lStatus = w25qxxxWriteEnInt(lDeviceCtx);
        if (lStatus != W25QXXX_STATUS_OK) {
            return lStatus;
        }

        w25qxxxBuildAddrCmd(lHeader, w25qxxxGetProgCmd(lDeviceCtx), address + lOffset, lDeviceCtx->info.addressWidth);
        lStatus = w25qxxxTransferInt(lDeviceCtx, lHeader, lHeaderLength, &buffer[lOffset], (uint16_t)lChunkLength, NULL, 0U);
        if (lStatus != W25QXXX_STATUS_OK) {
            return lStatus;
        }

        lStatus = w25qxxxWaitReadyInt(lDeviceCtx, W25QXXX_PAGE_PROGRAM_TIMEOUT_MS);
        if (lStatus != W25QXXX_STATUS_OK) {
            return lStatus;
        }

        lOffset += lChunkLength;
    }

    return W25QXXX_STATUS_OK;
}

eW25qxxxStatus w25qxxxEraseSector(eW25qxxxMapType device, uint32_t address)
{
    stW25qxxxDevice *lDeviceCtx;
    uint8_t lHeader[5];
    uint8_t lHeaderLength;
    eW25qxxxStatus lStatus;

    lDeviceCtx = w25qxxxGetDevCtx(device);
    if (!w25qxxxIsReadyXfer(lDeviceCtx)) {
        return W25QXXX_STATUS_NOT_READY;
    }

    if ((address % lDeviceCtx->info.sectorSizeBytes) != 0U) {
        return W25QXXX_STATUS_OUT_OF_RANGE;
    }

    if (!w25qxxxIsRangeValid(lDeviceCtx, address, lDeviceCtx->info.sectorSizeBytes)) {
        return W25QXXX_STATUS_OUT_OF_RANGE;
    }

    lStatus = w25qxxxWriteEnInt(lDeviceCtx);
    if (lStatus != W25QXXX_STATUS_OK) {
        return lStatus;
    }

    lHeaderLength = (lDeviceCtx->info.addressWidth == 4U) ? 5U : 4U;
    w25qxxxBuildAddrCmd(lHeader, w25qxxxGetSectEraseCmd(lDeviceCtx), address, lDeviceCtx->info.addressWidth);
    lStatus = w25qxxxTransferInt(lDeviceCtx, lHeader, lHeaderLength, NULL, 0U, NULL, 0U);
    if (lStatus != W25QXXX_STATUS_OK) {
        return lStatus;
    }

    return w25qxxxWaitReadyInt(lDeviceCtx, W25QXXX_SECTOR_ERASE_TIMEOUT_MS);
}

eW25qxxxStatus w25qxxxEraseBlock64k(eW25qxxxMapType device, uint32_t address)
{
    stW25qxxxDevice *lDeviceCtx;
    uint8_t lHeader[5];
    uint8_t lHeaderLength;
    eW25qxxxStatus lStatus;

    lDeviceCtx = w25qxxxGetDevCtx(device);
    if (!w25qxxxIsReadyXfer(lDeviceCtx)) {
        return W25QXXX_STATUS_NOT_READY;
    }

    if ((address % lDeviceCtx->info.blockSizeBytes) != 0U) {
        return W25QXXX_STATUS_OUT_OF_RANGE;
    }

    if (!w25qxxxIsRangeValid(lDeviceCtx, address, lDeviceCtx->info.blockSizeBytes)) {
        return W25QXXX_STATUS_OUT_OF_RANGE;
    }

    lStatus = w25qxxxWriteEnInt(lDeviceCtx);
    if (lStatus != W25QXXX_STATUS_OK) {
        return lStatus;
    }

    lHeaderLength = (lDeviceCtx->info.addressWidth == 4U) ? 5U : 4U;
    w25qxxxBuildAddrCmd(lHeader, w25qxxxGetBlkEraseCmd(lDeviceCtx), address, lDeviceCtx->info.addressWidth);
    lStatus = w25qxxxTransferInt(lDeviceCtx, lHeader, lHeaderLength, NULL, 0U, NULL, 0U);
    if (lStatus != W25QXXX_STATUS_OK) {
        return lStatus;
    }

    return w25qxxxWaitReadyInt(lDeviceCtx, W25QXXX_BLOCK_ERASE_TIMEOUT_MS);
}

eW25qxxxStatus w25qxxxEraseChip(eW25qxxxMapType device)
{
    stW25qxxxDevice *lDeviceCtx;
    uint8_t lCommand;
    eW25qxxxStatus lStatus;

    lDeviceCtx = w25qxxxGetDevCtx(device);
    if (!w25qxxxIsReadyXfer(lDeviceCtx)) {
        return W25QXXX_STATUS_NOT_READY;
    }

    lStatus = w25qxxxWriteEnInt(lDeviceCtx);
    if (lStatus != W25QXXX_STATUS_OK) {
        return lStatus;
    }

    lCommand = W25QXXX_CMD_CHIP_ERASE;
    lStatus = w25qxxxTransferInt(lDeviceCtx, &lCommand, 1U, NULL, 0U, NULL, 0U);
    if (lStatus != W25QXXX_STATUS_OK) {
        return lStatus;
    }

    return w25qxxxWaitReadyInt(lDeviceCtx, W25QXXX_CHIP_ERASE_TIMEOUT_MS);
}

static bool w25qxxxIsValidDevMap(eW25qxxxMapType device)
{
    return ((uint32_t)device < (uint32_t)W25QXXX_DEV_MAX);
}

static stW25qxxxDevice *w25qxxxGetDevCtx(eW25qxxxMapType device)
{
    if (!w25qxxxIsValidDevMap(device)) {
        return NULL;
    }

    if (!gW25qxxxDefCfgDone[device]) {
        w25qxxxLoadDefCfg(device, &gW25qxxxDevices[device].cfg);
        w25qxxxClrInfo(&gW25qxxxDevices[device].info);
        gW25qxxxDevices[device].isReady = false;
        gW25qxxxDefCfgDone[device] = true;
    }

    return &gW25qxxxDevices[device];
}

static void w25qxxxLoadDefCfg(eW25qxxxMapType device, stW25qxxxCfg *cfg)
{
    if (cfg == NULL) {
        return;
    }

    w25qxxxLoadPlatformDefaultCfg(device, cfg);
}

static void w25qxxxClrInfo(stW25qxxxInfo *info)
{
    if (info == NULL) {
        return;
    }

    info->manufacturerId = 0U;
    info->memoryType = 0U;
    info->capacityId = 0U;
    info->addressWidth = 0U;
    info->pageSizeBytes = 0U;
    info->totalSizeBytes = 0U;
    info->sectorSizeBytes = 0U;
    info->blockSizeBytes = 0U;
}

static bool w25qxxxIsValidDev(const stW25qxxxDevice *device)
{
    return (device != NULL) && w25qxxxPlatformIsValidCfg(&device->cfg);
}

static bool w25qxxxIsReadyXfer(const stW25qxxxDevice *device)
{
    return w25qxxxIsValidDev(device) && device->isReady;
}

static bool w25qxxxIsValidCapacityId(uint8_t capacityId)
{
    return (capacityId >= 0x10U) && (capacityId < 32U);
}

static bool w25qxxxIsRangeValid(const stW25qxxxDevice *device, uint32_t address, uint32_t length)
{
    if (!w25qxxxIsReadyXfer(device)) {
        return false;
    }

    if (length == 0U) {
        return address <= device->info.totalSizeBytes;
    }

    if (address >= device->info.totalSizeBytes) {
        return false;
    }

    return length <= (device->info.totalSizeBytes - address);
}

static void w25qxxxFillInfo(stW25qxxxInfo *info)
{
    if (info == NULL) {
        return;
    }

    info->totalSizeBytes = (uint32_t)(1UL << info->capacityId);
    info->pageSizeBytes = W25QXXX_PAGE_SIZE;
    info->sectorSizeBytes = W25QXXX_SECTOR_SIZE;
    info->blockSizeBytes = W25QXXX_BLOCK64K_SIZE;
    info->addressWidth = (info->totalSizeBytes > 0x01000000UL) ? 4U : 3U;
}

static eW25qxxxStatus w25qxxxMapPortStatus(eDrvStatus status)
{
    switch (status) {
        case DRV_STATUS_OK:
        case DRV_STATUS_INVALID_PARAM:
        case DRV_STATUS_NOT_READY:
        case DRV_STATUS_BUSY:
        case DRV_STATUS_TIMEOUT:
        case DRV_STATUS_NACK:
        case DRV_STATUS_UNSUPPORTED:
        case DRV_STATUS_ID_NOTMATCH:
        case DRV_STATUS_ERROR:
            return (eW25qxxxStatus)status;
        default:
            return W25QXXX_STATUS_ERROR;
    }
}

static const stW25qxxxSpiInterface *w25qxxxGetSpiIf(const stW25qxxxDevice *device)
{
    if (!w25qxxxIsValidDev(device)) {
        return NULL;
    }

    return w25qxxxGetPlatformSpiInterface(&device->cfg);
}

static eW25qxxxStatus w25qxxxTransferInt(const stW25qxxxDevice *device, const uint8_t *writeBuffer, uint16_t writeLength, const uint8_t *secondWriteBuffer, uint16_t secondWriteLength, uint8_t *readBuffer, uint16_t readLength)
{
    const stW25qxxxSpiInterface *lSpiIf;

    lSpiIf = w25qxxxGetSpiIf(device);
    if ((lSpiIf == NULL) || (lSpiIf->transfer == NULL)) {
        return W25QXXX_STATUS_NOT_READY;
    }

    return w25qxxxMapPortStatus(lSpiIf->transfer((uint8_t)device->cfg.linkId,
                                                  writeBuffer,
                                                  writeLength,
                                                  secondWriteBuffer,
                                                  secondWriteLength,
                                                  readBuffer,
                                                  readLength,
                                                  W25QXXX_PORT_READ_FILL_DATA));
}

static void w25qxxxBuildAddrCmd(uint8_t *header, uint8_t command, uint32_t address, uint8_t addressWidth)
{
    header[0] = command;
    if (addressWidth == 4U) {
        header[1] = (uint8_t)(address >> 24);
        header[2] = (uint8_t)(address >> 16);
        header[3] = (uint8_t)(address >> 8);
        header[4] = (uint8_t)address;
    } else {
        header[1] = (uint8_t)(address >> 16);
        header[2] = (uint8_t)(address >> 8);
        header[3] = (uint8_t)address;
    }
}

static eW25qxxxStatus w25qxxxReadStatus1Int(const stW25qxxxDevice *device, uint8_t *statusValue)
{
    uint8_t lCommand;

    if ((device == NULL) || (statusValue == NULL)) {
        return W25QXXX_STATUS_INVALID_PARAM;
    }

    lCommand = W25QXXX_CMD_READ_STATUS1;
    return w25qxxxTransferInt(device, &lCommand, 1U, NULL, 0U, statusValue, 1U);
}

static eW25qxxxStatus w25qxxxReadJedecIdInt(const stW25qxxxDevice *device, uint8_t *manufacturerId, uint8_t *memoryType, uint8_t *capacityId)
{
    uint8_t lCommand;
    uint8_t lJedecId[3];
    eW25qxxxStatus lStatus;

    if ((device == NULL) || (manufacturerId == NULL) || (memoryType == NULL) || (capacityId == NULL)) {
        return W25QXXX_STATUS_INVALID_PARAM;
    }

    lCommand = W25QXXX_CMD_JEDEC_ID;
    lStatus = w25qxxxTransferInt(device, &lCommand, 1U, NULL, 0U, lJedecId, 3U);
    if (lStatus != W25QXXX_STATUS_OK) {
        return lStatus;
    }

    *manufacturerId = lJedecId[0];
    *memoryType = lJedecId[1];
    *capacityId = lJedecId[2];
    return W25QXXX_STATUS_OK;
}

static eW25qxxxStatus w25qxxxWriteEnInt(const stW25qxxxDevice *device)
{
    uint8_t lCommand;
    uint8_t lStatusValue;
    eW25qxxxStatus lStatus;

    if (device == NULL) {
        return W25QXXX_STATUS_INVALID_PARAM;
    }

    lCommand = W25QXXX_CMD_WRITE_ENABLE;
    lStatus = w25qxxxTransferInt(device, &lCommand, 1U, NULL, 0U, NULL, 0U);
    if (lStatus != W25QXXX_STATUS_OK) {
        return lStatus;
    }

    lStatus = w25qxxxReadStatus1Int(device, &lStatusValue);
    if (lStatus != W25QXXX_STATUS_OK) {
        return lStatus;
    }

    if ((lStatusValue & W25QXXX_STATUS1_WEL_MASK) == 0U) {
        return W25QXXX_STATUS_ERROR;
    }

    return W25QXXX_STATUS_OK;
}

static eW25qxxxStatus w25qxxxWaitReadyInt(const stW25qxxxDevice *device, uint32_t timeoutMs)
{
    uint8_t lStatusValue;
    uint32_t lElapsedMs;
    eW25qxxxStatus lStatus;

    if (device == NULL) {
        return W25QXXX_STATUS_INVALID_PARAM;
    }

    lElapsedMs = 0U;
    while (true) {
        lStatus = w25qxxxReadStatus1Int(device, &lStatusValue);
        if (lStatus != W25QXXX_STATUS_OK) {
            return lStatus;
        }

        if ((lStatusValue & W25QXXX_STATUS1_BUSY_MASK) == 0U) {
            return W25QXXX_STATUS_OK;
        }

        if (lElapsedMs >= timeoutMs) {
            return W25QXXX_STATUS_TIMEOUT;
        }

        w25qxxxPlatformDelayMs(W25QXXX_BUSY_POLL_DELAY_MS);
        lElapsedMs += W25QXXX_BUSY_POLL_DELAY_MS;
    }
}

static uint8_t w25qxxxGetReadCmd(const stW25qxxxDevice *device)
{
    return (device->info.addressWidth == 4U) ? W25QXXX_CMD_READ_DATA_4B : W25QXXX_CMD_READ_DATA;
}

static uint8_t w25qxxxGetProgCmd(const stW25qxxxDevice *device)
{
    return (device->info.addressWidth == 4U) ? W25QXXX_CMD_PAGE_PROGRAM_4B : W25QXXX_CMD_PAGE_PROGRAM;
}

static uint8_t w25qxxxGetSectEraseCmd(const stW25qxxxDevice *device)
{
    return (device->info.addressWidth == 4U) ? W25QXXX_CMD_SECTOR_ERASE_4B : W25QXXX_CMD_SECTOR_ERASE;
}

static uint8_t w25qxxxGetBlkEraseCmd(const stW25qxxxDevice *device)
{
    return (device->info.addressWidth == 4U) ? W25QXXX_CMD_BLOCK_ERASE_64K_4B : W25QXXX_CMD_BLOCK_ERASE_64K;
}
/**************************End of file********************************/
