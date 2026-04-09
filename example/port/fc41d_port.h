/************************************************************************************
* @file     : fc41d_port.h
* @brief    : FC41D project port helpers.
***********************************************************************************/
#ifndef FC41D_PORT_H
#define FC41D_PORT_H

#include <stdbool.h>
#include <stdint.h>

#include "fc41d.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef FC41D_PORT_LINE_BUF_SIZE
#define FC41D_PORT_LINE_BUF_SIZE            256U
#endif

#ifndef FC41D_PORT_CMD_BUF_SIZE
#define FC41D_PORT_CMD_BUF_SIZE             256U
#endif

#ifndef FC41D_PORT_PAYLOAD_BUF_SIZE
#define FC41D_PORT_PAYLOAD_BUF_SIZE         256U
#endif

#ifndef FC41D_PORT_BLE_RX_BUF_SIZE
#define FC41D_PORT_BLE_RX_BUF_SIZE          512U
#endif

#ifndef FC41D_PORT_WIFI_RX_BUF_SIZE
#define FC41D_PORT_WIFI_RX_BUF_SIZE         512U
#endif

#ifndef FC41D_PORT_TX_TIMEOUT_MS
#define FC41D_PORT_TX_TIMEOUT_MS            100U
#endif

#ifndef FC41D_PORT_PROC_BUDGET
#define FC41D_PORT_PROC_BUDGET              8U
#endif

#ifndef FC41D_PORT_BLE_PREFIX
#define FC41D_PORT_BLE_PREFIX               "+BLE:"
#endif

#ifndef FC41D_PORT_WIFI_PREFIX
#define FC41D_PORT_WIFI_PREFIX              "+WIFI:"
#endif

typedef bool (*fc41dPortRouteFunc)(const uint8_t *lineBuf, uint16_t lineLen,
                                   eFc41dRxChannel *channel, const uint8_t **payloadBuf,
                                   uint16_t *payloadLen, void *userCtx);

typedef struct stFc41dPortAssembleCfg {
    uint8_t uart;
    uint8_t *lineBuf;
    uint16_t lineBufSize;
    uint8_t *cmdBuf;
    uint16_t cmdBufSize;
    uint8_t *payloadBuf;
    uint16_t payloadBufSize;
    uint8_t *bleRxStorage;
    uint32_t bleRxCapacity;
    uint8_t *wifiRxStorage;
    uint32_t wifiRxCapacity;
    uint32_t txTimeoutMs;
    uint8_t procBudget;
    const char *blePrefix;
    const char *wifiPrefix;
    fc41dPortRouteFunc routeFunc;
    void *routeUserCtx;
} stFc41dPortAssembleCfg;

eFc41dStatus fc41dPortGetDefAssembleCfg(eFc41dMapType device, stFc41dPortAssembleCfg *cfg);
eFc41dStatus fc41dPortGetAssembleCfg(eFc41dMapType device, stFc41dPortAssembleCfg *cfg);
eFc41dStatus fc41dPortSetAssembleCfg(eFc41dMapType device, const stFc41dPortAssembleCfg *cfg);
bool fc41dPortIsValidAssembleCfg(const stFc41dPortAssembleCfg *cfg);
bool fc41dPortRouteDefault(const uint8_t *lineBuf, uint16_t lineLen,
                           eFc41dRxChannel *channel, const uint8_t **payloadBuf,
                           uint16_t *payloadLen, void *userCtx);

#ifdef __cplusplus
}
#endif

#endif  // FC41D_PORT_H
/**************************End of file********************************/