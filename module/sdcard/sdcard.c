/************************************************************************************
* @file     : sdcard.c
* @brief    : SD card block storage module implementation.
* @details  : Manages logical device instances, platform-backed card probing,
*             block range validation, and generic read/write/sync flows.
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "sdcard.h"

#include "sdcard_assembly.h"

#include <stddef.h>

static stSdcardDevice gSdcardDevices[SDCARD_DEV_MAX];
static bool gSdcardDefCfgDone[SDCARD_DEV_MAX] = {false};

__attribute__((weak)) void sdcardLoadPlatformDefaultCfg(eSdcardMapType device, stSdcardCfg *cfg)
{
    (void)device;

    if (cfg == NULL) {
        return;
    }

    cfg->linkId = 0U;
    cfg->initTimeoutMs = SDCARD_DEFAULT_INIT_TIMEOUT_MS;
}

__attribute__((weak)) const stSdcardInterface *sdcardGetPlatformInterface(const stSdcardCfg *cfg)
{
    (void)cfg;
    return NULL;
}

__attribute__((weak)) bool sdcardPlatformIsValidCfg(const stSdcardCfg *cfg)
{
    (void)cfg;
    return false;
}

static bool sdcardIsValidDevMap(eSdcardMapType device);
static stSdcardDevice *sdcardGetDevCtx(eSdcardMapType device);
static void sdcardLoadDefCfg(eSdcardMapType device, stSdcardCfg *cfg);
static void sdcardClrInfo(stSdcardInfo *info);
static bool sdcardIsValidDev(const stSdcardDevice *device);
static bool sdcardIsReadyXfer(const stSdcardDevice *device);
static bool sdcardIsRangeValid(const stSdcardDevice *device, uint32_t startBlock, uint32_t blockCount);
static void sdcardNormalizeInfo(stSdcardInfo *info);
static const stSdcardInterface *sdcardGetIf(const stSdcardDevice *device);
static eSdcardStatus sdcardRefreshStatusInt(stSdcardDevice *device, bool *isPresent, bool *isWriteProtected);
static eSdcardStatus sdcardRefreshInfoInt(stSdcardDevice *device);

eSdcardStatus sdcardGetDefCfg(eSdcardMapType device, stSdcardCfg *cfg)
{
    if ((cfg == NULL) || !sdcardIsValidDevMap(device)) {
        return SDCARD_STATUS_INVALID_PARAM;
    }

    sdcardLoadDefCfg(device, cfg);
    return SDCARD_STATUS_OK;
}

eSdcardStatus sdcardGetCfg(eSdcardMapType device, stSdcardCfg *cfg)
{
    stSdcardDevice *lDeviceCtx;

    if (cfg == NULL) {
        return SDCARD_STATUS_INVALID_PARAM;
    }

    lDeviceCtx = sdcardGetDevCtx(device);
    if (lDeviceCtx == NULL) {
        return SDCARD_STATUS_INVALID_PARAM;
    }

    *cfg = lDeviceCtx->cfg;
    return SDCARD_STATUS_OK;
}

eSdcardStatus sdcardSetCfg(eSdcardMapType device, const stSdcardCfg *cfg)
{
    stSdcardDevice *lDeviceCtx;

    if ((cfg == NULL) || !sdcardPlatformIsValidCfg(cfg)) {
        return SDCARD_STATUS_INVALID_PARAM;
    }

    lDeviceCtx = sdcardGetDevCtx(device);
    if (lDeviceCtx == NULL) {
        return SDCARD_STATUS_INVALID_PARAM;
    }

    lDeviceCtx->cfg = *cfg;
    sdcardClrInfo(&lDeviceCtx->info);
    lDeviceCtx->isReady = false;
    gSdcardDefCfgDone[device] = true;
    return SDCARD_STATUS_OK;
}

eSdcardStatus sdcardInit(eSdcardMapType device)
{
    stSdcardDevice *lDeviceCtx;
    const stSdcardInterface *lSdIf;
    eSdcardStatus lStatus;

    lDeviceCtx = sdcardGetDevCtx(device);
    if (!sdcardIsValidDev(lDeviceCtx)) {
        return SDCARD_STATUS_INVALID_PARAM;
    }

    lSdIf = sdcardGetIf(lDeviceCtx);
    if ((lSdIf == NULL) || (lSdIf->init == NULL) || (lSdIf->getStatus == NULL) || (lSdIf->readBlocks == NULL)) {
        return sdcardPlatformIsValidCfg(&lDeviceCtx->cfg) ?
               SDCARD_STATUS_NOT_READY :
               SDCARD_STATUS_INVALID_PARAM;
    }

    lStatus = (eSdcardStatus)lSdIf->init((uint8_t)lDeviceCtx->cfg.linkId,
                                         lDeviceCtx->cfg.initTimeoutMs);
    if (lStatus != SDCARD_STATUS_OK) {
        return lStatus;
    }

    lDeviceCtx->isReady = false;
    sdcardClrInfo(&lDeviceCtx->info);

    lStatus = sdcardRefreshStatusInt(lDeviceCtx, NULL, NULL);
    if (lStatus != SDCARD_STATUS_OK) {
        return lStatus;
    }

    lStatus = sdcardRefreshInfoInt(lDeviceCtx);
    if (lStatus != SDCARD_STATUS_OK) {
        return lStatus;
    }

    lDeviceCtx->isReady = true;
    return SDCARD_STATUS_OK;
}

bool sdcardIsReady(eSdcardMapType device)
{
    return sdcardIsReadyXfer(sdcardGetDevCtx(device));
}

const stSdcardInfo *sdcardGetInfo(eSdcardMapType device)
{
    stSdcardDevice *lDeviceCtx;

    lDeviceCtx = sdcardGetDevCtx(device);
    if (!sdcardIsReadyXfer(lDeviceCtx)) {
        return NULL;
    }

    return &lDeviceCtx->info;
}

eSdcardStatus sdcardGetStatus(eSdcardMapType device, bool *isPresent, bool *isWriteProtected)
{
    stSdcardDevice *lDeviceCtx;

    lDeviceCtx = sdcardGetDevCtx(device);
    if (!sdcardIsValidDev(lDeviceCtx)) {
        return SDCARD_STATUS_INVALID_PARAM;
    }

    return sdcardRefreshStatusInt(lDeviceCtx, isPresent, isWriteProtected);
}

eSdcardStatus sdcardReadBlocks(eSdcardMapType device, uint32_t startBlock, uint8_t *buffer, uint32_t blockCount)
{
    stSdcardDevice *lDeviceCtx;
    const stSdcardInterface *lSdIf;

    lDeviceCtx = sdcardGetDevCtx(device);
    if (!sdcardIsReadyXfer(lDeviceCtx)) {
        return SDCARD_STATUS_NOT_READY;
    }

    if ((buffer == NULL) || (blockCount == 0U)) {
        return SDCARD_STATUS_INVALID_PARAM;
    }

    if (!sdcardIsRangeValid(lDeviceCtx, startBlock, blockCount)) {
        return SDCARD_STATUS_OUT_OF_RANGE;
    }

    lSdIf = sdcardGetIf(lDeviceCtx);
    if ((lSdIf == NULL) || (lSdIf->readBlocks == NULL)) {
        return SDCARD_STATUS_NOT_READY;
    }

    return (eSdcardStatus)lSdIf->readBlocks((uint8_t)lDeviceCtx->cfg.linkId,
                                            startBlock,
                                            buffer,
                                            blockCount);
}

eSdcardStatus sdcardWriteBlocks(eSdcardMapType device, uint32_t startBlock, const uint8_t *buffer, uint32_t blockCount)
{
    stSdcardDevice *lDeviceCtx;
    const stSdcardInterface *lSdIf;

    lDeviceCtx = sdcardGetDevCtx(device);
    if (!sdcardIsReadyXfer(lDeviceCtx)) {
        return SDCARD_STATUS_NOT_READY;
    }

    if ((buffer == NULL) || (blockCount == 0U)) {
        return SDCARD_STATUS_INVALID_PARAM;
    }

    if (lDeviceCtx->info.isWriteProtected) {
        return SDCARD_STATUS_WRITE_PROTECTED;
    }

    if (!sdcardIsRangeValid(lDeviceCtx, startBlock, blockCount)) {
        return SDCARD_STATUS_OUT_OF_RANGE;
    }

    lSdIf = sdcardGetIf(lDeviceCtx);
    if ((lSdIf == NULL) || (lSdIf->writeBlocks == NULL)) {
        return SDCARD_STATUS_UNSUPPORTED;
    }

    return (eSdcardStatus)lSdIf->writeBlocks((uint8_t)lDeviceCtx->cfg.linkId,
                                             startBlock,
                                             buffer,
                                             blockCount);
}

eSdcardStatus sdcardSync(eSdcardMapType device)
{
    stSdcardDevice *lDeviceCtx;
    const stSdcardInterface *lSdIf;

    lDeviceCtx = sdcardGetDevCtx(device);
    if (!sdcardIsReadyXfer(lDeviceCtx)) {
        return SDCARD_STATUS_NOT_READY;
    }

    lSdIf = sdcardGetIf(lDeviceCtx);
    if ((lSdIf == NULL) || (lSdIf->ioctl == NULL)) {
        return SDCARD_STATUS_OK;
    }

    return (eSdcardStatus)lSdIf->ioctl((uint8_t)lDeviceCtx->cfg.linkId,
                                       (uint32_t)eSDCARD_IOCTL_SYNC,
                                       NULL);
}

eSdcardStatus sdcardTrim(eSdcardMapType device, uint32_t startBlock, uint32_t blockCount)
{
    stSdcardDevice *lDeviceCtx;
    const stSdcardInterface *lSdIf;
    stSdcardTrimRange lTrimRange;

    lDeviceCtx = sdcardGetDevCtx(device);
    if (!sdcardIsReadyXfer(lDeviceCtx)) {
        return SDCARD_STATUS_NOT_READY;
    }

    if (blockCount == 0U) {
        return SDCARD_STATUS_OK;
    }

    if (lDeviceCtx->info.isWriteProtected) {
        return SDCARD_STATUS_WRITE_PROTECTED;
    }

    if (!sdcardIsRangeValid(lDeviceCtx, startBlock, blockCount)) {
        return SDCARD_STATUS_OUT_OF_RANGE;
    }

    lSdIf = sdcardGetIf(lDeviceCtx);
    if ((lSdIf == NULL) || (lSdIf->ioctl == NULL)) {
        return SDCARD_STATUS_UNSUPPORTED;
    }

    lTrimRange.startBlock = startBlock;
    lTrimRange.blockCount = blockCount;
    return (eSdcardStatus)lSdIf->ioctl((uint8_t)lDeviceCtx->cfg.linkId,
                                       (uint32_t)eSDCARD_IOCTL_TRIM,
                                       &lTrimRange);
}

static bool sdcardIsValidDevMap(eSdcardMapType device)
{
    return ((uint32_t)device < (uint32_t)SDCARD_DEV_MAX);
}

static stSdcardDevice *sdcardGetDevCtx(eSdcardMapType device)
{
    if (!sdcardIsValidDevMap(device)) {
        return NULL;
    }

    if (!gSdcardDefCfgDone[device]) {
        sdcardLoadDefCfg(device, &gSdcardDevices[device].cfg);
        gSdcardDefCfgDone[device] = true;
    }

    return &gSdcardDevices[device];
}

static void sdcardLoadDefCfg(eSdcardMapType device, stSdcardCfg *cfg)
{
    sdcardLoadPlatformDefaultCfg(device, cfg);
}

static void sdcardClrInfo(stSdcardInfo *info)
{
    if (info == NULL) {
        return;
    }

    info->blockSize = SDCARD_DEFAULT_BLOCK_SIZE;
    info->blockCount = 0U;
    info->eraseBlockSize = 0U;
    info->capacityBytes = 0ULL;
    info->isPresent = false;
    info->isWriteProtected = false;
    info->isHighCapacity = false;
}

static bool sdcardIsValidDev(const stSdcardDevice *device)
{
    return (device != NULL) && sdcardPlatformIsValidCfg(&device->cfg);
}

static bool sdcardIsReadyXfer(const stSdcardDevice *device)
{
    return sdcardIsValidDev(device) && device->isReady && device->info.isPresent;
}

static bool sdcardIsRangeValid(const stSdcardDevice *device, uint32_t startBlock, uint32_t blockCount)
{
    uint64_t lEndBlock;

    if ((device == NULL) || (blockCount == 0U) || (device->info.blockCount == 0U)) {
        return false;
    }

    lEndBlock = (uint64_t)startBlock + (uint64_t)blockCount;
    return lEndBlock <= (uint64_t)device->info.blockCount;
}

static void sdcardNormalizeInfo(stSdcardInfo *info)
{
    if (info == NULL) {
        return;
    }

    if (info->blockSize == 0U) {
        info->blockSize = SDCARD_DEFAULT_BLOCK_SIZE;
    }

    if ((info->capacityBytes == 0ULL) && (info->blockCount > 0U)) {
        info->capacityBytes = (uint64_t)info->blockSize * (uint64_t)info->blockCount;
    }

    if (info->eraseBlockSize == 0U) {
        info->eraseBlockSize = info->blockSize;
    }
}

static const stSdcardInterface *sdcardGetIf(const stSdcardDevice *device)
{
    return sdcardIsValidDev(device) ? sdcardGetPlatformInterface(&device->cfg) : NULL;
}

static eSdcardStatus sdcardRefreshStatusInt(stSdcardDevice *device, bool *isPresent, bool *isWriteProtected)
{
    const stSdcardInterface *lSdIf;
    bool lIsPresent;
    bool lIsWriteProtected;
    eSdcardStatus lStatus;

    if (!sdcardIsValidDev(device)) {
        return SDCARD_STATUS_INVALID_PARAM;
    }

    lSdIf = sdcardGetIf(device);
    if ((lSdIf == NULL) || (lSdIf->getStatus == NULL)) {
        return SDCARD_STATUS_NOT_READY;
    }

    lStatus = (eSdcardStatus)lSdIf->getStatus((uint8_t)device->cfg.linkId,
                                              &lIsPresent,
                                              &lIsWriteProtected);
    if (lStatus != SDCARD_STATUS_OK) {
        return lStatus;
    }

    device->info.isPresent = lIsPresent;
    device->info.isWriteProtected = lIsWriteProtected;

    if (isPresent != NULL) {
        *isPresent = lIsPresent;
    }

    if (isWriteProtected != NULL) {
        *isWriteProtected = lIsWriteProtected;
    }

    if (!lIsPresent) {
        device->isReady = false;
        return SDCARD_STATUS_NO_MEDIUM;
    }

    return SDCARD_STATUS_OK;
}

static eSdcardStatus sdcardRefreshInfoInt(stSdcardDevice *device)
{
    const stSdcardInterface *lSdIf;
    stSdcardInfo lInfo;
    eSdcardStatus lStatus;

    if (!sdcardIsValidDev(device)) {
        return SDCARD_STATUS_INVALID_PARAM;
    }

    lSdIf = sdcardGetIf(device);
    if ((lSdIf == NULL) || (lSdIf->ioctl == NULL)) {
        return SDCARD_STATUS_UNSUPPORTED;
    }

    lInfo = device->info;
    lStatus = (eSdcardStatus)lSdIf->ioctl((uint8_t)device->cfg.linkId,
                                          (uint32_t)eSDCARD_IOCTL_GET_INFO,
                                          &lInfo);
    if (lStatus != SDCARD_STATUS_OK) {
        return lStatus;
    }

    sdcardNormalizeInfo(&lInfo);
    if (lInfo.blockCount == 0U) {
        return SDCARD_STATUS_NOT_READY;
    }

    lInfo.isPresent = device->info.isPresent;
    lInfo.isWriteProtected = device->info.isWriteProtected;
    device->info = lInfo;
    return SDCARD_STATUS_OK;
}
/**************************End of file********************************/