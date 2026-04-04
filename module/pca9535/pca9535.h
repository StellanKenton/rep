#ifndef PCA9535_H
#define PCA9535_H

#include <stdbool.h>
#include <stdint.h>

#include "drvanlogiic.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ePca9535DevMap {
    PCA9535_DEV0 = 0,
    PCA9535_DEV_MAX,
} ePca9535MapType;

#define PCA9535_IIC_ADDRESS_LLL              0x20U
#define PCA9535_IIC_ADDRESS_LLH              0x21U
#define PCA9535_IIC_ADDRESS_LHL              0x22U
#define PCA9535_IIC_ADDRESS_LHH              0x23U
#define PCA9535_IIC_ADDRESS_HLL              0x24U
#define PCA9535_IIC_ADDRESS_HLH              0x25U
#define PCA9535_IIC_ADDRESS_HHL              0x26U
#define PCA9535_IIC_ADDRESS_HHH              0x27U

#define PCA9535_REG_INPUT_PORT0              0x00U
#define PCA9535_REG_INPUT_PORT1              0x01U
#define PCA9535_REG_OUTPUT_PORT0             0x02U
#define PCA9535_REG_OUTPUT_PORT1             0x03U
#define PCA9535_REG_POLARITY_PORT0           0x04U
#define PCA9535_REG_POLARITY_PORT1           0x05U
#define PCA9535_REG_CONFIGURATION_PORT0      0x06U
#define PCA9535_REG_CONFIGURATION_PORT1      0x07U

#define PCA9535_STATUS_OK                    DRV_STATUS_OK
#define PCA9535_STATUS_INVALID_PARAM         DRV_STATUS_INVALID_PARAM
#define PCA9535_STATUS_NOT_READY             DRV_STATUS_NOT_READY
#define PCA9535_STATUS_BUSY                  DRV_STATUS_BUSY
#define PCA9535_STATUS_TIMEOUT               DRV_STATUS_TIMEOUT
#define PCA9535_STATUS_NACK                  DRV_STATUS_NACK
#define PCA9535_STATUS_UNSUPPORTED           DRV_STATUS_UNSUPPORTED
#define PCA9535_STATUS_ERROR                 DRV_STATUS_ERROR

typedef eDrvStatus (*pca9535IicInitFunc)(uint8_t bus);
typedef eDrvStatus (*pca9535IicWriteRegFunc)(uint8_t bus, uint8_t address, const uint8_t *regBuf, uint16_t regLen, const uint8_t *buffer, uint16_t length);
typedef eDrvStatus (*pca9535IicReadRegFunc)(uint8_t bus, uint8_t address, const uint8_t *regBuf, uint16_t regLen, uint8_t *buffer, uint16_t length);

typedef struct stPca9535IicInterface {
    pca9535IicInitFunc init;
    pca9535IicWriteRegFunc writeReg;
    pca9535IicReadRegFunc readReg;
} stPca9535IicInterface;

typedef struct stPca9535Cfg {
    eDrvAnlogIicPortMap iic;
    uint8_t address;
    uint16_t outputValue;
    uint16_t polarityMask;
    uint16_t directionMask;
    bool resetBeforeInit;
} stPca9535Cfg;

typedef struct stPca9535Dev {
    stPca9535Cfg cfg;
    uint16_t inputValue;
    uint16_t outputValue;
    uint16_t polarityMask;
    uint16_t directionMask;
    bool isReady;
} stPca9535Device;

eDrvStatus pca9535GetDefCfg(ePca9535MapType device);
eDrvStatus pca9535Init(ePca9535MapType device);
bool pca9535IsReady(ePca9535MapType device);
eDrvStatus pca9535ReadReg(ePca9535MapType device, uint8_t regAddr, uint8_t *value);
eDrvStatus pca9535WriteReg(ePca9535MapType device, uint8_t regAddr, uint8_t value);
eDrvStatus pca9535ReadInputPort(ePca9535MapType device, uint16_t *value);
eDrvStatus pca9535GetOutputPort(ePca9535MapType device, uint16_t *value);
eDrvStatus pca9535SetOutputPort(ePca9535MapType device, uint16_t value);
eDrvStatus pca9535ModifyOutputPort(ePca9535MapType device, uint16_t mask, uint16_t value);
eDrvStatus pca9535GetPolarityPort(ePca9535MapType device, uint16_t *value);
eDrvStatus pca9535SetPolarityPort(ePca9535MapType device, uint16_t value);
eDrvStatus pca9535GetDirectionPort(ePca9535MapType device, uint16_t *value);
eDrvStatus pca9535SetDirectionPort(ePca9535MapType device, uint16_t value);

#ifdef __cplusplus
}
#endif

#endif  // PCA9535_H
