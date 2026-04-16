/************************************************************************************
* @file     : update.h
* @brief    : Reusable firmware update service public interface.
* @details  : Defines the generic update state machine, metadata records, and
*             high-level APIs used by boot-side or manager-side update flows.
* @author   : GitHub Copilot
* @date     : 2026-04-16
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REP_SERVICE_UPDATE_H
#define REP_SERVICE_UPDATE_H

#include <stdbool.h>
#include <stdint.h>

#include "rep_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef UPDATE_LOG_SUPPORT
#define UPDATE_LOG_SUPPORT                  1
#endif

#ifndef UPDATE_PROCESS_CHUNK_SIZE
#define UPDATE_PROCESS_CHUNK_SIZE           1024U
#endif

#ifndef UPDATE_META_COMMIT_MARKER
#define UPDATE_META_COMMIT_MARKER           0x00000000UL
#endif

#ifndef UPDATE_META_RECORD_MAGIC
#define UPDATE_META_RECORD_MAGIC            0x55504D54UL
#endif

#define UPDATE_IMAGE_MAGIC                  0x55504454UL
#define UPDATE_BOOT_RECORD_MAGIC            0x4254464CUL
#define UPDATE_HEADER_VERSION               0x00000001UL

typedef enum eUpdateRegion {
    E_UPDATE_REGION_BOOT_RECORD = 0,
    E_UPDATE_REGION_RUN_APP,
    E_UPDATE_REGION_STAGING_APP,
    E_UPDATE_REGION_STAGING_APP_HEADER,
    E_UPDATE_REGION_BACKUP_APP,
    E_UPDATE_REGION_BACKUP_APP_HEADER,
    E_UPDATE_REGION_BOOT_IMAGE,
    E_UPDATE_REGION_FACTORY_APP,
    E_UPDATE_REGION_MAX,
} eUpdateRegion;

typedef enum eUpdateState {
    E_UPDATE_STATE_UNINIT = 0,
    E_UPDATE_STATE_IDLE,
    E_UPDATE_STATE_CHECK_REQUEST,
    E_UPDATE_STATE_VALIDATE_STAGING,
    E_UPDATE_STATE_PREPARE_BACKUP,
    E_UPDATE_STATE_BACKUP_RUN_APP,
    E_UPDATE_STATE_VERIFY_BACKUP,
    E_UPDATE_STATE_ERASE_TARGET,
    E_UPDATE_STATE_PROGRAM_TARGET,
    E_UPDATE_STATE_VERIFY_TARGET,
    E_UPDATE_STATE_COMMIT_RESULT,
    E_UPDATE_STATE_ROLLBACK_ERASE_TARGET,
    E_UPDATE_STATE_ROLLBACK_PROGRAM_BACKUP,
    E_UPDATE_STATE_VERIFY_ROLLBACK,
    E_UPDATE_STATE_JUMP_TARGET,
    E_UPDATE_STATE_ERROR,
    E_UPDATE_STATE_MAX,
} eUpdateState;

typedef enum eUpdateRequestFlag {
    E_UPDATE_REQUEST_IDLE = 0,
    E_UPDATE_REQUEST_PROGRAM_APP,
    E_UPDATE_REQUEST_PROGRAM_BOOT,
    E_UPDATE_REQUEST_BACKUP_DONE,
    E_UPDATE_REQUEST_PROGRAM_DONE,
    E_UPDATE_REQUEST_RUN_APP,
    E_UPDATE_REQUEST_FAILED,
} eUpdateRequestFlag;

typedef enum eUpdateImageType {
    E_UPDATE_IMAGE_TYPE_APP = 0,
    E_UPDATE_IMAGE_TYPE_BOOT,
    E_UPDATE_IMAGE_TYPE_FACTORY,
} eUpdateImageType;

typedef enum eUpdateImageState {
    E_UPDATE_IMAGE_STATE_EMPTY = 0,
    E_UPDATE_IMAGE_STATE_RECEIVING,
    E_UPDATE_IMAGE_STATE_READY,
    E_UPDATE_IMAGE_STATE_INVALID,
} eUpdateImageState;

typedef enum eUpdateError {
    E_UPDATE_ERROR_NONE = 0,
    E_UPDATE_ERROR_INVALID_PARAM,
    E_UPDATE_ERROR_INVALID_CFG,
    E_UPDATE_ERROR_STORAGE_NOT_READY,
    E_UPDATE_ERROR_META_READ_FAILED,
    E_UPDATE_ERROR_META_WRITE_FAILED,
    E_UPDATE_ERROR_REQUEST_INVALID,
    E_UPDATE_ERROR_STAGING_INVALID,
    E_UPDATE_ERROR_BACKUP_INVALID,
    E_UPDATE_ERROR_IMAGE_CRC_MISMATCH,
    E_UPDATE_ERROR_ERASE_FAILED,
    E_UPDATE_ERROR_PROGRAM_FAILED,
    E_UPDATE_ERROR_VERIFY_FAILED,
    E_UPDATE_ERROR_ROLLBACK_FAILED,
    E_UPDATE_ERROR_JUMP_UNAVAILABLE,
} eUpdateError;

#pragma pack(push, 1)
typedef struct stUpdateImageHeader {
    uint32_t magic;
    uint32_t headerVersion;
    uint32_t imageType;
    uint32_t imageVersion;
    uint32_t imageSize;
    uint32_t imageCrc32;
    uint32_t writeOffset;
    uint32_t imageState;
    uint32_t reserved[7];
    uint32_t headerCrc32;
} stUpdateImageHeader;

typedef struct stUpdateBootRecord {
    uint32_t magic;
    uint32_t requestFlag;
    uint32_t lastError;
    uint32_t targetRegion;
    uint32_t stagingCrc32;
    uint32_t backupCrc32;
    uint32_t imageSize;
    uint32_t sequence;
    uint32_t reserved[7];
    uint32_t recordCrc32;
} stUpdateBootRecord;

typedef struct stUpdateMetaRecord {
    uint32_t recordMagic;
    uint32_t sequence;
    uint32_t payloadLength;
    uint32_t payloadCrc32;
    uint32_t headerCrc32;
    uint8_t payload[sizeof(stUpdateBootRecord)];
    uint32_t commitMarker;
} stUpdateMetaRecord;
#pragma pack(pop)

typedef struct stUpdateStatus {
    eUpdateState state;
    uint32_t lastTickMs;
    uint32_t currentOffset;
    uint32_t totalSize;
    uint32_t activeCrc32;
    uint32_t targetRegion;
    eUpdateRequestFlag requestFlag;
    eUpdateError lastError;
    bool isUpdateRequested;
    bool isRollbackActive;
    bool isReady;
} stUpdateStatus;

void updateReset(void);
bool updateInit(void);
void updateProcess(uint32_t nowTickMs);
const stUpdateStatus *updateGetStatus(void);
bool updateRequestProgramRegion(uint32_t targetRegion);
bool updateReadBootRecord(stUpdateBootRecord *record);
bool updateWriteBootRecord(const stUpdateBootRecord *record);
bool updateJumpToTargetIfValid(void);

#ifdef __cplusplus
}
#endif

#endif  // REP_SERVICE_UPDATE_H
/**************************End of file********************************/
