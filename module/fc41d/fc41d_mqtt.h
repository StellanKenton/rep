/************************************************************************************
* @file     : fc41d_mqtt.h
* @brief    : FC41D MQTT AT helper declarations.
***********************************************************************************/
#ifndef FC41D_MQTT_H
#define FC41D_MQTT_H

#include "fc41d.h"

#ifdef __cplusplus
extern "C" {
#endif

bool fc41dMqttIsUrc(const uint8_t *lineBuf, uint16_t lineLen);
bool fc41dMqttTryParseConnState(const uint8_t *lineBuf, uint16_t lineLen, uint8_t *state);
bool fc41dMqttTryParseSubRecv(const uint8_t *lineBuf, uint16_t lineLen, const uint8_t **payload, uint16_t *payloadLen);
eFc41dStatus fc41dMqttBuildUserCfgCommand(const char *clientId, const char *username, const char *password, char *buffer, uint16_t bufferSize);
eFc41dStatus fc41dMqttBuildRecvModeCommand(uint8_t mode, char *buffer, uint16_t bufferSize);
eFc41dStatus fc41dMqttBuildConnectCommand(const char *host, uint16_t port, char *buffer, uint16_t bufferSize);
eFc41dStatus fc41dMqttBuildLoginCommand(const char *clientId, const char *username, const char *password, char *buffer, uint16_t bufferSize);
eFc41dStatus fc41dMqttBuildQueryCommand(char *buffer, uint16_t bufferSize);
eFc41dStatus fc41dMqttBuildSubscribeCommand(const char *topic, uint8_t qos, char *buffer, uint16_t bufferSize);
eFc41dStatus fc41dMqttBuildPublishRawCommand(const char *topic, uint16_t payloadLen, uint8_t qos, uint8_t retain, char *buffer, uint16_t bufferSize);

#ifdef __cplusplus
}
#endif

#endif
