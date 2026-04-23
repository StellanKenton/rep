/************************************************************************************
* @file     : esp32c5.h
* @brief    : ESP32-C5 BLE module public interface.
* @details  : Exposes project-independent configuration, lifecycle, state, and
*             data-plane APIs for ESP-AT based BLE peripheral mode.
* @author   : GitHub Copilot
* @date     : 2026-04-23
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef ESP32C5_H
#define ESP32C5_H

#include <stdbool.h>
#include <stdint.h>

#include "../../rep.h"
#include "../../comm/flowparser/flowparser_stream.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eEsp32c5MapType {
    ESP32C5_DEV0 = 0,
    ESP32C5_DEV_MAX,
} eEsp32c5MapType;

typedef enum eEsp32c5Role {
    ESP32C5_ROLE_NONE = 0,
    ESP32C5_ROLE_BLE_PERIPHERAL,
    ESP32C5_ROLE_MAX,
} eEsp32c5Role;

typedef enum eEsp32c5RunState {
    ESP32C5_RUN_UNINIT = 0,
    ESP32C5_RUN_IDLE,
    ESP32C5_RUN_BOOTING,
    ESP32C5_RUN_CONFIGURING,
    ESP32C5_RUN_READY,
    ESP32C5_RUN_ERROR,
} eEsp32c5RunState;

#ifndef ESP32C5_STREAM_RX_STORAGE_SIZE
#define ESP32C5_STREAM_RX_STORAGE_SIZE       512U
#endif

#ifndef ESP32C5_STREAM_LINE_BUFFER_SIZE
#define ESP32C5_STREAM_LINE_BUFFER_SIZE      512U
#endif

#ifndef ESP32C5_RX_POLL_CHUNK_SIZE
#define ESP32C5_RX_POLL_CHUNK_SIZE           64U
#endif

#ifndef ESP32C5_DATA_RX_BUFFER_SIZE
#define ESP32C5_DATA_RX_BUFFER_SIZE          512U
#endif

#ifndef ESP32C5_DATA_TX_BUFFER_SIZE
#define ESP32C5_DATA_TX_BUFFER_SIZE          512U
#endif

#ifndef ESP32C5_BLE_TX_CHUNK_SIZE
#define ESP32C5_BLE_TX_CHUNK_SIZE            20U
#endif

#ifndef ESP32C5_CTRL_CMD_BUFFER_SIZE
#define ESP32C5_CTRL_CMD_BUFFER_SIZE         192U
#endif

#ifndef ESP32C5_DEFAULT_TX_TIMEOUT_MS
#define ESP32C5_DEFAULT_TX_TIMEOUT_MS        100U
#endif

#ifndef ESP32C5_DEFAULT_CMD_TIMEOUT_MS
#define ESP32C5_DEFAULT_CMD_TIMEOUT_MS       5000U
#endif

#ifndef ESP32C5_DEFAULT_PROMPT_TIMEOUT_MS
#define ESP32C5_DEFAULT_PROMPT_TIMEOUT_MS    2000U
#endif

#ifndef ESP32C5_DEFAULT_FINAL_TIMEOUT_MS
#define ESP32C5_DEFAULT_FINAL_TIMEOUT_MS     5000U
#endif

#ifndef ESP32C5_DEFAULT_BOOT_WAIT_MS
#define ESP32C5_DEFAULT_BOOT_WAIT_MS         0U
#endif

#ifndef ESP32C5_DEFAULT_RESET_PULSE_MS
#define ESP32C5_DEFAULT_RESET_PULSE_MS       1000U
#endif

#ifndef ESP32C5_DEFAULT_RESET_WAIT_MS
#define ESP32C5_DEFAULT_RESET_WAIT_MS        200U
#endif

#ifndef ESP32C5_DEFAULT_READY_TIMEOUT_MS
#define ESP32C5_DEFAULT_READY_TIMEOUT_MS     4000U
#endif

#ifndef ESP32C5_DEFAULT_READY_PROBE_MS
#define ESP32C5_DEFAULT_READY_PROBE_MS       100U
#endif

#ifndef ESP32C5_DEFAULT_RETRY_INTERVAL_MS
#define ESP32C5_DEFAULT_RETRY_INTERVAL_MS    1000U
#endif

#ifndef ESP32C5_MAC_ADDRESS_TEXT_MAX_LENGTH
#define ESP32C5_MAC_ADDRESS_TEXT_MAX_LENGTH  18U
#endif

#ifndef ESP32C5_BLE_NAME_MAX_LENGTH
#define ESP32C5_BLE_NAME_MAX_LENGTH          32U
#endif

#ifndef ESP32C5_DEFAULT_ADV_INTERVAL_MIN
#define ESP32C5_DEFAULT_ADV_INTERVAL_MIN     200U
#endif

#ifndef ESP32C5_DEFAULT_ADV_INTERVAL_MAX
#define ESP32C5_DEFAULT_ADV_INTERVAL_MAX     200U
#endif

typedef eDrvStatus eEsp32c5Status;

#define ESP32C5_STATUS_OK                    DRV_STATUS_OK
#define ESP32C5_STATUS_INVALID_PARAM         DRV_STATUS_INVALID_PARAM
#define ESP32C5_STATUS_NOT_READY             DRV_STATUS_NOT_READY
#define ESP32C5_STATUS_BUSY                  DRV_STATUS_BUSY
#define ESP32C5_STATUS_TIMEOUT               DRV_STATUS_TIMEOUT
#define ESP32C5_STATUS_UNSUPPORTED           DRV_STATUS_UNSUPPORTED
#define ESP32C5_STATUS_ERROR                 DRV_STATUS_ERROR
#define ESP32C5_STATUS_OVERFLOW              ((eEsp32c5Status)(DRV_STATUS_ERROR + 1))
#define ESP32C5_STATUS_STREAM_FAIL           ((eEsp32c5Status)(DRV_STATUS_ERROR + 2))

typedef enum eEsp32c5RawMatchSta {
    ESP32C5_RAW_MATCH_NONE = 0,
    ESP32C5_RAW_MATCH_NEED_MORE,
    ESP32C5_RAW_MATCH_OK,
} eEsp32c5RawMatchSta;

typedef void (*esp32c5LineFunc)(void *userData, const uint8_t *lineBuf, uint16_t lineLen);
typedef bool (*esp32c5UrcMatchFunc)(void *userData, const uint8_t *lineBuf, uint16_t lineLen);
typedef eEsp32c5RawMatchSta (*esp32c5RawMatchFunc)(void *userData,
                                                   const uint8_t *buf,
                                                   uint16_t availLen,
                                                   uint16_t *frameLen);

typedef struct stEsp32c5Cfg {
    uint8_t linkId;
    uint8_t resetPin;
    uint16_t rxPollChunkSize;
    uint32_t txTimeoutMs;
    uint32_t bootWaitMs;
    uint32_t resetPulseMs;
    uint32_t resetWaitMs;
    uint32_t readyTimeoutMs;
    uint32_t readyProbeMs;
    uint32_t retryIntervalMs;
} stEsp32c5Cfg;

typedef struct stEsp32c5BleCfg {
    uint8_t initMode;
    char name[ESP32C5_BLE_NAME_MAX_LENGTH + 1U];
    uint16_t advIntervalMin;
    uint16_t advIntervalMax;
    uint8_t rxServiceIndex;
    uint8_t rxCharIndex;
    uint8_t txServiceIndex;
    uint8_t txCharIndex;
} stEsp32c5BleCfg;

typedef struct stEsp32c5Info {
    bool isInit;
    bool isReady;
    bool isBusy;
    bool hasLastResult;
    eFlowParserStage stage;
    eFlowParserResult lastResult;
    uint32_t rxBytes;
    uint32_t urcCount;
} stEsp32c5Info;

typedef struct stEsp32c5State {
    eEsp32c5Role role;
    eEsp32c5RunState runState;
    bool isReady;
    bool isBusy;
    bool isBleAdvertising;
    bool isBleConnected;
    bool isReadyUrcSeen;
    bool hasMacAddress;
    uint8_t connIndex;
    eEsp32c5Status lastError;
    char macAddress[ESP32C5_MAC_ADDRESS_TEXT_MAX_LENGTH];
} stEsp32c5State;

eEsp32c5Status esp32c5GetDefCfg(eEsp32c5MapType device, stEsp32c5Cfg *cfg);
eEsp32c5Status esp32c5GetCfg(eEsp32c5MapType device, stEsp32c5Cfg *cfg);
eEsp32c5Status esp32c5SetCfg(eEsp32c5MapType device, const stEsp32c5Cfg *cfg);
eEsp32c5Status esp32c5GetDefBleCfg(eEsp32c5MapType device, stEsp32c5BleCfg *cfg);
eEsp32c5Status esp32c5SetBleCfg(eEsp32c5MapType device, const stEsp32c5BleCfg *cfg);
eEsp32c5Status esp32c5Init(eEsp32c5MapType device);
void esp32c5Reset(eEsp32c5MapType device);
eEsp32c5Status esp32c5Start(eEsp32c5MapType device, eEsp32c5Role role);
eEsp32c5Status esp32c5DisconnectBle(eEsp32c5MapType device);
void esp32c5Stop(eEsp32c5MapType device);
eEsp32c5Status esp32c5Process(eEsp32c5MapType device, uint32_t nowTickMs);
bool esp32c5IsReady(eEsp32c5MapType device);
const stEsp32c5Info *esp32c5GetInfo(eEsp32c5MapType device);
const stEsp32c5State *esp32c5GetState(eEsp32c5MapType device);
uint16_t esp32c5GetRxLength(eEsp32c5MapType device);
uint16_t esp32c5ReadData(eEsp32c5MapType device, uint8_t *buffer, uint16_t bufferSize);
eEsp32c5Status esp32c5WriteData(eEsp32c5MapType device, const uint8_t *buffer, uint16_t length);
bool esp32c5GetCachedMac(eEsp32c5MapType device, char *buffer, uint16_t bufferSize);
eEsp32c5Status esp32c5SetUrcHandler(eEsp32c5MapType device, esp32c5LineFunc handler, void *userData);
eEsp32c5Status esp32c5SetUrcMatcher(eEsp32c5MapType device, esp32c5UrcMatchFunc matcher, void *userData);
eEsp32c5Status esp32c5SetRawMatcher(eEsp32c5MapType device, esp32c5RawMatchFunc matcher, void *userData);

#ifdef __cplusplus
}
#endif

#endif  // ESP32C5_H
/**************************End of file********************************/
