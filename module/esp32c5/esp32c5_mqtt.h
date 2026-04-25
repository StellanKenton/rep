/************************************************************************************
* @file     : esp32c5_mqtt.h
* @brief    : ESP32-C5 MQTT AT helper declarations.
* @details  : Builds MQTT commands and parses MQTT URCs for ESP-AT.
* @author   : GitHub Copilot
* @date     : 2026-04-25
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef ESP32C5_MQTT_H
#define ESP32C5_MQTT_H

#include "esp32c5.h"

#ifdef __cplusplus
extern "C" {
#endif

bool esp32c5MqttIsUrc(const uint8_t *lineBuf, uint16_t lineLen);
bool esp32c5MqttTryParseConnState(const uint8_t *lineBuf, uint16_t lineLen, uint8_t *state);
bool esp32c5MqttTryParseSubRecv(const uint8_t *lineBuf,
                                uint16_t lineLen,
                                const uint8_t **payload,
                                uint16_t *payloadLen);
eEsp32c5Status esp32c5MqttBuildUserCfgCommand(const char *clientId,
                                              const char *username,
                                              const char *password,
                                              char *buffer,
                                              uint16_t bufferSize);
eEsp32c5Status esp32c5MqttBuildConnectCommand(const char *host, uint16_t port, char *buffer, uint16_t bufferSize);
eEsp32c5Status esp32c5MqttBuildQueryCommand(char *buffer, uint16_t bufferSize);
eEsp32c5Status esp32c5MqttBuildSubscribeCommand(const char *topic, uint8_t qos, char *buffer, uint16_t bufferSize);
eEsp32c5Status esp32c5MqttBuildPublishCommand(const char *topic,
                                              const uint8_t *payload,
                                              uint16_t payloadLen,
                                              uint8_t qos,
                                              uint8_t retain,
                                              char *buffer,
                                              uint16_t bufferSize);
eEsp32c5Status esp32c5MqttBuildPublishRawCommand(const char *topic,
                                                 uint16_t payloadLen,
                                                 uint8_t qos,
                                                 uint8_t retain,
                                                 char *buffer,
                                                 uint16_t bufferSize);

#ifdef __cplusplus
}
#endif

#endif  // ESP32C5_MQTT_H
/**************************End of file********************************/
