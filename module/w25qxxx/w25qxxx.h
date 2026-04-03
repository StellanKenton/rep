/************************************************************************************
* @file     : w25qxxx.h
* @brief    : W25Qxxx SPI NOR flash module public interface.
* @details  : This module keeps reusable flash logic in the core layer and
*             relies on the port layer to bind logical device instances to the
*             project drvspi implementation.
***********************************************************************************/
#ifndef W25QXXX_H
#define W25QXXX_H

#include <stdbool.h>
#include <stdint.h>

#include "Rep/drvlayer/drvspi/drvspi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eW25qxxxDevMap {
    W25QXXX_DEV0 = 0,
    W25QXXX_DEV1,
    W25QXXX_DEV_MAX,
} eW25qxxxMapType;

#define W25QXXX_MANUFACTURER_ID              0xEFU
#define W25QXXX_PAGE_SIZE                    256U
#define W25QXXX_SECTOR_SIZE                  4096UL
#define W25QXXX_BLOCK64K_SIZE                65536UL
#define W25QXXX_MAX_TRANSFER_LENGTH          65535UL
#define W25QXXX_PAGE_PROGRAM_TIMEOUT_MS      10U
#define W25QXXX_SECTOR_ERASE_TIMEOUT_MS      500U
#define W25QXXX_BLOCK_ERASE_TIMEOUT_MS       3000U
#define W25QXXX_CHIP_ERASE_TIMEOUT_MS        120000U
#define W25QXXX_BUSY_POLL_DELAY_MS           1U


#define W25QXXX_CMD_WRITE_ENABLE             0x06U
#define W25QXXX_CMD_READ_STATUS1             0x05U
#define W25QXXX_CMD_JEDEC_ID                 0x9FU
#define W25QXXX_CMD_READ_DATA                0x03U
#define W25QXXX_CMD_READ_DATA_4B             0x13U
#define W25QXXX_CMD_PAGE_PROGRAM             0x02U
#define W25QXXX_CMD_PAGE_PROGRAM_4B          0x12U
#define W25QXXX_CMD_SECTOR_ERASE             0x20U
#define W25QXXX_CMD_SECTOR_ERASE_4B          0x21U
#define W25QXXX_CMD_BLOCK_ERASE_64K          0xD8U
#define W25QXXX_CMD_BLOCK_ERASE_64K_4B       0xDCU
#define W25QXXX_CMD_CHIP_ERASE               0xC7U

#define W25QXXX_STATUS1_BUSY_MASK            0x01U
#define W25QXXX_STATUS1_WEL_MASK             0x02U

typedef eDrvStatus eW25qxxxStatus;

#define W25QXXX_STATUS_OK                 DRV_STATUS_OK
#define W25QXXX_STATUS_INVALID_PARAM      DRV_STATUS_INVALID_PARAM
#define W25QXXX_STATUS_NOT_READY          DRV_STATUS_NOT_READY
#define W25QXXX_STATUS_BUSY               DRV_STATUS_BUSY
#define W25QXXX_STATUS_TIMEOUT            DRV_STATUS_TIMEOUT
#define W25QXXX_STATUS_NACK               DRV_STATUS_NACK
#define W25QXXX_STATUS_UNSUPPORTED        DRV_STATUS_UNSUPPORTED
#define W25QXXX_STATUS_DEVICE_ID_MISMATCH DRV_STATUS_ID_NOTMATCH
#define W25QXXX_STATUS_ERROR              DRV_STATUS_ERROR
#define W25QXXX_STATUS_OUT_OF_RANGE       ((eW25qxxxStatus)(DRV_STATUS_ERROR + 1))

typedef enum eW25qxxxPortSpiType {
    W25QXXX_PORT_SPI_TYPE_NONE = 0,
    W25QXXX_PORT_SPI_TYPE_HARDWARE,
    W25QXXX_PORT_SPI_TYPE_MAX,
} eW25qxxxPortSpiType;

typedef eDrvStatus (*w25qxxxPortSpiInitFunc)(uint8_t bus);
typedef eDrvStatus (*w25qxxxPortSpiTransferFunc)(uint8_t bus, const uint8_t *writeBuffer, uint16_t writeLength, const uint8_t *secondWriteBuffer, uint16_t secondWriteLength, uint8_t *readBuffer, uint16_t readLength, uint8_t readFillData);

typedef struct stW25qxxxPortSpiInterface {
    w25qxxxPortSpiInitFunc init;
    w25qxxxPortSpiTransferFunc transfer;
} stW25qxxxPortSpiInterface;

typedef struct stW25qxxxPortSpiBinding {
    eW25qxxxPortSpiType type;
    uint8_t bus;
    const stW25qxxxPortSpiInterface *spiIf;
} stW25qxxxPortSpiBinding;

typedef struct stW25qxxxCfg {
    stW25qxxxPortSpiBinding spiBind;
} stW25qxxxCfg;

typedef struct stW25qxxxInfo {
    uint8_t manufacturerId;
    uint8_t memoryType;
    uint8_t capacityId;
    uint8_t addressWidth;
    uint16_t pageSizeBytes;
    uint32_t totalSizeBytes;
    uint32_t sectorSizeBytes;
    uint32_t blockSizeBytes;
} stW25qxxxInfo;

typedef struct stW25qxxxDevice {
    stW25qxxxCfg cfg;
    stW25qxxxInfo info;
    bool isReady;
} stW25qxxxDevice;

eW25qxxxStatus w25qxxxGetDefCfg(eW25qxxxMapType device);
eW25qxxxStatus w25qxxxGetCfg(eW25qxxxMapType device, stW25qxxxCfg *cfg);
eW25qxxxStatus w25qxxxSetCfg(eW25qxxxMapType device, const stW25qxxxCfg *cfg);
eW25qxxxStatus w25qxxxInit(eW25qxxxMapType device);
bool w25qxxxIsReady(eW25qxxxMapType device);
const stW25qxxxInfo *w25qxxxGetInfo(eW25qxxxMapType device);
eW25qxxxStatus w25qxxxReadJedecId(eW25qxxxMapType device, uint8_t *manufacturerId, uint8_t *memoryType, uint8_t *capacityId);
eW25qxxxStatus w25qxxxReadStatus1(eW25qxxxMapType device, uint8_t *statusValue);
eW25qxxxStatus w25qxxxWaitReady(eW25qxxxMapType device, uint32_t timeoutMs);
eW25qxxxStatus w25qxxxRead(eW25qxxxMapType device, uint32_t address, uint8_t *buffer, uint32_t length);
eW25qxxxStatus w25qxxxWrite(eW25qxxxMapType device, uint32_t address, const uint8_t *buffer, uint32_t length);
eW25qxxxStatus w25qxxxEraseSector(eW25qxxxMapType device, uint32_t address);
eW25qxxxStatus w25qxxxEraseBlock64k(eW25qxxxMapType device, uint32_t address);
eW25qxxxStatus w25qxxxEraseChip(eW25qxxxMapType device);

#ifdef __cplusplus
}
#endif

#endif  // W25QXXX_H
/**************************End of file********************************/
