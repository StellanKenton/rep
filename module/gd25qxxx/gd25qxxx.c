/************************************************************************************
* @file     : gd25qxxx.c
* @brief    : GD25Qxxx SPI NOR flash module implementation.
* @details  : This file manages logical device instances, JEDEC probing, busy
*             polling, paged programming, and erase flows through port hooks.
***********************************************************************************/
#include "gd25qxxx.h"

#include "gd25qxxx_port.h"

#include <stddef.h>

static stGd25qxxxDevice gGd25qxxxDevices[GD25QXXX_DEV_MAX];
static bool gGd25qxxxDefCfgDone[GD25QXXX_DEV_MAX] = {false};

static bool gd25qxxxIsValidDevMap(eGd25qxxxMapType device);
static stGd25qxxxDevice *gd25qxxxGetDevCtx(eGd25qxxxMapType device);
static void gd25qxxxLoadDefCfg(eGd25qxxxMapType device, stGd25qxxxCfg *cfg);
static void gd25qxxxClrInfo(stGd25qxxxInfo *info);
static bool gd25qxxxIsValidDev(const stGd25qxxxDevice *device);
static bool gd25qxxxIsReadyXfer(const stGd25qxxxDevice *device);
static bool gd25qxxxIsValidCapacityId(uint8_t capacityId);
static bool gd25qxxxIsRangeValid(const stGd25qxxxDevice *device, uint32_t address, uint32_t length);
static void gd25qxxxFillInfo(stGd25qxxxInfo *info);
static eGd25qxxxStatus gd25qxxxMapPortStatus(eDrvStatus status);
static const stGd25qxxxPortSpiInterface *gd25qxxxGetSpiIf(const stGd25qxxxDevice *device);
static eGd25qxxxStatus gd25qxxxTransferInt(const stGd25qxxxDevice *device, const uint8_t *writeBuffer, uint16_t writeLength, const uint8_t *secondWriteBuffer, uint16_t secondWriteLength, uint8_t *readBuffer, uint16_t readLength);
static void gd25qxxxBuildAddrCmd(uint8_t *header, uint8_t command, uint32_t address, uint8_t addressWidth);
static eGd25qxxxStatus gd25qxxxReadStatus1Int(const stGd25qxxxDevice *device, uint8_t *statusValue);
static eGd25qxxxStatus gd25qxxxReadJedecIdInt(const stGd25qxxxDevice *device, uint8_t *manufacturerId, uint8_t *memoryType, uint8_t *capacityId);
static eGd25qxxxStatus gd25qxxxWriteEnInt(const stGd25qxxxDevice *device);
static eGd25qxxxStatus gd25qxxxWaitReadyInt(const stGd25qxxxDevice *device, uint32_t timeoutMs);
static uint8_t gd25qxxxGetReadCmd(const stGd25qxxxDevice *device);
static uint8_t gd25qxxxGetProgCmd(const stGd25qxxxDevice *device);
static uint8_t gd25qxxxGetSectEraseCmd(const stGd25qxxxDevice *device);
static uint8_t gd25qxxxGetBlkEraseCmd(const stGd25qxxxDevice *device);

eGd25qxxxStatus gd25qxxxGetDefCfg(eGd25qxxxMapType device)
{
    stGd25qxxxDevice *lDeviceCtx;

    lDeviceCtx = gd25qxxxGetDevCtx(device);
    if (lDeviceCtx == NULL) {
        return GD25QXXX_STATUS_INVALID_PARAM;
    }

    gd25qxxxLoadDefCfg(device, &lDeviceCtx->cfg);
    gd25qxxxClrInfo(&lDeviceCtx->info);
    lDeviceCtx->isReady = false;
    gGd25qxxxDefCfgDone[device] = true;
    return GD25QXXX_STATUS_OK;
}

eGd25qxxxStatus gd25qxxxGetCfg(eGd25qxxxMapType device, stGd25qxxxCfg *cfg)
{
    stGd25qxxxDevice *lDeviceCtx;

    if (cfg == NULL) {
        return GD25QXXX_STATUS_INVALID_PARAM;
    }

    lDeviceCtx = gd25qxxxGetDevCtx(device);
    if (lDeviceCtx == NULL) {
        return GD25QXXX_STATUS_INVALID_PARAM;
    }

    *cfg = lDeviceCtx->cfg;
    return GD25QXXX_STATUS_OK;
}

eGd25qxxxStatus gd25qxxxSetCfg(eGd25qxxxMapType device, const stGd25qxxxCfg *cfg)
{
    stGd25qxxxDevice *lDeviceCtx;

    if ((cfg == NULL) || !gd25qxxxPortIsValidBind(&cfg->spiBind)) {
        return GD25QXXX_STATUS_INVALID_PARAM;
    }

    lDeviceCtx = gd25qxxxGetDevCtx(device);
    if (lDeviceCtx == NULL) {
        return GD25QXXX_STATUS_INVALID_PARAM;
    }

    lDeviceCtx->cfg = *cfg;
    gd25qxxxClrInfo(&lDeviceCtx->info);
    lDeviceCtx->isReady = false;
    gGd25qxxxDefCfgDone[device] = true;
    return GD25QXXX_STATUS_OK;
}

eGd25qxxxStatus gd25qxxxInit(eGd25qxxxMapType device)
{
    const stGd25qxxxPortSpiInterface *lSpiIf;
    stGd25qxxxDevice *lDeviceCtx;
    eGd25qxxxStatus lStatus;

    lDeviceCtx = gd25qxxxGetDevCtx(device);
    if (!gd25qxxxIsValidDev(lDeviceCtx)) {
        return GD25QXXX_STATUS_INVALID_PARAM;
    }

    if (!gd25qxxxPortHasValidSpiIf(&lDeviceCtx->cfg.spiBind)) {
        return gd25qxxxPortIsValidBind(&lDeviceCtx->cfg.spiBind) ?
               GD25QXXX_STATUS_NOT_READY :
               GD25QXXX_STATUS_INVALID_PARAM;
    }

    lSpiIf = gd25qxxxPortGetSpiIf(&lDeviceCtx->cfg.spiBind);
    lStatus = gd25qxxxMapPortStatus(lSpiIf->init(lDeviceCtx->cfg.spiBind.bus));
    if (lStatus != GD25QXXX_STATUS_OK) {
        return lStatus;
    }

    lDeviceCtx->isReady = false;
    gd25qxxxClrInfo(&lDeviceCtx->info);

    lStatus = gd25qxxxReadJedecIdInt(lDeviceCtx,
                                     &lDeviceCtx->info.manufacturerId,
                                     &lDeviceCtx->info.memoryType,
                                     &lDeviceCtx->info.capacityId);
    if (lStatus != GD25QXXX_STATUS_OK) {
        return lStatus;
    }

    if (lDeviceCtx->info.manufacturerId != GD25QXXX_MANUFACTURER_ID) {
        return GD25QXXX_STATUS_DEVICE_ID_MISMATCH;
    }

    if (!gd25qxxxIsValidCapacityId(lDeviceCtx->info.capacityId)) {
        return GD25QXXX_STATUS_UNSUPPORTED;
    }

    gd25qxxxFillInfo(&lDeviceCtx->info);
    lDeviceCtx->isReady = true;
    return GD25QXXX_STATUS_OK;
}

bool gd25qxxxIsReady(eGd25qxxxMapType device)
{
    return gd25qxxxIsReadyXfer(gd25qxxxGetDevCtx(device));
}

const stGd25qxxxInfo *gd25qxxxGetInfo(eGd25qxxxMapType device)
{
    stGd25qxxxDevice *lDeviceCtx;

    lDeviceCtx = gd25qxxxGetDevCtx(device);
    if (!gd25qxxxIsReadyXfer(lDeviceCtx)) {
        return NULL;
    }

    return &lDeviceCtx->info;
}

eGd25qxxxStatus gd25qxxxReadJedecId(eGd25qxxxMapType device, uint8_t *manufacturerId, uint8_t *memoryType, uint8_t *capacityId)
{
    const stGd25qxxxPortSpiInterface *lSpiIf;
    stGd25qxxxDevice *lDeviceCtx;
    eGd25qxxxStatus lStatus;

    if ((manufacturerId == NULL) || (memoryType == NULL) || (capacityId == NULL)) {
        return GD25QXXX_STATUS_INVALID_PARAM;
    }

    lDeviceCtx = gd25qxxxGetDevCtx(device);
    if (!gd25qxxxIsValidDev(lDeviceCtx)) {
        return GD25QXXX_STATUS_INVALID_PARAM;
    }

    lSpiIf = gd25qxxxPortGetSpiIf(&lDeviceCtx->cfg.spiBind);
    if (lSpiIf == NULL) {
        return GD25QXXX_STATUS_NOT_READY;
    }

    lStatus = gd25qxxxMapPortStatus(lSpiIf->init(lDeviceCtx->cfg.spiBind.bus));
    if (lStatus != GD25QXXX_STATUS_OK) {
        return lStatus;
    }

    return gd25qxxxReadJedecIdInt(lDeviceCtx, manufacturerId, memoryType, capacityId);
}

eGd25qxxxStatus gd25qxxxReadStatus1(eGd25qxxxMapType device, uint8_t *statusValue)
{
    stGd25qxxxDevice *lDeviceCtx;

    lDeviceCtx = gd25qxxxGetDevCtx(device);
    if (!gd25qxxxIsReadyXfer(lDeviceCtx)) {
        return GD25QXXX_STATUS_NOT_READY;
    }

    return gd25qxxxReadStatus1Int(lDeviceCtx, statusValue);
}

eGd25qxxxStatus gd25qxxxWaitReady(eGd25qxxxMapType device, uint32_t timeoutMs)
{
    stGd25qxxxDevice *lDeviceCtx;

    lDeviceCtx = gd25qxxxGetDevCtx(device);
    if (!gd25qxxxIsReadyXfer(lDeviceCtx)) {
        return GD25QXXX_STATUS_NOT_READY;
    }

    return gd25qxxxWaitReadyInt(lDeviceCtx, timeoutMs);
}

eGd25qxxxStatus gd25qxxxRead(eGd25qxxxMapType device, uint32_t address, uint8_t *buffer, uint32_t length)
{
    stGd25qxxxDevice *lDeviceCtx;
    uint8_t lHeader[5];
    uint32_t lOffset;
    uint32_t lChunkLength;
    uint8_t lHeaderLength;
    eGd25qxxxStatus lStatus;

    lDeviceCtx = gd25qxxxGetDevCtx(device);
    if (!gd25qxxxIsReadyXfer(lDeviceCtx)) {
        return GD25QXXX_STATUS_NOT_READY;
    }

    if ((buffer == NULL) && (length > 0U)) {
        return GD25QXXX_STATUS_INVALID_PARAM;
    }

    if (length == 0U) {
        return GD25QXXX_STATUS_OK;
    }

    if (!gd25qxxxIsRangeValid(lDeviceCtx, address, length)) {
        return GD25QXXX_STATUS_OUT_OF_RANGE;
    }

    lHeaderLength = (lDeviceCtx->info.addressWidth == 4U) ? 5U : 4U;
    lOffset = 0U;
    while (lOffset < length) {
        lChunkLength = length - lOffset;
        if (lChunkLength > GD25QXXX_MAX_TRANSFER_LENGTH) {
            lChunkLength = GD25QXXX_MAX_TRANSFER_LENGTH;
        }

        gd25qxxxBuildAddrCmd(lHeader, gd25qxxxGetReadCmd(lDeviceCtx), address + lOffset, lDeviceCtx->info.addressWidth);
        lStatus = gd25qxxxTransferInt(lDeviceCtx, lHeader, lHeaderLength, NULL, 0U, &buffer[lOffset], (uint16_t)lChunkLength);
        if (lStatus != GD25QXXX_STATUS_OK) {
            return lStatus;
        }

        lOffset += lChunkLength;
    }

    return GD25QXXX_STATUS_OK;
}

eGd25qxxxStatus gd25qxxxWrite(eGd25qxxxMapType device, uint32_t address, const uint8_t *buffer, uint32_t length)
{
    stGd25qxxxDevice *lDeviceCtx;
    uint8_t lHeader[5];
    uint32_t lOffset;
    uint32_t lChunkLength;
    uint32_t lPageOffset;
    uint32_t lPageRemain;
    uint8_t lHeaderLength;
    eGd25qxxxStatus lStatus;

    lDeviceCtx = gd25qxxxGetDevCtx(device);
    if (!gd25qxxxIsReadyXfer(lDeviceCtx)) {
        return GD25QXXX_STATUS_NOT_READY;
    }

    if ((buffer == NULL) && (length > 0U)) {
        return GD25QXXX_STATUS_INVALID_PARAM;
    }

    if (length == 0U) {
        return GD25QXXX_STATUS_OK;
    }

    if (!gd25qxxxIsRangeValid(lDeviceCtx, address, length)) {
        return GD25QXXX_STATUS_OUT_OF_RANGE;
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

        lStatus = gd25qxxxWriteEnInt(lDeviceCtx);
        if (lStatus != GD25QXXX_STATUS_OK) {
            return lStatus;
        }

        gd25qxxxBuildAddrCmd(lHeader, gd25qxxxGetProgCmd(lDeviceCtx), address + lOffset, lDeviceCtx->info.addressWidth);
        lStatus = gd25qxxxTransferInt(lDeviceCtx, lHeader, lHeaderLength, &buffer[lOffset], (uint16_t)lChunkLength, NULL, 0U);
        if (lStatus != GD25QXXX_STATUS_OK) {
            return lStatus;
        }

        lStatus = gd25qxxxWaitReadyInt(lDeviceCtx, GD25QXXX_PAGE_PROGRAM_TIMEOUT_MS);
        if (lStatus != GD25QXXX_STATUS_OK) {
            return lStatus;
        }

        lOffset += lChunkLength;
    }

    return GD25QXXX_STATUS_OK;
}

eGd25qxxxStatus gd25qxxxEraseSector(eGd25qxxxMapType device, uint32_t address)
{
    stGd25qxxxDevice *lDeviceCtx;
    uint8_t lHeader[5];
    uint8_t lHeaderLength;
    eGd25qxxxStatus lStatus;

    lDeviceCtx = gd25qxxxGetDevCtx(device);
    if (!gd25qxxxIsReadyXfer(lDeviceCtx)) {
        return GD25QXXX_STATUS_NOT_READY;
    }

    if ((address % lDeviceCtx->info.sectorSizeBytes) != 0U) {
        return GD25QXXX_STATUS_OUT_OF_RANGE;
    }

    if (!gd25qxxxIsRangeValid(lDeviceCtx, address, lDeviceCtx->info.sectorSizeBytes)) {
        return GD25QXXX_STATUS_OUT_OF_RANGE;
    }

    lStatus = gd25qxxxWriteEnInt(lDeviceCtx);
    if (lStatus != GD25QXXX_STATUS_OK) {
        return lStatus;
    }

    lHeaderLength = (lDeviceCtx->info.addressWidth == 4U) ? 5U : 4U;
    gd25qxxxBuildAddrCmd(lHeader, gd25qxxxGetSectEraseCmd(lDeviceCtx), address, lDeviceCtx->info.addressWidth);
    lStatus = gd25qxxxTransferInt(lDeviceCtx, lHeader, lHeaderLength, NULL, 0U, NULL, 0U);
    if (lStatus != GD25QXXX_STATUS_OK) {
        return lStatus;
    }

    return gd25qxxxWaitReadyInt(lDeviceCtx, GD25QXXX_SECTOR_ERASE_TIMEOUT_MS);
}

eGd25qxxxStatus gd25qxxxEraseBlock64k(eGd25qxxxMapType device, uint32_t address)
{
    stGd25qxxxDevice *lDeviceCtx;
    uint8_t lHeader[5];
    uint8_t lHeaderLength;
    eGd25qxxxStatus lStatus;

    lDeviceCtx = gd25qxxxGetDevCtx(device);
    if (!gd25qxxxIsReadyXfer(lDeviceCtx)) {
        return GD25QXXX_STATUS_NOT_READY;
    }

    if ((address % lDeviceCtx->info.blockSizeBytes) != 0U) {
        return GD25QXXX_STATUS_OUT_OF_RANGE;
    }

    if (!gd25qxxxIsRangeValid(lDeviceCtx, address, lDeviceCtx->info.blockSizeBytes)) {
        return GD25QXXX_STATUS_OUT_OF_RANGE;
    }

    lStatus = gd25qxxxWriteEnInt(lDeviceCtx);
    if (lStatus != GD25QXXX_STATUS_OK) {
        return lStatus;
    }

    lHeaderLength = (lDeviceCtx->info.addressWidth == 4U) ? 5U : 4U;
    gd25qxxxBuildAddrCmd(lHeader, gd25qxxxGetBlkEraseCmd(lDeviceCtx), address, lDeviceCtx->info.addressWidth);
    lStatus = gd25qxxxTransferInt(lDeviceCtx, lHeader, lHeaderLength, NULL, 0U, NULL, 0U);
    if (lStatus != GD25QXXX_STATUS_OK) {
        return lStatus;
    }

    return gd25qxxxWaitReadyInt(lDeviceCtx, GD25QXXX_BLOCK_ERASE_TIMEOUT_MS);
}

eGd25qxxxStatus gd25qxxxEraseChip(eGd25qxxxMapType device)
{
    stGd25qxxxDevice *lDeviceCtx;
    uint8_t lCommand;
    eGd25qxxxStatus lStatus;

    lDeviceCtx = gd25qxxxGetDevCtx(device);
    if (!gd25qxxxIsReadyXfer(lDeviceCtx)) {
        return GD25QXXX_STATUS_NOT_READY;
    }

    lStatus = gd25qxxxWriteEnInt(lDeviceCtx);
    if (lStatus != GD25QXXX_STATUS_OK) {
        return lStatus;
    }

    lCommand = GD25QXXX_CMD_CHIP_ERASE;
    lStatus = gd25qxxxTransferInt(lDeviceCtx, &lCommand, 1U, NULL, 0U, NULL, 0U);
    if (lStatus != GD25QXXX_STATUS_OK) {
        return lStatus;
    }

    return gd25qxxxWaitReadyInt(lDeviceCtx, GD25QXXX_CHIP_ERASE_TIMEOUT_MS);
}

static bool gd25qxxxIsValidDevMap(eGd25qxxxMapType device)
{
    return ((uint32_t)device < (uint32_t)GD25QXXX_DEV_MAX);
}

static stGd25qxxxDevice *gd25qxxxGetDevCtx(eGd25qxxxMapType device)
{
    if (!gd25qxxxIsValidDevMap(device)) {
        return NULL;
    }

    if (!gGd25qxxxDefCfgDone[device]) {
        gd25qxxxLoadDefCfg(device, &gGd25qxxxDevices[device].cfg);
        gd25qxxxClrInfo(&gGd25qxxxDevices[device].info);
        gGd25qxxxDevices[device].isReady = false;
        gGd25qxxxDefCfgDone[device] = true;
    }

    return &gGd25qxxxDevices[device];
}

static void gd25qxxxLoadDefCfg(eGd25qxxxMapType device, stGd25qxxxCfg *cfg)
{
    if (cfg == NULL) {
        return;
    }

    gd25qxxxPortGetDefCfg(device, cfg);
}

static void gd25qxxxClrInfo(stGd25qxxxInfo *info)
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

static bool gd25qxxxIsValidDev(const stGd25qxxxDevice *device)
{
    return (device != NULL) && gd25qxxxPortIsValidBind(&device->cfg.spiBind);
}

static bool gd25qxxxIsReadyXfer(const stGd25qxxxDevice *device)
{
    return gd25qxxxIsValidDev(device) && device->isReady;
}

static bool gd25qxxxIsValidCapacityId(uint8_t capacityId)
{
    return (capacityId >= 0x10U) && (capacityId < 32U);
}

static bool gd25qxxxIsRangeValid(const stGd25qxxxDevice *device, uint32_t address, uint32_t length)
{
    if (!gd25qxxxIsReadyXfer(device)) {
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

static void gd25qxxxFillInfo(stGd25qxxxInfo *info)
{
    if (info == NULL) {
        return;
    }

    info->totalSizeBytes = (uint32_t)(1UL << info->capacityId);
    info->pageSizeBytes = GD25QXXX_PAGE_SIZE;
    info->sectorSizeBytes = GD25QXXX_SECTOR_SIZE;
    info->blockSizeBytes = GD25QXXX_BLOCK64K_SIZE;
    info->addressWidth = (info->totalSizeBytes > 0x01000000UL) ? 4U : 3U;
}

static eGd25qxxxStatus gd25qxxxMapPortStatus(eDrvStatus status)
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
            return (eGd25qxxxStatus)status;
        default:
            return GD25QXXX_STATUS_ERROR;
    }
}

static const stGd25qxxxPortSpiInterface *gd25qxxxGetSpiIf(const stGd25qxxxDevice *device)
{
    if (!gd25qxxxIsValidDev(device)) {
        return NULL;
    }

    return gd25qxxxPortGetSpiIf(&device->cfg.spiBind);
}

static eGd25qxxxStatus gd25qxxxTransferInt(const stGd25qxxxDevice *device, const uint8_t *writeBuffer, uint16_t writeLength, const uint8_t *secondWriteBuffer, uint16_t secondWriteLength, uint8_t *readBuffer, uint16_t readLength)
{
    const stGd25qxxxPortSpiInterface *lSpiIf;

    lSpiIf = gd25qxxxGetSpiIf(device);
    if ((lSpiIf == NULL) || (lSpiIf->transfer == NULL)) {
        return GD25QXXX_STATUS_NOT_READY;
    }

    return gd25qxxxMapPortStatus(lSpiIf->transfer(device->cfg.spiBind.bus,
                                                   writeBuffer,
                                                   writeLength,
                                                   secondWriteBuffer,
                                                   secondWriteLength,
                                                   readBuffer,
                                                   readLength,
                                                   GD25QXXX_PORT_READ_FILL_DATA));
}

static void gd25qxxxBuildAddrCmd(uint8_t *header, uint8_t command, uint32_t address, uint8_t addressWidth)
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

static eGd25qxxxStatus gd25qxxxReadStatus1Int(const stGd25qxxxDevice *device, uint8_t *statusValue)
{
    uint8_t lCommand;

    if ((device == NULL) || (statusValue == NULL)) {
        return GD25QXXX_STATUS_INVALID_PARAM;
    }

    lCommand = GD25QXXX_CMD_READ_STATUS1;
    return gd25qxxxTransferInt(device, &lCommand, 1U, NULL, 0U, statusValue, 1U);
}

static eGd25qxxxStatus gd25qxxxReadJedecIdInt(const stGd25qxxxDevice *device, uint8_t *manufacturerId, uint8_t *memoryType, uint8_t *capacityId)
{
    uint8_t lCommand;
    uint8_t lJedecId[3];
    eGd25qxxxStatus lStatus;

    if ((device == NULL) || (manufacturerId == NULL) || (memoryType == NULL) || (capacityId == NULL)) {
        return GD25QXXX_STATUS_INVALID_PARAM;
    }

    lCommand = GD25QXXX_CMD_JEDEC_ID;
    lStatus = gd25qxxxTransferInt(device, &lCommand, 1U, NULL, 0U, lJedecId, 3U);
    if (lStatus != GD25QXXX_STATUS_OK) {
        return lStatus;
    }

    *manufacturerId = lJedecId[0];
    *memoryType = lJedecId[1];
    *capacityId = lJedecId[2];
    return GD25QXXX_STATUS_OK;
}

static eGd25qxxxStatus gd25qxxxWriteEnInt(const stGd25qxxxDevice *device)
{
    uint8_t lCommand;
    uint8_t lStatusValue;
    eGd25qxxxStatus lStatus;

    if (device == NULL) {
        return GD25QXXX_STATUS_INVALID_PARAM;
    }

    lCommand = GD25QXXX_CMD_WRITE_ENABLE;
    lStatus = gd25qxxxTransferInt(device, &lCommand, 1U, NULL, 0U, NULL, 0U);
    if (lStatus != GD25QXXX_STATUS_OK) {
        return lStatus;
    }

    lStatus = gd25qxxxReadStatus1Int(device, &lStatusValue);
    if (lStatus != GD25QXXX_STATUS_OK) {
        return lStatus;
    }

    if ((lStatusValue & GD25QXXX_STATUS1_WEL_MASK) == 0U) {
        return GD25QXXX_STATUS_ERROR;
    }

    return GD25QXXX_STATUS_OK;
}

static eGd25qxxxStatus gd25qxxxWaitReadyInt(const stGd25qxxxDevice *device, uint32_t timeoutMs)
{
    uint8_t lStatusValue;
    uint32_t lElapsedMs;
    eGd25qxxxStatus lStatus;

    if (device == NULL) {
        return GD25QXXX_STATUS_INVALID_PARAM;
    }

    lElapsedMs = 0U;
    while (true) {
        lStatus = gd25qxxxReadStatus1Int(device, &lStatusValue);
        if (lStatus != GD25QXXX_STATUS_OK) {
            return lStatus;
        }

        if ((lStatusValue & GD25QXXX_STATUS1_BUSY_MASK) == 0U) {
            return GD25QXXX_STATUS_OK;
        }

        if (lElapsedMs >= timeoutMs) {
            return GD25QXXX_STATUS_TIMEOUT;
        }

        gd25qxxxPortDelayMs(GD25QXXX_BUSY_POLL_DELAY_MS);
        lElapsedMs += GD25QXXX_BUSY_POLL_DELAY_MS;
    }
}

static uint8_t gd25qxxxGetReadCmd(const stGd25qxxxDevice *device)
{
    return (device->info.addressWidth == 4U) ? GD25QXXX_CMD_READ_DATA_4B : GD25QXXX_CMD_READ_DATA;
}

static uint8_t gd25qxxxGetProgCmd(const stGd25qxxxDevice *device)
{
    return (device->info.addressWidth == 4U) ? GD25QXXX_CMD_PAGE_PROGRAM_4B : GD25QXXX_CMD_PAGE_PROGRAM;
}

static uint8_t gd25qxxxGetSectEraseCmd(const stGd25qxxxDevice *device)
{
    return (device->info.addressWidth == 4U) ? GD25QXXX_CMD_SECTOR_ERASE_4B : GD25QXXX_CMD_SECTOR_ERASE;
}

static uint8_t gd25qxxxGetBlkEraseCmd(const stGd25qxxxDevice *device)
{
    return (device->info.addressWidth == 4U) ? GD25QXXX_CMD_BLOCK_ERASE_64K_4B : GD25QXXX_CMD_BLOCK_ERASE_64K;
}
/**************************End of file********************************/
