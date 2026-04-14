/***********************************************************************************
* @file     : fc41d_wifi.c
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "fc41d_wifi.h"

#include <stddef.h>
#include <string.h>

#include "fc41d_priv.h"

static const char *const gFc41dWifiStaConnectedUrcPatterns[] = {
	"+STACONNECTED*",
	"+STAGOTIP*",
};

static const char *const gFc41dWifiStaDisconnectedUrcPatterns[] = {
	"+STADISCONN*",
};

static const char *const gFc41dWifiApActiveUrcPatterns[] = {
	"+SOFTAPSTART*",
};

static const char *const gFc41dWifiApStoppedUrcPatterns[] = {
	"+SOFTAPSTOP*",
};

static const char *const gFc41dWifiStaStopCmdSeq[] = {
	"AT+QSTASTOP",
};

static const char *const gFc41dWifiApStopCmdSeq[] = {
	"AT+QSOFTAPSTOP",
};

static const char *const *fc41dWifiGetDefInitCmdSeq(const stFc41dWifiCfg *cfg, uint8_t *cmdSeqLen);
static const char *const *fc41dWifiGetDefStartCmdSeq(const stFc41dWifiCfg *cfg, uint8_t *cmdSeqLen);
static const char *const *fc41dWifiGetDefStopCmdSeq(const stFc41dWifiCfg *cfg, uint8_t *cmdSeqLen);
static bool fc41dWifiMatchUrcPatterns(const uint8_t *lineBuf, uint16_t lineLen, const char *const *patterns, uint8_t patternCnt);

eFc41dStatus fc41dWifiSend(eFc41dMapType device, uint8_t linkId, const uint8_t *data, uint16_t len)
{
	const stFc41dAtCmdInfo *cmdInfo;
	stFc41dAtOpt opt;
	char cmdBuf[48U];
	uint32_t cmdLen;

	if ((data == NULL) || (len == 0U)) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	cmdInfo = fc41dAtGetCmdInfo(FC41D_AT_CATALOG_CMD_QISEND);
	if (cmdInfo == NULL) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	if (fc41dBuildFmtCmd(cmdBuf, (uint16_t)sizeof(cmdBuf), "%s=%u,%u\r\n",
			cmdInfo->name, (unsigned int)linkId, (unsigned int)len) != FC41D_STATUS_OK) {
		return FC41D_STATUS_ERROR;
	}

	cmdLen = (uint32_t)strlen(cmdBuf);
	fc41dAtGetBaseOpt(&opt);
	opt.needPrompt = true;
	opt.finalToutMs = 5000U;
	return fc41dExecAt(device, (const uint8_t *)cmdBuf, (uint16_t)cmdLen, data, len, &opt, NULL);
}

stRingBuffer *fc41dWifiGetRxRingBuffer(eFc41dMapType device)
{
	return fc41dGetRxRbByChannel(fc41dGetCtx(device), FC41D_RX_CHANNEL_WIFI);
}

uint32_t fc41dWifiRead(eFc41dMapType device, uint8_t *buffer, uint32_t length)
{
	stRingBuffer *rb = fc41dWifiGetRxRingBuffer(device);
	return (rb != NULL) ? ringBufferRead(rb, buffer, length) : 0U;
}

uint32_t fc41dWifiPeek(eFc41dMapType device, uint8_t *buffer, uint32_t length)
{
	stRingBuffer *rb = fc41dWifiGetRxRingBuffer(device);
	return (rb != NULL) ? ringBufferPeek(rb, buffer, length) : 0U;
}

uint32_t fc41dWifiDiscard(eFc41dMapType device, uint32_t length)
{
	stRingBuffer *rb = fc41dWifiGetRxRingBuffer(device);
	return (rb != NULL) ? ringBufferDiscard(rb, length) : 0U;
}

eFc41dStatus fc41dWifiClearRx(eFc41dMapType device)
{
	stRingBuffer *rb = fc41dWifiGetRxRingBuffer(device);

	if (rb == NULL) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	return (ringBufferReset(rb) == RINGBUFFER_OK) ? FC41D_STATUS_OK : FC41D_STATUS_ERROR;
}

void fc41dWifiSyncInfoFromCfg(stFc41dCtx *ctx)
{
	if (ctx == NULL) {
		return;
	}

	ctx->info.wifi.enableRx = ctx->cfg.wifi.enableRx;
	ctx->info.wifi.workMode = ctx->cfg.wifi.workMode;
	fc41dWifiResetState(ctx);
}


void fc41dWifiResetState(stFc41dCtx *ctx)
{
	if (ctx == NULL) {
		return;
	}

	if (ctx->cfg.wifi.workMode == FC41D_WIFI_WORK_MODE_STA) {
		ctx->info.wifi.state = FC41D_WIFI_STATE_STA_INIT;
	} else if (ctx->cfg.wifi.workMode == FC41D_WIFI_WORK_MODE_AP) {
		ctx->info.wifi.state = FC41D_WIFI_STATE_AP_INIT;
	} else {
		ctx->info.wifi.state = FC41D_WIFI_STATE_IDLE;
	}
	ctx->wifiCmdStepIndex = 0U;
	ctx->wifiStaStartIssued = false;
	ctx->wifiReconnectCount = 0U;
	ctx->wifiReconnectTick = 0U;
	ctx->info.wifi.reconnectRetryCount = 0U;
}

void fc41dWifiAccumulateRxStats(stFc41dCtx *ctx, uint32_t written, uint32_t dropped)
{
	if (ctx == NULL) {
		return;
	}

	ctx->info.wifi.rxRoutedBytes += written;
	ctx->info.wifi.rxDroppedBytes += dropped;
}

eFc41dStatus fc41dWifiProcessStateMachine(stFc41dCtx *ctx, eFc41dMapType device)
{
	eFc41dStatus status;
	stFc41dWifiCfg prevCfg;
	const char *const *cmdSeq;
	uint8_t cmdSeqLen;
	uint32_t nowTick;

	if (ctx == NULL) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	if (ctx->info.wifi.workMode != ctx->cfg.wifi.workMode) {
		prevCfg = ctx->cfg.wifi;
		prevCfg.workMode = ctx->info.wifi.workMode;
		cmdSeq = fc41dWifiGetDefStopCmdSeq(&prevCfg, &cmdSeqLen);
		status = fc41dExecCmdSeq(device, FC41D_EXEC_OWNER_WIFI, cmdSeq, cmdSeqLen, &ctx->wifiCmdStepIndex);
		if (status == FC41D_STATUS_BUSY) {
			return FC41D_STATUS_OK;
		}
		if (status != FC41D_STATUS_OK) {
			ctx->info.wifi.state = FC41D_WIFI_STATE_ERROR;
			return status;
		}
		ctx->info.wifi.workMode = ctx->cfg.wifi.workMode;
		fc41dWifiResetState(ctx);
	}

	switch (ctx->info.wifi.state) {
		case FC41D_WIFI_STATE_IDLE:
		case FC41D_WIFI_STATE_STA_CONNECTED:
		case FC41D_WIFI_STATE_AP_ACTIVE:
		case FC41D_WIFI_STATE_AP_STOPPED:
			return FC41D_STATUS_OK;
		case FC41D_WIFI_STATE_STA_DISCONNECTED:
			if (!ctx->cfg.wifi.autoReconnect) {
				return FC41D_STATUS_OK;
			}
			if ((ctx->cfg.wifi.reconnectMaxRetries != 0U) &&
				(ctx->wifiReconnectCount >= ctx->cfg.wifi.reconnectMaxRetries)) {
				return FC41D_STATUS_OK;
			}
			nowTick = fc41dPlatformGetTickMs();
			if ((ctx->cfg.wifi.reconnectIntervalMs != 0U) &&
				((uint32_t)(nowTick - ctx->wifiReconnectTick) < ctx->cfg.wifi.reconnectIntervalMs)) {
				return FC41D_STATUS_OK;
			}
			ctx->wifiCmdStepIndex = 0U;
			ctx->wifiStaStartIssued = false;
			ctx->info.wifi.state = FC41D_WIFI_STATE_STA_CONNECTING;
			return FC41D_STATUS_OK;
		case FC41D_WIFI_STATE_STA_INIT:
			cmdSeq = fc41dWifiGetDefInitCmdSeq(&ctx->cfg.wifi, &cmdSeqLen);
			status = fc41dExecCmdSeq(device, FC41D_EXEC_OWNER_WIFI, cmdSeq, cmdSeqLen, &ctx->wifiCmdStepIndex);
			if (status == FC41D_STATUS_BUSY) {
				return FC41D_STATUS_OK;
			}
			ctx->wifiStaStartIssued = false;
			ctx->info.wifi.state = (status == FC41D_STATUS_OK) ? FC41D_WIFI_STATE_STA_CONNECTING : FC41D_WIFI_STATE_ERROR;
			return status;
		case FC41D_WIFI_STATE_STA_CONNECTING:
			if (ctx->wifiStaStartIssued) {
				return FC41D_STATUS_OK;
			}
			cmdSeq = fc41dWifiGetDefStartCmdSeq(&ctx->cfg.wifi, &cmdSeqLen);
			if (cmdSeqLen == 0U) {
				ctx->info.wifi.state = FC41D_WIFI_STATE_STA_DISCONNECTED;
				return FC41D_STATUS_OK;
			}
			status = fc41dExecCmdSeq(device, FC41D_EXEC_OWNER_WIFI, cmdSeq, cmdSeqLen, &ctx->wifiCmdStepIndex);
			if (status == FC41D_STATUS_BUSY) {
				return FC41D_STATUS_OK;
			}
			ctx->wifiStaStartIssued = (status == FC41D_STATUS_OK);
			ctx->info.wifi.state = (status == FC41D_STATUS_OK) ? FC41D_WIFI_STATE_STA_CONNECTING : FC41D_WIFI_STATE_ERROR;
			return status;
		case FC41D_WIFI_STATE_AP_INIT:
			cmdSeq = fc41dWifiGetDefInitCmdSeq(&ctx->cfg.wifi, &cmdSeqLen);
			status = fc41dExecCmdSeq(device, FC41D_EXEC_OWNER_WIFI, cmdSeq, cmdSeqLen, &ctx->wifiCmdStepIndex);
			if (status == FC41D_STATUS_BUSY) {
				return FC41D_STATUS_OK;
			}
			ctx->info.wifi.state = (status == FC41D_STATUS_OK) ? FC41D_WIFI_STATE_AP_STARTING : FC41D_WIFI_STATE_ERROR;
			return status;
		case FC41D_WIFI_STATE_AP_STARTING:
			cmdSeq = fc41dWifiGetDefStartCmdSeq(&ctx->cfg.wifi, &cmdSeqLen);
			if (cmdSeqLen == 0U) {
				ctx->info.wifi.state = FC41D_WIFI_STATE_AP_STOPPED;
				return FC41D_STATUS_OK;
			}
			status = fc41dExecCmdSeq(device, FC41D_EXEC_OWNER_WIFI, cmdSeq, cmdSeqLen, &ctx->wifiCmdStepIndex);
			if (status == FC41D_STATUS_BUSY) {
				return FC41D_STATUS_OK;
			}
			ctx->info.wifi.state = (status == FC41D_STATUS_OK) ? FC41D_WIFI_STATE_AP_ACTIVE : FC41D_WIFI_STATE_ERROR;
			return status;
		case FC41D_WIFI_STATE_ERROR:
		default:
			return FC41D_STATUS_ERROR;
	}
}

void fc41dWifiUpdateLinkStateByUrc(stFc41dCtx *ctx, const uint8_t *lineBuf, uint16_t lineLen)
{
	if (ctx == NULL) {
		return;
	}

	if ((lineBuf == NULL) || (lineLen == 0U) || (lineBuf[0] != '+')) {
		return;
	}

	if (fc41dWifiMatchUrcPatterns(lineBuf, lineLen, gFc41dWifiStaDisconnectedUrcPatterns,
			(uint8_t)(sizeof(gFc41dWifiStaDisconnectedUrcPatterns) / sizeof(gFc41dWifiStaDisconnectedUrcPatterns[0])))) {
		ctx->wifiStaStartIssued = false;
		ctx->wifiCmdStepIndex = 0U;
		ctx->wifiReconnectTick = fc41dPlatformGetTickMs();
		if (ctx->wifiReconnectCount < UINT8_MAX) {
			ctx->wifiReconnectCount++;
		}
		ctx->info.wifi.reconnectRetryCount = ctx->wifiReconnectCount;
		ctx->info.wifi.state = FC41D_WIFI_STATE_STA_DISCONNECTED;
	} else if (fc41dWifiMatchUrcPatterns(lineBuf, lineLen, gFc41dWifiStaConnectedUrcPatterns,
			(uint8_t)(sizeof(gFc41dWifiStaConnectedUrcPatterns) / sizeof(gFc41dWifiStaConnectedUrcPatterns[0])))) {
		ctx->wifiStaStartIssued = false;
		ctx->wifiReconnectCount = 0U;
		ctx->info.wifi.reconnectRetryCount = 0U;
		ctx->info.wifi.state = FC41D_WIFI_STATE_STA_CONNECTED;
	} else if (fc41dWifiMatchUrcPatterns(lineBuf, lineLen, gFc41dWifiApActiveUrcPatterns,
			(uint8_t)(sizeof(gFc41dWifiApActiveUrcPatterns) / sizeof(gFc41dWifiApActiveUrcPatterns[0])))) {
		ctx->info.wifi.state = FC41D_WIFI_STATE_AP_ACTIVE;
	} else if (fc41dWifiMatchUrcPatterns(lineBuf, lineLen, gFc41dWifiApStoppedUrcPatterns,
			(uint8_t)(sizeof(gFc41dWifiApStoppedUrcPatterns) / sizeof(gFc41dWifiApStoppedUrcPatterns[0])))) {
		ctx->info.wifi.state = (ctx->info.wifi.workMode == FC41D_WIFI_WORK_MODE_AP) ?
			FC41D_WIFI_STATE_AP_STOPPED : FC41D_WIFI_STATE_STA_DISCONNECTED;
	}
}

static bool fc41dWifiMatchUrcPatterns(const uint8_t *lineBuf, uint16_t lineLen, const char *const *patterns, uint8_t patternCnt)
{
	uint8_t idx;

	for (idx = 0U; idx < patternCnt; idx++) {
		if (fc41dAtMatchPattern(lineBuf, lineLen, patterns[idx])) {
			return true;
		}
	}

	return false;
}

static const char *const *fc41dWifiGetDefInitCmdSeq(const stFc41dWifiCfg *cfg, uint8_t *cmdSeqLen)
{
	if (cmdSeqLen == NULL) {
		return NULL;
	}

	*cmdSeqLen = 0U;
	if ((cfg != NULL) && (cfg->initCmdSeq != NULL) && (cfg->initCmdSeqLen > 0U)) {
		*cmdSeqLen = cfg->initCmdSeqLen;
		return cfg->initCmdSeq;
	}

	return NULL;
}

static const char *const *fc41dWifiGetDefStartCmdSeq(const stFc41dWifiCfg *cfg, uint8_t *cmdSeqLen)
{
	if (cmdSeqLen == NULL) {
		return NULL;
	}

	*cmdSeqLen = 0U;
	if ((cfg != NULL) && (cfg->startCmdSeq != NULL) && (cfg->startCmdSeqLen > 0U)) {
		*cmdSeqLen = cfg->startCmdSeqLen;
		return cfg->startCmdSeq;
	}

	return NULL;
}

static const char *const *fc41dWifiGetDefStopCmdSeq(const stFc41dWifiCfg *cfg, uint8_t *cmdSeqLen)
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

	if (cfg->workMode == FC41D_WIFI_WORK_MODE_STA) {
		*cmdSeqLen = (uint8_t)(sizeof(gFc41dWifiStaStopCmdSeq) / sizeof(gFc41dWifiStaStopCmdSeq[0]));
		return gFc41dWifiStaStopCmdSeq;
	}

	if (cfg->workMode == FC41D_WIFI_WORK_MODE_AP) {
		*cmdSeqLen = (uint8_t)(sizeof(gFc41dWifiApStopCmdSeq) / sizeof(gFc41dWifiApStopCmdSeq[0]));
		return gFc41dWifiApStopCmdSeq;
	}

	return NULL;
}

/**************************End of file********************************/
