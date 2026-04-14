/************************************************************************************
* @file     : fc41d_wifi.h
* @brief    : FC41D WiFi public interface.
* @details  : Exposes WiFi RX buffer access helpers.
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef FC41D_WIFI_H
#define FC41D_WIFI_H

#include "fc41d_base.h"

#ifdef __cplusplus
extern "C" {
#endif

stRingBuffer *fc41dWifiGetRxRingBuffer(eFc41dMapType device);
uint32_t fc41dWifiRead(eFc41dMapType device, uint8_t *buffer, uint32_t length);
uint32_t fc41dWifiPeek(eFc41dMapType device, uint8_t *buffer, uint32_t length);
uint32_t fc41dWifiDiscard(eFc41dMapType device, uint32_t length);
eFc41dStatus fc41dWifiClearRx(eFc41dMapType device);

#ifdef __cplusplus
}
#endif

#endif  // FC41D_WIFI_H
/**************************End of file********************************/
