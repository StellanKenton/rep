/************************************************************************************
* @file     : pca9535_assembly.h
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef PCA9535_ASSEMBLY_H
#define PCA9535_ASSEMBLY_H

#include <stdint.h>

#include "pca9535.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef eDrvStatus (*pca9535IicInitFunc)(uint8_t bus);
typedef eDrvStatus (*pca9535IicWriteRegFunc)(uint8_t bus, uint8_t address, const uint8_t *regBuf, uint16_t regLen, const uint8_t *buffer, uint16_t length);
typedef eDrvStatus (*pca9535IicReadRegFunc)(uint8_t bus, uint8_t address, const uint8_t *regBuf, uint16_t regLen, uint8_t *buffer, uint16_t length);

typedef struct stPca9535IicInterface {
    pca9535IicInitFunc init;
    pca9535IicWriteRegFunc writeReg;
    pca9535IicReadRegFunc readReg;
} stPca9535IicInterface;

typedef struct stPca9535AssembleCfg {
    uint8_t linkId;
} stPca9535AssembleCfg;

typedef struct stPca9535Ops {
    void (*loadDefaultCfg)(ePca9535MapType device, stPca9535Cfg *cfg);
    const stPca9535IicInterface *(*getIicInterface)(ePca9535MapType device);
    bool (*isValidAssemble)(ePca9535MapType device);
    uint8_t (*getLinkId)(ePca9535MapType device);
    void (*resetInit)(void);
    void (*resetWrite)(bool assertReset);
    uint32_t (*getResetAssertDelayMs)(void);
    uint32_t (*getResetReleaseDelayMs)(void);
    void (*delayMs)(uint32_t delayMs);
} stPca9535Ops;

const stPca9535Ops *pca9535PortGetOps(void);

#ifdef __cplusplus
}
#endif

#endif  // PCA9535_ASSEMBLY_H
/**************************End of file********************************/
