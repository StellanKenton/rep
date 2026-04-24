/************************************************************************************
* @file     : esp32c5_data.h
* @brief    : ESP32-C5 internal data-plane declarations.
* @details  : Keeps RX/TX ring state and BLE notify prompt payload helpers.
* @author   : GitHub Copilot
* @date     : 2026-04-23
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef ESP32C5_DATA_H
#define ESP32C5_DATA_H

#include <stdbool.h>
#include <stdint.h>

#include "esp32c5.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct stEsp32c5DataPlane {
    uint8_t rxStorage[ESP32C5_DATA_RX_BUFFER_SIZE];
    uint8_t txStorage[ESP32C5_DATA_TX_BUFFER_SIZE];
    uint16_t rxHead;
    uint16_t rxTail;
    uint16_t rxUsed;
    uint16_t txHead;
    uint16_t txTail;
    uint16_t txUsed;
    uint16_t txPendingLen;
    uint8_t txPendingBuf[ESP32C5_BLE_TX_CHUNK_SIZE];
} stEsp32c5DataPlane;

void esp32c5DataReset(stEsp32c5DataPlane *dataPlane);
uint16_t esp32c5DataGetRxLength(const stEsp32c5DataPlane *dataPlane);
uint16_t esp32c5DataRead(stEsp32c5DataPlane *dataPlane, uint8_t *buffer, uint16_t bufferSize);
void esp32c5DataStoreRx(stEsp32c5DataPlane *dataPlane, const uint8_t *buffer, uint16_t length);
eEsp32c5Status esp32c5DataWrite(stEsp32c5DataPlane *dataPlane, const uint8_t *buffer, uint16_t length);
bool esp32c5DataHasPendingTx(const stEsp32c5DataPlane *dataPlane);
void esp32c5DataClearPendingTx(stEsp32c5DataPlane *dataPlane);
void esp32c5DataConfirmPendingTx(stEsp32c5DataPlane *dataPlane);
eEsp32c5Status esp32c5DataBuildBleNotify(stEsp32c5DataPlane *dataPlane,
                                         uint8_t connIndex,
                                         uint8_t serviceIndex,
                                         uint8_t charIndex,
                                         uint16_t maxPayloadLen,
                                         char *cmdBuf,
                                         uint16_t bufferSize,
                                         const uint8_t **payloadBuf,
                                         uint16_t *payloadLen);
bool esp32c5DataTryStoreUrcPayload(stEsp32c5DataPlane *dataPlane, const uint8_t *lineBuf, uint16_t lineLen);

#ifdef __cplusplus
}
#endif

#endif  // ESP32C5_DATA_H
/**************************End of file********************************/
