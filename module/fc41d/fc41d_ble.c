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

#include <ctype.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "fc41d_priv.h"

static eFc41dStatus fc41dBleBuildFmtCmd(char *cmdBuf, uint16_t cmdBufSize, const char *fmt, ...);
static eFc41dStatus fc41dBleExecOptionalCmdText(eFc41dMapType device, const char *cmdText);
static const char *fc41dBleGetDefInitCmdText(const stFc41dBleCfg *cfg);
static const char *fc41dBleGetDefStartCmdText(const stFc41dBleCfg *cfg);
static const char *fc41dBleGetDefStopCmdText(const stFc41dBleCfg *cfg);
static bool fc41dBleLineHasToken(const uint8_t *lineBuf, uint16_t lineLen, const char *token);

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

	return fc41dBleBuildFmtCmd(cmdBuf, cmdBufSize, "%s=%04X", cmdInfo->name, serviceUuid);
}

eFc41dStatus fc41dAtBuildBleGattCharCmd(char *cmdBuf, uint16_t cmdBufSize, uint16_t charUuid)
{
	const stFc41dAtCmdInfo *cmdInfo = fc41dAtGetCmdInfo(FC41D_AT_CATALOG_CMD_QBLEGATTSCHAR);

	if (cmdInfo == NULL) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	return fc41dBleBuildFmtCmd(cmdBuf, cmdBufSize, "%s=%04X", cmdInfo->name, charUuid);
}

eFc41dStatus fc41dAtBuildBleAdvParamCmd(char *cmdBuf, uint16_t cmdBufSize, uint16_t intervalMin, uint16_t intervalMax)
{
	const stFc41dAtCmdInfo *cmdInfo = fc41dAtGetCmdInfo(FC41D_AT_CATALOG_CMD_QBLEADVPARAM);

	if (cmdInfo == NULL) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	return fc41dBleBuildFmtCmd(cmdBuf, cmdBufSize, "%s=%u,%u",
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

	if (ctx->cfg.ble.workMode == FC41D_BLE_WORK_MODE_PERIPHERAL) {
		ctx->info.ble.state = FC41D_BLE_STATE_PERIPHERAL_INIT;
	} else if (ctx->cfg.ble.workMode == FC41D_BLE_WORK_MODE_CENTRAL) {
		ctx->info.ble.state = FC41D_BLE_STATE_CENTRAL_INIT;
	} else {
		ctx->info.ble.state = FC41D_BLE_STATE_IDLE;
	}
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

	if (ctx == NULL) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	if (ctx->info.ble.workMode != ctx->cfg.ble.workMode) {
		prevCfg = ctx->cfg.ble;
		prevCfg.workMode = ctx->info.ble.workMode;
		(void)fc41dBleExecOptionalCmdText(device, fc41dBleGetDefStopCmdText(&prevCfg));
		ctx->info.ble.workMode = ctx->cfg.ble.workMode;
		if (ctx->cfg.ble.workMode == FC41D_BLE_WORK_MODE_PERIPHERAL) {
			ctx->info.ble.state = FC41D_BLE_STATE_PERIPHERAL_INIT;
		} else if (ctx->cfg.ble.workMode == FC41D_BLE_WORK_MODE_CENTRAL) {
			ctx->info.ble.state = FC41D_BLE_STATE_CENTRAL_INIT;
		} else {
			ctx->info.ble.state = FC41D_BLE_STATE_IDLE;
		}
	}

	switch (ctx->info.ble.state) {
		case FC41D_BLE_STATE_IDLE:
		case FC41D_BLE_STATE_PERIPHERAL_WAIT_CONNECT:
		case FC41D_BLE_STATE_PERIPHERAL_CONNECTED:
		case FC41D_BLE_STATE_CENTRAL_WAIT_CONNECT:
		case FC41D_BLE_STATE_CENTRAL_CONNECTED:
			return FC41D_STATUS_OK;

		case FC41D_BLE_STATE_PERIPHERAL_DISCONNECTED:
			ctx->info.ble.state = FC41D_BLE_STATE_PERIPHERAL_ADV_START;
			return FC41D_STATUS_OK;

		case FC41D_BLE_STATE_CENTRAL_DISCONNECTED:
			ctx->info.ble.state = FC41D_BLE_STATE_CENTRAL_SCAN_START;
			return FC41D_STATUS_OK;

		case FC41D_BLE_STATE_PERIPHERAL_INIT:
			status = fc41dBleExecOptionalCmdText(device, fc41dBleGetDefInitCmdText(&ctx->cfg.ble));
			ctx->info.ble.state = (status == FC41D_STATUS_OK) ? FC41D_BLE_STATE_PERIPHERAL_ADV_START : FC41D_BLE_STATE_ERROR;
			return status;

		case FC41D_BLE_STATE_PERIPHERAL_ADV_START:
			status = fc41dBleExecOptionalCmdText(device, fc41dBleGetDefStartCmdText(&ctx->cfg.ble));
			ctx->info.ble.state = (status == FC41D_STATUS_OK) ? FC41D_BLE_STATE_PERIPHERAL_WAIT_CONNECT : FC41D_BLE_STATE_ERROR;
			return status;

		case FC41D_BLE_STATE_CENTRAL_INIT:
			status = fc41dBleExecOptionalCmdText(device, fc41dBleGetDefInitCmdText(&ctx->cfg.ble));
			ctx->info.ble.state = (status == FC41D_STATUS_OK) ? FC41D_BLE_STATE_CENTRAL_SCAN_START : FC41D_BLE_STATE_ERROR;
			return status;

		case FC41D_BLE_STATE_CENTRAL_SCAN_START:
			status = fc41dBleExecOptionalCmdText(device, fc41dBleGetDefStartCmdText(&ctx->cfg.ble));
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

	if (!fc41dBleLineHasToken(lineBuf, lineLen, "BLE")) {
		return;
	}

	if (fc41dBleLineHasToken(lineBuf, lineLen, "DISCONN")) {
		ctx->info.ble.state = (ctx->info.ble.workMode == FC41D_BLE_WORK_MODE_CENTRAL) ?
			FC41D_BLE_STATE_CENTRAL_DISCONNECTED : FC41D_BLE_STATE_PERIPHERAL_DISCONNECTED;
	} else if (fc41dBleLineHasToken(lineBuf, lineLen, "CONN")) {
		ctx->info.ble.state = (ctx->info.ble.workMode == FC41D_BLE_WORK_MODE_CENTRAL) ?
			FC41D_BLE_STATE_CENTRAL_CONNECTED : FC41D_BLE_STATE_PERIPHERAL_CONNECTED;
	}
}

static eFc41dStatus fc41dBleBuildFmtCmd(char *cmdBuf, uint16_t cmdBufSize, const char *fmt, ...)
{
	va_list args;
	int written;

	if ((cmdBuf == NULL) || (cmdBufSize == 0U) || (fmt == NULL)) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	va_start(args, fmt);
	written = vsnprintf(cmdBuf, cmdBufSize, fmt, args);
	va_end(args);

	if ((written < 0) || ((uint32_t)written >= cmdBufSize)) {
		cmdBuf[0] = '\0';
		return FC41D_STATUS_ERROR;
	}

	return FC41D_STATUS_OK;
}

static eFc41dStatus fc41dBleExecOptionalCmdText(eFc41dMapType device, const char *cmdText)
{
	if ((cmdText == NULL) || (cmdText[0] == '\0')) {
		return FC41D_STATUS_OK;
	}

	return fc41dExecAtText(device, cmdText, NULL, NULL);
}

static const char *fc41dBleGetDefInitCmdText(const stFc41dBleCfg *cfg)
{
	if (cfg == NULL) {
		return NULL;
	}

	if (cfg->initCmdText != NULL) {
		return cfg->initCmdText;
	}

	if (cfg->workMode == FC41D_BLE_WORK_MODE_PERIPHERAL) {
		return "AT+QBLEINIT=2";
	}

	if (cfg->workMode == FC41D_BLE_WORK_MODE_CENTRAL) {
		return "AT+QBLEINIT=1";
	}

	return NULL;
}

static const char *fc41dBleGetDefStartCmdText(const stFc41dBleCfg *cfg)
{
	if (cfg == NULL) {
		return NULL;
	}

	if (cfg->startCmdText != NULL) {
		return cfg->startCmdText;
	}

	if (cfg->workMode == FC41D_BLE_WORK_MODE_PERIPHERAL) {
		return "AT+QBLEADVSTART";
	}

	if (cfg->workMode == FC41D_BLE_WORK_MODE_CENTRAL) {
		return "AT+QBLESCAN=1";
	}

	return NULL;
}

static const char *fc41dBleGetDefStopCmdText(const stFc41dBleCfg *cfg)
{
	if (cfg == NULL) {
		return NULL;
	}

	if (cfg->stopCmdText != NULL) {
		return cfg->stopCmdText;
	}

	if (cfg->workMode == FC41D_BLE_WORK_MODE_PERIPHERAL) {
		return "AT+QBLEADVSTOP";
	}

	if (cfg->workMode == FC41D_BLE_WORK_MODE_CENTRAL) {
		return "AT+QBLESCAN=0";
	}

	return NULL;
}

static bool fc41dBleLineHasToken(const uint8_t *lineBuf, uint16_t lineLen, const char *token)
{
	uint16_t idx;
	uint32_t tokenLen;
	uint32_t matchIdx;

	if ((lineBuf == NULL) || (token == NULL)) {
		return false;
	}

	tokenLen = (uint32_t)strlen(token);
	if ((tokenLen == 0U) || ((uint32_t)lineLen < tokenLen)) {
		return false;
	}

	for (idx = 0U; idx <= (uint16_t)(lineLen - tokenLen); idx++) {
		for (matchIdx = 0U; matchIdx < tokenLen; matchIdx++) {
			if (toupper((int)lineBuf[idx + matchIdx]) != toupper((int)token[matchIdx])) {
				break;
			}
		}

		if (matchIdx == tokenLen) {
			return true;
		}
	}

	return false;
}

/**************************End of file********************************/