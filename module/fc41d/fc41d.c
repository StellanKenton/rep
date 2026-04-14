/***********************************************************************************
* @file     : fc41d.c
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include <ctype.h>
#include <stddef.h>
#include <string.h>

#include "fc41d_priv.h"

static stFc41dCtx gFc41dCtx[FC41D_DEV_MAX];

__attribute__((weak)) void fc41dLoadPlatformDefaultCfg(eFc41dMapType device, stFc41dCfg *cfg)
{
	(void)device;

	if (cfg == NULL) {
		return;
	}

	cfg->ble.enableRx = true;
	cfg->wifi.enableRx = true;
	cfg->ble.rxOverwriteOnFull = true;
	cfg->wifi.rxOverwriteOnFull = true;
	cfg->ble.workMode = FC41D_BLE_WORK_MODE_DISABLED;
	cfg->ble.initCmdText = NULL;
	cfg->ble.startCmdText = NULL;
	cfg->ble.stopCmdText = NULL;
	cfg->wifi.workMode = FC41D_WIFI_WORK_MODE_DISABLED;
	cfg->wifi.initCmdText = NULL;
	cfg->wifi.startCmdText = NULL;
	cfg->wifi.stopCmdText = NULL;
	cfg->execGuardMs = 5000U;
	cfg->bootMode = FC41D_MODE_COMMAND;
}

__attribute__((weak)) bool fc41dPlatformIsValidAssemble(eFc41dMapType device)
{
	(void)device;
	return false;
}

__attribute__((weak)) uint32_t fc41dPlatformGetTickMs(void)
{
	return 0U;
}

__attribute__((weak)) eFc41dStatus fc41dPlatformInitTransport(eFc41dMapType device)
{
	(void)device;
	return FC41D_STATUS_NOT_READY;
}

__attribute__((weak)) void fc41dPlatformPollRx(eFc41dMapType device)
{
	(void)device;
}

__attribute__((weak)) eFc41dStatus fc41dPlatformInitRxBuffers(eFc41dMapType device, stRingBuffer *bleRxRb, stRingBuffer *wifiRxRb)
{
	(void)device;
	(void)bleRxRb;
	(void)wifiRxRb;
	return FC41D_STATUS_NOT_READY;
}

__attribute__((weak)) eFlowParserStrmSta fc41dPlatformInitAtStream(eFc41dMapType device, stFlowParserStream *stream,
																	flowparserStreamLineFunc urcHandler, void *urcUserCtx)
{
	(void)device;
	(void)stream;
	(void)urcHandler;
	(void)urcUserCtx;
	return FLOWPARSER_STREAM_NOT_INIT;
}

__attribute__((weak)) bool fc41dPlatformRouteLine(eFc41dMapType device, const uint8_t *lineBuf, uint16_t lineLen,
												   eFc41dRxChannel *channel, const uint8_t **payloadBuf, uint16_t *payloadLen)
{
	(void)device;
	(void)lineBuf;
	(void)lineLen;
	(void)channel;
	(void)payloadBuf;
	(void)payloadLen;
	return false;
}

static bool fc41dIsValidCfg(const stFc41dCfg *cfg);
static void fc41dSyncInfoFromCfg(stFc41dCtx *ctx);
static void fc41dEnsureDefCfgLoaded(stFc41dCtx *ctx, eFc41dMapType device);
static eFc41dStatus fc41dMapExecResult(eFc41dAtExecResult result);
static void fc41dClearExecState(stFc41dCtx *ctx);
static void fc41dAbortExec(stFc41dCtx *ctx, eFc41dAtExecResult result);
static eFc41dStatus fc41dSubmitExec(stFc41dCtx *ctx, eFc41dExecOwner owner, const uint8_t *cmdBuf, uint16_t cmdLen,
									 const uint8_t *payloadBuf, uint16_t payloadLen,
									 const stFc41dAtOpt *opt, stFc41dAtResp *resp);
static void fc41dBuildAtSpec(const stFc41dAtOpt *opt, stFlowParserSpec *spec);
static void fc41dExecLineHandler(void *userData, const uint8_t *lineBuf, uint16_t lineLen);
static void fc41dExecDoneHandler(void *userData, eFlowParserResult result);
static void fc41dHandleUrcLine(void *userData, const uint8_t *lineBuf, uint16_t lineLen);
static bool fc41dPushRxPayload(stFc41dCtx *ctx, eFc41dRxChannel channel, const uint8_t *payloadBuf, uint16_t payloadLen);
static eFc41dStatus fc41dServiceFlowParser(stFc41dCtx *ctx, eFc41dMapType device);
static void fc41dUpdateModeFromStates(stFc41dCtx *ctx);
static eFc41dTxnOwner fc41dMapTxnOwner(uint8_t owner);
static eFc41dTxnStage fc41dMapTxnStage(eFlowParserStage stage);

bool fc41dIsValidDevMap(eFc41dMapType device)
{
	return ((uint32_t)device < (uint32_t)FC41D_DEV_MAX);
}

stFc41dCtx *fc41dGetCtx(eFc41dMapType device)
{
	stFc41dCtx *ctx;

	if (!fc41dIsValidDevMap(device)) {
		return NULL;
	}

	ctx = &gFc41dCtx[device];
	fc41dEnsureDefCfgLoaded(ctx, device);
	return ctx;
}

bool fc41dIsReadyCtx(const stFc41dCtx *ctx)
{
	return (ctx != NULL) && ctx->info.isReady;
}

stRingBuffer *fc41dGetRxRbByChannel(stFc41dCtx *ctx, eFc41dRxChannel channel)
{
	if (ctx == NULL) {
		return NULL;
	}

	if (channel == FC41D_RX_CHANNEL_BLE) {
		return &ctx->bleRxRb;
	}
	if (channel == FC41D_RX_CHANNEL_WIFI) {
		return &ctx->wifiRxRb;
	}

	return NULL;
}

static bool fc41dIsValidCfg(const stFc41dCfg *cfg)
{
	return (cfg != NULL) &&
		   (cfg->bootMode <= FC41D_MODE_WIFI_DATA) &&
		   (cfg->ble.workMode <= FC41D_BLE_WORK_MODE_CENTRAL) &&
		   (cfg->wifi.workMode <= FC41D_WIFI_WORK_MODE_AP);
}

static void fc41dSyncInfoFromCfg(stFc41dCtx *ctx)
{
	if (ctx == NULL) {
		return;
	}

	fc41dBleSyncInfoFromCfg(ctx);
	fc41dWifiSyncInfoFromCfg(ctx);
}

static void fc41dEnsureDefCfgLoaded(stFc41dCtx *ctx, eFc41dMapType device)
{
	if ((ctx == NULL) || ctx->defCfgLoaded) {
		return;
	}

	(void)memset(ctx, 0, sizeof(*ctx));
	ctx->device = device;
	fc41dLoadPlatformDefaultCfg(device, &ctx->cfg);
	fc41dSyncInfoFromCfg(ctx);
	ctx->info.mode = ctx->cfg.bootMode;
	ctx->execResult = FC41D_AT_RESULT_NONE;
	ctx->lastExecResult = FC41D_AT_RESULT_NONE;
	ctx->defCfgLoaded = true;
}

eFc41dStatus fc41dGetDefCfg(eFc41dMapType device, stFc41dCfg *cfg)
{
	if ((cfg == NULL) || !fc41dIsValidDevMap(device)) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	fc41dLoadPlatformDefaultCfg(device, cfg);
	return FC41D_STATUS_OK;
}

eFc41dStatus fc41dGetCfg(eFc41dMapType device, stFc41dCfg *cfg)
{
	stFc41dCtx *ctx;

	if (cfg == NULL) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	ctx = fc41dGetCtx(device);
	if (ctx == NULL) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	*cfg = ctx->cfg;
	return FC41D_STATUS_OK;
}

eFc41dStatus fc41dSetCfg(eFc41dMapType device, const stFc41dCfg *cfg)
{
	stFc41dCtx *ctx;

	if (!fc41dIsValidCfg(cfg)) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	ctx = fc41dGetCtx(device);
	if (ctx == NULL) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	if (ctx->info.isReady) {
		flowparserStreamReset(&ctx->atStream);
	}
	fc41dAbortExec(ctx, FC41D_AT_RESULT_NONE);
	ctx->cfg = *cfg;
	fc41dSyncInfoFromCfg(ctx);
	ctx->info.mode = cfg->bootMode;
	ctx->info.isReady = false;
	return FC41D_STATUS_OK;
}

eFc41dStatus fc41dInit(eFc41dMapType device)
{
	stFc41dCtx *ctx;
	eFlowParserStrmSta fpStatus;
	eFc41dStatus status;

	ctx = fc41dGetCtx(device);
	if ((ctx == NULL) || !fc41dIsValidCfg(&ctx->cfg)) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	if (!fc41dPlatformIsValidAssemble(device)) {
		return FC41D_STATUS_NOT_READY;
	}

	status = fc41dPlatformInitTransport(device);
	if (status != FC41D_STATUS_OK) {
		return status;
	}

	status = fc41dPlatformInitRxBuffers(device, &ctx->bleRxRb, &ctx->wifiRxRb);
	if (status != FC41D_STATUS_OK) {
		return status;
	}

	fpStatus = fc41dPlatformInitAtStream(device, &ctx->atStream, fc41dHandleUrcLine, ctx);
	if (fpStatus != FLOWPARSER_STREAM_OK) {
		return FC41D_STATUS_ERROR;
	}

	ctx->execDone = false;
	ctx->execResult = FC41D_AT_RESULT_NONE;
	ctx->lastExecResult = FC41D_AT_RESULT_NONE;
	ctx->activeResp = NULL;
	ctx->execOwner = FC41D_EXEC_OWNER_NONE;
	fc41dSyncInfoFromCfg(ctx);
	ctx->info.isReady = true;
	ctx->info.mode = ctx->cfg.bootMode;
	return FC41D_STATUS_OK;
}

bool fc41dIsReady(eFc41dMapType device)
{
	return fc41dIsReadyCtx(fc41dGetCtx(device));
}

eFc41dStatus fc41dProcess(eFc41dMapType device)
{
	stFc41dCtx *ctx;
	eFc41dStatus status;

	ctx = fc41dGetCtx(device);
	if (!fc41dIsReadyCtx(ctx)) {
		return FC41D_STATUS_NOT_READY;
	}

	status = fc41dServiceFlowParser(ctx, device);
	if (status != FC41D_STATUS_OK) {
		return status;
	}

	if (flowparserStreamIsBusy(&ctx->atStream)) {
		return FC41D_STATUS_OK;
	}

	if ((ctx->execOwner == FC41D_EXEC_OWNER_API) && ctx->execDone) {
		fc41dReleaseExecOwner(ctx, FC41D_EXEC_OWNER_API);
	}

	status = fc41dBleProcessStateMachine(ctx, device);
	if (status != FC41D_STATUS_OK) {
		return status;
	}

	status = fc41dWifiProcessStateMachine(ctx, device);
	if (status != FC41D_STATUS_OK) {
		return status;
	}

	fc41dUpdateModeFromStates(ctx);
	return FC41D_STATUS_OK;
}

eFc41dStatus fc41dGetInfo(eFc41dMapType device, stFc41dInfo *info)
{
	stFc41dCtx *ctx;

	if (info == NULL) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	ctx = fc41dGetCtx(device);
	if (ctx == NULL) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	*info = ctx->info;
	return FC41D_STATUS_OK;
}

eFc41dStatus fc41dRecover(eFc41dMapType device)
{
	stFc41dCtx *ctx;

	ctx = fc41dGetCtx(device);
	if (!fc41dIsReadyCtx(ctx)) {
		return FC41D_STATUS_NOT_READY;
	}

	flowparserStreamReset(&ctx->atStream);
	fc41dAbortExec(ctx, FC41D_AT_RESULT_ERROR);
	fc41dBleResetState(ctx);
	fc41dWifiResetState(ctx);
	ctx->info.mode = ctx->cfg.bootMode;
	return FC41D_STATUS_OK;
}

bool fc41dExecAtIsBusy(eFc41dMapType device)
{
	stFc41dCtx *ctx = fc41dGetCtx(device);

	if (!fc41dIsReadyCtx(ctx)) {
		return false;
	}

	return (ctx->execOwner != FC41D_EXEC_OWNER_NONE) || flowparserStreamIsBusy(&ctx->atStream);
}

eFc41dAtExecResult fc41dGetLastExecResult(eFc41dMapType device)
{
	stFc41dCtx *ctx = fc41dGetCtx(device);
	return (ctx != NULL) ? ctx->lastExecResult : FC41D_AT_RESULT_NONE;
}

eFc41dStatus fc41dGetTxnStatus(eFc41dMapType device, stFc41dTxnStatus *status)
{
	stFc41dCtx *ctx;

	if (status == NULL) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	ctx = fc41dGetCtx(device);
	if (ctx == NULL) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	status->isBusy = ((ctx->execOwner != FC41D_EXEC_OWNER_NONE) || flowparserStreamIsBusy(&ctx->atStream));
	status->owner = fc41dMapTxnOwner(ctx->execOwner);
	status->stage = fc41dMapTxnStage(flowparserStreamGetStage(&ctx->atStream));
	status->currentResult = ctx->execResult;
	status->lastResult = ctx->lastExecResult;
	return FC41D_STATUS_OK;
}

void fc41dReleaseExecOwner(stFc41dCtx *ctx, eFc41dExecOwner owner)
{
	if ((ctx == NULL) || (ctx->execOwner != (uint8_t)owner)) {
		return;
	}

	fc41dClearExecState(ctx);
}

static void fc41dBuildAtSpec(const stFc41dAtOpt *opt, stFlowParserSpec *spec)
{
	stFc41dAtOpt baseOpt;

	if (spec == NULL) {
		return;
	}

	fc41dAtGetBaseOpt(&baseOpt);
	(void)memset(spec, 0, sizeof(*spec));
	spec->responseDonePatterns = baseOpt.responseDonePatterns;
	spec->responseDonePatternCnt = baseOpt.responseDonePatternCnt;
	spec->finalDonePatterns = baseOpt.finalDonePatterns;
	spec->finalDonePatternCnt = baseOpt.finalDonePatternCnt;
	spec->errorPatterns = baseOpt.errorPatterns;
	spec->errorPatternCnt = baseOpt.errorPatternCnt;
	spec->totalToutMs = baseOpt.totalToutMs;
	spec->responseToutMs = baseOpt.responseToutMs;
	spec->promptToutMs = baseOpt.promptToutMs;
	spec->finalToutMs = baseOpt.finalToutMs;
	spec->needPrompt = baseOpt.needPrompt;

	if (opt == NULL) {
		return;
	}

	if ((opt->responseDonePatterns != NULL) && (opt->responseDonePatternCnt > 0U)) {
		spec->responseDonePatterns = opt->responseDonePatterns;
		spec->responseDonePatternCnt = opt->responseDonePatternCnt;
	}
	if ((opt->finalDonePatterns != NULL) && (opt->finalDonePatternCnt > 0U)) {
		spec->finalDonePatterns = opt->finalDonePatterns;
		spec->finalDonePatternCnt = opt->finalDonePatternCnt;
	}
	if ((opt->errorPatterns != NULL) && (opt->errorPatternCnt > 0U)) {
		spec->errorPatterns = opt->errorPatterns;
		spec->errorPatternCnt = opt->errorPatternCnt;
	}
	if (opt->totalToutMs != 0U) {
		spec->totalToutMs = opt->totalToutMs;
	}
	if (opt->responseToutMs != 0U) {
		spec->responseToutMs = opt->responseToutMs;
	}
	if (opt->promptToutMs != 0U) {
		spec->promptToutMs = opt->promptToutMs;
	}
	if (opt->finalToutMs != 0U) {
		spec->finalToutMs = opt->finalToutMs;
	}
	spec->needPrompt = opt->needPrompt;
}

static eFc41dTxnOwner fc41dMapTxnOwner(uint8_t owner)
{
	switch ((eFc41dExecOwner)owner) {
		case FC41D_EXEC_OWNER_API:
			return FC41D_TXN_OWNER_API;
		case FC41D_EXEC_OWNER_BLE:
			return FC41D_TXN_OWNER_BLE;
		case FC41D_EXEC_OWNER_WIFI:
			return FC41D_TXN_OWNER_WIFI;
		case FC41D_EXEC_OWNER_NONE:
		default:
			return FC41D_TXN_OWNER_NONE;
	}
}

static eFc41dTxnStage fc41dMapTxnStage(eFlowParserStage stage)
{
	switch (stage) {
		case FLOWPARSER_STAGE_WAIT_RESPONSE:
			return FC41D_TXN_STAGE_WAIT_RESPONSE;
		case FLOWPARSER_STAGE_WAIT_PROMPT:
			return FC41D_TXN_STAGE_WAIT_PROMPT;
		case FLOWPARSER_STAGE_WAIT_FINAL:
			return FC41D_TXN_STAGE_WAIT_FINAL;
		case FLOWPARSER_STAGE_IDLE:
		default:
			return FC41D_TXN_STAGE_IDLE;
	}
}

static void fc41dClearExecState(stFc41dCtx *ctx)
{
	if (ctx == NULL) {
		return;
	}

	ctx->activeResp = NULL;
	ctx->execOwner = FC41D_EXEC_OWNER_NONE;
	ctx->execDone = false;
	ctx->execStartTick = 0U;
}

static void fc41dAbortExec(stFc41dCtx *ctx, eFc41dAtExecResult result)
{
	if (ctx == NULL) {
		return;
	}

	if ((ctx->activeResp != NULL) && (result != FC41D_AT_RESULT_NONE)) {
		ctx->activeResp->result = result;
	}

	if (result != FC41D_AT_RESULT_NONE) {
		ctx->execResult = result;
		ctx->lastExecResult = result;
	}

	fc41dClearExecState(ctx);
}

static eFc41dStatus fc41dSubmitExec(stFc41dCtx *ctx, eFc41dExecOwner owner, const uint8_t *cmdBuf, uint16_t cmdLen,
									 const uint8_t *payloadBuf, uint16_t payloadLen,
									 const stFc41dAtOpt *opt, stFc41dAtResp *resp)
{
	stFlowParserReq req;
	eFlowParserStrmSta fpStatus;

	if ((ctx == NULL) || (cmdBuf == NULL) || (cmdLen == 0U)) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	if (!fc41dIsReadyCtx(ctx)) {
		return FC41D_STATUS_NOT_READY;
	}

	if ((ctx->execOwner == FC41D_EXEC_OWNER_API) && ctx->execDone && !flowparserStreamIsBusy(&ctx->atStream)) {
		fc41dReleaseExecOwner(ctx, FC41D_EXEC_OWNER_API);
	}

	if ((ctx->execOwner != FC41D_EXEC_OWNER_NONE) || flowparserStreamIsBusy(&ctx->atStream)) {
		return FC41D_STATUS_BUSY;
	}

	if (resp != NULL) {
		resp->lineLen = 0U;
		resp->lineCount = 0U;
		resp->result = FC41D_AT_RESULT_NONE;
		if ((resp->lineBuf != NULL) && (resp->lineBufSize > 0U)) {
			resp->lineBuf[0] = '\0';
		}
	}

	fc41dBuildAtSpec(opt, &ctx->execSpec);
	if ((ctx->cfg.execGuardMs != 0U) && ((opt == NULL) || (opt->totalToutMs == 0U))) {
		ctx->execSpec.totalToutMs = ctx->cfg.execGuardMs;
	}

	(void)memset(&req, 0, sizeof(req));
	req.spec = &ctx->execSpec;
	req.cmdBuf = cmdBuf;
	req.cmdLen = cmdLen;
	req.payloadBuf = payloadBuf;
	req.payloadLen = payloadLen;
	req.lineHandler = fc41dExecLineHandler;
	req.doneHandler = fc41dExecDoneHandler;
	req.userData = ctx;

	ctx->activeResp = resp;
	ctx->execDone = false;
	ctx->execResult = FC41D_AT_RESULT_NONE;
	ctx->execOwner = (uint8_t)owner;
	ctx->execStartTick = fc41dPlatformGetTickMs();

	fpStatus = flowparserStreamSubmit(&ctx->atStream, &req);
	if (fpStatus == FLOWPARSER_STREAM_BUSY) {
		fc41dClearExecState(ctx);
		ctx->execResult = FC41D_AT_RESULT_BUSY;
		return FC41D_STATUS_BUSY;
	}
	if (fpStatus != FLOWPARSER_STREAM_OK) {
		fc41dAbortExec(ctx, (fpStatus == FLOWPARSER_STREAM_PORT_FAIL) ? FC41D_AT_RESULT_SEND_FAIL : FC41D_AT_RESULT_ERROR);
		return FC41D_STATUS_ERROR;
	}

	return FC41D_STATUS_OK;
}

static void fc41dExecLineHandler(void *userData, const uint8_t *lineBuf, uint16_t lineLen)
{
	stFc41dCtx *ctx = (stFc41dCtx *)userData;
	uint16_t copyLen;

	if ((ctx == NULL) || (ctx->activeResp == NULL)) {
		return;
	}

	ctx->activeResp->lineCount++;
	if ((ctx->activeResp->lineBuf == NULL) || (ctx->activeResp->lineBufSize == 0U)) {
		return;
	}

	copyLen = lineLen;
	if (copyLen >= ctx->activeResp->lineBufSize) {
		copyLen = (uint16_t)(ctx->activeResp->lineBufSize - 1U);
	}

	if ((lineBuf != NULL) && (copyLen > 0U)) {
		(void)memcpy(ctx->activeResp->lineBuf, lineBuf, copyLen);
	}
	ctx->activeResp->lineBuf[copyLen] = '\0';
	ctx->activeResp->lineLen = copyLen;
}

static void fc41dExecDoneHandler(void *userData, eFlowParserResult result)
{
	stFc41dCtx *ctx = (stFc41dCtx *)userData;

	if (ctx == NULL) {
		return;
	}

	switch (result) {
		case FLOWPARSER_RESULT_OK:
			ctx->execResult = FC41D_AT_RESULT_OK;
			break;
		case FLOWPARSER_RESULT_ERROR:
			ctx->execResult = FC41D_AT_RESULT_ERROR;
			break;
		case FLOWPARSER_RESULT_TIMEOUT:
			ctx->execResult = FC41D_AT_RESULT_TIMEOUT;
			break;
		case FLOWPARSER_RESULT_OVERFLOW:
			ctx->execResult = FC41D_AT_RESULT_OVERFLOW;
			break;
		case FLOWPARSER_RESULT_SEND_FAIL:
			ctx->execResult = FC41D_AT_RESULT_SEND_FAIL;
			break;
		default:
			ctx->execResult = FC41D_AT_RESULT_ERROR;
			break;
	}

	if (ctx->activeResp != NULL) {
		ctx->activeResp->result = ctx->execResult;
	}
	ctx->execDone = true;
	ctx->lastExecResult = ctx->execResult;
}

static eFc41dStatus fc41dMapExecResult(eFc41dAtExecResult result)
{
	switch (result) {
		case FC41D_AT_RESULT_OK:
			return FC41D_STATUS_OK;
		case FC41D_AT_RESULT_TIMEOUT:
			return FC41D_STATUS_TIMEOUT;
		case FC41D_AT_RESULT_BUSY:
			return FC41D_STATUS_BUSY;
		default:
			return FC41D_STATUS_ERROR;
	}
}

static eFc41dStatus fc41dServiceFlowParser(stFc41dCtx *ctx, eFc41dMapType device)
{
	eFlowParserStrmSta fpStatus;

	if (ctx == NULL) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	fc41dPlatformPollRx(device);
	fpStatus = flowparserStreamProc(&ctx->atStream);
	if ((fpStatus == FLOWPARSER_STREAM_OK) || (fpStatus == FLOWPARSER_STREAM_EMPTY) || (fpStatus == FLOWPARSER_STREAM_BUSY)) {
		return FC41D_STATUS_OK;
	}

	return FC41D_STATUS_ERROR;
}

eFc41dStatus fc41dExecOptionalCmdText(eFc41dMapType device, eFc41dExecOwner owner, const char *cmdText)
{
	stFc41dCtx *ctx;
	uint8_t cmdBuf[256U];
	uint32_t cmdLen;
	eFc41dStatus status;

	if ((cmdText == NULL) || (cmdText[0] == '\0')) {
		return FC41D_STATUS_OK;
	}

	ctx = fc41dGetCtx(device);
	if (!fc41dIsReadyCtx(ctx)) {
		return FC41D_STATUS_NOT_READY;
	}

	if (ctx->execOwner == (uint8_t)owner) {
		if (flowparserStreamIsBusy(&ctx->atStream) || !ctx->execDone) {
			return FC41D_STATUS_BUSY;
		}

		status = fc41dMapExecResult(ctx->execResult);
		fc41dReleaseExecOwner(ctx, owner);
		return status;
	}

	if ((ctx->execOwner != FC41D_EXEC_OWNER_NONE) || flowparserStreamIsBusy(&ctx->atStream)) {
		return FC41D_STATUS_BUSY;
	}

	cmdLen = (uint32_t)strlen(cmdText);
	if ((cmdLen == 0U) || (cmdLen >= (uint32_t)(sizeof(cmdBuf) - 2U))) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	(void)memcpy(cmdBuf, cmdText, cmdLen);
	if ((cmdLen < 2U) || (cmdBuf[cmdLen - 2U] != '\r') || (cmdBuf[cmdLen - 1U] != '\n')) {
		cmdBuf[cmdLen++] = '\r';
		cmdBuf[cmdLen++] = '\n';
	}

	status = fc41dSubmitExec(ctx, owner, cmdBuf, (uint16_t)cmdLen, NULL, 0U, NULL, NULL);
	return (status == FC41D_STATUS_OK) ? FC41D_STATUS_BUSY : status;
}

eFc41dStatus fc41dExecAt(eFc41dMapType device, const uint8_t *cmdBuf, uint16_t cmdLen,
						 const uint8_t *payloadBuf, uint16_t payloadLen,
						 const stFc41dAtOpt *opt, stFc41dAtResp *resp)
{
	stFc41dCtx *ctx;

	if ((cmdBuf == NULL) || (cmdLen == 0U)) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	ctx = fc41dGetCtx(device);
	return fc41dSubmitExec(ctx, FC41D_EXEC_OWNER_API, cmdBuf, cmdLen, payloadBuf, payloadLen, opt, resp);
}

bool fc41dLineHasToken(const uint8_t *lineBuf, uint16_t lineLen, const char *token)
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

static bool fc41dPushRxPayload(stFc41dCtx *ctx, eFc41dRxChannel channel, const uint8_t *payloadBuf, uint16_t payloadLen)
{
	stRingBuffer *rb;
	uint32_t freeLen;
	uint32_t cap;
	uint32_t dropped = 0U;
	bool overwrite;
	uint32_t written;

	if ((ctx == NULL) || (payloadBuf == NULL) || (payloadLen == 0U)) {
		return false;
	}

	rb = fc41dGetRxRbByChannel(ctx, channel);
	if (rb == NULL) {
		return false;
	}

	overwrite = (channel == FC41D_RX_CHANNEL_BLE) ? ctx->cfg.ble.rxOverwriteOnFull : ctx->cfg.wifi.rxOverwriteOnFull;
	freeLen = ringBufferGetFree(rb);
	cap = ringBufferGetCapacity(rb);

	if (!overwrite && (freeLen < (uint32_t)payloadLen)) {
		dropped = payloadLen;
		written = 0U;
	} else if (overwrite) {
		if ((uint32_t)payloadLen > freeLen) {
			if ((uint32_t)payloadLen >= cap) {
				dropped = ringBufferGetUsed(rb) + (uint32_t)payloadLen - cap;
			} else {
				dropped = (uint32_t)payloadLen - freeLen;
			}
		}
		written = ringBufferWriteOverwrite(rb, payloadBuf, payloadLen);
	} else {
		written = ringBufferWrite(rb, payloadBuf, payloadLen);
	}

	if (channel == FC41D_RX_CHANNEL_BLE) {
		fc41dBleAccumulateRxStats(ctx, written, dropped);
	} else if (channel == FC41D_RX_CHANNEL_WIFI) {
		fc41dWifiAccumulateRxStats(ctx, written, dropped);
	}

	return written == (uint32_t)payloadLen;
}

static void fc41dUpdateModeFromStates(stFc41dCtx *ctx)
{
	if (ctx == NULL) {
		return;
	}

	if ((ctx->info.ble.state == FC41D_BLE_STATE_PERIPHERAL_CONNECTED) ||
		(ctx->info.ble.state == FC41D_BLE_STATE_CENTRAL_CONNECTED)) {
		ctx->info.mode = FC41D_MODE_BLE_DATA;
		return;
	}

	if ((ctx->info.wifi.state == FC41D_WIFI_STATE_STA_CONNECTED) ||
		(ctx->info.wifi.state == FC41D_WIFI_STATE_AP_ACTIVE)) {
		ctx->info.mode = FC41D_MODE_WIFI_DATA;
		return;
	}

	ctx->info.mode = FC41D_MODE_COMMAND;
}

static void fc41dHandleUrcLine(void *userData, const uint8_t *lineBuf, uint16_t lineLen)
{
	stFc41dCtx *ctx = (stFc41dCtx *)userData;
	const uint8_t *payloadBuf = NULL;
	uint16_t payloadLen = 0U;
	eFc41dRxChannel channel = FC41D_RX_CHANNEL_NONE;

	if (ctx == NULL) {
		return;
	}

	ctx->info.urcLineCount++;
	fc41dBleUpdateLinkStateByUrc(ctx, lineBuf, lineLen);
	fc41dWifiUpdateLinkStateByUrc(ctx, lineBuf, lineLen);
	if (!fc41dPlatformRouteLine(ctx->device, lineBuf, lineLen, &channel, &payloadBuf, &payloadLen)) {
		ctx->info.unknownUrcLineCount++;
		return;
	}

	if (((channel == FC41D_RX_CHANNEL_BLE) && !ctx->cfg.ble.enableRx) ||
		((channel == FC41D_RX_CHANNEL_WIFI) && !ctx->cfg.wifi.enableRx)) {
		return;
	}

	if (fc41dPushRxPayload(ctx, channel, payloadBuf, payloadLen)) {
		ctx->info.lastRxChannel = channel;
	}

	fc41dUpdateModeFromStates(ctx);
}

/**************************End of file********************************/
