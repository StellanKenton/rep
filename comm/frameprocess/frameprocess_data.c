/************************************************************************************
* @file     : frameprocess_data.c
* @brief    : Frame process protocol data encode and decode helpers.
***********************************************************************************/
#include "frameprocess_data.h"

#include <string.h>

#include "frameprocess.h"

#define FRM_PROC_HANDSHAKE_PAYLOAD_LEN      16U
#define FRM_PROC_HANDSHAKE_RSP_LEN          12U
#define FRM_PROC_HEARTBEAT_PAYLOAD_LEN      0U
#define FRM_PROC_HEARTBEAT_RSP_LEN          7U
#define FRM_PROC_DISCONNECT_PAYLOAD_LEN     0U
#define FRM_PROC_SELFCHECK_PAYLOAD_LEN      3U
#define FRM_PROC_DEVICE_INFO_PAYLOAD_LEN    18U
#define FRM_PROC_BLE_INFO_PAYLOAD_LEN       12U
#define FRM_PROC_CPR_PAYLOAD_LEN            13U

static void frmProcDataWriteBe16(uint8_t *buffer, uint16_t value)
{
    buffer[0] = (uint8_t)((value >> 8U) & 0xFFU);
    buffer[1] = (uint8_t)(value & 0xFFU);
}

static void frmProcDataWriteBe32(uint8_t *buffer, uint32_t value)
{
    buffer[0] = (uint8_t)((value >> 24U) & 0xFFU);
    buffer[1] = (uint8_t)((value >> 16U) & 0xFFU);
    buffer[2] = (uint8_t)((value >> 8U) & 0xFFU);
    buffer[3] = (uint8_t)(value & 0xFFU);
}

bool frmProcDataParseRx(uint8_t cmd, const uint8_t *payload, uint16_t payloadLen, stFrmDataRxStore *rxStore)
{
    if ((rxStore == NULL) || ((payload == NULL) && (payloadLen != 0U))) {
        return false;
    }

    switch ((eFrmProcCmdType)cmd) {
        case FRM_PROC_CMD_HANDSHAKE:
            if (payloadLen != FRM_PROC_HANDSHAKE_PAYLOAD_LEN) {
                return false;
            }
            (void)memcpy(rxStore->handshake.cipher, payload, payloadLen);
            rxStore->flags.bits.handshake = 1U;
            return true;
        case FRM_PROC_CMD_HEARTBEAT:
            if (payloadLen != FRM_PROC_HEARTBEAT_PAYLOAD_LEN) {
                return false;
            }
            rxStore->heartbeat.reserved = 0U;
            rxStore->flags.bits.heartbeat = 1U;
            return true;
        case FRM_PROC_CMD_DISCONNECT:
            if (payloadLen != FRM_PROC_DISCONNECT_PAYLOAD_LEN) {
                return false;
            }
            rxStore->disconnect.reserved = 0U;
            rxStore->flags.bits.disconnect = 1U;
            return true;
        case FRM_PROC_CMD_GET_DEVICE_INFO:
            if (payloadLen != 0U) {
                return false;
            }
            rxStore->getDeviceInfo.reserved = 0U;
            rxStore->flags.bits.getDeviceInfo = 1U;
            return true;
        case FRM_PROC_CMD_GET_BLE_INFO:
            if (payloadLen != 0U) {
                return false;
            }
            rxStore->getBleInfo.reserved = 0U;
            rxStore->flags.bits.getBleInfo = 1U;
            return true;
        default:
            return false;
    }
}

bool frmProcDataBuildTx(uint8_t cmd, const stFrmDataTxStore *txStore, uint8_t *payload, uint16_t payloadBufSize, uint16_t *payloadLen)
{
    if ((txStore == NULL) || ((payload == NULL) && (payloadBufSize != 0U)) || (payloadLen == NULL)) {
        return false;
    }

    switch ((eFrmProcCmdType)cmd) {
        case FRM_PROC_CMD_HANDSHAKE:
            if (payloadBufSize < FRM_PROC_HANDSHAKE_RSP_LEN) {
                return false;
            }
            (void)memcpy(payload, txStore->handshake.macString, FRM_PROC_HANDSHAKE_RSP_LEN);
            *payloadLen = FRM_PROC_HANDSHAKE_RSP_LEN;
            return true;
        case FRM_PROC_CMD_HEARTBEAT:
            if (payloadBufSize < FRM_PROC_HEARTBEAT_RSP_LEN) {
                return false;
            }
            payload[0] = txStore->heartbeat.batteryLevel;
            frmProcDataWriteBe16(&payload[1], txStore->heartbeat.batteryVoltage10mV);
            payload[3] = txStore->heartbeat.batteryStatus;
            payload[4] = txStore->heartbeat.electrodePresent;
            payload[5] = txStore->heartbeat.electrodeModel;
            payload[6] = txStore->heartbeat.electrodeAttached;
            *payloadLen = FRM_PROC_HEARTBEAT_RSP_LEN;
            return true;
        case FRM_PROC_CMD_DISCONNECT:
            *payloadLen = 0U;
            return true;
        case FRM_PROC_CMD_SELF_CHECK:
            if (payloadBufSize < FRM_PROC_SELFCHECK_PAYLOAD_LEN) {
                return false;
            }
            payload[0] = txStore->selfCheck.cprModule;
            payload[1] = txStore->selfCheck.powerModule;
            payload[2] = txStore->selfCheck.commModule;
            *payloadLen = FRM_PROC_SELFCHECK_PAYLOAD_LEN;
            return true;
        case FRM_PROC_CMD_GET_DEVICE_INFO:
            if (payloadBufSize < FRM_PROC_DEVICE_INFO_PAYLOAD_LEN) {
                return false;
            }
            payload[0] = txStore->deviceInfo.deviceType;
            (void)memcpy(&payload[1], txStore->deviceInfo.deviceSn, sizeof(txStore->deviceInfo.deviceSn));
            payload[14] = txStore->deviceInfo.fwVerMajor;
            payload[15] = txStore->deviceInfo.fwVerMinor;
            payload[16] = txStore->deviceInfo.fwVerPatch;
            payload[17] = txStore->deviceInfo.reserved;
            *payloadLen = FRM_PROC_DEVICE_INFO_PAYLOAD_LEN;
            return true;
        case FRM_PROC_CMD_GET_BLE_INFO:
            if (payloadBufSize < FRM_PROC_BLE_INFO_PAYLOAD_LEN) {
                return false;
            }
            (void)memcpy(payload, txStore->bleInfo.macString, FRM_PROC_BLE_INFO_PAYLOAD_LEN);
            *payloadLen = FRM_PROC_BLE_INFO_PAYLOAD_LEN;
            return true;
        case FRM_PROC_CMD_CPR_DATA:
            if (payloadBufSize < FRM_PROC_CPR_PAYLOAD_LEN) {
                return false;
            }
            frmProcDataWriteBe32(&payload[0], txStore->cprData.timestampMs);
            frmProcDataWriteBe16(&payload[4], txStore->cprData.pressRate);
            payload[6] = txStore->cprData.pressDepth;
            payload[7] = txStore->cprData.reboundDepth;
            payload[8] = txStore->cprData.interruptSeconds;
            frmProcDataWriteBe32(&payload[9], txStore->cprData.eventId);
            *payloadLen = FRM_PROC_CPR_PAYLOAD_LEN;
            return true;
        default:
            return false;
    }
}
/**************************End of file********************************/
