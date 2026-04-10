/************************************************************************************
* @file     : gd25qxxx.h
* @brief    : GD25Qxxx SPI NOR flash module public interface.
* @details  : This module keeps reusable flash logic in the core layer and
*             relies on the port layer to bind logical device instances to the
*             project drvspi implementation.
***********************************************************************************/
#ifndef GD25QXXX_H
#define GD25QXXX_H

#include <stdbool.h>
#include <stdint.h>

#include "Rep/driver/drvspi/drvspi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eGd25qxxxDevMap {
    GD25Q32_MEM = 0,
    GD25QXXX_DEV_MAX,
} eGd25qxxxMapType;

#define GD25QXXX_MANUFACTURER_ID              0xC8U
#define GD25QXXX_PAGE_SIZE                    256U
#define GD25QXXX_SECTOR_SIZE                  4096UL
#define GD25QXXX_BLOCK64K_SIZE                65536UL
#define GD25QXXX_MAX_TRANSFER_LENGTH          65535UL
#define GD25QXXX_PAGE_PROGRAM_TIMEOUT_MS      10U
#define GD25QXXX_SECTOR_ERASE_TIMEOUT_MS      500U
#define GD25QXXX_BLOCK_ERASE_TIMEOUT_MS       3000U
#define GD25QXXX_CHIP_ERASE_TIMEOUT_MS        120000U
#define GD25QXXX_BUSY_POLL_DELAY_MS           1U

#define GD25QXXX_CMD_WRITE_ENABLE             0x06U
#define GD25QXXX_CMD_READ_STATUS1             0x05U
#define GD25QXXX_CMD_JEDEC_ID                 0x9FU
#define GD25QXXX_CMD_READ_DATA                0x03U
#define GD25QXXX_CMD_READ_DATA_4B             0x13U
#define GD25QXXX_CMD_PAGE_PROGRAM             0x02U
#define GD25QXXX_CMD_PAGE_PROGRAM_4B          0x12U
#define GD25QXXX_CMD_SECTOR_ERASE             0x20U
#define GD25QXXX_CMD_SECTOR_ERASE_4B          0x21U
#define GD25QXXX_CMD_BLOCK_ERASE_64K          0xD8U
#define GD25QXXX_CMD_BLOCK_ERASE_64K_4B       0xDCU
#define GD25QXXX_CMD_CHIP_ERASE               0xC7U

#define GD25QXXX_STATUS1_BUSY_MASK            0x01U
#define GD25QXXX_STATUS1_WEL_MASK             0x02U

typedef eDrvStatus eGd25qxxxStatus;

#define GD25QXXX_STATUS_OK                 DRV_STATUS_OK
#define GD25QXXX_STATUS_INVALID_PARAM      DRV_STATUS_INVALID_PARAM
#define GD25QXXX_STATUS_NOT_READY          DRV_STATUS_NOT_READY
#define GD25QXXX_STATUS_BUSY               DRV_STATUS_BUSY
#define GD25QXXX_STATUS_TIMEOUT            DRV_STATUS_TIMEOUT
#define GD25QXXX_STATUS_NACK               DRV_STATUS_NACK
#define GD25QXXX_STATUS_UNSUPPORTED        DRV_STATUS_UNSUPPORTED
#define GD25QXXX_STATUS_DEVICE_ID_MISMATCH DRV_STATUS_ID_NOTMATCH
#define GD25QXXX_STATUS_ERROR              DRV_STATUS_ERROR
#define GD25QXXX_STATUS_OUT_OF_RANGE       ((eGd25qxxxStatus)(DRV_STATUS_ERROR + 1))

typedef eDrvStatus (*gd25qxxxSpiInitFunc)(uint8_t bus);
typedef eDrvStatus (*gd25qxxxSpiTransferFunc)(uint8_t bus, const uint8_t *writeBuffer, uint16_t writeLength, const uint8_t *secondWriteBuffer, uint16_t secondWriteLength, uint8_t *readBuffer, uint16_t readLength, uint8_t readFillData);

typedef struct stGd25qxxxSpiInterface {
    gd25qxxxSpiInitFunc init;
    gd25qxxxSpiTransferFunc transfer;
} stGd25qxxxSpiInterface;

typedef struct stGd25qxxxCfg {
    uint8_t linkId;
} stGd25qxxxCfg;

typedef struct stGd25qxxxInfo {
    uint8_t manufacturerId;
    uint8_t memoryType;
    uint8_t capacityId;
    uint8_t addressWidth;
    uint16_t pageSizeBytes;
    uint32_t totalSizeBytes;
    uint32_t sectorSizeBytes;
    uint32_t blockSizeBytes;
} stGd25qxxxInfo;

typedef struct stGd25qxxxDevice {
    stGd25qxxxCfg cfg;
    stGd25qxxxInfo info;
    bool isReady;
} stGd25qxxxDevice;

eGd25qxxxStatus gd25qxxxGetDefCfg(eGd25qxxxMapType device, stGd25qxxxCfg *cfg);
eGd25qxxxStatus gd25qxxxGetCfg(eGd25qxxxMapType device, stGd25qxxxCfg *cfg);
eGd25qxxxStatus gd25qxxxSetCfg(eGd25qxxxMapType device, const stGd25qxxxCfg *cfg);
eGd25qxxxStatus gd25qxxxInit(eGd25qxxxMapType device);
bool gd25qxxxIsReady(eGd25qxxxMapType device);
const stGd25qxxxInfo *gd25qxxxGetInfo(eGd25qxxxMapType device);
eGd25qxxxStatus gd25qxxxReadJedecId(eGd25qxxxMapType device, uint8_t *manufacturerId, uint8_t *memoryType, uint8_t *capacityId);
eGd25qxxxStatus gd25qxxxReadStatus1(eGd25qxxxMapType device, uint8_t *statusValue);
eGd25qxxxStatus gd25qxxxWaitReady(eGd25qxxxMapType device, uint32_t timeoutMs);
eGd25qxxxStatus gd25qxxxRead(eGd25qxxxMapType device, uint32_t address, uint8_t *buffer, uint32_t length);
eGd25qxxxStatus gd25qxxxWrite(eGd25qxxxMapType device, uint32_t address, const uint8_t *buffer, uint32_t length);
eGd25qxxxStatus gd25qxxxEraseSector(eGd25qxxxMapType device, uint32_t address);
eGd25qxxxStatus gd25qxxxEraseBlock64k(eGd25qxxxMapType device, uint32_t address);
eGd25qxxxStatus gd25qxxxEraseChip(eGd25qxxxMapType device);

#ifdef __cplusplus
}
#endif

#endif  // GD25QXXX_H
/**************************End of file********************************/
