/************************************************************************************
* @file     : fc41d_ble.h
* @brief    : FC41D BLE public interface.
* @details  : Exposes BLE AT helper builders and BLE RX buffer access helpers.
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef FC41D_BLE_H
#define FC41D_BLE_H

#include "fc41d_base.h"

#ifdef __cplusplus
extern "C" {
#endif

eFc41dStatus fc41dAtBuildBleNameCmd(char *cmdBuf, uint16_t cmdBufSize, const char *name);
eFc41dStatus fc41dAtBuildBleGattServiceCmd(char *cmdBuf, uint16_t cmdBufSize, uint16_t serviceUuid);
eFc41dStatus fc41dAtBuildBleGattCharCmd(char *cmdBuf, uint16_t cmdBufSize, uint16_t charUuid);
eFc41dStatus fc41dAtBuildBleAdvParamCmd(char *cmdBuf, uint16_t cmdBufSize, uint16_t intervalMin, uint16_t intervalMax);
eFc41dStatus fc41dAtBuildBleAdvDataCmd(char *cmdBuf, uint16_t cmdBufSize, const uint8_t *advData, uint16_t advLen);

stRingBuffer *fc41dBleGetRxRingBuffer(eFc41dMapType device);
uint32_t fc41dBleRead(eFc41dMapType device, uint8_t *buffer, uint32_t length);
uint32_t fc41dBlePeek(eFc41dMapType device, uint8_t *buffer, uint32_t length);
uint32_t fc41dBleDiscard(eFc41dMapType device, uint32_t length);
eFc41dStatus fc41dBleClearRx(eFc41dMapType device);

#ifdef __cplusplus
}
#endif

#endif  // FC41D_BLE_H
/**************************End of file********************************/
