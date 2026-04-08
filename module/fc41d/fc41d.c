#include "fc41d.h"

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

	cfg->enableBleRx = true;
	cfg->enableWifiRx = true;
	cfg->bleRxOverwriteOnFull = true;
	cfg->wifiRxOverwriteOnFull = true;
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
static void fc41dEnsureDefCfgLoaded(stFc41dCtx *ctx, eFc41dMapType device);
static eFc41dStatus fc41dMapExecResult(eFc41dAtExecResult result);
static void fc41dBuildAtSpec(const stFc41dAtOpt *opt, stFlowParserSpec *spec);
static void fc41dExecLineHandler(void *userData, const uint8_t *lineBuf, uint16_t lineLen);
static void fc41dExecDoneHandler(void *userData, eFlowParserResult result);
static void fc41dHandleUrcLine(void *userData, const uint8_t *lineBuf, uint16_t lineLen);
static bool fc41dPushRxPayload(stFc41dCtx *ctx, eFc41dRxChannel channel, const uint8_t *payloadBuf, uint16_t payloadLen);

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
	return (cfg != NULL) && (cfg->bootMode <= FC41D_MODE_WIFI_DATA);
}

static void fc41dEnsureDefCfgLoaded(stFc41dCtx *ctx, eFc41dMapType device)
{
	if ((ctx == NULL) || ctx->defCfgLoaded) {
		return;
	}

	(void)memset(ctx, 0, sizeof(*ctx));
	fc41dLoadPlatformDefaultCfg(device, &ctx->cfg);
	ctx->info.enableBleRx = ctx->cfg.enableBleRx;
	ctx->info.enableWifiRx = ctx->cfg.enableWifiRx;
	ctx->info.mode = ctx->cfg.bootMode;
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

	ctx->cfg = *cfg;
	ctx->info.enableBleRx = cfg->enableBleRx;
	ctx->info.enableWifiRx = cfg->enableWifiRx;
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
	ctx->activeResp = NULL;
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
	eFlowParserStrmSta fpStatus;

	ctx = fc41dGetCtx(device);
	if (!fc41dIsReadyCtx(ctx)) {
		return FC41D_STATUS_NOT_READY;
	}

	fc41dPlatformPollRx(device);
	fpStatus = flowparserStreamProc(&ctx->atStream);
	if ((fpStatus == FLOWPARSER_STREAM_OK) || (fpStatus == FLOWPARSER_STREAM_EMPTY)) {
		return FC41D_STATUS_OK;
	}

	return FC41D_STATUS_ERROR;
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

eFc41dStatus fc41dExecAt(eFc41dMapType device, const uint8_t *cmdBuf, uint16_t cmdLen,
						 const uint8_t *payloadBuf, uint16_t payloadLen,
						 const stFc41dAtOpt *opt, stFc41dAtResp *resp)
{
	stFc41dCtx *ctx;
	stFlowParserSpec spec;
	stFlowParserReq req;
	eFlowParserStrmSta fpStatus;
	uint32_t startTick;
	uint32_t guardMs;

	if ((cmdBuf == NULL) || (cmdLen == 0U)) {
		return FC41D_STATUS_INVALID_PARAM;
	}

	ctx = fc41dGetCtx(device);
	if (!fc41dIsReadyCtx(ctx)) {
		return FC41D_STATUS_NOT_READY;
	}

	if (flowparserStreamIsBusy(&ctx->atStream)) {
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

	fc41dBuildAtSpec(opt, &spec);
	(void)memset(&req, 0, sizeof(req));
	req.spec = &spec;
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

	fpStatus = flowparserStreamSubmit(&ctx->atStream, &req);
	if (fpStatus == FLOWPARSER_STREAM_BUSY) {
		ctx->activeResp = NULL;
		return FC41D_STATUS_BUSY;
	}
	if (fpStatus != FLOWPARSER_STREAM_OK) {
		ctx->activeResp = NULL;
		return FC41D_STATUS_ERROR;
	}

	startTick = fc41dPlatformGetTickMs();
	guardMs = (ctx->cfg.execGuardMs != 0U) ? ctx->cfg.execGuardMs : (spec.totalToutMs + 50U);
	if (guardMs == 0U) {
		guardMs = 5000U;
	}

	while (flowparserStreamIsBusy(&ctx->atStream)) {
		(void)fc41dProcess(device);
		if ((uint32_t)(fc41dPlatformGetTickMs() - startTick) >= guardMs) {
			ctx->execResult = FC41D_AT_RESULT_TIMEOUT;
			ctx->execDone = true;
			break;
		}
	}

	if ((resp != NULL) && (resp->result == FC41D_AT_RESULT_NONE)) {
		resp->result = ctx->execResult;
	}

	ctx->activeResp = NULL;
	return fc41dMapExecResult(ctx->execResult);
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

	overwrite = (channel == FC41D_RX_CHANNEL_BLE) ? ctx->cfg.bleRxOverwriteOnFull : ctx->cfg.wifiRxOverwriteOnFull;
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
		ctx->info.bleRxRoutedBytes += written;
		ctx->info.bleRxDroppedBytes += dropped;
	} else if (channel == FC41D_RX_CHANNEL_WIFI) {
		ctx->info.wifiRxRoutedBytes += written;
		ctx->info.wifiRxDroppedBytes += dropped;
	}

	return written == (uint32_t)payloadLen;
}

static void fc41dHandleUrcLine(void *userData, const uint8_t *lineBuf, uint16_t lineLen)
{
	stFc41dCtx *ctx = (stFc41dCtx *)userData;
	const uint8_t *payloadBuf = NULL;
	uint16_t payloadLen = 0U;
	eFc41dRxChannel channel = FC41D_RX_CHANNEL_NONE;
	eFc41dMapType device;

	if (ctx == NULL) {
		return;
	}

	device = (eFc41dMapType)(ctx - &gFc41dCtx[0]);
	ctx->info.urcLineCount++;
	if (!fc41dPlatformRouteLine(device, lineBuf, lineLen, &channel, &payloadBuf, &payloadLen)) {
		ctx->info.unknownUrcLineCount++;
		return;
	}

	if (((channel == FC41D_RX_CHANNEL_BLE) && !ctx->cfg.enableBleRx) ||
		((channel == FC41D_RX_CHANNEL_WIFI) && !ctx->cfg.enableWifiRx)) {
		return;
	}

	if (fc41dPushRxPayload(ctx, channel, payloadBuf, payloadLen)) {
		ctx->info.lastRxChannel = channel;
	}
}
/**************************End of file********************************/
