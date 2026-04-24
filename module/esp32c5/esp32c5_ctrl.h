/************************************************************************************
* @file     : esp32c5_ctrl.h
* @brief    : ESP32-C5 internal control-plane declarations.
* @details  : Declares runtime container, startup state machine, and control helpers.
* @author   : GitHub Copilot
* @date     : 2026-04-23
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef ESP32C5_CTRL_H
#define ESP32C5_CTRL_H

#include <stdbool.h>
#include <stdint.h>

#include "esp32c5_assembly.h"
#include "esp32c5_data.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP32C5_MAC_QUERY_PREFIX              "+BLEADDR:"

typedef struct stEsp32c5UrcCb {
    esp32c5LineFunc pfHandler;
    void *handlerUserData;
    esp32c5UrcMatchFunc pfMatcher;
    void *matcherUserData;
    esp32c5RawMatchFunc pfRawMatcher;
    void *rawMatcherUserData;
} stEsp32c5UrcCb;

typedef enum eEsp32c5CtrlStage {
    ESP32C5_CTRL_STAGE_IDLE = 0,
    ESP32C5_CTRL_STAGE_ASSERT_RESET,
    ESP32C5_CTRL_STAGE_RESET_HOLD,
    ESP32C5_CTRL_STAGE_WAIT_READY,
    ESP32C5_CTRL_STAGE_DISABLE_ECHO,
    ESP32C5_CTRL_STAGE_BLE_INIT,
    ESP32C5_CTRL_STAGE_BLE_SET_NAME,
    ESP32C5_CTRL_STAGE_BLE_SET_ADV_PARAM,
    ESP32C5_CTRL_STAGE_BLE_SET_ADV_DATA,
    ESP32C5_CTRL_STAGE_BLE_ADV_START,
    ESP32C5_CTRL_STAGE_QUERY_MAC,
    ESP32C5_CTRL_STAGE_RUNNING,
} eEsp32c5CtrlStage;

typedef enum eEsp32c5CtrlTxnKind {
    ESP32C5_CTRL_TXN_NONE = 0,
    ESP32C5_CTRL_TXN_STAGE,
    ESP32C5_CTRL_TXN_DATA_TX,
    ESP32C5_CTRL_TXN_BLE_CFG_MTU,
    ESP32C5_CTRL_TXN_BLE_DISCONNECT,
} eEsp32c5CtrlTxnKind;

typedef struct stEsp32c5CtrlPlane {
    char cmdBuf[ESP32C5_CTRL_CMD_BUFFER_SIZE];
    uint32_t nextActionTick;
    uint32_t readyDeadlineTick;
    eEsp32c5CtrlStage stage;
    eEsp32c5CtrlTxnKind txnKind;
    bool isTxnDone;
    eEsp32c5Status txnStatus;
} stEsp32c5CtrlPlane;

typedef struct stEsp32c5Device {
    stEsp32c5Cfg cfg;
    stEsp32c5BleCfg bleCfg;
    stEsp32c5Info info;
    stEsp32c5State state;
    stEsp32c5UrcCb urcCb;
    stFlowParserStream stream;
    uint8_t rxStorage[ESP32C5_STREAM_RX_STORAGE_SIZE];
    uint8_t lineBuf[ESP32C5_STREAM_LINE_BUFFER_SIZE];
    stEsp32c5DataPlane dataPlane;
    stEsp32c5CtrlPlane ctrlPlane;
} stEsp32c5Device;

bool esp32c5IsValidRole(eEsp32c5Role role);
void esp32c5LoadDefBleCfg(stEsp32c5BleCfg *cfg);
bool esp32c5IsValidText(const char *text, uint16_t maxLength, bool allowEmpty);
void esp32c5ResetState(stEsp32c5Device *device);
void esp32c5SyncInfo(stEsp32c5Device *device);
void esp32c5SyncState(stEsp32c5Device *device);
const stEsp32c5TransportInterface *esp32c5GetTransport(const stEsp32c5Device *device);
const stEsp32c5ControlInterface *esp32c5GetControl(eEsp32c5MapType device);
eEsp32c5Status esp32c5MapDrvStatus(eDrvStatus status);
eEsp32c5Status esp32c5MapStreamStatus(eFlowParserStrmSta status);
eEsp32c5Status esp32c5MapResult(eFlowParserResult result);
eEsp32c5Status esp32c5CtrlStart(stEsp32c5Device *device, eEsp32c5Role role);
eEsp32c5Status esp32c5CtrlDisconnectBle(stEsp32c5Device *device);
void esp32c5CtrlStop(stEsp32c5Device *device);
eEsp32c5Status esp32c5CtrlProcess(stEsp32c5Device *device, eEsp32c5MapType deviceId, uint32_t nowTickMs);
void esp32c5CtrlScheduleRetry(stEsp32c5Device *device, eEsp32c5MapType deviceId, uint32_t nowTickMs, eEsp32c5Status status);
bool esp32c5CtrlIsUrc(const stEsp32c5Device *device, const uint8_t *lineBuf, uint16_t lineLen);
void esp32c5CtrlHandleUrc(stEsp32c5Device *device, const uint8_t *lineBuf, uint16_t lineLen);
void esp32c5CtrlHandleTxnLine(stEsp32c5Device *device, const uint8_t *lineBuf, uint16_t lineLen);
void esp32c5CtrlHandleTxnDone(stEsp32c5Device *device, eFlowParserResult result);
void esp32c5TxnLineThunk(void *userData, const uint8_t *lineBuf, uint16_t lineLen);
void esp32c5TxnDoneThunk(void *userData, eFlowParserResult result);

#ifdef __cplusplus
}
#endif

#endif  // ESP32C5_CTRL_H
/**************************End of file********************************/
