/************************************************************************************
* @file     : wt2003hx.h
* @brief    : WT2003HX audio module public interface.
* @details  : Provides UART frame commands, receive processing, and cached module
*             status for WT2003HX-compatible audio modules.
* @author   : 
* @date     : 2026-04-30
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef WT2003HX_H
#define WT2003HX_H

#include <stdbool.h>
#include <stdint.h>

#include "rep_config.h"
#include "framepareser.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WT2003HX_PROTOCOL_ID                   0x2003A7E1UL
#define WT2003HX_FRAME_HEAD                    0x7EU
#define WT2003HX_FRAME_TAIL                    0xEFU
#define WT2003HX_FRAME_MIN_LEN                 5U
#define WT2003HX_FRAME_MAX_LEN                 32U
#define WT2003HX_PARAM_NAME_MAX_LEN            8U
#define WT2003HX_VERSION_MAX_LEN               24U
#define WT2003HX_STREAM_BUF_SIZE               64U
#define WT2003HX_FRAME_BUF_SIZE                32U
#define WT2003HX_RX_TEMP_SIZE                  32U
#define WT2003HX_TX_BUF_SIZE                   16U
#define WT2003HX_POWER_DELAY_MS                1050U
#define WT2003HX_TX_TIMEOUT_MS                 100U

#define WT2003HX_CMD_EXTFLASH_INDEX_PLAY       0xA0U
#define WT2003HX_CMD_EXTFLASH_NAME_PLAY        0xA1U
#define WT2003HX_CMD_PLAY_STOP                 0xAAU
#define WT2003HX_CMD_PLAY_PAUSE                0xABU
#define WT2003HX_CMD_VOLUME_SET                0xAEU
#define WT2003HX_CMD_PLAY_MODE                 0xAFU
#define WT2003HX_CMD_OUTPUT_MODE_SWITCH        0xB6U
#define WT2003HX_CMD_CHECK_VERSION             0xC0U
#define WT2003HX_CMD_CHECK_VOLUME_SET          0xC1U
#define WT2003HX_CMD_CHECK_STATE               0xC2U
#define WT2003HX_CMD_CHECK_MUSIC_NUM           0xC3U
#define WT2003HX_CMD_CHECK_CONNECT_STATE       0xCAU

typedef enum eWt2003hxMapType {
    WT2003HX_DEV0 = 0,
    WT2003HX_DEV_MAX,
} eWt2003hxMapType;

typedef enum eWt2003hxPlayMode {
    WT2003HX_PLAY_MODE_SINGLE = 0,
    WT2003HX_PLAY_MODE_SINGLE_CYCLE = 1,
    WT2003HX_PLAY_MODE_ALL_CYCLE = 2,
    WT2003HX_PLAY_MODE_RANDOM = 3,
} eWt2003hxPlayMode;

typedef enum eWt2003hxOutputMode {
    WT2003HX_OUTPUT_MODE_SPK = 0,
    WT2003HX_OUTPUT_MODE_DAC = 1,
} eWt2003hxOutputMode;

typedef enum eWt2003hxPlayState {
    WT2003HX_PLAY_STATE_UNKNOWN = 0,
    WT2003HX_PLAY_STATE_PLAY = 1,
    WT2003HX_PLAY_STATE_STOP = 2,
    WT2003HX_PLAY_STATE_PAUSE = 3,
} eWt2003hxPlayState;

typedef eDrvStatus eWt2003hxStatus;

#define WT2003HX_STATUS_OK                     DRV_STATUS_OK
#define WT2003HX_STATUS_INVALID_PARAM          DRV_STATUS_INVALID_PARAM
#define WT2003HX_STATUS_NOT_READY              DRV_STATUS_NOT_READY
#define WT2003HX_STATUS_BUSY                   DRV_STATUS_BUSY
#define WT2003HX_STATUS_TIMEOUT                DRV_STATUS_TIMEOUT
#define WT2003HX_STATUS_UNSUPPORTED            DRV_STATUS_UNSUPPORTED
#define WT2003HX_STATUS_ERROR                  DRV_STATUS_ERROR

typedef struct stWt2003hxCfg {
    uint8_t linkId;
    uint8_t enablePin;
    uint32_t powerDelayMs;
    uint32_t txTimeoutMs;
} stWt2003hxCfg;

typedef struct stWt2003hxInfo {
    uint8_t version[WT2003HX_VERSION_MAX_LEN];
    uint8_t versionLen;
    uint16_t musicNum;
    uint8_t volume;
    uint8_t connectState;
    eWt2003hxPlayState playState;
    uint8_t lastReplyCmd;
    uint32_t lastReplyTick;
} stWt2003hxInfo;

typedef struct stWt2003hxDevice {
    stWt2003hxCfg cfg;
    stWt2003hxInfo info;
    stFrmPsr parser;
    uint8_t streamBuf[WT2003HX_STREAM_BUF_SIZE];
    uint8_t frameBuf[WT2003HX_FRAME_BUF_SIZE];
    uint8_t rxTemp[WT2003HX_RX_TEMP_SIZE];
    bool isReady;
    bool defCfgLoaded;
} stWt2003hxDevice;

eWt2003hxStatus wt2003hxGetDefCfg(eWt2003hxMapType device, stWt2003hxCfg *cfg);
eWt2003hxStatus wt2003hxGetCfg(eWt2003hxMapType device, stWt2003hxCfg *cfg);
eWt2003hxStatus wt2003hxSetCfg(eWt2003hxMapType device, const stWt2003hxCfg *cfg);
eWt2003hxStatus wt2003hxInit(eWt2003hxMapType device);
bool wt2003hxIsReady(eWt2003hxMapType device);
eWt2003hxStatus wt2003hxProcess(eWt2003hxMapType device);
eWt2003hxStatus wt2003hxPlayName(eWt2003hxMapType device, const uint8_t *name, uint8_t nameLen);
eWt2003hxStatus wt2003hxPlayIndex(eWt2003hxMapType device, uint16_t index);
eWt2003hxStatus wt2003hxStop(eWt2003hxMapType device);
eWt2003hxStatus wt2003hxPause(eWt2003hxMapType device);
eWt2003hxStatus wt2003hxSetVolume(eWt2003hxMapType device, uint8_t volume);
eWt2003hxStatus wt2003hxSetPlayMode(eWt2003hxMapType device, eWt2003hxPlayMode mode);
eWt2003hxStatus wt2003hxSetOutputMode(eWt2003hxMapType device, eWt2003hxOutputMode mode);
eWt2003hxStatus wt2003hxQuery(eWt2003hxMapType device, uint8_t cmd);
bool wt2003hxGetInfo(eWt2003hxMapType device, stWt2003hxInfo *info);

#ifdef __cplusplus
}
#endif

#endif  // WT2003HX_H
/**************************End of file********************************/
