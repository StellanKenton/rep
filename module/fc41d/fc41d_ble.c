/***********************************************************************************
* @file     : fc41d_ble.c
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "fc41d_ble.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "fc41d_priv.h"

static const char *const gFc41dBlePeripheralInitCmdSeq[] = {
	"AT+QBLEINIT=2",
};

static const char *const gFc41dBleCentralInitCmdSeq[] = {
	"AT+QBLEINIT=1",
};

static const char *const gFc41dBlePeripheralStartCmdSeq[] = {
	"AT+QBLEADVSTART",
};

static const char *const gFc41dBleCentralStartCmdSeq[] = {
	"AT+QBLESCAN=1",
};

static const char *const gFc41dBlePeripheralStopCmdSeq[] = {
	"AT+QBLEADVSTOP",
};

static const char *const gFc41dBleCentralStopCmdSeq[] = {
	"AT+QBLESCAN=0",
};

static const char *const gFc41dBleConnectedUrcPatterns[] = {
	"+BLECONN*",
};

static const char *const gFc41dBleDisconnectedUrcPatterns[] = {
	"+BLEDISCONN*",
};

static const char *const *fc41dBleGetDefInitCmdSeq(const stFc41dBleCfg *cfg, uint8_t *cmdSeqLen);
static const char *const *fc41dBleGetDefStartCmdSeq(const stFc41dBleCfg *cfg, uint8_t *cmdSeqLen);
static const char *const *fc41dBleGetDefStopCmdSeq(const stFc41dBleCfg *cfg, uint8_t *cmdSeqLen);
static bool fc41dBleMatchUrcPatterns(const uint8_t *lineBuf, uint16_t lineLen, const char *const *patterns, uint8_t patternCnt);

eFc41dStatus fc41dAtBuildBleNameCmd(char *cmdBuf, uint16_t cmdBufSize, const char *name)
{
	if (name == NULL) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	return fc41dAtBuildSetCmd(cmdBuf, cmdBufSize, FC41D_AT_CATALOG_CMD_QBLENAME, name);
}

eFc41dStatus fc41dAtBuildBleGattServiceCmd(char *cmdBuf, uint16_t cmdBufSize, uint16_t serviceUuid)
{
	const stFc41dAtCmdInfo *cmdInfo = fc41dAtGetCmdInfo(FC41D_AT_CATALOG_CMD_QBLEGATTSSRV);

	if (cmdInfo == NULL) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	return fc41dBuildFmtCmd(cmdBuf, cmdBufSize, "%s=%04X", cmdInfo->name, serviceUuid);
}

eFc41dStatus fc41dAtBuildBleGattCharCmd(char *cmdBuf, uint16_t cmdBufSize, uint16_t charUuid)
{
	const stFc41dAtCmdInfo *cmdInfo = fc41dAtGetCmdInfo(FC41D_AT_CATALOG_CMD_QBLEGATTSCHAR);

	if (cmdInfo == NULL) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	return fc41dBuildFmtCmd(cmdBuf, cmdBufSize, "%s=%04X", cmdInfo->name, charUuid);
}

eFc41dStatus fc41dAtBuildBleAdvParamCmd(char *cmdBuf, uint16_t cmdBufSize, uint16_t intervalMin, uint16_t intervalMax)
{
	const stFc41dAtCmdInfo *cmdInfo = fc41dAtGetCmdInfo(FC41D_AT_CATALOG_CMD_QBLEADVPARAM);

	if (cmdInfo == NULL) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	return fc41dBuildFmtCmd(cmdBuf, cmdBufSize, "%s=%u,%u",
							   cmdInfo->name,
							   (unsigned int)intervalMin,
							   (unsigned int)intervalMax);
}

eFc41dStatus fc41dAtBuildBleAdvDataCmd(char *cmdBuf, uint16_t cmdBufSize, const uint8_t *advData, uint16_t advLen)
{
	const stFc41dAtCmdInfo *cmdInfo = fc41dAtGetCmdInfo(FC41D_AT_CATALOG_CMD_QBLEADVDATA);
	uint16_t idx;
	uint32_t offset;
	int written;

	if ((cmdInfo == NULL) || (cmdBuf == NULL) || (cmdBufSize == 0U) || ((advLen > 0U) && (advData == NULL))) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	written = snprintf(cmdBuf, cmdBufSize, "%s=", cmdInfo->name);
	if ((written < 0) || ((uint32_t)written >= cmdBufSize)) {
		return FC41D_STATUS_ERROR;
	}

	offset = (uint32_t)written;
	for (idx = 0U; idx < advLen; idx++) {
		written = snprintf(&cmdBuf[offset], (size_t)(cmdBufSize - offset), "%02X", advData[idx]);
		if ((written < 0) || ((uint32_t)written >= (uint32_t)(cmdBufSize - offset))) {
			cmdBuf[0] = '\0';
			return FC41D_STATUS_ERROR;
		}
		offset += (uint32_t)written;
	}

	return FC41D_STATUS_OK;
}

eFc41dStatus fc41dBleSend(eFc41dMapType device, const uint8_t *data, uint16_t len)
{
	const stFc41dAtCmdInfo *cmdInfo;
	stFc41dAtOpt opt;
	char cmdBuf[48U];
	uint32_t cmdLen;

	if ((data == NULL) || (len == 0U)) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	cmdInfo = fc41dAtGetCmdInfo(FC41D_AT_CATALOG_CMD_QBLEGATTSNTFY);
	if (cmdInfo == NULL) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	if (fc41dBuildFmtCmd(cmdBuf, (uint16_t)sizeof(cmdBuf), "%s=%u\r\n", cmdInfo->name, (unsigned int)len) != FC41D_STATUS_OK) {
		return FC41D_STATUS_ERROR;
	}

	cmdLen = (uint32_t)strlen(cmdBuf);
	fc41dAtGetBaseOpt(&opt);
	opt.needPrompt = true;
	opt.finalToutMs = 5000U;
	return fc41dExecAt(device, (const uint8_t *)cmdBuf, (uint16_t)cmdLen, data, len, &opt, NULL);
}

stRingBuffer *fc41dBleGetRxRingBuffer(eFc41dMapType device)
{
	return fc41dGetRxRbByChannel(fc41dGetCtx(device), FC41D_RX_CHANNEL_BLE);
}

uint32_t fc41dBleRead(eFc41dMapType device, uint8_t *buffer, uint32_t length)
{
	stRingBuffer *rb = fc41dBleGetRxRingBuffer(device);
	return (rb != NULL) ? ringBufferRead(rb, buffer, length) : 0U;
}

uint32_t fc41dBlePeek(eFc41dMapType device, uint8_t *buffer, uint32_t length)
{
	stRingBuffer *rb = fc41dBleGetRxRingBuffer(device);
	return (rb != NULL) ? ringBufferPeek(rb, buffer, length) : 0U;
}

uint32_t fc41dBleDiscard(eFc41dMapType device, uint32_t length)
{
	stRingBuffer *rb = fc41dBleGetRxRingBuffer(device);
	return (rb != NULL) ? ringBufferDiscard(rb, length) : 0U;
}

eFc41dStatus fc41dBleClearRx(eFc41dMapType device)
{
	stRingBuffer *rb = fc41dBleGetRxRingBuffer(device);

	if (rb == NULL) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	return (ringBufferReset(rb) == RINGBUFFER_OK) ? FC41D_STATUS_OK : FC41D_STATUS_ERROR;
}

void fc41dBleSyncInfoFromCfg(stFc41dCtx *ctx)
{
	if (ctx == NULL) {
		return;
	}

	ctx->info.ble.enableRx = ctx->cfg.ble.enableRx;
	ctx->info.ble.workMode = ctx->cfg.ble.workMode;
	fc41dBleResetState(ctx);
}


void fc41dBleResetState(stFc41dCtx *ctx)
{
	if (ctx == NULL) {
		return;
	}

	if (ctx->cfg.ble.workMode == FC41D_BLE_WORK_MODE_PERIPHERAL) {
		ctx->info.ble.state = FC41D_BLE_STATE_PERIPHERAL_INIT;
	} else if (ctx->cfg.ble.workMode == FC41D_BLE_WORK_MODE_CENTRAL) {
		ctx->info.ble.state = FC41D_BLE_STATE_CENTRAL_INIT;
	} else {
		ctx->info.ble.state = FC41D_BLE_STATE_IDLE;
	}
	ctx->bleCmdStepIndex = 0U;
}

void fc41dBleAccumulateRxStats(stFc41dCtx *ctx, uint32_t written, uint32_t dropped)
{
	if (ctx == NULL) {
		return;
	}

	ctx->info.ble.rxRoutedBytes += written;
	ctx->info.ble.rxDroppedBytes += dropped;
}

eFc41dStatus fc41dBleProcessStateMachine(stFc41dCtx *ctx, eFc41dMapType device)
{
	eFc41dStatus status;
	stFc41dBleCfg prevCfg;
	const char *const *cmdSeq;
	uint8_t cmdSeqLen;

	if (ctx == NULL) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	if (ctx->info.ble.workMode != ctx->cfg.ble.workMode) {
		prevCfg = ctx->cfg.ble;
		prevCfg.workMode = ctx->info.ble.workMode;
		cmdSeq = fc41dBleGetDefStopCmdSeq(&prevCfg, &cmdSeqLen);
		status = fc41dExecCmdSeq(device, FC41D_EXEC_OWNER_BLE, cmdSeq, cmdSeqLen, &ctx->bleCmdStepIndex);
		if (status == FC41D_STATUS_BUSY) {
			return FC41D_STATUS_OK;
		}
		if (status != FC41D_STATUS_OK) {
			ctx->info.ble.state = FC41D_BLE_STATE_ERROR;
			return status;
		}
		ctx->info.ble.workMode = ctx->cfg.ble.workMode;
		fc41dBleResetState(ctx);
	}

	switch (ctx->info.ble.state) {
		case FC41D_BLE_STATE_IDLE:
		case FC41D_BLE_STATE_PERIPHERAL_WAIT_CONNECT:
		case FC41D_BLE_STATE_PERIPHERAL_CONNECTED:
		case FC41D_BLE_STATE_CENTRAL_WAIT_CONNECT:
		case FC41D_BLE_STATE_CENTRAL_CONNECTED:
			return FC41D_STATUS_OK;

		case FC41D_BLE_STATE_PERIPHERAL_DISCONNECTED:
			ctx->bleCmdStepIndex = 0U;
			ctx->info.ble.state = FC41D_BLE_STATE_PERIPHERAL_ADV_START;
			return FC41D_STATUS_OK;

		case FC41D_BLE_STATE_CENTRAL_DISCONNECTED:
			ctx->bleCmdStepIndex = 0U;
			ctx->info.ble.state = FC41D_BLE_STATE_CENTRAL_SCAN_START;
			return FC41D_STATUS_OK;

		case FC41D_BLE_STATE_PERIPHERAL_INIT:
			cmdSeq = fc41dBleGetDefInitCmdSeq(&ctx->cfg.ble, &cmdSeqLen);
			status = fc41dExecCmdSeq(device, FC41D_EXEC_OWNER_BLE, cmdSeq, cmdSeqLen, &ctx->bleCmdStepIndex);
			if (status == FC41D_STATUS_BUSY) {
				return FC41D_STATUS_OK;
			}
			ctx->info.ble.state = (status == FC41D_STATUS_OK) ? FC41D_BLE_STATE_PERIPHERAL_ADV_START : FC41D_BLE_STATE_ERROR;
			return status;

		case FC41D_BLE_STATE_PERIPHERAL_ADV_START:
			cmdSeq = fc41dBleGetDefStartCmdSeq(&ctx->cfg.ble, &cmdSeqLen);
			status = fc41dExecCmdSeq(device, FC41D_EXEC_OWNER_BLE, cmdSeq, cmdSeqLen, &ctx->bleCmdStepIndex);
			if (status == FC41D_STATUS_BUSY) {
				return FC41D_STATUS_OK;
			}
			ctx->info.ble.state = (status == FC41D_STATUS_OK) ? FC41D_BLE_STATE_PERIPHERAL_WAIT_CONNECT : FC41D_BLE_STATE_ERROR;
			return status;

		case FC41D_BLE_STATE_CENTRAL_INIT:
			cmdSeq = fc41dBleGetDefInitCmdSeq(&ctx->cfg.ble, &cmdSeqLen);
			status = fc41dExecCmdSeq(device, FC41D_EXEC_OWNER_BLE, cmdSeq, cmdSeqLen, &ctx->bleCmdStepIndex);
			if (status == FC41D_STATUS_BUSY) {
				return FC41D_STATUS_OK;
			}
			ctx->info.ble.state = (status == FC41D_STATUS_OK) ? FC41D_BLE_STATE_CENTRAL_SCAN_START : FC41D_BLE_STATE_ERROR;
			return status;

		case FC41D_BLE_STATE_CENTRAL_SCAN_START:
			cmdSeq = fc41dBleGetDefStartCmdSeq(&ctx->cfg.ble, &cmdSeqLen);
			status = fc41dExecCmdSeq(device, FC41D_EXEC_OWNER_BLE, cmdSeq, cmdSeqLen, &ctx->bleCmdStepIndex);
			if (status == FC41D_STATUS_BUSY) {
				return FC41D_STATUS_OK;
			}
			ctx->info.ble.state = (status == FC41D_STATUS_OK) ? FC41D_BLE_STATE_CENTRAL_WAIT_CONNECT : FC41D_BLE_STATE_ERROR;
			return status;

		case FC41D_BLE_STATE_ERROR:
		default:
			return FC41D_STATUS_ERROR;
	}
}

void fc41dBleUpdateLinkStateByUrc(stFc41dCtx *ctx, const uint8_t *lineBuf, uint16_t lineLen)
{
	if (ctx == NULL) {
		return;
	}

	if ((lineBuf == NULL) || (lineLen == 0U) || (lineBuf[0] != '+')) {
		return;
	}

	if (fc41dBleMatchUrcPatterns(lineBuf, lineLen, gFc41dBleDisconnectedUrcPatterns,
			(uint8_t)(sizeof(gFc41dBleDisconnectedUrcPatterns) / sizeof(gFc41dBleDisconnectedUrcPatterns[0])))) {
		ctx->bleCmdStepIndex = 0U;
		ctx->info.ble.state = (ctx->info.ble.workMode == FC41D_BLE_WORK_MODE_CENTRAL) ?
			FC41D_BLE_STATE_CENTRAL_DISCONNECTED : FC41D_BLE_STATE_PERIPHERAL_DISCONNECTED;
	} else if (fc41dBleMatchUrcPatterns(lineBuf, lineLen, gFc41dBleConnectedUrcPatterns,
			(uint8_t)(sizeof(gFc41dBleConnectedUrcPatterns) / sizeof(gFc41dBleConnectedUrcPatterns[0])))) {
		ctx->info.ble.state = (ctx->info.ble.workMode == FC41D_BLE_WORK_MODE_CENTRAL) ?
			FC41D_BLE_STATE_CENTRAL_CONNECTED : FC41D_BLE_STATE_PERIPHERAL_CONNECTED;
	}
}

static bool fc41dBleMatchUrcPatterns(const uint8_t *lineBuf, uint16_t lineLen, const char *const *patterns, uint8_t patternCnt)
{
	uint8_t idx;

	for (idx = 0U; idx < patternCnt; idx++) {
		if (fc41dAtMatchPattern(lineBuf, lineLen, patterns[idx])) {
			return true;
		}
	}

	return false;
}

static const char *const *fc41dBleGetDefInitCmdSeq(const stFc41dBleCfg *cfg, uint8_t *cmdSeqLen)
{
	if (cmdSeqLen == NULL) {
		return NULL;
	}

	*cmdSeqLen = 0U;
	if (cfg == NULL) {
		return NULL;
	}

	if ((cfg->initCmdSeq != NULL) && (cfg->initCmdSeqLen > 0U)) {
		*cmdSeqLen = cfg->initCmdSeqLen;
		return cfg->initCmdSeq;
	}

	if (cfg->workMode == FC41D_BLE_WORK_MODE_PERIPHERAL) {
		*cmdSeqLen = (uint8_t)(sizeof(gFc41dBlePeripheralInitCmdSeq) / sizeof(gFc41dBlePeripheralInitCmdSeq[0]));
		return gFc41dBlePeripheralInitCmdSeq;
	}

	if (cfg->workMode == FC41D_BLE_WORK_MODE_CENTRAL) {
		*cmdSeqLen = (uint8_t)(sizeof(gFc41dBleCentralInitCmdSeq) / sizeof(gFc41dBleCentralInitCmdSeq[0]));
		return gFc41dBleCentralInitCmdSeq;
	}

	return NULL;
}

static const char *const *fc41dBleGetDefStartCmdSeq(const stFc41dBleCfg *cfg, uint8_t *cmdSeqLen)
{
	if (cmdSeqLen == NULL) {
		return NULL;
	}

	*cmdSeqLen = 0U;
	if (cfg == NULL) {
		return NULL;
	}

	if ((cfg->startCmdSeq != NULL) && (cfg->startCmdSeqLen > 0U)) {
		*cmdSeqLen = cfg->startCmdSeqLen;
		return cfg->startCmdSeq;
	}

	if (cfg->workMode == FC41D_BLE_WORK_MODE_PERIPHERAL) {
		*cmdSeqLen = (uint8_t)(sizeof(gFc41dBlePeripheralStartCmdSeq) / sizeof(gFc41dBlePeripheralStartCmdSeq[0]));
		return gFc41dBlePeripheralStartCmdSeq;
	}

	if (cfg->workMode == FC41D_BLE_WORK_MODE_CENTRAL) {
		*cmdSeqLen = (uint8_t)(sizeof(gFc41dBleCentralStartCmdSeq) / sizeof(gFc41dBleCentralStartCmdSeq[0]));
		return gFc41dBleCentralStartCmdSeq;
	}

	return NULL;
}

static const char *const *fc41dBleGetDefStopCmdSeq(const stFc41dBleCfg *cfg, uint8_t *cmdSeqLen)
{
	if (cmdSeqLen == NULL) {
		return NULL;
	}

	*cmdSeqLen = 0U;
	if (cfg == NULL) {
		return NULL;
	}

	if ((cfg->stopCmdSeq != NULL) && (cfg->stopCmdSeqLen > 0U)) {
		*cmdSeqLen = cfg->stopCmdSeqLen;
		return cfg->stopCmdSeq;
	}

	if (cfg->workMode == FC41D_BLE_WORK_MODE_PERIPHERAL) {
		*cmdSeqLen = (uint8_t)(sizeof(gFc41dBlePeripheralStopCmdSeq) / sizeof(gFc41dBlePeripheralStopCmdSeq[0]));
		return gFc41dBlePeripheralStopCmdSeq;
	}

	if (cfg->workMode == FC41D_BLE_WORK_MODE_CENTRAL) {
		*cmdSeqLen = (uint8_t)(sizeof(gFc41dBleCentralStopCmdSeq) / sizeof(gFc41dBleCentralStopCmdSeq[0]));
		return gFc41dBleCentralStopCmdSeq;
	}

	return NULL;
}

/**************************End of file********************************/
