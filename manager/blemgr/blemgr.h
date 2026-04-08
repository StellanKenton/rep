/************************************************************************************
* @file     : blemgr.h
* @brief    : BLE manager service backed by the FC41D module.
* @details  : Configures FC41D BLE advertising and parses packets received from the
*             PC-side BLE test script.
***********************************************************************************/
#ifndef BLEMGR_H
#define BLEMGR_H

#include <stdbool.h>
#include <stdint.h>

#include "../service_lifecycle.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BLEMGR_TAG                          "BleMgr"
#define BLEMGR_LAST_PAYLOAD_PREVIEW_MAX     32U

typedef enum eBleMgrState {
    eBLEMGR_STATE_UNINIT = 0,
    eBLEMGR_STATE_READY,
    eBLEMGR_STATE_CONFIGURING,
    eBLEMGR_STATE_WAIT_HANDSHAKE,
    eBLEMGR_STATE_RUNNING,
    eBLEMGR_STATE_STOPPED,
    eBLEMGR_STATE_FAULT,
} eBleMgrState;

typedef struct stBleMgrStatus {
    stManagerServiceLifecycle lifecycle;
    eBleMgrState state;
    bool isModuleReady;
    bool isConfigured;
    bool isHandshakeDone;
    uint32_t rxPacketCount;
    uint32_t rxInvalidPacketCount;
    uint32_t rxDroppedBytes;
    uint8_t lastCmd;
    uint16_t lastPayloadLen;
    uint8_t bleMac[6];
    char bleName[32];
    char bleVersion[40];
    uint8_t lastPayloadPreview[BLEMGR_LAST_PAYLOAD_PREVIEW_MAX];
} stBleMgrStatus;

bool blemgrInit(void);
bool blemgrStart(void);
void blemgrStop(void);
void blemgrProcess(void);
eBleMgrState blemgrGetState(void);
eManagerLifecycleError blemgrGetLastError(void);
const stBleMgrStatus *blemgrGetStatus(void);

#ifdef __cplusplus
}
#endif

#endif  // BLEMGR_H
/**************************End of file********************************/