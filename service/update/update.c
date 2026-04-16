/***********************************************************************************
* @file     : update.c
* @brief    : Reusable firmware update state machine implementation.
* @details  : Keeps the generic update flow on logical regions and metadata
*             records so projects only need to bind storage devices in the
*             platform layer.
* @author   : GitHub Copilot
* @date     : 2026-04-16
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "update.h"

#include <stddef.h>
#include <string.h>

#include "update_debug.h"
#include "update_port.h"

#if (UPDATE_LOG_SUPPORT == 1)
#include "../console/log.h"
#endif

#define UPDATE_LOG_TAG                       "update"
#define UPDATE_META_SLOT_COUNT               2U
#define UPDATE_CRC32_INIT_VALUE              0xFFFFFFFFUL
#define UPDATE_STACK_POINTER_MASK            0x2FFE0000UL
#define UPDATE_STACK_POINTER_BASE            0x20000000UL

typedef struct stUpdateContext {
    stUpdateCfg cfg;
    stUpdateBootRecord bootRecord;
    stUpdateImageHeader stagingHeader;
    stUpdateImageHeader backupHeader;
    uint32_t bootRecordMetaSequence;
    uint32_t stagingHeaderMetaSequence;
    uint32_t backupHeaderMetaSequence;
    uint32_t verifyExpectedCrc32;
    uint32_t lastProgressLogOffset;
    uint32_t processChunkSize;
    bool hasBackupRegion;
    bool targetWasErased;
    bool isInitialized;
} stUpdateContext;

static stUpdateContext gUpdateContext;
static stUpdateStatus gUpdateStatus;
static uint8_t gUpdateIoBuffer[UPDATE_PROCESS_CHUNK_SIZE];

static void updateSetState(eUpdateState state, uint32_t totalSize, uint32_t expectedCrc32);
static uint32_t updateResolveNowTick(uint32_t nowTickMs);
static bool updateIsRegionConfigured(uint8_t regionId);
static bool updateGetRegionCfg(uint8_t regionId, stUpdateRegionCfg *cfg);
static bool updateStorageRead(uint8_t regionId, uint32_t offset, uint8_t *buffer, uint32_t length);
static bool updateStorageWrite(uint8_t regionId, uint32_t offset, const uint8_t *buffer, uint32_t length);
static bool updateStorageErase(uint8_t regionId, uint32_t offset, uint32_t length);
static bool updateInitStorageDevices(void);
static bool updateValidateCfg(void);
static uint32_t updateCrc32Update(uint32_t crc, const uint8_t *data, uint32_t length);
static uint32_t updateCrc32Finalize(uint32_t crc);
static uint32_t updateCalcHeaderCrc(const stUpdateImageHeader *header);
static uint32_t updateCalcBootRecordCrc(const stUpdateBootRecord *record);
static bool updateIsHeaderValid(const stUpdateImageHeader *header, uint32_t maxImageSize, bool requireReady);
static bool updateIsBootRecordValid(const stUpdateBootRecord *record);
static void updateBuildDefaultBootRecord(stUpdateBootRecord *record);
static void updateBuildDefaultImageHeader(stUpdateImageHeader *header, uint32_t imageType);
static bool updateMetaLoad(uint8_t regionId, void *payload, uint32_t payloadSize, uint32_t *sequenceOut);
static bool updateMetaCommit(uint8_t regionId, const void *payload, uint32_t payloadSize, uint32_t nextSequence);
static uint32_t updateGetNextSequence(uint32_t currentSequence);
static bool updateLoadBootRecord(stUpdateBootRecord *record, uint32_t *sequenceOut);
static bool updateStoreBootRecord(const stUpdateBootRecord *record, uint32_t *sequenceInOut);
static bool updateLoadImageHeader(uint8_t regionId, stUpdateImageHeader *header, uint32_t *sequenceOut, uint32_t imageTypeHint);
static bool updateStoreImageHeader(uint8_t regionId, const stUpdateImageHeader *header, uint32_t *sequenceInOut);
static void updateLogStateTransition(eUpdateState from, eUpdateState to);
static void updateLogProgress(const char *stageName);
static void updateMarkError(eUpdateError error);
static bool updateShouldUseBackup(void);
static bool updateShouldRollback(void);
static bool updateValidateExecutableRegion(uint8_t regionId);
static uint32_t updateGetCopyLimit(void);
static void updateHandleFailure(eUpdateError error);
static void updateHandleCheckRequest(void);
static void updateHandleValidateStaging(void);
static void updateHandlePrepareBackup(void);
static void updateHandleBackupRunApp(void);
static void updateHandleVerifyBackup(void);
static void updateHandleEraseTarget(void);
static void updateHandleProgramTarget(void);
static void updateHandleVerifyTarget(void);
static void updateHandleCommitResult(void);
static void updateHandleRollbackEraseTarget(void);
static void updateHandleRollbackProgramBackup(void);
static void updateHandleVerifyRollback(void);

static void updateSetState(eUpdateState state, uint32_t totalSize, uint32_t expectedCrc32)
{
    eUpdateState lOldState = gUpdateStatus.state;

    if (lOldState != state) {
        updateLogStateTransition(lOldState, state);
        updateDbgNotifyStateChanged(lOldState, state);
    }

    gUpdateStatus.state = state;
    gUpdateStatus.currentOffset = 0U;
    gUpdateStatus.totalSize = totalSize;
    gUpdateStatus.activeCrc32 = (totalSize > 0U) ? UPDATE_CRC32_INIT_VALUE : 0U;
    gUpdateContext.verifyExpectedCrc32 = expectedCrc32;
    gUpdateContext.lastProgressLogOffset = 0U;
}

static uint32_t updateResolveNowTick(uint32_t nowTickMs)
{
    if (nowTickMs != 0U) {
        return nowTickMs;
    }

    return updatePortGetTickMs();
}

static bool updateIsRegionConfigured(uint8_t regionId)
{
    stUpdateRegionCfg lCfg;

    if (!updateGetRegionCfg(regionId, &lCfg)) {
        return false;
    }

    return (lCfg.size > 0U) && (lCfg.storageId < E_UPDATE_STORAGE_MAX);
}

static bool updateGetRegionCfg(uint8_t regionId, stUpdateRegionCfg *cfg)
{
    if ((cfg == NULL) || (regionId >= E_UPDATE_REGION_MAX)) {
        return false;
    }

    *cfg = gUpdateContext.cfg.regions[regionId];
    return cfg->size > 0U;
}

static bool updateStorageRead(uint8_t regionId, uint32_t offset, uint8_t *buffer, uint32_t length)
{
    const stUpdateStorageOps *lOps;
    stUpdateRegionCfg lCfg;
    uint32_t lAddress;

    if ((buffer == NULL) || (length == 0U) || !updateGetRegionCfg(regionId, &lCfg)) {
        return false;
    }

    if (!lCfg.isReadable || (offset > lCfg.size) || (length > (lCfg.size - offset))) {
        return false;
    }

    lOps = updatePortGetStorageOps(lCfg.storageId);
    if ((lOps == NULL) || (lOps->read == NULL)) {
        return false;
    }

    lAddress = lCfg.startAddress + offset;
    if ((lOps->isRangeValid != NULL) && !lOps->isRangeValid(lAddress, length)) {
        return false;
    }

    return lOps->read(lAddress, buffer, length);
}

static bool updateStorageWrite(uint8_t regionId, uint32_t offset, const uint8_t *buffer, uint32_t length)
{
    const stUpdateStorageOps *lOps;
    stUpdateRegionCfg lCfg;
    uint32_t lAddress;

    if ((buffer == NULL) || (length == 0U) || !updateGetRegionCfg(regionId, &lCfg)) {
        return false;
    }

    if (!lCfg.isWritable || (offset > lCfg.size) || (length > (lCfg.size - offset))) {
        return false;
    }

    lOps = updatePortGetStorageOps(lCfg.storageId);
    if ((lOps == NULL) || (lOps->write == NULL)) {
        return false;
    }

    lAddress = lCfg.startAddress + offset;
    if ((lOps->isRangeValid != NULL) && !lOps->isRangeValid(lAddress, length)) {
        return false;
    }

    return lOps->write(lAddress, buffer, length);
}

static bool updateStorageErase(uint8_t regionId, uint32_t offset, uint32_t length)
{
    const stUpdateStorageOps *lOps;
    stUpdateRegionCfg lCfg;
    uint32_t lAddress;

    if ((length == 0U) || !updateGetRegionCfg(regionId, &lCfg)) {
        return false;
    }

    if (!lCfg.isWritable || (offset > lCfg.size) || (length > (lCfg.size - offset))) {
        return false;
    }

    lOps = updatePortGetStorageOps(lCfg.storageId);
    if ((lOps == NULL) || (lOps->erase == NULL)) {
        return false;
    }

    lAddress = lCfg.startAddress + offset;
    if ((lOps->isRangeValid != NULL) && !lOps->isRangeValid(lAddress, length)) {
        return false;
    }

    return lOps->erase(lAddress, length);
}

static bool updateInitStorageDevices(void)
{
    uint8_t lInitialized[E_UPDATE_STORAGE_MAX] = {0U};
    uint32_t lIndex;

    for (lIndex = 0U; lIndex < E_UPDATE_REGION_MAX; lIndex++) {
        const stUpdateStorageOps *lOps;
        stUpdateRegionCfg lCfg;

        if (!updateGetRegionCfg((uint8_t)lIndex, &lCfg)) {
            continue;
        }

        if (lCfg.storageId >= E_UPDATE_STORAGE_MAX) {
            return false;
        }

        if (lInitialized[lCfg.storageId] != 0U) {
            continue;
        }

        lOps = updatePortGetStorageOps(lCfg.storageId);
        if ((lOps == NULL) || (lOps->init == NULL) || !lOps->init()) {
            return false;
        }

        lInitialized[lCfg.storageId] = 1U;
    }

    return true;
}

static bool updateValidateCfg(void)
{
    uint32_t lFirstIndex;
    uint32_t lSecondIndex;

    if (!updateIsRegionConfigured(E_UPDATE_REGION_RUN_APP) ||
        !updateIsRegionConfigured(E_UPDATE_REGION_STAGING_APP) ||
        !updateIsRegionConfigured(E_UPDATE_REGION_STAGING_APP_HEADER) ||
        !updateIsRegionConfigured(E_UPDATE_REGION_BOOT_RECORD)) {
        return false;
    }

    if (!gUpdateContext.cfg.regions[E_UPDATE_REGION_RUN_APP].isExecutable) {
        return false;
    }

    if (!gUpdateContext.cfg.regions[E_UPDATE_REGION_STAGING_APP].isWritable) {
        return false;
    }

    gUpdateContext.hasBackupRegion = updateIsRegionConfigured(E_UPDATE_REGION_BACKUP_APP) &&
                                     updateIsRegionConfigured(E_UPDATE_REGION_BACKUP_APP_HEADER);

    if (gUpdateContext.cfg.enableRollback && gUpdateContext.hasBackupRegion) {
        if (!gUpdateContext.cfg.regions[E_UPDATE_REGION_BACKUP_APP].isWritable) {
            return false;
        }

        if (gUpdateContext.cfg.regions[E_UPDATE_REGION_BACKUP_APP].size <
            gUpdateContext.cfg.regions[E_UPDATE_REGION_RUN_APP].size) {
            return false;
        }
    }

    for (lFirstIndex = 0U; lFirstIndex < E_UPDATE_REGION_MAX; lFirstIndex++) {
        stUpdateRegionCfg lFirst;

        if (!updateGetRegionCfg((uint8_t)lFirstIndex, &lFirst)) {
            continue;
        }

        if ((lFirst.eraseUnit == 0U) || (lFirst.progUnit == 0U)) {
            return false;
        }

        if ((lFirst.headerReserveSize > 0U) && (lFirst.size <= lFirst.headerReserveSize)) {
            return false;
        }

        for (lSecondIndex = lFirstIndex + 1U; lSecondIndex < E_UPDATE_REGION_MAX; lSecondIndex++) {
            stUpdateRegionCfg lSecond;

            if (!updateGetRegionCfg((uint8_t)lSecondIndex, &lSecond)) {
                continue;
            }

            if (lFirst.storageId != lSecond.storageId) {
                continue;
            }

            if ((lFirst.startAddress < (lSecond.startAddress + lSecond.size)) &&
                (lSecond.startAddress < (lFirst.startAddress + lFirst.size))) {
                return false;
            }
        }
    }

    return true;
}

static uint32_t updateCrc32Update(uint32_t crc, const uint8_t *data, uint32_t length)
{
    uint32_t lIndex;
    uint8_t lBit;

    if ((data == NULL) && (length > 0U)) {
        return crc;
    }

    for (lIndex = 0U; lIndex < length; lIndex++) {
        crc ^= data[lIndex];
        for (lBit = 0U; lBit < 8U; lBit++) {
            if ((crc & 1U) != 0U) {
                crc = (crc >> 1) ^ 0xEDB88320UL;
            } else {
                crc >>= 1;
            }
        }
    }

    return crc;
}

static uint32_t updateCrc32Finalize(uint32_t crc)
{
    return crc ^ 0xFFFFFFFFUL;
}

static uint32_t updateCalcHeaderCrc(const stUpdateImageHeader *header)
{
    if (header == NULL) {
        return 0U;
    }

    return updateCrc32Finalize(updateCrc32Update(UPDATE_CRC32_INIT_VALUE,
                                                 (const uint8_t *)header,
                                                 offsetof(stUpdateImageHeader, headerCrc32)));
}

static uint32_t updateCalcBootRecordCrc(const stUpdateBootRecord *record)
{
    if (record == NULL) {
        return 0U;
    }

    return updateCrc32Finalize(updateCrc32Update(UPDATE_CRC32_INIT_VALUE,
                                                 (const uint8_t *)record,
                                                 offsetof(stUpdateBootRecord, recordCrc32)));
}

static bool updateIsHeaderValid(const stUpdateImageHeader *header, uint32_t maxImageSize, bool requireReady)
{
    if (header == NULL) {
        return false;
    }

    if ((header->magic != UPDATE_IMAGE_MAGIC) ||
        (header->headerVersion != UPDATE_HEADER_VERSION) ||
        (header->imageSize == 0U) ||
        (header->imageSize > maxImageSize) ||
        (header->headerCrc32 != updateCalcHeaderCrc(header))) {
        return false;
    }

    if (requireReady) {
        if ((header->imageState != (uint32_t)E_UPDATE_IMAGE_STATE_READY) ||
            (header->writeOffset < header->imageSize)) {
            return false;
        }
    }

    return true;
}

static bool updateIsBootRecordValid(const stUpdateBootRecord *record)
{
    if (record == NULL) {
        return false;
    }

    if ((record->magic != UPDATE_BOOT_RECORD_MAGIC) ||
        (record->requestFlag > (uint32_t)E_UPDATE_REQUEST_FAILED) ||
        (record->targetRegion >= E_UPDATE_REGION_MAX)) {
        return false;
    }

    return record->recordCrc32 == updateCalcBootRecordCrc(record);
}

static void updateBuildDefaultBootRecord(stUpdateBootRecord *record)
{
    if (record == NULL) {
        return;
    }

    (void)memset(record, 0, sizeof(*record));
    record->magic = UPDATE_BOOT_RECORD_MAGIC;
    record->requestFlag = (uint32_t)E_UPDATE_REQUEST_IDLE;
    record->targetRegion = (uint32_t)E_UPDATE_REGION_RUN_APP;
    record->recordCrc32 = updateCalcBootRecordCrc(record);
}

static void updateBuildDefaultImageHeader(stUpdateImageHeader *header, uint32_t imageType)
{
    if (header == NULL) {
        return;
    }

    (void)memset(header, 0, sizeof(*header));
    header->magic = UPDATE_IMAGE_MAGIC;
    header->headerVersion = UPDATE_HEADER_VERSION;
    header->imageType = imageType;
    header->imageState = (uint32_t)E_UPDATE_IMAGE_STATE_EMPTY;
    header->headerCrc32 = updateCalcHeaderCrc(header);
}

static bool updateMetaLoad(uint8_t regionId, void *payload, uint32_t payloadSize, uint32_t *sequenceOut)
{
    stUpdateRegionCfg lCfg;
    stUpdateMetaRecord lRecord;
    bool lHasValidRecord = false;
    uint32_t lSlotCount;
    uint32_t lSlotIndex;
    uint32_t lBestSequence = 0U;

    if ((payload == NULL) || (payloadSize > sizeof(lRecord.payload)) || !updateGetRegionCfg(regionId, &lCfg) ||
        (lCfg.eraseUnit == 0U) || (lCfg.size < lCfg.eraseUnit)) {
        return false;
    }

    lSlotCount = lCfg.size / lCfg.eraseUnit;
    if (lSlotCount > UPDATE_META_SLOT_COUNT) {
        lSlotCount = UPDATE_META_SLOT_COUNT;
    }

    for (lSlotIndex = 0U; lSlotIndex < lSlotCount; lSlotIndex++) {
        uint32_t lPayloadCrc;
        uint32_t lHeaderCrc;

        if (!updateStorageRead(regionId, lSlotIndex * lCfg.eraseUnit, (uint8_t *)&lRecord, sizeof(lRecord))) {
            continue;
        }

        lHeaderCrc = updateCrc32Finalize(updateCrc32Update(UPDATE_CRC32_INIT_VALUE,
                                                           (const uint8_t *)&lRecord,
                                                           offsetof(stUpdateMetaRecord, headerCrc32)));

        if ((lRecord.recordMagic != gUpdateContext.cfg.metaRecordMagic) ||
            (lRecord.commitMarker != gUpdateContext.cfg.metaCommitMarker) ||
            (lRecord.payloadLength != payloadSize) ||
            (lRecord.headerCrc32 != lHeaderCrc)) {
            continue;
        }

        lPayloadCrc = updateCrc32Finalize(updateCrc32Update(UPDATE_CRC32_INIT_VALUE,
                                                            lRecord.payload,
                                                            lRecord.payloadLength));
        if (lRecord.payloadCrc32 != lPayloadCrc) {
            continue;
        }

        if (!lHasValidRecord || (lRecord.sequence >= lBestSequence)) {
            lHasValidRecord = true;
            lBestSequence = lRecord.sequence;
            memcpy(payload, lRecord.payload, payloadSize);
        }
    }

    if (sequenceOut != NULL) {
        *sequenceOut = lBestSequence;
    }

    return lHasValidRecord;
}

static bool updateMetaCommit(uint8_t regionId, const void *payload, uint32_t payloadSize, uint32_t nextSequence)
{
    stUpdateRegionCfg lCfg;
    stUpdateMetaRecord lRecord;
    stUpdateMetaRecord lReadback;
    uint32_t lSlotIndex;
    uint32_t lActiveSlot = 0U;
    uint32_t lActiveSequence = 0U;
    uint32_t lSlotCount;
    bool lHasActiveSlot = false;
    uint32_t lTargetSlot;
    uint32_t lCommitOffset;

    if ((payload == NULL) || (payloadSize > sizeof(lRecord.payload)) || !updateGetRegionCfg(regionId, &lCfg) ||
        (lCfg.eraseUnit == 0U) || (lCfg.size < lCfg.eraseUnit)) {
        return false;
    }

    lSlotCount = lCfg.size / lCfg.eraseUnit;
    if (lSlotCount > UPDATE_META_SLOT_COUNT) {
        lSlotCount = UPDATE_META_SLOT_COUNT;
    }

    if (lSlotCount == 0U) {
        return false;
    }

    for (lSlotIndex = 0U; lSlotIndex < lSlotCount; lSlotIndex++) {
        uint32_t lPayloadCrc;
        uint32_t lHeaderCrc;

        if (!updateStorageRead(regionId, lSlotIndex * lCfg.eraseUnit, (uint8_t *)&lReadback, sizeof(lReadback))) {
            continue;
        }

        lHeaderCrc = updateCrc32Finalize(updateCrc32Update(UPDATE_CRC32_INIT_VALUE,
                                                           (const uint8_t *)&lReadback,
                                                           offsetof(stUpdateMetaRecord, headerCrc32)));
        if ((lReadback.recordMagic != gUpdateContext.cfg.metaRecordMagic) ||
            (lReadback.commitMarker != gUpdateContext.cfg.metaCommitMarker) ||
            (lReadback.payloadLength != payloadSize) ||
            (lReadback.headerCrc32 != lHeaderCrc)) {
            continue;
        }

        lPayloadCrc = updateCrc32Finalize(updateCrc32Update(UPDATE_CRC32_INIT_VALUE,
                                                            lReadback.payload,
                                                            lReadback.payloadLength));
        if (lReadback.payloadCrc32 != lPayloadCrc) {
            continue;
        }

        if (!lHasActiveSlot || (lReadback.sequence >= lActiveSequence)) {
            lHasActiveSlot = true;
            lActiveSlot = lSlotIndex;
            lActiveSequence = lReadback.sequence;
        }
    }

    if (lHasActiveSlot && (lSlotCount > 1U)) {
        lTargetSlot = (lActiveSlot + 1U) % lSlotCount;
    } else {
        lTargetSlot = 0U;
    }

    (void)memset(&lRecord, 0xFF, sizeof(lRecord));
    lRecord.recordMagic = gUpdateContext.cfg.metaRecordMagic;
    lRecord.sequence = nextSequence;
    lRecord.payloadLength = payloadSize;
    memcpy(lRecord.payload, payload, payloadSize);
    lRecord.payloadCrc32 = updateCrc32Finalize(updateCrc32Update(UPDATE_CRC32_INIT_VALUE,
                                                                 lRecord.payload,
                                                                 payloadSize));
    lRecord.headerCrc32 = updateCrc32Finalize(updateCrc32Update(UPDATE_CRC32_INIT_VALUE,
                                                                (const uint8_t *)&lRecord,
                                                                offsetof(stUpdateMetaRecord, headerCrc32)));

    if (!updateStorageErase(regionId, lTargetSlot * lCfg.eraseUnit, lCfg.eraseUnit)) {
        return false;
    }

    lCommitOffset = (lTargetSlot * lCfg.eraseUnit) + offsetof(stUpdateMetaRecord, commitMarker);

    if (!updateStorageWrite(regionId, lTargetSlot * lCfg.eraseUnit, (const uint8_t *)&lRecord, offsetof(stUpdateMetaRecord, commitMarker))) {
        return false;
    }

    if (!updateStorageRead(regionId, lTargetSlot * lCfg.eraseUnit, (uint8_t *)&lReadback, offsetof(stUpdateMetaRecord, commitMarker))) {
        return false;
    }

    if ((lReadback.recordMagic != lRecord.recordMagic) ||
        (lReadback.sequence != lRecord.sequence) ||
        (lReadback.payloadLength != lRecord.payloadLength) ||
        (lReadback.payloadCrc32 != lRecord.payloadCrc32) ||
        (lReadback.headerCrc32 != lRecord.headerCrc32)) {
        return false;
    }

    return updateStorageWrite(regionId,
                              lCommitOffset,
                              (const uint8_t *)&gUpdateContext.cfg.metaCommitMarker,
                              sizeof(gUpdateContext.cfg.metaCommitMarker));
}

static uint32_t updateGetNextSequence(uint32_t currentSequence)
{
    if ((currentSequence == 0U) || (currentSequence == 0xFFFFFFFFUL)) {
        return 1U;
    }

    return currentSequence + 1U;
}

static bool updateLoadBootRecord(stUpdateBootRecord *record, uint32_t *sequenceOut)
{
    stUpdateBootRecord lRecord;

    if (record == NULL) {
        return false;
    }

    if (!updateMetaLoad(E_UPDATE_REGION_BOOT_RECORD, &lRecord, sizeof(lRecord), sequenceOut) || !updateIsBootRecordValid(&lRecord)) {
        updateBuildDefaultBootRecord(record);
        if (sequenceOut != NULL) {
            *sequenceOut = 0U;
        }
        return true;
    }

    *record = lRecord;
    return true;
}

static bool updateStoreBootRecord(const stUpdateBootRecord *record, uint32_t *sequenceInOut)
{
    stUpdateBootRecord lRecord;
    uint32_t lNextSequence;

    if ((record == NULL) || (sequenceInOut == NULL)) {
        return false;
    }

    lRecord = *record;
    lNextSequence = updateGetNextSequence(*sequenceInOut);
    lRecord.sequence = lNextSequence;
    lRecord.recordCrc32 = updateCalcBootRecordCrc(&lRecord);
    if (!updateMetaCommit(E_UPDATE_REGION_BOOT_RECORD, &lRecord, sizeof(lRecord), lNextSequence)) {
        return false;
    }

    *sequenceInOut = lNextSequence;
    gUpdateContext.bootRecord = lRecord;
    return true;
}

static bool updateLoadImageHeader(uint8_t regionId, stUpdateImageHeader *header, uint32_t *sequenceOut, uint32_t imageTypeHint)
{
    stUpdateImageHeader lHeader;
    stUpdateRegionCfg lCfg;
    uint8_t lImageRegion;

    if (header == NULL) {
        return false;
    }

    lImageRegion = (regionId == E_UPDATE_REGION_STAGING_APP_HEADER) ? E_UPDATE_REGION_STAGING_APP : E_UPDATE_REGION_BACKUP_APP;
    if (!updateGetRegionCfg(lImageRegion, &lCfg)) {
        return false;
    }

    if (!updateMetaLoad(regionId, &lHeader, sizeof(lHeader), sequenceOut) ||
        !updateIsHeaderValid(&lHeader, lCfg.size, false)) {
        updateBuildDefaultImageHeader(header, imageTypeHint);
        if (sequenceOut != NULL) {
            *sequenceOut = 0U;
        }
        return true;
    }

    *header = lHeader;
    return true;
}

static bool updateStoreImageHeader(uint8_t regionId, const stUpdateImageHeader *header, uint32_t *sequenceInOut)
{
    stUpdateImageHeader lHeader;
    uint32_t lNextSequence;

    if ((header == NULL) || (sequenceInOut == NULL)) {
        return false;
    }

    lHeader = *header;
    lHeader.headerCrc32 = updateCalcHeaderCrc(&lHeader);
    lNextSequence = updateGetNextSequence(*sequenceInOut);
    if (!updateMetaCommit(regionId, &lHeader, sizeof(lHeader), lNextSequence)) {
        return false;
    }

    *sequenceInOut = lNextSequence;
    if (regionId == E_UPDATE_REGION_STAGING_APP_HEADER) {
        gUpdateContext.stagingHeader = lHeader;
    } else if (regionId == E_UPDATE_REGION_BACKUP_APP_HEADER) {
        gUpdateContext.backupHeader = lHeader;
    }

    return true;
}

static void updateLogStateTransition(eUpdateState from, eUpdateState to)
{
#if (UPDATE_LOG_SUPPORT == 1)
    LOG_I(UPDATE_LOG_TAG,
          "state: %s -> %s",
          updateDbgGetStateName(from),
          updateDbgGetStateName(to));
#else
    (void)from;
    (void)to;
#endif
}

static void updateLogProgress(const char *stageName)
{
#if (UPDATE_LOG_SUPPORT == 1)
    if ((stageName == NULL) || (gUpdateStatus.totalSize == 0U)) {
        return;
    }

    if ((gUpdateStatus.currentOffset == gUpdateStatus.totalSize) ||
        ((gUpdateStatus.currentOffset - gUpdateContext.lastProgressLogOffset) >= (32UL * 1024UL))) {
        gUpdateContext.lastProgressLogOffset = gUpdateStatus.currentOffset;
        LOG_I(UPDATE_LOG_TAG,
              "%s: %lu / %lu bytes",
              stageName,
              (unsigned long)gUpdateStatus.currentOffset,
              (unsigned long)gUpdateStatus.totalSize);
    }
#else
    (void)stageName;
#endif
}

static void updateMarkError(eUpdateError error)
{
    gUpdateStatus.lastError = error;
    gUpdateContext.bootRecord.lastError = (uint32_t)error;
}

static bool updateShouldUseBackup(void)
{
    return gUpdateContext.cfg.enableRollback && gUpdateContext.hasBackupRegion;
}

static bool updateShouldRollback(void)
{
    return updateShouldUseBackup() && !gUpdateStatus.isRollbackActive && gUpdateContext.targetWasErased &&
           updateIsHeaderValid(&gUpdateContext.backupHeader,
                               gUpdateContext.cfg.regions[E_UPDATE_REGION_BACKUP_APP].size,
                               true);
}

static bool updateValidateExecutableRegion(uint8_t regionId)
{
    stUpdateRegionCfg lCfg;
    uint32_t lVectors[2] = {0U, 0U};
    uint32_t lResetHandler;

    if (!updateGetRegionCfg(regionId, &lCfg) || !lCfg.isExecutable || !lCfg.isReadable) {
        return false;
    }

    if (!updateStorageRead(regionId, 0U, (uint8_t *)lVectors, sizeof(lVectors))) {
        return false;
    }

    if ((lVectors[0] & UPDATE_STACK_POINTER_MASK) != UPDATE_STACK_POINTER_BASE) {
        return false;
    }

    if ((lVectors[1] & 0x1U) == 0U) {
        return false;
    }

    lResetHandler = lVectors[1] & 0xFFFFFFFEUL;
    return (lResetHandler >= lCfg.startAddress) && (lResetHandler < (lCfg.startAddress + lCfg.size));
}

static uint32_t updateGetCopyLimit(void)
{
    if ((gUpdateContext.processChunkSize == 0U) || (gUpdateContext.processChunkSize > sizeof(gUpdateIoBuffer))) {
        return sizeof(gUpdateIoBuffer);
    }

    return gUpdateContext.processChunkSize;
}

static void updateHandleFailure(eUpdateError error)
{
    updateMarkError(error);

    if (updateShouldRollback()) {
        gUpdateStatus.isRollbackActive = true;
        updateSetState(E_UPDATE_STATE_ROLLBACK_ERASE_TARGET,
                       gUpdateContext.backupHeader.imageSize,
                       gUpdateContext.backupHeader.imageCrc32);
        return;
    }

    gUpdateContext.bootRecord.requestFlag = (uint32_t)E_UPDATE_REQUEST_FAILED;
    gUpdateStatus.requestFlag = E_UPDATE_REQUEST_FAILED;
    gUpdateStatus.isUpdateRequested = false;
    (void)updateStoreBootRecord(&gUpdateContext.bootRecord, &gUpdateContext.bootRecordMetaSequence);
    updateSetState(E_UPDATE_STATE_ERROR, 0U, 0U);
}

static void updateHandleCheckRequest(void)
{
    if (!updateLoadBootRecord(&gUpdateContext.bootRecord, &gUpdateContext.bootRecordMetaSequence)) {
        updateHandleFailure(E_UPDATE_ERROR_META_READ_FAILED);
        return;
    }

    gUpdateStatus.requestFlag = (eUpdateRequestFlag)gUpdateContext.bootRecord.requestFlag;
    gUpdateStatus.targetRegion = gUpdateContext.bootRecord.targetRegion;
    gUpdateStatus.isUpdateRequested = (gUpdateStatus.requestFlag == E_UPDATE_REQUEST_PROGRAM_APP) ||
                                      (gUpdateStatus.requestFlag == E_UPDATE_REQUEST_PROGRAM_BOOT) ||
                                      (gUpdateStatus.requestFlag == E_UPDATE_REQUEST_BACKUP_DONE) ||
                                      (gUpdateStatus.requestFlag == E_UPDATE_REQUEST_PROGRAM_DONE);

    switch (gUpdateStatus.requestFlag) {
        case E_UPDATE_REQUEST_IDLE:
            updateSetState(E_UPDATE_STATE_IDLE, 0U, 0U);
            break;
        case E_UPDATE_REQUEST_PROGRAM_APP:
            updateSetState(E_UPDATE_STATE_VALIDATE_STAGING,
                           gUpdateContext.bootRecord.imageSize,
                           gUpdateContext.bootRecord.stagingCrc32);
            break;
        case E_UPDATE_REQUEST_PROGRAM_BOOT:
            updateHandleFailure(E_UPDATE_ERROR_REQUEST_INVALID);
            break;
        case E_UPDATE_REQUEST_BACKUP_DONE:
            updateSetState(E_UPDATE_STATE_ERASE_TARGET,
                           gUpdateContext.cfg.regions[E_UPDATE_REGION_RUN_APP].size,
                           gUpdateContext.bootRecord.stagingCrc32);
            break;
        case E_UPDATE_REQUEST_PROGRAM_DONE:
            updateSetState(E_UPDATE_STATE_VERIFY_TARGET,
                           gUpdateContext.bootRecord.imageSize,
                           gUpdateContext.bootRecord.stagingCrc32);
            break;
        case E_UPDATE_REQUEST_RUN_APP:
            updateSetState(E_UPDATE_STATE_JUMP_TARGET, 0U, 0U);
            break;
        case E_UPDATE_REQUEST_FAILED:
        default:
            updateSetState(E_UPDATE_STATE_ERROR, 0U, 0U);
            break;
    }
}

static void updateHandleValidateStaging(void)
{
    uint32_t lChunkSize;
    uint32_t lRemaining;

    if (gUpdateStatus.currentOffset == 0U) {
        if (!updateLoadImageHeader(E_UPDATE_REGION_STAGING_APP_HEADER,
                                   &gUpdateContext.stagingHeader,
                                   &gUpdateContext.stagingHeaderMetaSequence,
                                   E_UPDATE_IMAGE_TYPE_APP) ||
            !updateIsHeaderValid(&gUpdateContext.stagingHeader,
                                 gUpdateContext.cfg.regions[E_UPDATE_REGION_STAGING_APP].size,
                                 true)) {
            updateHandleFailure(E_UPDATE_ERROR_STAGING_INVALID);
            return;
        }

        gUpdateStatus.totalSize = gUpdateContext.stagingHeader.imageSize;
        gUpdateContext.verifyExpectedCrc32 = gUpdateContext.stagingHeader.imageCrc32;
        gUpdateStatus.activeCrc32 = UPDATE_CRC32_INIT_VALUE;
    }

    lRemaining = gUpdateStatus.totalSize - gUpdateStatus.currentOffset;
    lChunkSize = updateGetCopyLimit();
    if (lChunkSize > lRemaining) {
        lChunkSize = lRemaining;
    }

    if (!updateStorageRead(E_UPDATE_REGION_STAGING_APP, gUpdateStatus.currentOffset, gUpdateIoBuffer, lChunkSize)) {
        updateHandleFailure(E_UPDATE_ERROR_META_READ_FAILED);
        return;
    }

    gUpdateStatus.activeCrc32 = updateCrc32Update(gUpdateStatus.activeCrc32, gUpdateIoBuffer, lChunkSize);
    gUpdateStatus.currentOffset += lChunkSize;
    updateLogProgress("validate staging");

    if (gUpdateStatus.currentOffset < gUpdateStatus.totalSize) {
        updatePortFeedWatchdog();
        return;
    }

    if (updateCrc32Finalize(gUpdateStatus.activeCrc32) != gUpdateContext.stagingHeader.imageCrc32) {
        updateHandleFailure(E_UPDATE_ERROR_IMAGE_CRC_MISMATCH);
        return;
    }

    if (updateShouldUseBackup()) {
        updateSetState(E_UPDATE_STATE_PREPARE_BACKUP,
                       gUpdateContext.cfg.regions[E_UPDATE_REGION_RUN_APP].size,
                       0U);
    } else {
        updateSetState(E_UPDATE_STATE_ERASE_TARGET,
                       gUpdateContext.cfg.regions[E_UPDATE_REGION_RUN_APP].size,
                       gUpdateContext.stagingHeader.imageCrc32);
    }
}

static void updateHandlePrepareBackup(void)
{
    stUpdateImageHeader lBackupHeader;

    if (!updateShouldUseBackup()) {
        updateSetState(E_UPDATE_STATE_ERASE_TARGET,
                       gUpdateContext.cfg.regions[E_UPDATE_REGION_RUN_APP].size,
                       gUpdateContext.stagingHeader.imageCrc32);
        return;
    }

    if (!updateStorageErase(E_UPDATE_REGION_BACKUP_APP,
                            0U,
                            gUpdateContext.cfg.regions[E_UPDATE_REGION_BACKUP_APP].size)) {
        updateHandleFailure(E_UPDATE_ERROR_ERASE_FAILED);
        return;
    }

    updatePortFeedWatchdog();

    updateBuildDefaultImageHeader(&lBackupHeader, E_UPDATE_IMAGE_TYPE_APP);
    lBackupHeader.imageVersion = 0U;
    lBackupHeader.imageSize = gUpdateContext.cfg.regions[E_UPDATE_REGION_RUN_APP].size;
    lBackupHeader.imageCrc32 = 0U;
    lBackupHeader.writeOffset = 0U;
    lBackupHeader.imageState = (uint32_t)E_UPDATE_IMAGE_STATE_RECEIVING;
    if (!updateStoreImageHeader(E_UPDATE_REGION_BACKUP_APP_HEADER,
                                &lBackupHeader,
                                &gUpdateContext.backupHeaderMetaSequence)) {
        updateHandleFailure(E_UPDATE_ERROR_META_WRITE_FAILED);
        return;
    }

    gUpdateContext.backupHeader = lBackupHeader;
    updateSetState(E_UPDATE_STATE_BACKUP_RUN_APP,
                   gUpdateContext.cfg.regions[E_UPDATE_REGION_RUN_APP].size,
                   0U);
}

static void updateHandleBackupRunApp(void)
{
    uint32_t lChunkSize;
    uint32_t lRemaining;

    lRemaining = gUpdateStatus.totalSize - gUpdateStatus.currentOffset;
    lChunkSize = updateGetCopyLimit();
    if (lChunkSize > lRemaining) {
        lChunkSize = lRemaining;
    }

    if (!updateStorageRead(E_UPDATE_REGION_RUN_APP, gUpdateStatus.currentOffset, gUpdateIoBuffer, lChunkSize) ||
        !updateStorageWrite(E_UPDATE_REGION_BACKUP_APP, gUpdateStatus.currentOffset, gUpdateIoBuffer, lChunkSize)) {
        updateHandleFailure(E_UPDATE_ERROR_PROGRAM_FAILED);
        return;
    }

    gUpdateStatus.activeCrc32 = updateCrc32Update(gUpdateStatus.activeCrc32, gUpdateIoBuffer, lChunkSize);
    gUpdateStatus.currentOffset += lChunkSize;
    updateLogProgress("backup run app");
    updatePortFeedWatchdog();

    if (gUpdateStatus.currentOffset < gUpdateStatus.totalSize) {
        return;
    }

    gUpdateContext.backupHeader.imageSize = gUpdateStatus.totalSize;
    gUpdateContext.backupHeader.imageCrc32 = updateCrc32Finalize(gUpdateStatus.activeCrc32);
    gUpdateContext.backupHeader.writeOffset = gUpdateStatus.totalSize;
    gUpdateContext.backupHeader.imageState = (uint32_t)E_UPDATE_IMAGE_STATE_READY;
    if (!updateStoreImageHeader(E_UPDATE_REGION_BACKUP_APP_HEADER,
                                &gUpdateContext.backupHeader,
                                &gUpdateContext.backupHeaderMetaSequence)) {
        updateHandleFailure(E_UPDATE_ERROR_META_WRITE_FAILED);
        return;
    }

    gUpdateContext.bootRecord.backupCrc32 = gUpdateContext.backupHeader.imageCrc32;
    gUpdateContext.bootRecord.requestFlag = (uint32_t)E_UPDATE_REQUEST_BACKUP_DONE;
    if (!updateStoreBootRecord(&gUpdateContext.bootRecord, &gUpdateContext.bootRecordMetaSequence)) {
        updateHandleFailure(E_UPDATE_ERROR_META_WRITE_FAILED);
        return;
    }

    gUpdateStatus.requestFlag = E_UPDATE_REQUEST_BACKUP_DONE;
    updateSetState(E_UPDATE_STATE_VERIFY_BACKUP,
                   gUpdateContext.backupHeader.imageSize,
                   gUpdateContext.backupHeader.imageCrc32);
}

static void updateHandleVerifyBackup(void)
{
    uint32_t lChunkSize;
    uint32_t lRemaining;

    lRemaining = gUpdateStatus.totalSize - gUpdateStatus.currentOffset;
    lChunkSize = updateGetCopyLimit();
    if (lChunkSize > lRemaining) {
        lChunkSize = lRemaining;
    }

    if (!updateStorageRead(E_UPDATE_REGION_BACKUP_APP, gUpdateStatus.currentOffset, gUpdateIoBuffer, lChunkSize)) {
        updateHandleFailure(E_UPDATE_ERROR_VERIFY_FAILED);
        return;
    }

    gUpdateStatus.activeCrc32 = updateCrc32Update(gUpdateStatus.activeCrc32, gUpdateIoBuffer, lChunkSize);
    gUpdateStatus.currentOffset += lChunkSize;
    updateLogProgress("verify backup");
    updatePortFeedWatchdog();

    if (gUpdateStatus.currentOffset < gUpdateStatus.totalSize) {
        return;
    }

    if (updateCrc32Finalize(gUpdateStatus.activeCrc32) != gUpdateContext.backupHeader.imageCrc32) {
        updateHandleFailure(E_UPDATE_ERROR_BACKUP_INVALID);
        return;
    }

    updateSetState(E_UPDATE_STATE_ERASE_TARGET,
                   gUpdateContext.stagingHeader.imageSize,
                   gUpdateContext.stagingHeader.imageCrc32);
}

static void updateHandleEraseTarget(void)
{
    stUpdateRegionCfg lCfg;
    uint32_t lEraseChunk;
    uint32_t lRemaining;

    if (!updateGetRegionCfg(E_UPDATE_REGION_RUN_APP, &lCfg)) {
        updateHandleFailure(E_UPDATE_ERROR_INVALID_CFG);
        return;
    }

    lRemaining = lCfg.size - gUpdateStatus.currentOffset;
    lEraseChunk = lCfg.eraseUnit;
    if (lEraseChunk > lRemaining) {
        lEraseChunk = lRemaining;
    }

    if (!updateStorageErase(E_UPDATE_REGION_RUN_APP, gUpdateStatus.currentOffset, lEraseChunk)) {
        updateHandleFailure(E_UPDATE_ERROR_ERASE_FAILED);
        return;
    }

    gUpdateStatus.currentOffset += lEraseChunk;
    gUpdateContext.targetWasErased = true;
    updateLogProgress("erase target");
    updatePortFeedWatchdog();

    if (gUpdateStatus.currentOffset < lCfg.size) {
        return;
    }

    updateSetState(E_UPDATE_STATE_PROGRAM_TARGET,
                   gUpdateContext.stagingHeader.imageSize,
                   gUpdateContext.stagingHeader.imageCrc32);
}

static void updateHandleProgramTarget(void)
{
    uint32_t lChunkSize;
    uint32_t lRemaining;

    lRemaining = gUpdateStatus.totalSize - gUpdateStatus.currentOffset;
    lChunkSize = updateGetCopyLimit();
    if (lChunkSize > lRemaining) {
        lChunkSize = lRemaining;
    }

    if (!updateStorageRead(E_UPDATE_REGION_STAGING_APP, gUpdateStatus.currentOffset, gUpdateIoBuffer, lChunkSize) ||
        !updateStorageWrite(E_UPDATE_REGION_RUN_APP, gUpdateStatus.currentOffset, gUpdateIoBuffer, lChunkSize)) {
        updateHandleFailure(E_UPDATE_ERROR_PROGRAM_FAILED);
        return;
    }

    gUpdateStatus.currentOffset += lChunkSize;
    updateLogProgress("program target");
    updatePortFeedWatchdog();

    if (gUpdateStatus.currentOffset < gUpdateStatus.totalSize) {
        return;
    }

    gUpdateContext.bootRecord.requestFlag = (uint32_t)E_UPDATE_REQUEST_PROGRAM_DONE;
    if (!updateStoreBootRecord(&gUpdateContext.bootRecord, &gUpdateContext.bootRecordMetaSequence)) {
        updateHandleFailure(E_UPDATE_ERROR_META_WRITE_FAILED);
        return;
    }

    gUpdateStatus.requestFlag = E_UPDATE_REQUEST_PROGRAM_DONE;
    updateSetState(E_UPDATE_STATE_VERIFY_TARGET,
                   gUpdateContext.stagingHeader.imageSize,
                   gUpdateContext.stagingHeader.imageCrc32);
}

static void updateHandleVerifyTarget(void)
{
    uint32_t lChunkSize;
    uint32_t lRemaining;

    lRemaining = gUpdateStatus.totalSize - gUpdateStatus.currentOffset;
    lChunkSize = updateGetCopyLimit();
    if (lChunkSize > lRemaining) {
        lChunkSize = lRemaining;
    }

    if (!updateStorageRead(E_UPDATE_REGION_RUN_APP, gUpdateStatus.currentOffset, gUpdateIoBuffer, lChunkSize)) {
        updateHandleFailure(E_UPDATE_ERROR_VERIFY_FAILED);
        return;
    }

    gUpdateStatus.activeCrc32 = updateCrc32Update(gUpdateStatus.activeCrc32, gUpdateIoBuffer, lChunkSize);
    gUpdateStatus.currentOffset += lChunkSize;
    updateLogProgress("verify target");
    updatePortFeedWatchdog();

    if (gUpdateStatus.currentOffset < gUpdateStatus.totalSize) {
        return;
    }

    if (updateCrc32Finalize(gUpdateStatus.activeCrc32) != gUpdateContext.stagingHeader.imageCrc32) {
        updateHandleFailure(E_UPDATE_ERROR_VERIFY_FAILED);
        return;
    }

    updateSetState(E_UPDATE_STATE_COMMIT_RESULT, 0U, 0U);
}

static void updateHandleCommitResult(void)
{
    gUpdateContext.bootRecord.requestFlag = (uint32_t)E_UPDATE_REQUEST_RUN_APP;
    gUpdateContext.bootRecord.lastError = (uint32_t)E_UPDATE_ERROR_NONE;
    gUpdateContext.bootRecord.targetRegion = (uint32_t)E_UPDATE_REGION_RUN_APP;
    gUpdateStatus.lastError = E_UPDATE_ERROR_NONE;

    gUpdateContext.stagingHeader.imageState = (uint32_t)E_UPDATE_IMAGE_STATE_INVALID;
    gUpdateContext.stagingHeader.headerCrc32 = updateCalcHeaderCrc(&gUpdateContext.stagingHeader);

    if (!updateStoreImageHeader(E_UPDATE_REGION_STAGING_APP_HEADER,
                                &gUpdateContext.stagingHeader,
                                &gUpdateContext.stagingHeaderMetaSequence) ||
        !updateStoreBootRecord(&gUpdateContext.bootRecord, &gUpdateContext.bootRecordMetaSequence)) {
        updateHandleFailure(E_UPDATE_ERROR_META_WRITE_FAILED);
        return;
    }

    gUpdateStatus.requestFlag = E_UPDATE_REQUEST_RUN_APP;
    gUpdateStatus.isUpdateRequested = false;
    gUpdateStatus.isRollbackActive = false;
    updateSetState(E_UPDATE_STATE_JUMP_TARGET, 0U, 0U);
}

static void updateHandleRollbackEraseTarget(void)
{
    stUpdateRegionCfg lCfg;
    uint32_t lEraseChunk;
    uint32_t lRemaining;

    if (!updateGetRegionCfg(E_UPDATE_REGION_RUN_APP, &lCfg)) {
        updateHandleFailure(E_UPDATE_ERROR_INVALID_CFG);
        return;
    }

    lRemaining = lCfg.size - gUpdateStatus.currentOffset;
    lEraseChunk = lCfg.eraseUnit;
    if (lEraseChunk > lRemaining) {
        lEraseChunk = lRemaining;
    }

    if (!updateStorageErase(E_UPDATE_REGION_RUN_APP, gUpdateStatus.currentOffset, lEraseChunk)) {
        updateHandleFailure(E_UPDATE_ERROR_ROLLBACK_FAILED);
        return;
    }

    gUpdateStatus.currentOffset += lEraseChunk;
    updateLogProgress("rollback erase target");
    updatePortFeedWatchdog();

    if (gUpdateStatus.currentOffset < lCfg.size) {
        return;
    }

    updateSetState(E_UPDATE_STATE_ROLLBACK_PROGRAM_BACKUP,
                   gUpdateContext.backupHeader.imageSize,
                   gUpdateContext.backupHeader.imageCrc32);
}

static void updateHandleRollbackProgramBackup(void)
{
    uint32_t lChunkSize;
    uint32_t lRemaining;

    lRemaining = gUpdateStatus.totalSize - gUpdateStatus.currentOffset;
    lChunkSize = updateGetCopyLimit();
    if (lChunkSize > lRemaining) {
        lChunkSize = lRemaining;
    }

    if (!updateStorageRead(E_UPDATE_REGION_BACKUP_APP, gUpdateStatus.currentOffset, gUpdateIoBuffer, lChunkSize) ||
        !updateStorageWrite(E_UPDATE_REGION_RUN_APP, gUpdateStatus.currentOffset, gUpdateIoBuffer, lChunkSize)) {
        updateHandleFailure(E_UPDATE_ERROR_ROLLBACK_FAILED);
        return;
    }

    gUpdateStatus.currentOffset += lChunkSize;
    updateLogProgress("rollback program backup");
    updatePortFeedWatchdog();

    if (gUpdateStatus.currentOffset < gUpdateStatus.totalSize) {
        return;
    }

    updateSetState(E_UPDATE_STATE_VERIFY_ROLLBACK,
                   gUpdateContext.backupHeader.imageSize,
                   gUpdateContext.backupHeader.imageCrc32);
}

static void updateHandleVerifyRollback(void)
{
    uint32_t lChunkSize;
    uint32_t lRemaining;

    lRemaining = gUpdateStatus.totalSize - gUpdateStatus.currentOffset;
    lChunkSize = updateGetCopyLimit();
    if (lChunkSize > lRemaining) {
        lChunkSize = lRemaining;
    }

    if (!updateStorageRead(E_UPDATE_REGION_RUN_APP, gUpdateStatus.currentOffset, gUpdateIoBuffer, lChunkSize)) {
        updateHandleFailure(E_UPDATE_ERROR_ROLLBACK_FAILED);
        return;
    }

    gUpdateStatus.activeCrc32 = updateCrc32Update(gUpdateStatus.activeCrc32, gUpdateIoBuffer, lChunkSize);
    gUpdateStatus.currentOffset += lChunkSize;
    updateLogProgress("verify rollback");
    updatePortFeedWatchdog();

    if (gUpdateStatus.currentOffset < gUpdateStatus.totalSize) {
        return;
    }

    if (updateCrc32Finalize(gUpdateStatus.activeCrc32) != gUpdateContext.backupHeader.imageCrc32) {
        updateHandleFailure(E_UPDATE_ERROR_ROLLBACK_FAILED);
        return;
    }

    gUpdateContext.bootRecord.requestFlag = (uint32_t)E_UPDATE_REQUEST_FAILED;
    if (!updateStoreBootRecord(&gUpdateContext.bootRecord, &gUpdateContext.bootRecordMetaSequence)) {
        updateHandleFailure(E_UPDATE_ERROR_META_WRITE_FAILED);
        return;
    }

    gUpdateStatus.requestFlag = E_UPDATE_REQUEST_FAILED;
    gUpdateStatus.isRollbackActive = false;
    updateSetState(E_UPDATE_STATE_JUMP_TARGET, 0U, 0U);
}

void updateReset(void)
{
    (void)memset(&gUpdateContext, 0, sizeof(gUpdateContext));
    (void)memset(&gUpdateStatus, 0, sizeof(gUpdateStatus));
    gUpdateStatus.state = E_UPDATE_STATE_UNINIT;
    gUpdateStatus.lastError = E_UPDATE_ERROR_NONE;
    gUpdateStatus.requestFlag = E_UPDATE_REQUEST_IDLE;
}

bool updateInit(void)
{
    updateReset();

    if (!updatePortLoadDefaultCfg(&gUpdateContext.cfg) || !updateValidateCfg() || !updateInitStorageDevices()) {
        gUpdateStatus.lastError = E_UPDATE_ERROR_INVALID_CFG;
        gUpdateStatus.state = E_UPDATE_STATE_ERROR;
        return false;
    }

    gUpdateContext.processChunkSize = gUpdateContext.cfg.processChunkSize;
    if (gUpdateContext.processChunkSize == 0U) {
        gUpdateContext.processChunkSize = UPDATE_PROCESS_CHUNK_SIZE;
    }

    gUpdateStatus.isReady = true;
    gUpdateContext.isInitialized = true;
    updateSetState(E_UPDATE_STATE_CHECK_REQUEST, 0U, 0U);
    return true;
}

void updateProcess(uint32_t nowTickMs)
{
    if (!gUpdateContext.isInitialized) {
        return;
    }

    gUpdateStatus.lastTickMs = updateResolveNowTick(nowTickMs);
    switch (gUpdateStatus.state) {
        case E_UPDATE_STATE_CHECK_REQUEST:
            updateHandleCheckRequest();
            break;
        case E_UPDATE_STATE_VALIDATE_STAGING:
            updateHandleValidateStaging();
            break;
        case E_UPDATE_STATE_PREPARE_BACKUP:
            updateHandlePrepareBackup();
            break;
        case E_UPDATE_STATE_BACKUP_RUN_APP:
            updateHandleBackupRunApp();
            break;
        case E_UPDATE_STATE_VERIFY_BACKUP:
            updateHandleVerifyBackup();
            break;
        case E_UPDATE_STATE_ERASE_TARGET:
            updateHandleEraseTarget();
            break;
        case E_UPDATE_STATE_PROGRAM_TARGET:
            updateHandleProgramTarget();
            break;
        case E_UPDATE_STATE_VERIFY_TARGET:
            updateHandleVerifyTarget();
            break;
        case E_UPDATE_STATE_COMMIT_RESULT:
            updateHandleCommitResult();
            break;
        case E_UPDATE_STATE_ROLLBACK_ERASE_TARGET:
            updateHandleRollbackEraseTarget();
            break;
        case E_UPDATE_STATE_ROLLBACK_PROGRAM_BACKUP:
            updateHandleRollbackProgramBackup();
            break;
        case E_UPDATE_STATE_VERIFY_ROLLBACK:
            updateHandleVerifyRollback();
            break;
        case E_UPDATE_STATE_IDLE:
        case E_UPDATE_STATE_JUMP_TARGET:
        case E_UPDATE_STATE_ERROR:
        case E_UPDATE_STATE_UNINIT:
        default:
            break;
    }
}

const stUpdateStatus *updateGetStatus(void)
{
    return &gUpdateStatus;
}

bool updateRequestProgramRegion(uint32_t targetRegion)
{
    stUpdateBootRecord lRecord;
    stUpdateImageHeader lHeader;
    uint32_t lRecordSequence = 0U;
    uint32_t lHeaderSequence = 0U;

    if (!gUpdateContext.isInitialized || (targetRegion >= E_UPDATE_REGION_MAX) ||
        (targetRegion != E_UPDATE_REGION_RUN_APP)) {
        return false;
    }

    if (!updateLoadBootRecord(&lRecord, &lRecordSequence) ||
        !updateLoadImageHeader(E_UPDATE_REGION_STAGING_APP_HEADER,
                               &lHeader,
                               &lHeaderSequence,
                               E_UPDATE_IMAGE_TYPE_APP) ||
        !updateIsHeaderValid(&lHeader,
                             gUpdateContext.cfg.regions[E_UPDATE_REGION_STAGING_APP].size,
                             true)) {
        return false;
    }

    lRecord.magic = UPDATE_BOOT_RECORD_MAGIC;
    lRecord.requestFlag = (uint32_t)E_UPDATE_REQUEST_PROGRAM_APP;
    lRecord.lastError = (uint32_t)E_UPDATE_ERROR_NONE;
    lRecord.targetRegion = targetRegion;
    lRecord.stagingCrc32 = lHeader.imageCrc32;
    lRecord.backupCrc32 = 0U;
    lRecord.imageSize = lHeader.imageSize;

    if (!updateStoreBootRecord(&lRecord, &lRecordSequence)) {
        return false;
    }

    gUpdateContext.bootRecordMetaSequence = lRecordSequence;
    gUpdateStatus.requestFlag = (eUpdateRequestFlag)lRecord.requestFlag;
    gUpdateStatus.targetRegion = targetRegion;
    gUpdateStatus.isUpdateRequested = true;
    updateSetState(E_UPDATE_STATE_CHECK_REQUEST, 0U, 0U);
    return true;
}

bool updateReadBootRecord(stUpdateBootRecord *record)
{
    uint32_t lSequence = 0U;

    if (record == NULL) {
        return false;
    }

    return updateLoadBootRecord(record, &lSequence);
}

bool updateJumpToTargetIfValid(void)
{
    uint32_t lTargetRegion;

    if (!gUpdateContext.isInitialized) {
        return false;
    }

    lTargetRegion = (gUpdateContext.bootRecord.targetRegion < E_UPDATE_REGION_MAX) ?
                    gUpdateContext.bootRecord.targetRegion :
                    (uint32_t)E_UPDATE_REGION_RUN_APP;
    if (!updateValidateExecutableRegion((uint8_t)lTargetRegion)) {
        gUpdateStatus.lastError = E_UPDATE_ERROR_JUMP_UNAVAILABLE;
        return false;
    }

    return updatePortJumpToRegion((uint8_t)lTargetRegion);
}

/**************************End of file********************************/