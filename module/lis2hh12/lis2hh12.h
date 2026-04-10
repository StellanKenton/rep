/************************************************************************************
* @file     : lis2hh12.h
* @brief    : LIS2HH12 accelerometer module public interface.
* @details  : This module keeps LIS2HH12 register semantics in the core layer
*             and relies on assembly hooks to bind the device to the shared IIC
*             driver implementations.
***********************************************************************************/
#ifndef LIS2HH12_H
#define LIS2HH12_H

#include <stdbool.h>
#include <stdint.h>

#include "drviic.h"
#include "drvanlogiic.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eLis2hh12DevMap {
    LIS2HH12_DEV0 = 0,
    LIS2HH12_DEV_MAX,
} eLis2hh12MapType;

#define LIS2HH12_IIC_ADDRESS_LOW           0x1EU
#define LIS2HH12_IIC_ADDRESS_HIGH          0x1DU
#define LIS2HH12_WHO_AM_I_EXPECTED         0x41U

#define LIS2HH12_REG_TEMP_L                0x0BU
#define LIS2HH12_REG_TEMP_H                0x0CU
#define LIS2HH12_REG_WHO_AM_I              0x0FU
#define LIS2HH12_REG_CTRL1                 0x20U
#define LIS2HH12_REG_CTRL2                 0x21U
#define LIS2HH12_REG_CTRL3                 0x22U
#define LIS2HH12_REG_CTRL4                 0x23U
#define LIS2HH12_REG_CTRL5                 0x24U
#define LIS2HH12_REG_CTRL6                 0x25U
#define LIS2HH12_REG_OUT_X_L               0x28U
#define LIS2HH12_REG_FIFO_CTRL             0x2EU
#define LIS2HH12_REG_FIFO_SRC              0x2FU

#define LIS2HH12_CTRL1_XEN_BIT             0x01U
#define LIS2HH12_CTRL1_YEN_BIT             0x02U
#define LIS2HH12_CTRL1_ZEN_BIT             0x04U
#define LIS2HH12_CTRL1_BDU_BIT             0x08U
#define LIS2HH12_CTRL1_ODR_MASK            0x70U

#define LIS2HH12_CTRL2_HPM_MASK            0x18U
#define LIS2HH12_CTRL2_FDS_MASK            0x04U
#define LIS2HH12_CTRL2_DFC_MASK            0x60U

#define LIS2HH12_CTRL3_STOP_FTH_BIT        0x40U
#define LIS2HH12_CTRL3_FIFO_EN_BIT         0x80U

#define LIS2HH12_CTRL4_IF_ADD_INC_BIT      0x04U
#define LIS2HH12_CTRL4_FS_MASK             0x30U
#define LIS2HH12_CTRL4_BW_SCALE_ODR_BIT    0x08U
#define LIS2HH12_CTRL4_BW_MASK             0xC0U

#define LIS2HH12_CTRL5_SOFT_RESET_BIT      0x40U

#define LIS2HH12_FIFO_CTRL_FTH_MASK        0x1FU
#define LIS2HH12_FIFO_CTRL_MODE_MASK       0xE0U

#define LIS2HH12_FIFO_SRC_FSS_MASK         0x1FU
#define LIS2HH12_FIFO_SRC_EMPTY_BIT        0x20U
#define LIS2HH12_FIFO_SRC_OVR_BIT          0x40U
#define LIS2HH12_FIFO_SRC_FTH_BIT          0x80U

#define LIS2HH12_SAMPLE_BYTES              6U
#define LIS2HH12_FIFO_CAPACITY             32U

#define LIS2HH12_STATUS_OK                 DRV_STATUS_OK
#define LIS2HH12_STATUS_INVALID_PARAM      DRV_STATUS_INVALID_PARAM
#define LIS2HH12_STATUS_NOT_READY          DRV_STATUS_NOT_READY
#define LIS2HH12_STATUS_BUSY               DRV_STATUS_BUSY
#define LIS2HH12_STATUS_TIMEOUT            DRV_STATUS_TIMEOUT
#define LIS2HH12_STATUS_NACK               DRV_STATUS_NACK
#define LIS2HH12_STATUS_UNSUPPORTED        DRV_STATUS_UNSUPPORTED
#define LIS2HH12_STATUS_DEVICE_ID_MISMATCH DRV_STATUS_ID_NOTMATCH
#define LIS2HH12_STATUS_ERROR              DRV_STATUS_ERROR
#define LIS2HH12_STATUS_DATA_INVALID       ((eDrvStatus)(DRV_STATUS_ERROR + 1))
#define LIS2HH12_STATUS_BUFFER_TOO_SMALL   ((eDrvStatus)(DRV_STATUS_ERROR + 2))

typedef enum eLis2hh12DataRate {
    LIS2HH12_DATA_RATE_OFF = 0x00,
    LIS2HH12_DATA_RATE_10HZ = 0x01,
    LIS2HH12_DATA_RATE_50HZ = 0x02,
    LIS2HH12_DATA_RATE_100HZ = 0x03,
    LIS2HH12_DATA_RATE_200HZ = 0x04,
    LIS2HH12_DATA_RATE_400HZ = 0x05,
    LIS2HH12_DATA_RATE_800HZ = 0x06,
} eLis2hh12DataRate;

typedef enum eLis2hh12FullScale {
    LIS2HH12_FULL_SCALE_2G = 0x00,
    LIS2HH12_FULL_SCALE_16G = 0x01,
    LIS2HH12_FULL_SCALE_4G = 0x02,
    LIS2HH12_FULL_SCALE_8G = 0x03,
} eLis2hh12FullScale;

typedef enum eLis2hh12FilterIntPath {
    LIS2HH12_FILTER_INT_DISABLE = 0x00,
    LIS2HH12_FILTER_INT_GEN2 = 0x01,
    LIS2HH12_FILTER_INT_GEN1 = 0x02,
    LIS2HH12_FILTER_INT_BOTH = 0x03,
} eLis2hh12FilterIntPath;

typedef enum eLis2hh12FilterOutPath {
    LIS2HH12_FILTER_OUT_BYPASS = 0x00,
    LIS2HH12_FILTER_OUT_LP = 0x01,
    LIS2HH12_FILTER_OUT_HP = 0x02,
} eLis2hh12FilterOutPath;

typedef enum eLis2hh12FilterLowBandwidth {
    LIS2HH12_FILTER_LOW_BW_ODR_DIV_50 = 0x00,
    LIS2HH12_FILTER_LOW_BW_ODR_DIV_100 = 0x01,
    LIS2HH12_FILTER_LOW_BW_ODR_DIV_9 = 0x02,
    LIS2HH12_FILTER_LOW_BW_ODR_DIV_400 = 0x03,
} eLis2hh12FilterLowBandwidth;

typedef enum eLis2hh12FilterAntiAliasBandwidth {
    LIS2HH12_FILTER_AA_AUTO = 0x00,
    LIS2HH12_FILTER_AA_408HZ = 0x10,
    LIS2HH12_FILTER_AA_211HZ = 0x11,
    LIS2HH12_FILTER_AA_105HZ = 0x12,
    LIS2HH12_FILTER_AA_50HZ = 0x13,
} eLis2hh12FilterAntiAliasBandwidth;

typedef enum eLis2hh12FifoMode {
    LIS2HH12_FIFO_MODE_OFF = 0x00,
    LIS2HH12_FIFO_MODE_BYPASS = 0x10,
    LIS2HH12_FIFO_MODE_FIFO = 0x11,
    LIS2HH12_FIFO_MODE_STREAM = 0x12,
    LIS2HH12_FIFO_MODE_STREAM_TO_FIFO = 0x13,
    LIS2HH12_FIFO_MODE_BYPASS_TO_STREAM = 0x14,
    LIS2HH12_FIFO_MODE_BYPASS_TO_FIFO = 0x17,
} eLis2hh12FifoMode;

typedef struct stLis2hh12Cfg {
    uint8_t address;
    uint8_t fifoWatermark;
    uint8_t retryMax;
    eLis2hh12DataRate dataRate;
    eLis2hh12FullScale fullScale;
    eLis2hh12FilterIntPath filterIntPath;
    eLis2hh12FilterOutPath filterOutPath;
    eLis2hh12FilterLowBandwidth filterLowBandwidth;
    eLis2hh12FilterAntiAliasBandwidth filterAntiAliasBandwidth;
    eLis2hh12FifoMode fifoMode;
    bool blockDataUpdate;
    bool autoIncrement;
} stLis2hh12Cfg;

typedef struct stLis2hh12Sample {
    int16_t x;
    int16_t y;
    int16_t z;
} stLis2hh12Sample;

typedef struct stLis2hh12FifoStatus {
    uint8_t sampleCount;
    bool isEmpty;
    bool isOverrun;
    bool isThresholdReached;
} stLis2hh12FifoStatus;

typedef struct stLis2hh12Dev {
    stLis2hh12Cfg cfg;
    stLis2hh12Sample lastSample;
    stLis2hh12FifoStatus lastFifoStatus;
    bool isReady;
} stLis2hh12Device;

eDrvStatus lis2hh12GetDefCfg(eLis2hh12MapType device, stLis2hh12Cfg *cfg);
eDrvStatus lis2hh12GetCfg(eLis2hh12MapType device, stLis2hh12Cfg *cfg);
eDrvStatus lis2hh12SetCfg(eLis2hh12MapType device, const stLis2hh12Cfg *cfg);
eDrvStatus lis2hh12Init(eLis2hh12MapType device);
bool lis2hh12IsReady(eLis2hh12MapType device);
eDrvStatus lis2hh12ReadId(eLis2hh12MapType device, uint8_t *devId);
eDrvStatus lis2hh12ReadReg(eLis2hh12MapType device, uint8_t regAddr, uint8_t *value);
eDrvStatus lis2hh12WriteReg(eLis2hh12MapType device, uint8_t regAddr, uint8_t value);
eDrvStatus lis2hh12ReadRaw(eLis2hh12MapType device, stLis2hh12Sample *sample);
eDrvStatus lis2hh12ReadFifoStatus(eLis2hh12MapType device, stLis2hh12FifoStatus *status);
eDrvStatus lis2hh12ReadFifoSamples(eLis2hh12MapType device, stLis2hh12Sample *samples, uint8_t maxSamples, uint8_t *sampleCount);

#ifdef __cplusplus
}
#endif

#endif  // LIS2HH12_H
/**************************End of file********************************/