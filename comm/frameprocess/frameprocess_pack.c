/************************************************************************************
* @file     : frameprocess_pack.c
* @brief    : Frame process business response handlers.
***********************************************************************************/
#include "frameprocess_pack.h"

#include <string.h>

#include "main.h"
#include "Rep/system/system.h"
#include "frameprocess.h"

#define FRM_PROC_DEVICE_TYPE           0x00U
#define FRM_PROC_BATTERY_LEVEL         0x05U
#define FRM_PROC_BATTERY_VOLTAGE_10MV  370U
#define FRM_PROC_BATTERY_STATUS        0x03U
#define FRM_PROC_ELECTRODE_PRESENT     0x01U
#define FRM_PROC_ELECTRODE_MODEL       0x01U
#define FRM_PROC_ELECTRODE_ATTACHED    0x01U

static const uint8_t gFrmProcDeviceSn[13] = "MCOSGD32SN001";

static void frmProcPackBuildMacString(uint8_t *buffer)
{
    static const char gHexTable[] = "0123456789ABCDEF";
    const uint8_t lMac[6] = {
        MAC_ADDR0,
        MAC_ADDR1,
        MAC_ADDR2,
        MAC_ADDR3,
        MAC_ADDR4,
        MAC_ADDR5,
    };
    uint32_t lIndex;

    if (buffer == NULL) {
        return;
    }

    for (lIndex = 0U; lIndex < 6U; lIndex++) {
        buffer[lIndex * 2U] = (uint8_t)gHexTable[(lMac[lIndex] >> 4U) & 0x0FU];
        buffer[(lIndex * 2U) + 1U] = (uint8_t)gHexTable[lMac[lIndex] & 0x0FU];
    }
}

static void frmProcPackSetTxFlag(stFrmProcCtx *ctx, uint32_t flagMask, bool isUrgent)
{
    ctx->txStore.flags.value |= flagMask;
    if (isUrgent) {
        ctx->txUrgentMask |= flagMask;
    } else {
        ctx->txUrgentMask &= ~flagMask;
    }
}

bool frmProcPackOnRx(stFrmProcCtx *ctx, uint8_t cmd)
{
    if (ctx == NULL) {
        return false;
    }

    switch ((eFrmProcCmdType)cmd) {
        case FRM_PROC_CMD_HANDSHAKE:
            frmProcPackBuildMacString(ctx->txStore.handshake.macString);
            ctx->runFlags.bits.isLinkUp = 1U;
            ctx->hasReportedSelfCheck = true;
            ctx->txStore.selfCheck.cprModule = 0x01U;
            ctx->txStore.selfCheck.powerModule = 0x01U;
            ctx->txStore.selfCheck.commModule = 0x01U;
            frmProcPackSetTxFlag(ctx, FRM_PROC_TX_FLAG_HANDSHAKE_MASK, true);
            frmProcPackSetTxFlag(ctx, FRM_PROC_TX_FLAG_SELFCHECK_MASK, false);
            return true;
        case FRM_PROC_CMD_HEARTBEAT:
            ctx->txStore.heartbeat.batteryLevel = FRM_PROC_BATTERY_LEVEL;
            ctx->txStore.heartbeat.batteryVoltage10mV = FRM_PROC_BATTERY_VOLTAGE_10MV;
            ctx->txStore.heartbeat.batteryStatus = FRM_PROC_BATTERY_STATUS;
            ctx->txStore.heartbeat.electrodePresent = FRM_PROC_ELECTRODE_PRESENT;
            ctx->txStore.heartbeat.electrodeModel = FRM_PROC_ELECTRODE_MODEL;
            ctx->txStore.heartbeat.electrodeAttached = FRM_PROC_ELECTRODE_ATTACHED;
            frmProcPackSetTxFlag(ctx, FRM_PROC_TX_FLAG_HEARTBEAT_MASK, true);
            return true;
        case FRM_PROC_CMD_DISCONNECT:
            ctx->runFlags.bits.isLinkUp = 0U;
            ctx->hasReportedSelfCheck = false;
            ctx->txStore.disconnect.reserved = 0U;
            frmProcPackSetTxFlag(ctx, FRM_PROC_TX_FLAG_DISCONNECT_MASK, true);
            return true;
        case FRM_PROC_CMD_GET_DEVICE_INFO:
            ctx->txStore.deviceInfo.deviceType = FRM_PROC_DEVICE_TYPE;
            (void)memcpy(ctx->txStore.deviceInfo.deviceSn, gFrmProcDeviceSn, sizeof(gFrmProcDeviceSn));
            ctx->txStore.deviceInfo.fwVerMajor = FW_VER_MAJOR;
            ctx->txStore.deviceInfo.fwVerMinor = FW_VER_MINOR;
            ctx->txStore.deviceInfo.fwVerPatch = FW_VER_PATCH;
            ctx->txStore.deviceInfo.reserved = 0U;
            frmProcPackSetTxFlag(ctx, FRM_PROC_TX_FLAG_DEVICEINFO_MASK, false);
            return true;
        case FRM_PROC_CMD_GET_BLE_INFO:
            frmProcPackBuildMacString(ctx->txStore.bleInfo.macString);
            frmProcPackSetTxFlag(ctx, FRM_PROC_TX_FLAG_BLEINFO_MASK, false);
            return true;
        default:
            return true;
    }
}
/**************************End of file********************************/
