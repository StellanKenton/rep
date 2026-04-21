/************************************************************************************
* @file     : fc41d_data.h
* @brief    : FC41D internal data-plane declarations.
* @details  : This file keeps RX/TX ring state and payload extraction helpers.
* @author   : GitHub Copilot
* @date     : 2026-04-21
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef FC41D_DATA_H
#define FC41D_DATA_H

#include <stdbool.h>
#include <stdint.h>

#include "fc41d.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct stFc41dDataPlane {
    uint8_t rxStorage[FC41D_DATA_RX_BUFFER_SIZE];
    uint8_t txStorage[FC41D_DATA_TX_BUFFER_SIZE];
    uint16_t rxHead;
    uint16_t rxTail;
    uint16_t rxUsed;
    uint16_t txHead;
    uint16_t txTail;
    uint16_t txUsed;
    uint16_t txPendingLen;
    uint8_t txPendingBuf[FC41D_BLE_TX_CHUNK_SIZE];
} stFc41dDataPlane;

void fc41dDataReset(stFc41dDataPlane *dataPlane);
uint16_t fc41dDataGetRxLength(const stFc41dDataPlane *dataPlane);
uint16_t fc41dDataRead(stFc41dDataPlane *dataPlane, uint8_t *buffer, uint16_t bufferSize);
eFc41dStatus fc41dDataWrite(stFc41dDataPlane *dataPlane, const uint8_t *buffer, uint16_t length);
bool fc41dDataHasPendingTx(const stFc41dDataPlane *dataPlane);
void fc41dDataClearPendingTx(stFc41dDataPlane *dataPlane);
void fc41dDataConfirmPendingTx(stFc41dDataPlane *dataPlane);
eFc41dStatus fc41dDataBuildBleNotify(stFc41dDataPlane *dataPlane, const char *charUuid, char *cmdBuf, uint16_t bufferSize);
bool fc41dDataTryStoreUrcPayload(stFc41dDataPlane *dataPlane, const uint8_t *lineBuf, uint16_t lineLen);

#ifdef __cplusplus
}
#endif

#endif  // FC41D_DATA_H
/**************************End of file********************************/
