#include "tm1651_port.h"

#include <stddef.h>

#include "rep_config.h"

#if (REP_RTOS_SYSTEM == REP_RTOS_FREERTOS)
#include "FreeRTOS.h"
#include "task.h"
#endif

typedef struct stTm1651PortWriteCtx {
    const uint8_t *buffer;
    uint8_t length;
} stTm1651PortWriteCtx;

static bool gTm1651PortReady = false;

static eDrvStatus tm1651PortSoftIicInitAdpt(uint8_t bus);
static eDrvStatus tm1651PortSoftIicWriteFrameAdpt(uint8_t bus, const uint8_t *buffer, uint8_t length);
static eDrvStatus tm1651PortSoftIicWriteFrameAction(eDrvAnlogIicPortMap iic, stDrvAnlogIicBspInterface *bspInterface, void *context);
static const stTm1651PortIicInterface *tm1651PortGetBindIicIf(eTm1651PortIicType type);
static eDrvStatus tm1651PortEnsureReady(void);
static uint16_t tm1651PortGetHalfPeriodUs(const stDrvAnlogIicBspInterface *bspInterface);
static void tm1651PortDelayHalfPeriod(const stDrvAnlogIicBspInterface *bspInterface);
static void tm1651PortSendStart(eDrvAnlogIicPortMap iic, stDrvAnlogIicBspInterface *bspInterface);
static void tm1651PortSendStop(eDrvAnlogIicPortMap iic, stDrvAnlogIicBspInterface *bspInterface);
static eDrvStatus tm1651PortWriteByteLsbFirst(eDrvAnlogIicPortMap iic, stDrvAnlogIicBspInterface *bspInterface, uint8_t value);

static const stTm1651PortIicInterface gTm1651PortIicInterfaces[TM1651_PORT_IIC_TYPE_MAX] = {
    [TM1651_PORT_IIC_TYPE_SOFTWARE] = {
        .init = tm1651PortSoftIicInitAdpt,
        .writeFrame = tm1651PortSoftIicWriteFrameAdpt,
    },
};

static const stTm1651Cfg gTm1651PortDefCfg[TM1651_DEV_MAX] = {
    [TM1651_DEV0] = {
        .iicBind = {
            .type = TM1651_PORT_IIC_TYPE_SOFTWARE,
            .bus = (uint8_t)DRVANLOGIIC_TM,
            .iicIf = &gTm1651PortIicInterfaces[TM1651_PORT_IIC_TYPE_SOFTWARE],
        },
        .brightness = 7U,
        .digitCount = TM1651_DEFAULT_DIGIT_COUNT,
        .isDisplayOn = true,
    },
};

void tm1651PortGetDefBind(stTm1651PortIicBinding *bind)
{
    if (bind == NULL) {
        return;
    }

    bind->type = TM1651_PORT_IIC_TYPE_SOFTWARE;
    bind->bus = (uint8_t)DRVANLOGIIC_TM;
    bind->iicIf = tm1651PortGetBindIicIf(bind->type);
}

void tm1651PortGetDefCfg(eTm1651MapType device, stTm1651Cfg *cfg)
{
    if ((cfg == NULL) || ((uint32_t)device >= (uint32_t)TM1651_DEV_MAX)) {
        return;
    }

    *cfg = gTm1651PortDefCfg[device];
}

eDrvStatus tm1651PortSetSoftIic(stTm1651PortIicBinding *bind, eDrvAnlogIicPortMap iic)
{
    if ((bind == NULL) || ((uint8_t)iic >= (uint8_t)DRVANLOGIIC_MAX)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    bind->type = TM1651_PORT_IIC_TYPE_SOFTWARE;
    bind->bus = (uint8_t)iic;
    bind->iicIf = tm1651PortGetBindIicIf(bind->type);
    return DRV_STATUS_OK;
}

bool tm1651PortIsValidBind(const stTm1651PortIicBinding *bind)
{
    if (bind == NULL) {
        return false;
    }

    switch (bind->type) {
        case TM1651_PORT_IIC_TYPE_SOFTWARE:
            return (bind->bus < (uint8_t)DRVANLOGIIC_MAX) &&
                   (bind->iicIf == &gTm1651PortIicInterfaces[TM1651_PORT_IIC_TYPE_SOFTWARE]);
        default:
            return false;
    }
}

bool tm1651PortHasValidIicIf(const stTm1651PortIicBinding *bind)
{
    const stTm1651PortIicInterface *lInterface;

    lInterface = tm1651PortGetIicIf(bind);
    return (lInterface != NULL) &&
           (lInterface->init != NULL) &&
           (lInterface->writeFrame != NULL);
}

const stTm1651PortIicInterface *tm1651PortGetIicIf(const stTm1651PortIicBinding *bind)
{
    if (!tm1651PortIsValidBind(bind)) {
        return NULL;
    }

    return bind->iicIf;
}

eDrvStatus tm1651PortInit(void)
{
    eDrvStatus lStatus;

    if (gTm1651PortReady && tm1651IsReady(TM1651_DEV0)) {
        return DRV_STATUS_OK;
    }

    lStatus = tm1651Init(TM1651_DEV0);
    if (lStatus != DRV_STATUS_OK) {
        gTm1651PortReady = false;
        return lStatus;
    }

    gTm1651PortReady = true;
    return DRV_STATUS_OK;
}

bool tm1651PortIsReady(void)
{
    return gTm1651PortReady && tm1651IsReady(TM1651_DEV0);
}

eDrvStatus tm1651PortSetBrightness(uint8_t brightness)
{
    eDrvStatus lStatus;

    lStatus = tm1651PortEnsureReady();
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    return tm1651SetBrightness(TM1651_DEV0, brightness);
}

eDrvStatus tm1651PortSetDisplayOn(bool isDisplayOn)
{
    eDrvStatus lStatus;

    lStatus = tm1651PortEnsureReady();
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    return tm1651SetDisplayOn(TM1651_DEV0, isDisplayOn);
}

eDrvStatus tm1651PortDisplayDigits(uint8_t dig1, uint8_t dig2, uint8_t dig3, uint8_t dig4)
{
    eDrvStatus lStatus;

    lStatus = tm1651PortEnsureReady();
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    return tm1651DisplayDigits(TM1651_DEV0, dig1, dig2, dig3, dig4);
}

eDrvStatus tm1651PortClearDisplay(void)
{
    eDrvStatus lStatus;

    lStatus = tm1651PortEnsureReady();
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    return tm1651ClearDisplay(TM1651_DEV0);
}

eDrvStatus tm1651PortShowNone(void)
{
    eDrvStatus lStatus;

    lStatus = tm1651PortEnsureReady();
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    return tm1651ShowNone(TM1651_DEV0);
}

eDrvStatus tm1651PortShowNumber3(uint16_t value)
{
    eDrvStatus lStatus;

    lStatus = tm1651PortEnsureReady();
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    return tm1651ShowNumber3(TM1651_DEV0, value);
}

eDrvStatus tm1651PortShowError(uint16_t value)
{
    eDrvStatus lStatus;

    lStatus = tm1651PortEnsureReady();
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    return tm1651ShowError(TM1651_DEV0, value);
}

static const stTm1651PortIicInterface *tm1651PortGetBindIicIf(eTm1651PortIicType type)
{
    if ((uint32_t)type >= (uint32_t)TM1651_PORT_IIC_TYPE_MAX) {
        return NULL;
    }

    if (type == TM1651_PORT_IIC_TYPE_NONE) {
        return NULL;
    }

    return &gTm1651PortIicInterfaces[type];
}

static eDrvStatus tm1651PortEnsureReady(void)
{
    if (tm1651PortIsReady()) {
        return DRV_STATUS_OK;
    }

    return tm1651PortInit();
}

static uint16_t tm1651PortGetHalfPeriodUs(const stDrvAnlogIicBspInterface *bspInterface)
{
    if ((bspInterface == NULL) || (bspInterface->halfPeriodUs == 0U)) {
        return DRVANLOGIIC_DEFAULT_HALF_PERIOD_US;
    }

    return bspInterface->halfPeriodUs;
}

static void tm1651PortDelayHalfPeriod(const stDrvAnlogIicBspInterface *bspInterface)
{
    if ((bspInterface == NULL) || (bspInterface->delayUs == NULL)) {
        return;
    }

    bspInterface->delayUs(tm1651PortGetHalfPeriodUs(bspInterface));
}

static void tm1651PortSendStart(eDrvAnlogIicPortMap iic, stDrvAnlogIicBspInterface *bspInterface)
{
    bspInterface->setSda(iic, true);
    bspInterface->setScl(iic, true);
    tm1651PortDelayHalfPeriod(bspInterface);
    bspInterface->setSda(iic, false);
    tm1651PortDelayHalfPeriod(bspInterface);
    bspInterface->setScl(iic, false);
    tm1651PortDelayHalfPeriod(bspInterface);
}

static void tm1651PortSendStop(eDrvAnlogIicPortMap iic, stDrvAnlogIicBspInterface *bspInterface)
{
    bspInterface->setSda(iic, false);
    tm1651PortDelayHalfPeriod(bspInterface);
    bspInterface->setScl(iic, true);
    tm1651PortDelayHalfPeriod(bspInterface);
    bspInterface->setSda(iic, true);
    tm1651PortDelayHalfPeriod(bspInterface);
}

static eDrvStatus tm1651PortWriteByteLsbFirst(eDrvAnlogIicPortMap iic, stDrvAnlogIicBspInterface *bspInterface, uint8_t value)
{
    uint8_t lBitIndex;
    bool lIsAck;

    for (lBitIndex = 0U; lBitIndex < 8U; ++lBitIndex) {
        bspInterface->setSda(iic, (value & 0x01U) != 0U);
        tm1651PortDelayHalfPeriod(bspInterface);
        bspInterface->setScl(iic, true);
        tm1651PortDelayHalfPeriod(bspInterface);
        bspInterface->setScl(iic, false);
        tm1651PortDelayHalfPeriod(bspInterface);
        value >>= 1U;
    }

    bspInterface->setSda(iic, true);
    tm1651PortDelayHalfPeriod(bspInterface);
    bspInterface->setScl(iic, true);
    tm1651PortDelayHalfPeriod(bspInterface);
    lIsAck = !bspInterface->readSda(iic);
    bspInterface->setScl(iic, false);
    tm1651PortDelayHalfPeriod(bspInterface);
    return lIsAck ? DRV_STATUS_OK : DRV_STATUS_NACK;
}

static eDrvStatus tm1651PortSoftIicInitAdpt(uint8_t bus)
{
    if (bus >= (uint8_t)DRVANLOGIIC_MAX) {
        return DRV_STATUS_INVALID_PARAM;
    }

    return drvAnlogIicInit((eDrvAnlogIicPortMap)bus);
}

static eDrvStatus tm1651PortSoftIicWriteFrameAdpt(uint8_t bus, const uint8_t *buffer, uint8_t length)
{
    stTm1651PortWriteCtx lCtx;

    if ((bus >= (uint8_t)DRVANLOGIIC_MAX) || (buffer == NULL) || (length == 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    lCtx.buffer = buffer;
    lCtx.length = length;
    return drvAnlogIicBusAction((eDrvAnlogIicPortMap)bus, tm1651PortSoftIicWriteFrameAction, &lCtx);
}

static eDrvStatus tm1651PortSoftIicWriteFrameAction(eDrvAnlogIicPortMap iic, stDrvAnlogIicBspInterface *bspInterface, void *context)
{
    stTm1651PortWriteCtx *lCtx = (stTm1651PortWriteCtx *)context;
    uint8_t lIndex;
    eDrvStatus lStatus = DRV_STATUS_OK;

    if ((bspInterface == NULL) || (lCtx == NULL) || (lCtx->buffer == NULL) || (lCtx->length == 0U)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    tm1651PortSendStart(iic, bspInterface);
    for (lIndex = 0U; lIndex < lCtx->length; ++lIndex) {
        lStatus = tm1651PortWriteByteLsbFirst(iic, bspInterface, lCtx->buffer[lIndex]);
        if (lStatus != DRV_STATUS_OK) {
            break;
        }
    }
    tm1651PortSendStop(iic, bspInterface);
    return lStatus;
}
/**************************End of file********************************/
