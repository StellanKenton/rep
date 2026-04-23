/************************************************************************************
* @file     : sdcard.h
* @brief    : SD card block storage module public interface.
* @details  : This module keeps reusable SD card lifecycle and range validation in
*             the core layer and relies on project-side assembly hooks to bind the
*             actual SDIO/SPI controller implementation.
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef SDCARD_H
#define SDCARD_H

#include <stdbool.h>
#include <stdint.h>

#include "../../rep.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eSdcardDevMap {
    SDCARD_DEV0 = 0,
    SDCARD_DEV_MAX,
} eSdcardMapType;

typedef eDrvStatus eSdcardStatus;

#define SDCARD_STATUS_OK                    DRV_STATUS_OK
#define SDCARD_STATUS_INVALID_PARAM         DRV_STATUS_INVALID_PARAM
#define SDCARD_STATUS_NOT_READY             DRV_STATUS_NOT_READY
#define SDCARD_STATUS_BUSY                  DRV_STATUS_BUSY
#define SDCARD_STATUS_TIMEOUT               DRV_STATUS_TIMEOUT
#define SDCARD_STATUS_NACK                  DRV_STATUS_NACK
#define SDCARD_STATUS_UNSUPPORTED           DRV_STATUS_UNSUPPORTED
#define SDCARD_STATUS_DEVICE_ID_MISMATCH    DRV_STATUS_ID_NOTMATCH
#define SDCARD_STATUS_ERROR                 DRV_STATUS_ERROR
#define SDCARD_STATUS_NO_MEDIUM             ((eSdcardStatus)(DRV_STATUS_ERROR + 1))
#define SDCARD_STATUS_WRITE_PROTECTED       ((eSdcardStatus)(DRV_STATUS_ERROR + 2))
#define SDCARD_STATUS_OUT_OF_RANGE          ((eSdcardStatus)(DRV_STATUS_ERROR + 3))

#ifndef SDCARD_DEFAULT_BLOCK_SIZE
#define SDCARD_DEFAULT_BLOCK_SIZE           512UL
#endif

#ifndef SDCARD_DEFAULT_INIT_TIMEOUT_MS
#define SDCARD_DEFAULT_INIT_TIMEOUT_MS      1000UL
#endif

typedef struct stSdcardCfg {
    uint8_t linkId;
    uint32_t initTimeoutMs;
} stSdcardCfg;

typedef struct stSdcardInfo {
    uint32_t blockSize;
    uint32_t blockCount;
    uint32_t eraseBlockSize;
    uint64_t capacityBytes;
    bool isPresent;
    bool isWriteProtected;
    bool isHighCapacity;
} stSdcardInfo;

typedef struct stSdcardTrimRange {
    uint32_t startBlock;
    uint32_t blockCount;
} stSdcardTrimRange;

typedef struct stSdcardDevice {
    stSdcardCfg cfg;
    stSdcardInfo info;
    bool isReady;
} stSdcardDevice;

eSdcardStatus sdcardGetDefCfg(eSdcardMapType device, stSdcardCfg *cfg);
eSdcardStatus sdcardGetCfg(eSdcardMapType device, stSdcardCfg *cfg);
eSdcardStatus sdcardSetCfg(eSdcardMapType device, const stSdcardCfg *cfg);
eSdcardStatus sdcardInit(eSdcardMapType device);
bool sdcardIsReady(eSdcardMapType device);
const stSdcardInfo *sdcardGetInfo(eSdcardMapType device);
eSdcardStatus sdcardGetStatus(eSdcardMapType device, bool *isPresent, bool *isWriteProtected);
eSdcardStatus sdcardReadBlocks(eSdcardMapType device, uint32_t startBlock, uint8_t *buffer, uint32_t blockCount);
eSdcardStatus sdcardWriteBlocks(eSdcardMapType device, uint32_t startBlock, const uint8_t *buffer, uint32_t blockCount);
eSdcardStatus sdcardSync(eSdcardMapType device);
eSdcardStatus sdcardTrim(eSdcardMapType device, uint32_t startBlock, uint32_t blockCount);

#ifdef __cplusplus
}
#endif

#endif  // SDCARD_H
/**************************End of file********************************/
