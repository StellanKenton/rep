/************************************************************************************
* @file     : ec800m.h
* @brief    : EC800M-CN cellular module public interface.
* @details  : Exposes project-independent lifecycle and AT transaction APIs for
*             MQTT/HTTP usage over a platform-provided transport.
* @author   : GitHub Copilot
* @date     : 2026-04-27
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef EC800M_H
#define EC800M_H

#include <stdbool.h>
#include <stdint.h>

#include "../../rep.h"
#include "../../comm/flowparser/flowparser_stream.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eEc800mMapType {
    EC800M_DEV0 = 0,
    EC800M_DEV_MAX,
} eEc800mMapType;

typedef enum eEc800mServiceMode {
    EC800M_SERVICE_NONE = 0,
    EC800M_SERVICE_MQTT_HTTP,
    EC800M_SERVICE_MAX,
} eEc800mServiceMode;

typedef enum eEc800mRunState {
    EC800M_RUN_UNINIT = 0,
    EC800M_RUN_IDLE,
    EC800M_RUN_BOOTING,
    EC800M_RUN_CONFIGURING,
    EC800M_RUN_READY,
    EC800M_RUN_ERROR,
} eEc800mRunState;

#ifndef EC800M_STREAM_RX_STORAGE_SIZE
#define EC800M_STREAM_RX_STORAGE_SIZE        768U
#endif

#ifndef EC800M_STREAM_LINE_BUFFER_SIZE
#define EC800M_STREAM_LINE_BUFFER_SIZE       768U
#endif

#ifndef EC800M_RX_POLL_CHUNK_SIZE
#define EC800M_RX_POLL_CHUNK_SIZE            64U
#endif

#ifndef EC800M_CTRL_CMD_BUFFER_SIZE
#define EC800M_CTRL_CMD_BUFFER_SIZE          192U
#endif

#ifndef EC800M_DEFAULT_TX_TIMEOUT_MS
#define EC800M_DEFAULT_TX_TIMEOUT_MS         200U
#endif

#ifndef EC800M_DEFAULT_CMD_TIMEOUT_MS
#define EC800M_DEFAULT_CMD_TIMEOUT_MS        5000U
#endif

#ifndef EC800M_DEFAULT_PROMPT_TIMEOUT_MS
#define EC800M_DEFAULT_PROMPT_TIMEOUT_MS     5000U
#endif

#ifndef EC800M_DEFAULT_FINAL_TIMEOUT_MS
#define EC800M_DEFAULT_FINAL_TIMEOUT_MS      30000U
#endif

#ifndef EC800M_DEFAULT_BOOT_WAIT_MS
#define EC800M_DEFAULT_BOOT_WAIT_MS          800U
#endif

#ifndef EC800M_DEFAULT_PWRKEY_PULSE_MS
#define EC800M_DEFAULT_PWRKEY_PULSE_MS       600U
#endif

#ifndef EC800M_DEFAULT_RESET_PULSE_MS
#define EC800M_DEFAULT_RESET_PULSE_MS        200U
#endif

#ifndef EC800M_DEFAULT_RESET_WAIT_MS
#define EC800M_DEFAULT_RESET_WAIT_MS         300U
#endif

#ifndef EC800M_DEFAULT_READY_TIMEOUT_MS
#define EC800M_DEFAULT_READY_TIMEOUT_MS      30000U
#endif

#ifndef EC800M_DEFAULT_RETRY_INTERVAL_MS
#define EC800M_DEFAULT_RETRY_INTERVAL_MS     1000U
#endif

typedef eDrvStatus eEc800mStatus;

#define EC800M_STATUS_OK                     DRV_STATUS_OK
#define EC800M_STATUS_INVALID_PARAM          DRV_STATUS_INVALID_PARAM
#define EC800M_STATUS_NOT_READY              DRV_STATUS_NOT_READY
#define EC800M_STATUS_BUSY                   DRV_STATUS_BUSY
#define EC800M_STATUS_TIMEOUT                DRV_STATUS_TIMEOUT
#define EC800M_STATUS_UNSUPPORTED            DRV_STATUS_UNSUPPORTED
#define EC800M_STATUS_ERROR                  DRV_STATUS_ERROR
#define EC800M_STATUS_OVERFLOW               ((eEc800mStatus)(DRV_STATUS_ERROR + 1))
#define EC800M_STATUS_STREAM_FAIL            ((eEc800mStatus)(DRV_STATUS_ERROR + 2))

typedef void (*ec800mLineFunc)(void *userData, const uint8_t *lineBuf, uint16_t lineLen);
typedef bool (*ec800mUrcMatchFunc)(void *userData, const uint8_t *lineBuf, uint16_t lineLen);

typedef struct stEc800mCfg {
    uint8_t linkId;
    uint8_t pwrkeyPin;
    uint8_t resetPin;
    uint16_t rxPollChunkSize;
    uint32_t txTimeoutMs;
    uint32_t bootWaitMs;
    uint32_t pwrkeyPulseMs;
    uint32_t resetPulseMs;
    uint32_t resetWaitMs;
    uint32_t readyTimeoutMs;
    uint32_t retryIntervalMs;
} stEc800mCfg;

typedef struct stEc800mInfo {
    bool isInit;
    bool isReady;
    bool isBusy;
    bool hasLastResult;
    eFlowParserStage stage;
    eFlowParserResult lastResult;
    uint32_t rxBytes;
    uint32_t urcCount;
} stEc800mInfo;

typedef struct stEc800mState {
    eEc800mServiceMode serviceMode;
    eEc800mRunState runState;
    bool isReady;
    bool isBusy;
    bool isAtReady;
    bool isEchoDisabled;
    bool isSimChecked;
    bool isSignalChecked;
    eEc800mStatus lastError;
} stEc800mState;

eEc800mStatus ec800mGetDefCfg(eEc800mMapType device, stEc800mCfg *cfg);
eEc800mStatus ec800mGetCfg(eEc800mMapType device, stEc800mCfg *cfg);
eEc800mStatus ec800mSetCfg(eEc800mMapType device, const stEc800mCfg *cfg);
eEc800mStatus ec800mInit(eEc800mMapType device);
void ec800mReset(eEc800mMapType device);
eEc800mStatus ec800mStart(eEc800mMapType device, eEc800mServiceMode serviceMode);
void ec800mStop(eEc800mMapType device);
eEc800mStatus ec800mProcess(eEc800mMapType device, uint32_t nowTickMs);
bool ec800mIsReady(eEc800mMapType device);
const stEc800mInfo *ec800mGetInfo(eEc800mMapType device);
const stEc800mState *ec800mGetState(eEc800mMapType device);
eEc800mStatus ec800mSubmitTextCommand(eEc800mMapType device, const char *cmdText);
eEc800mStatus ec800mSubmitTextCommandEx(eEc800mMapType device, const char *cmdText, ec800mLineFunc lineHandler, void *userData);
eEc800mStatus ec800mSubmitPromptCommandEx(eEc800mMapType device, const char *cmdText, const uint8_t *payloadBuf, uint16_t payloadLen, ec800mLineFunc lineHandler, void *userData);
eEc800mStatus ec800mSetUrcHandler(eEc800mMapType device, ec800mLineFunc handler, void *userData);
eEc800mStatus ec800mSetUrcMatcher(eEc800mMapType device, ec800mUrcMatchFunc matcher, void *userData);

#ifdef __cplusplus
}
#endif

#endif  // EC800M_H
/**************************End of file********************************/
