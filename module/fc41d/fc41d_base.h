/************************************************************************************
* @file     : fc41d_base.h
* @brief    : FC41D combo wireless module base public interface.
* @details  : Provides common types and non-blocking AT transaction APIs.
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef FC41D_BASE_H
#define FC41D_BASE_H

#include <stdbool.h>
#include <stdint.h>

#include "ringbuffer.h"
#include "rep_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eFc41dMap {
    FC41D_DEV0 = 0,
    FC41D_DEV_MAX,
} eFc41dMapType;

typedef enum eFc41dRxChannel {
    FC41D_RX_CHANNEL_NONE = 0,
    FC41D_RX_CHANNEL_BLE,
    FC41D_RX_CHANNEL_WIFI,
} eFc41dRxChannel;

typedef enum eFc41dMode {
    FC41D_MODE_COMMAND = 0,
    FC41D_MODE_BLE_DATA,
    FC41D_MODE_WIFI_DATA,
} eFc41dMode;

typedef enum eFc41dBleWorkMode {
    FC41D_BLE_WORK_MODE_DISABLED = 0,
    FC41D_BLE_WORK_MODE_PERIPHERAL,
    FC41D_BLE_WORK_MODE_CENTRAL,
} eFc41dBleWorkMode;

typedef enum eFc41dBleState {
    FC41D_BLE_STATE_IDLE = 0,
    FC41D_BLE_STATE_PERIPHERAL_INIT,
    FC41D_BLE_STATE_PERIPHERAL_ADV_START,
    FC41D_BLE_STATE_PERIPHERAL_WAIT_CONNECT,
    FC41D_BLE_STATE_PERIPHERAL_CONNECTED,
    FC41D_BLE_STATE_PERIPHERAL_DISCONNECTED,
    FC41D_BLE_STATE_CENTRAL_INIT,
    FC41D_BLE_STATE_CENTRAL_SCAN_START,
    FC41D_BLE_STATE_CENTRAL_WAIT_CONNECT,
    FC41D_BLE_STATE_CENTRAL_CONNECTED,
    FC41D_BLE_STATE_CENTRAL_DISCONNECTED,
    FC41D_BLE_STATE_ERROR,
} eFc41dBleState;

typedef enum eFc41dWifiWorkMode {
    FC41D_WIFI_WORK_MODE_DISABLED = 0,
    FC41D_WIFI_WORK_MODE_STA,
    FC41D_WIFI_WORK_MODE_AP,
} eFc41dWifiWorkMode;

typedef enum eFc41dWifiState {
    FC41D_WIFI_STATE_IDLE = 0,
    FC41D_WIFI_STATE_STA_INIT,
    FC41D_WIFI_STATE_STA_CONNECTING,
    FC41D_WIFI_STATE_STA_SERVICE_STARTING,
    FC41D_WIFI_STATE_STA_CONNECTED,
    FC41D_WIFI_STATE_STA_DISCONNECTED,
    FC41D_WIFI_STATE_AP_INIT,
    FC41D_WIFI_STATE_AP_STARTING,
    FC41D_WIFI_STATE_AP_ACTIVE,
    FC41D_WIFI_STATE_AP_STOPPED,
    FC41D_WIFI_STATE_ERROR,
} eFc41dWifiState;

typedef enum eFc41dAtExecResult {
    FC41D_AT_RESULT_NONE = 0,
    FC41D_AT_RESULT_OK,
    FC41D_AT_RESULT_ERROR,
    FC41D_AT_RESULT_TIMEOUT,
    FC41D_AT_RESULT_OVERFLOW,
    FC41D_AT_RESULT_SEND_FAIL,
    FC41D_AT_RESULT_BUSY,
} eFc41dAtExecResult;

typedef enum eFc41dAtGroup {
    FC41D_AT_GROUP_GENERAL = 0,
    FC41D_AT_GROUP_WIFI,
    FC41D_AT_GROUP_BLE,
    FC41D_AT_GROUP_TCPUDP,
    FC41D_AT_GROUP_SSL,
    FC41D_AT_GROUP_MQTT,
    FC41D_AT_GROUP_HTTP,
} eFc41dAtGroup;

typedef enum eFc41dAtCatalogCmd {
    FC41D_AT_CATALOG_CMD_AT = 0,
    FC41D_AT_CATALOG_CMD_QRST,
    FC41D_AT_CATALOG_CMD_QVERSION,
    FC41D_AT_CATALOG_CMD_QECHO,
    FC41D_AT_CATALOG_CMD_QURCCFG,
    FC41D_AT_CATALOG_CMD_QPING,
    FC41D_AT_CATALOG_CMD_QGETIP,
    FC41D_AT_CATALOG_CMD_QSETBAND,
    FC41D_AT_CATALOG_CMD_QWLANOTA,
    FC41D_AT_CATALOG_CMD_QLOWPOWER,
    FC41D_AT_CATALOG_CMD_QDEEPSLEEP,
    FC41D_AT_CATALOG_CMD_QWLMAC,
    FC41D_AT_CATALOG_CMD_QAIRKISS,
    FC41D_AT_CATALOG_CMD_QSTAST,
    FC41D_AT_CATALOG_CMD_QSTADHCP,
    FC41D_AT_CATALOG_CMD_QSTADHCPDEF,
    FC41D_AT_CATALOG_CMD_QSTASTATIC,
    FC41D_AT_CATALOG_CMD_QSTASTOP,
    FC41D_AT_CATALOG_CMD_QSOFTAP,
    FC41D_AT_CATALOG_CMD_QAPSTATE,
    FC41D_AT_CATALOG_CMD_QAPSTATIC,
    FC41D_AT_CATALOG_CMD_QSOFTAPSTOP,
    FC41D_AT_CATALOG_CMD_QSTAAPINFO,
    FC41D_AT_CATALOG_CMD_QSTAAPINFODEF,
    FC41D_AT_CATALOG_CMD_QGETWIFISTATE,
    FC41D_AT_CATALOG_CMD_QWSCAN,
    FC41D_AT_CATALOG_CMD_QWEBCFG,
    FC41D_AT_CATALOG_CMD_QBLEINIT,
    FC41D_AT_CATALOG_CMD_QBLEADDR,
    FC41D_AT_CATALOG_CMD_QBLENAME,
    FC41D_AT_CATALOG_CMD_QBLEADVPARAM,
    FC41D_AT_CATALOG_CMD_QBLEADVDATA,
    FC41D_AT_CATALOG_CMD_QBLEGATTSSRV,
    FC41D_AT_CATALOG_CMD_QBLEGATTSCHAR,
    FC41D_AT_CATALOG_CMD_QBLEADVSTART,
    FC41D_AT_CATALOG_CMD_QBLEADVSTOP,
    FC41D_AT_CATALOG_CMD_QBLEGATTSNTFY,
    FC41D_AT_CATALOG_CMD_QBLESCAN,
    FC41D_AT_CATALOG_CMD_QBLESCANPARAM,
    FC41D_AT_CATALOG_CMD_QBLECONN,
    FC41D_AT_CATALOG_CMD_QBLECONNPARAM,
    FC41D_AT_CATALOG_CMD_QBLECFGMTU,
    FC41D_AT_CATALOG_CMD_QBLEGATTCNTFCFG,
    FC41D_AT_CATALOG_CMD_QBLEGATTCWR,
    FC41D_AT_CATALOG_CMD_QBLEGATTCRD,
    FC41D_AT_CATALOG_CMD_QBLEDISCONN,
    FC41D_AT_CATALOG_CMD_QBLESTAT,
    FC41D_AT_CATALOG_CMD_QICFG,
    FC41D_AT_CATALOG_CMD_QIOPEN,
    FC41D_AT_CATALOG_CMD_QISTATE,
    FC41D_AT_CATALOG_CMD_QISEND,
    FC41D_AT_CATALOG_CMD_QIRD,
    FC41D_AT_CATALOG_CMD_QIACCEPT,
    FC41D_AT_CATALOG_CMD_QISWTMD,
    FC41D_AT_CATALOG_CMD_QICLOSE,
    FC41D_AT_CATALOG_CMD_QIGETERROR,
    FC41D_AT_CATALOG_CMD_ATO,
    FC41D_AT_CATALOG_CMD_ESCAPE,
    FC41D_AT_CATALOG_CMD_QSSLCFG,
    FC41D_AT_CATALOG_CMD_QSSLCERT,
    FC41D_AT_CATALOG_CMD_QSSLOPEN,
    FC41D_AT_CATALOG_CMD_QSSLSEND,
    FC41D_AT_CATALOG_CMD_QSSLRECV,
    FC41D_AT_CATALOG_CMD_QSSLSTATE,
    FC41D_AT_CATALOG_CMD_QSSLCLOSE,
    FC41D_AT_CATALOG_CMD_QMTCFG,
    FC41D_AT_CATALOG_CMD_QMTOPEN,
    FC41D_AT_CATALOG_CMD_QMTCLOSE,
    FC41D_AT_CATALOG_CMD_QMTCONN,
    FC41D_AT_CATALOG_CMD_QMTDISC,
    FC41D_AT_CATALOG_CMD_QMTSUB,
    FC41D_AT_CATALOG_CMD_QMTUNS,
    FC41D_AT_CATALOG_CMD_QMTPUB,
    FC41D_AT_CATALOG_CMD_QMTRECV,
    FC41D_AT_CATALOG_CMD_QHTTPCFG,
    FC41D_AT_CATALOG_CMD_QHTTPGET,
    FC41D_AT_CATALOG_CMD_QHTTPPOST,
    FC41D_AT_CATALOG_CMD_QHTTPPUT,
    FC41D_AT_CATALOG_CMD_QHTTPREAD,
    FC41D_AT_CATALOG_CMD_MAX,
} eFc41dAtCatalogCmd;

typedef eFc41dAtCatalogCmd eFc41dAtCmd;

#define FC41D_AT_CMD_CHECK_ALIVE           FC41D_AT_CATALOG_CMD_AT
#define FC41D_AT_CMD_RESET                 FC41D_AT_CATALOG_CMD_QRST
#define FC41D_AT_CMD_STA_STOP              FC41D_AT_CATALOG_CMD_QSTASTOP
#define FC41D_AT_CMD_BLE_INIT_GATTS        FC41D_AT_CATALOG_CMD_QBLEINIT
#define FC41D_AT_CMD_BLE_ADDR_QUERY        FC41D_AT_CATALOG_CMD_QBLEADDR
#define FC41D_AT_CMD_VERSION_QUERY         FC41D_AT_CATALOG_CMD_QVERSION
#define FC41D_AT_CMD_BLE_ADV_START         FC41D_AT_CATALOG_CMD_QBLEADVSTART
#define FC41D_AT_CMD_MAX                   FC41D_AT_CATALOG_CMD_MAX

typedef struct stFc41dAtCmdInfo {
    eFc41dAtCatalogCmd cmd;
    eFc41dAtGroup group;
    const char *name;
    const char *summary;
} stFc41dAtCmdInfo;

typedef void (*pfFc41dAtRespLine)(void *userData, const uint8_t *lineBuf, uint16_t lineLen);

typedef eDrvStatus eFc41dStatus;

#define FC41D_STATUS_OK                     DRV_STATUS_OK
#define FC41D_STATUS_INVALID_PARAM          DRV_STATUS_INVALID_PARAM
#define FC41D_STATUS_NOT_READY              DRV_STATUS_NOT_READY
#define FC41D_STATUS_BUSY                   DRV_STATUS_BUSY
#define FC41D_STATUS_TIMEOUT                DRV_STATUS_TIMEOUT
#define FC41D_STATUS_NACK                   DRV_STATUS_NACK
#define FC41D_STATUS_UNSUPPORTED            DRV_STATUS_UNSUPPORTED
#define FC41D_STATUS_ID_NOTMATCH            DRV_STATUS_ID_NOTMATCH
#define FC41D_STATUS_ERROR                  DRV_STATUS_ERROR

typedef struct stFc41dBleCfg {
    bool enableRx;
    bool rxOverwriteOnFull;
    eFc41dBleWorkMode workMode;
    const char *const *initCmdSeq;
    uint8_t initCmdSeqLen;
    const char *const *startCmdSeq;
    uint8_t startCmdSeqLen;
    const char *const *stopCmdSeq;
    uint8_t stopCmdSeqLen;
} stFc41dBleCfg;

typedef struct stFc41dWifiCfg {
    bool enableRx;
    bool rxOverwriteOnFull;
    eFc41dWifiWorkMode workMode;
    const char *const *initCmdSeq;
    uint8_t initCmdSeqLen;
    const char *const *startCmdSeq;
    uint8_t startCmdSeqLen;
    const char *const *stopCmdSeq;
    uint8_t stopCmdSeqLen;
    bool autoReconnect;
    uint32_t reconnectIntervalMs;
    uint8_t reconnectMaxRetries;
} stFc41dWifiCfg;

typedef struct stFc41dCfg {
    stFc41dBleCfg ble;
    stFc41dWifiCfg wifi;
    uint32_t execGuardMs;
    eFc41dMode bootMode;
} stFc41dCfg;

typedef struct stFc41dBleInfo {
    bool enableRx;
    eFc41dBleWorkMode workMode;
    eFc41dBleState state;
    uint32_t rxDroppedBytes;
    uint32_t rxRoutedBytes;
} stFc41dBleInfo;

typedef struct stFc41dWifiInfo {
    bool enableRx;
    eFc41dWifiWorkMode workMode;
    eFc41dWifiState state;
    uint8_t reconnectRetryCount;
    uint32_t rxDroppedBytes;
    uint32_t rxRoutedBytes;
} stFc41dWifiInfo;

typedef struct stFc41dInfo {
    bool isReady;
    stFc41dBleInfo ble;
    stFc41dWifiInfo wifi;
    eFc41dMode mode;
    eFc41dRxChannel lastRxChannel;
    uint32_t urcLineCount;
    uint32_t unknownUrcLineCount;
} stFc41dInfo;

typedef struct stFc41dAtOpt {
    const char *const *responseDonePatterns;
    uint8_t responseDonePatternCnt;
    const char *const *finalDonePatterns;
    uint8_t finalDonePatternCnt;
    const char *const *errorPatterns;
    uint8_t errorPatternCnt;
    uint32_t totalToutMs;
    uint32_t responseToutMs;
    uint32_t promptToutMs;
    uint32_t finalToutMs;
    bool needPrompt;
} stFc41dAtOpt;

typedef struct stFc41dAtResp {
    uint8_t *lineBuf;
    uint16_t lineBufSize;
    uint16_t lineLen;
    uint16_t lineCount;
    pfFc41dAtRespLine pfLineCallback;
    void *lineCallbackUserData;
    eFc41dAtExecResult result;
} stFc41dAtResp;

typedef enum eFc41dTxnOwner {
    FC41D_TXN_OWNER_NONE = 0,
    FC41D_TXN_OWNER_API,
    FC41D_TXN_OWNER_BLE,
    FC41D_TXN_OWNER_WIFI,
} eFc41dTxnOwner;

typedef enum eFc41dTxnStage {
    FC41D_TXN_STAGE_IDLE = 0,
    FC41D_TXN_STAGE_WAIT_RESPONSE,
    FC41D_TXN_STAGE_WAIT_PROMPT,
    FC41D_TXN_STAGE_WAIT_FINAL,
} eFc41dTxnStage;

typedef struct stFc41dTxnStatus {
    bool isBusy;
    eFc41dTxnOwner owner;
    eFc41dTxnStage stage;
    eFc41dAtExecResult currentResult;
    eFc41dAtExecResult lastResult;
} stFc41dTxnStatus;

eFc41dStatus fc41dGetDefCfg(eFc41dMapType device, stFc41dCfg *cfg);
eFc41dStatus fc41dGetCfg(eFc41dMapType device, stFc41dCfg *cfg);
eFc41dStatus fc41dSetCfg(eFc41dMapType device, const stFc41dCfg *cfg);
eFc41dStatus fc41dInit(eFc41dMapType device);
bool fc41dIsReady(eFc41dMapType device);
eFc41dStatus fc41dProcess(eFc41dMapType device);
eFc41dStatus fc41dGetInfo(eFc41dMapType device, stFc41dInfo *info);
eFc41dStatus fc41dRecover(eFc41dMapType device);

eFc41dStatus fc41dExecAt(eFc41dMapType device, const uint8_t *cmdBuf, uint16_t cmdLen,
                         const uint8_t *payloadBuf, uint16_t payloadLen,
                         const stFc41dAtOpt *opt, stFc41dAtResp *resp);
bool fc41dExecAtIsBusy(eFc41dMapType device);
eFc41dAtExecResult fc41dGetLastExecResult(eFc41dMapType device);
eFc41dStatus fc41dGetTxnStatus(eFc41dMapType device, stFc41dTxnStatus *status);
const char *fc41dAtGetCmdText(eFc41dAtCatalogCmd cmd);
eFc41dStatus fc41dExecAtCmd(eFc41dMapType device, eFc41dAtCatalogCmd cmd, const stFc41dAtOpt *opt, stFc41dAtResp *resp);
eFc41dStatus fc41dExecAtText(eFc41dMapType device, const char *cmdText, const stFc41dAtOpt *opt, stFc41dAtResp *resp);
eFc41dStatus fc41dAtCheckAlive(eFc41dMapType device);
uint16_t fc41dAtGetCmdInfoCount(void);
const stFc41dAtCmdInfo *fc41dAtGetCmdInfo(eFc41dAtCatalogCmd cmd);
const stFc41dAtCmdInfo *fc41dAtGetCmdInfoByIndex(uint16_t index);
const stFc41dAtCmdInfo *fc41dAtFindCmdInfo(const char *name);
eFc41dStatus fc41dAtBuildExecCmd(char *cmdBuf, uint16_t cmdBufSize, eFc41dAtCatalogCmd cmd);
eFc41dStatus fc41dAtBuildQueryCmd(char *cmdBuf, uint16_t cmdBufSize, eFc41dAtCatalogCmd cmd);
eFc41dStatus fc41dAtBuildTestCmd(char *cmdBuf, uint16_t cmdBufSize, eFc41dAtCatalogCmd cmd);
eFc41dStatus fc41dAtBuildSetCmd(char *cmdBuf, uint16_t cmdBufSize, eFc41dAtCatalogCmd cmd, const char *args);

eFc41dStatus fc41dSetModeState(eFc41dMapType device, eFc41dMode mode);
eFc41dMode fc41dGetModeState(eFc41dMapType device);

#ifdef __cplusplus
}
#endif

#endif  // FC41D_BASE_H
/**************************End of file********************************/
