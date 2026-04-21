/************************************************************************************
* @file     : fc41d.h
* @brief    : FC41D module public interface.
* @details  : This file exposes FC41D configuration, lifecycle, state snapshot,
*             and data-plane APIs only. Internal control/data details stay inside
*             the module private headers.
* @author   : GitHub Copilot
* @date     : 2026-04-21
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef FC41D_H
#define FC41D_H

#include <stdbool.h>
#include <stdint.h>

#include "../../rep.h"
#include "../../comm/flowparser/flowparser_stream.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eFc41dMapType {
    FC41D_DEV0 = 0,
    FC41D_DEV_MAX,
} eFc41dMapType;

typedef enum eFc41dRole {
    FC41D_ROLE_NONE = 0,
    FC41D_ROLE_BLE_PERIPHERAL,
    FC41D_ROLE_MAX,
} eFc41dRole;

typedef enum eFc41dRunState {
    FC41D_RUN_UNINIT = 0,
    FC41D_RUN_IDLE,
    FC41D_RUN_BOOTING,
    FC41D_RUN_CONFIGURING,
    FC41D_RUN_READY,
    FC41D_RUN_ERROR,
} eFc41dRunState;

#ifndef FC41D_STREAM_RX_STORAGE_SIZE
#define FC41D_STREAM_RX_STORAGE_SIZE         256U
#endif

#ifndef FC41D_STREAM_LINE_BUFFER_SIZE
#define FC41D_STREAM_LINE_BUFFER_SIZE        128U
#endif

#ifndef FC41D_RX_POLL_CHUNK_SIZE
#define FC41D_RX_POLL_CHUNK_SIZE             64U
#endif

#ifndef FC41D_DATA_RX_BUFFER_SIZE
#define FC41D_DATA_RX_BUFFER_SIZE            512U
#endif

#ifndef FC41D_DATA_TX_BUFFER_SIZE
#define FC41D_DATA_TX_BUFFER_SIZE            512U
#endif

#ifndef FC41D_BLE_TX_CHUNK_SIZE
#define FC41D_BLE_TX_CHUNK_SIZE              48U
#endif

#ifndef FC41D_CTRL_CMD_BUFFER_SIZE
#define FC41D_CTRL_CMD_BUFFER_SIZE           160U
#endif

#ifndef FC41D_DEFAULT_TX_TIMEOUT_MS
#define FC41D_DEFAULT_TX_TIMEOUT_MS          100U
#endif

#ifndef FC41D_DEFAULT_CMD_TIMEOUT_MS
#define FC41D_DEFAULT_CMD_TIMEOUT_MS         5000U
#endif

#ifndef FC41D_DEFAULT_PROMPT_TIMEOUT_MS
#define FC41D_DEFAULT_PROMPT_TIMEOUT_MS      2000U
#endif

#ifndef FC41D_DEFAULT_FINAL_TIMEOUT_MS
#define FC41D_DEFAULT_FINAL_TIMEOUT_MS       5000U
#endif

#ifndef FC41D_DEFAULT_BOOT_WAIT_MS
#define FC41D_DEFAULT_BOOT_WAIT_MS           0U
#endif

#ifndef FC41D_DEFAULT_RESET_PULSE_MS
#define FC41D_DEFAULT_RESET_PULSE_MS         1000U
#endif

#ifndef FC41D_DEFAULT_RESET_WAIT_MS
#define FC41D_DEFAULT_RESET_WAIT_MS          0U
#endif

#ifndef FC41D_DEFAULT_READY_TIMEOUT_MS
#define FC41D_DEFAULT_READY_TIMEOUT_MS       3000U
#endif

#ifndef FC41D_DEFAULT_READY_SETTLE_MS
#define FC41D_DEFAULT_READY_SETTLE_MS        1000U
#endif

#ifndef FC41D_DEFAULT_RETRY_INTERVAL_MS
#define FC41D_DEFAULT_RETRY_INTERVAL_MS      1000U
#endif

#ifndef FC41D_MAC_ADDRESS_TEXT_MAX_LENGTH
#define FC41D_MAC_ADDRESS_TEXT_MAX_LENGTH    18U
#endif

#ifndef FC41D_BLE_NAME_MAX_LENGTH
#define FC41D_BLE_NAME_MAX_LENGTH            32U
#endif

#ifndef FC41D_BLE_UUID_MAX_LENGTH
#define FC41D_BLE_UUID_MAX_LENGTH            32U
#endif

typedef eDrvStatus eFc41dStatus;

#define FC41D_STATUS_OK                      DRV_STATUS_OK
#define FC41D_STATUS_INVALID_PARAM           DRV_STATUS_INVALID_PARAM
#define FC41D_STATUS_NOT_READY               DRV_STATUS_NOT_READY
#define FC41D_STATUS_BUSY                    DRV_STATUS_BUSY
#define FC41D_STATUS_TIMEOUT                 DRV_STATUS_TIMEOUT
#define FC41D_STATUS_UNSUPPORTED             DRV_STATUS_UNSUPPORTED
#define FC41D_STATUS_ERROR                   DRV_STATUS_ERROR
#define FC41D_STATUS_OVERFLOW                ((eFc41dStatus)(DRV_STATUS_ERROR + 1))
#define FC41D_STATUS_STREAM_FAIL             ((eFc41dStatus)(DRV_STATUS_ERROR + 2))

typedef void (*fc41dLineFunc)(void *userData, const uint8_t *lineBuf, uint16_t lineLen);
typedef bool (*fc41dUrcMatchFunc)(void *userData, const uint8_t *lineBuf, uint16_t lineLen);

typedef struct stFc41dCfg {
    uint8_t linkId;
    uint8_t resetPin;
    uint16_t rxPollChunkSize;
    uint32_t txTimeoutMs;
    uint32_t bootWaitMs;
    uint32_t resetPulseMs;
    uint32_t resetWaitMs;
    uint32_t readyTimeoutMs;
    uint32_t readySettleMs;
    uint32_t retryIntervalMs;
} stFc41dCfg;

typedef struct stFc41dBleCfg {
    uint8_t initMode;
    char name[FC41D_BLE_NAME_MAX_LENGTH + 1U];
    char serviceUuid[FC41D_BLE_UUID_MAX_LENGTH + 1U];
    char rxCharUuid[FC41D_BLE_UUID_MAX_LENGTH + 1U];
    char txCharUuid[FC41D_BLE_UUID_MAX_LENGTH + 1U];
} stFc41dBleCfg;

typedef struct stFc41dInfo {
    bool isInit;
    bool isReady;
    bool isBusy;
    bool hasLastResult;
    eFlowParserStage stage;
    eFlowParserResult lastResult;
    uint32_t rxBytes;
    uint32_t urcCount;
} stFc41dInfo;

typedef struct stFc41dState {
    eFc41dRole role;
    eFc41dRunState runState;
    bool isReady;
    bool isBusy;
    bool isBleAdvertising;
    bool isBleConnected;
    bool isReadyUrcSeen;
    bool hasMacAddress;
    eFc41dStatus lastError;
    char macAddress[FC41D_MAC_ADDRESS_TEXT_MAX_LENGTH];
} stFc41dState;

eFc41dStatus fc41dGetDefCfg(eFc41dMapType device, stFc41dCfg *cfg);
eFc41dStatus fc41dGetCfg(eFc41dMapType device, stFc41dCfg *cfg);
eFc41dStatus fc41dSetCfg(eFc41dMapType device, const stFc41dCfg *cfg);
eFc41dStatus fc41dGetDefBleCfg(eFc41dMapType device, stFc41dBleCfg *cfg);
eFc41dStatus fc41dSetBleCfg(eFc41dMapType device, const stFc41dBleCfg *cfg);
eFc41dStatus fc41dInit(eFc41dMapType device);
void fc41dReset(eFc41dMapType device);
eFc41dStatus fc41dStart(eFc41dMapType device, eFc41dRole role);
eFc41dStatus fc41dDisconnectBle(eFc41dMapType device);
void fc41dStop(eFc41dMapType device);
eFc41dStatus fc41dProcess(eFc41dMapType device, uint32_t nowTickMs);
bool fc41dIsReady(eFc41dMapType device);
const stFc41dInfo *fc41dGetInfo(eFc41dMapType device);
const stFc41dState *fc41dGetState(eFc41dMapType device);
uint16_t fc41dGetRxLength(eFc41dMapType device);
uint16_t fc41dReadData(eFc41dMapType device, uint8_t *buffer, uint16_t bufferSize);
eFc41dStatus fc41dWriteData(eFc41dMapType device, const uint8_t *buffer, uint16_t length);
bool fc41dGetCachedMac(eFc41dMapType device, char *buffer, uint16_t bufferSize);
eFc41dStatus fc41dSetUrcHandler(eFc41dMapType device, fc41dLineFunc handler, void *userData);
eFc41dStatus fc41dSetUrcMatcher(eFc41dMapType device, fc41dUrcMatchFunc matcher, void *userData);

#ifdef __cplusplus
}
#endif

#endif  // FC41D_H
/**************************End of file********************************/
