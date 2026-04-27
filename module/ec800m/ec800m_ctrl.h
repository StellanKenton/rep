/************************************************************************************
* @file     : ec800m_ctrl.h
* @brief    : EC800M internal control-plane declarations.
* @details  : Declares runtime container and startup state machine state.
* @author   : GitHub Copilot
* @date     : 2026-04-27
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef EC800M_CTRL_H
#define EC800M_CTRL_H

#include <stdbool.h>
#include <stdint.h>

#include "ec800m_assembly.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct stEc800mUrcCb {
    ec800mLineFunc pfHandler;
    void *handlerUserData;
    ec800mUrcMatchFunc pfMatcher;
    void *matcherUserData;
} stEc800mUrcCb;

typedef enum eEc800mCtrlStage {
    EC800M_CTRL_STAGE_IDLE = 0,
    EC800M_CTRL_STAGE_ASSERT_RESET_AND_PWRKEY,
    EC800M_CTRL_STAGE_RELEASE_RESET,
    EC800M_CTRL_STAGE_ASSERT_RESET_AGAIN,
    EC800M_CTRL_STAGE_RELEASE_PWRKEY,
    EC800M_CTRL_STAGE_ASSERT_PWRKEY,
    EC800M_CTRL_STAGE_WAIT_AT,
    EC800M_CTRL_STAGE_DISABLE_ECHO,
    EC800M_CTRL_STAGE_QUERY_CPIN,
    EC800M_CTRL_STAGE_QUERY_CSQ,
    EC800M_CTRL_STAGE_RUNNING,
} eEc800mCtrlStage;

typedef enum eEc800mCtrlTxnKind {
    EC800M_CTRL_TXN_NONE = 0,
    EC800M_CTRL_TXN_STAGE,
    EC800M_CTRL_TXN_USER_TEXT,
    EC800M_CTRL_TXN_USER_PROMPT,
} eEc800mCtrlTxnKind;

typedef struct stEc800mCtrlPlane {
    char cmdBuf[EC800M_CTRL_CMD_BUFFER_SIZE];
    uint32_t nextActionTick;
    uint32_t readyDeadlineTick;
    ec800mLineFunc userLineHandler;
    void *userData;
    eEc800mCtrlStage stage;
    eEc800mCtrlTxnKind txnKind;
    bool isTxnDone;
    eEc800mStatus txnStatus;
} stEc800mCtrlPlane;

typedef struct stEc800mDevice {
    stEc800mCfg cfg;
    stEc800mInfo info;
    stEc800mState state;
    stEc800mUrcCb urcCb;
    stFlowParserStream stream;
    uint8_t rxStorage[EC800M_STREAM_RX_STORAGE_SIZE];
    uint8_t lineBuf[EC800M_STREAM_LINE_BUFFER_SIZE];
    stEc800mCtrlPlane ctrlPlane;
} stEc800mDevice;

#ifdef __cplusplus
}
#endif

#endif  // EC800M_CTRL_H
/**************************End of file********************************/
