/************************************************************************************
* @file     : frameprocess_data.h
* @brief    : Frame process protocol data definitions.
* @details  : Declares RX/TX payload structures, flags, and encode/decode helpers.
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef FRAMEPROCESS_DATA_H
#define FRAMEPROCESS_DATA_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef union unFrmDataRxFlags {
    uint32_t value;
    struct {
        uint32_t handshake : 1;
        uint32_t heartbeat : 1;
        uint32_t disconnect : 1;
        uint32_t getDeviceInfo : 1;
        uint32_t getBleInfo : 1;
        uint32_t ackFrame : 1;
        uint32_t reserved : 26;
    } bits;
} unFrmDataRxFlags;

typedef union unFrmDataTxFlags {
    uint32_t value;
    struct {
        uint32_t handshake : 1;
        uint32_t heartbeat : 1;
        uint32_t disconnect : 1;
        uint32_t selfCheck : 1;
        uint32_t deviceInfo : 1;
        uint32_t bleInfo : 1;
        uint32_t cprData : 1;
        uint32_t ackPending : 1;
        uint32_t reserved : 24;
    } bits;
} unFrmDataTxFlags;

typedef struct stFrmDataRxHandshake {
    uint8_t cipher[16];
} stFrmDataRxHandshake;

typedef struct stFrmDataRxHeartbeat {
    uint8_t reserved;
} stFrmDataRxHeartbeat;

typedef struct stFrmDataRxDisconnect {
    uint8_t reserved;
} stFrmDataRxDisconnect;

typedef struct stFrmDataRxGetDeviceInfo {
    uint8_t reserved;
} stFrmDataRxGetDeviceInfo;

typedef struct stFrmDataRxGetBleInfo {
    uint8_t reserved;
} stFrmDataRxGetBleInfo;

typedef struct stFrmDataTxHandshake {
    uint8_t macString[12];
} stFrmDataTxHandshake;

typedef struct stFrmDataTxHeartbeat {
    uint8_t batteryLevel;
    uint16_t batteryVoltage10mV;
    uint8_t batteryStatus;
    uint8_t electrodePresent;
    uint8_t electrodeModel;
    uint8_t electrodeAttached;
} stFrmDataTxHeartbeat;

typedef struct stFrmDataTxDisconnect {
    uint8_t reserved;
} stFrmDataTxDisconnect;

typedef struct stFrmDataTxSelfCheck {
    uint8_t cprModule;
    uint8_t powerModule;
    uint8_t commModule;
} stFrmDataTxSelfCheck;

typedef struct stFrmDataTxDeviceInfo {
    uint8_t deviceType;
    uint8_t deviceSn[13];
    uint8_t fwVerMajor;
    uint8_t fwVerMinor;
    uint8_t fwVerPatch;
    uint8_t reserved;
} stFrmDataTxDeviceInfo;

typedef struct stFrmDataTxBleInfo {
    uint8_t macString[12];
} stFrmDataTxBleInfo;

typedef struct stFrmDataTxCprData {
    uint32_t timestampMs;
    uint16_t pressRate;
    uint8_t pressDepth;
    uint8_t reboundDepth;
    uint8_t interruptSeconds;
    uint32_t eventId;
} stFrmDataTxCprData;

typedef struct stFrmDataRxStore {
    unFrmDataRxFlags flags;
    stFrmDataRxHandshake handshake;
    stFrmDataRxHeartbeat heartbeat;
    stFrmDataRxDisconnect disconnect;
    stFrmDataRxGetDeviceInfo getDeviceInfo;
    stFrmDataRxGetBleInfo getBleInfo;
} stFrmDataRxStore;

typedef struct stFrmDataTxStore {
    unFrmDataTxFlags flags;
    stFrmDataTxHandshake handshake;
    stFrmDataTxHeartbeat heartbeat;
    stFrmDataTxDisconnect disconnect;
    stFrmDataTxSelfCheck selfCheck;
    stFrmDataTxDeviceInfo deviceInfo;
    stFrmDataTxBleInfo bleInfo;
    stFrmDataTxCprData cprData;
} stFrmDataTxStore;

bool frmProcDataParseRx(uint8_t cmd, const uint8_t *payload, uint16_t payloadLen, stFrmDataRxStore *rxStore);
bool frmProcDataBuildTx(uint8_t cmd, const stFrmDataTxStore *txStore, uint8_t *payload, uint16_t payloadBufSize, uint16_t *payloadLen);

#ifdef __cplusplus
}
#endif

#endif  // FRAMEPROCESS_DATA_H
/**************************End of file********************************/
