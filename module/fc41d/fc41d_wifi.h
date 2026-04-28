/************************************************************************************
* @file     : fc41d_wifi.h
* @brief    : FC41D WiFi AT helper declarations.
***********************************************************************************/
#ifndef FC41D_WIFI_H
#define FC41D_WIFI_H

#include "fc41d.h"

#ifdef __cplusplus
extern "C" {
#endif

bool fc41dWifiIsUrc(const uint8_t *lineBuf, uint16_t lineLen);
eFc41dStatus fc41dWifiBuildStationModeCommand(char *buffer, uint16_t bufferSize);
eFc41dStatus fc41dWifiBuildJoinCommand(const char *ssid, const char *password, char *buffer, uint16_t bufferSize);

#ifdef __cplusplus
}
#endif

#endif
