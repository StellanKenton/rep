#include "pca9535_port.h"

#include <stdbool.h>

#include "main.h"
#include "rep_config.h"
#include "log.h"
#include "drvanlogiic_port.h"

#if (REP_RTOS_SYSTEM == REP_RTOS_FREERTOS)
#include "FreeRTOS.h"
#include "task.h"
#endif

static bool gPca9535PortCycleCntReady = false;
static bool gPca9535PortReady = false;
static uint16_t gPca9535PortShowMask = 0U;

static void pca9535PortEnableCycleCnt(void);
static void pca9535PortEnableGpioClock(GPIO_TypeDef *gpioPort);
static eDrvStatus pca9535PortApplyShowMask(uint16_t mask);
static eDrvStatus pca9535PortEnsureReady(void);
static eDrvStatus pca9535PortSoftIicInitAdpt(uint8_t bus);
static eDrvStatus pca9535PortSoftIicWriteRegAdpt(uint8_t bus, uint8_t address, const uint8_t *regBuf, uint16_t regLen, const uint8_t *buffer, uint16_t length);
static eDrvStatus pca9535PortSoftIicReadRegAdpt(uint8_t bus, uint8_t address, const uint8_t *regBuf, uint16_t regLen, uint8_t *buffer, uint16_t length);

#define PCA9535_PORT_LOG_TAG                 "Pca9535Port"

static const stPca9535PortIicInterface gPca9535PortSoftIicInterface = {
    .init = pca9535PortSoftIicInitAdpt,
    .writeReg = pca9535PortSoftIicWriteRegAdpt,
    .readReg = pca9535PortSoftIicReadRegAdpt,
};

static const stPca9535Cfg gPca9535PortDefCfg[PCA9535_DEV_MAX] = {
    [PCA9535_DEV0] = {
        .linkId = DRVANLOGIIC_PCA,
        .address = PCA9535_IIC_ADDRESS_HLL,
        .outputValue = 0xFFFFU,
        .polarityMask = 0x0000U,
        .directionMask = 0xFFFFU,
        .resetBeforeInit = true,
    },
};

void pca9535LoadPlatformDefaultCfg(ePca9535MapType device, stPca9535Cfg *cfg)
{
    if ((cfg == NULL) || ((uint32_t)device >= (uint32_t)PCA9535_DEV_MAX)) {
        return;
    }

    *cfg = gPca9535PortDefCfg[device];
}

const stPca9535IicInterface *pca9535GetPlatformIicInterface(const stPca9535Cfg *cfg)
{
    if (!pca9535PortIsValidCfg(cfg)) {
        return NULL;
    }

    return &gPca9535PortSoftIicInterface;
}

bool pca9535PlatformIsValidCfg(const stPca9535Cfg *cfg)
{
    return pca9535PortIsValidCfg(cfg);
}

void pca9535PlatformResetInit(void)
{
    GPIO_InitTypeDef lGpioInit = {0};

    pca9535PortEnableGpioClock(PCA9535_RESET_GPIO_Port);

    lGpioInit.Pin = PCA9535_RESET_Pin;
    lGpioInit.Mode = GPIO_MODE_OUTPUT_PP;
    lGpioInit.Pull = GPIO_NOPULL;
    lGpioInit.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(PCA9535_RESET_GPIO_Port, &lGpioInit);
    pca9535PlatformResetWrite(false);
}

void pca9535PlatformResetWrite(bool assertReset)
{
    HAL_GPIO_WritePin(PCA9535_RESET_GPIO_Port,
                      PCA9535_RESET_Pin,
                      assertReset ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

void pca9535PlatformDelayMs(uint32_t delayMs)
{
#if (REP_RTOS_SYSTEM == REP_RTOS_FREERTOS)
    TickType_t lDelayTicks;

    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        lDelayTicks = pdMS_TO_TICKS(delayMs);
        if ((delayMs > 0U) && (lDelayTicks == 0U)) {
            lDelayTicks = 1U;
        }
        vTaskDelay(lDelayTicks);
        return;
    }
#endif

    if (delayMs == 0U) {
        return;
    }

    pca9535PortEnableCycleCnt();

    {
        uint32_t lCyclesPerMs = SystemCoreClock / 1000U;
        uint32_t lStartCycles;
        uint32_t lWaitCycles;

        if (lCyclesPerMs == 0U) {
            lCyclesPerMs = 1U;
        }

        lWaitCycles = lCyclesPerMs * delayMs;
        lStartCycles = DWT->CYCCNT;
        while ((DWT->CYCCNT - lStartCycles) < lWaitCycles) {
            __NOP();
        }
    }
}

void pca9535PortGetDefCfg(ePca9535MapType device, stPca9535Cfg *cfg)
{
    pca9535LoadPlatformDefaultCfg(device, cfg);
}

eDrvStatus pca9535PortAssembleSoftIic(stPca9535Cfg *cfg, uint8_t iic)
{
    if ((cfg == NULL) || ((uint8_t)iic >= (uint8_t)DRVANLOGIIC_MAX)) {
        return DRV_STATUS_INVALID_PARAM;
    }

    cfg->linkId = iic;
    return DRV_STATUS_OK;
}

bool pca9535PortIsValidCfg(const stPca9535Cfg *cfg)
{
    return (cfg != NULL) && ((uint8_t)cfg->linkId < (uint8_t)DRVANLOGIIC_MAX);
}

bool pca9535PortHasValidIicIf(const stPca9535Cfg *cfg)
{
    const stPca9535IicInterface *lInterface;

    lInterface = pca9535GetPlatformIicInterface(cfg);
    return (lInterface != NULL) &&
           (lInterface->init != NULL) &&
           (lInterface->writeReg != NULL) &&
           (lInterface->readReg != NULL);
}

const stPca9535PortIicInterface *pca9535PortGetIicIf(const stPca9535Cfg *cfg)
{
    return (const stPca9535PortIicInterface *)pca9535GetPlatformIicInterface(cfg);
}

eDrvStatus pca9535PortInit(void)
{
    eDrvStatus lStatus;

    if (gPca9535PortReady && pca9535IsReady(PCA9535_DEV0)) {
        return DRV_STATUS_OK;
    }

    lStatus = pca9535Init(PCA9535_DEV0);
    if (lStatus != DRV_STATUS_OK) {
        gPca9535PortReady = false;
        LOG_E(PCA9535_PORT_LOG_TAG, "Init failed, status=%d", (int)lStatus);
        return lStatus;
    }

    gPca9535PortReady = true;
    gPca9535PortShowMask = 0U;
    lStatus = pca9535PortApplyShowMask(gPca9535PortShowMask);
    if (lStatus != DRV_STATUS_OK) {
        gPca9535PortReady = false;
        LOG_E(PCA9535_PORT_LOG_TAG, "Apply default state failed, status=%d", (int)lStatus);
        return lStatus;
    }

    LOG_I(PCA9535_PORT_LOG_TAG, "Board mapping ready");
    return DRV_STATUS_OK;
}

bool pca9535PortIsReady(void)
{
    return gPca9535PortReady && pca9535IsReady(PCA9535_DEV0);
}

eDrvStatus pca9535PortLedOff(void)
{
    return pca9535PortSetShowMask(0U);
}

eDrvStatus pca9535PortLedLightNum(uint8_t num)
{
    static const uint16_t gPca9535PortNumMap[PCA9535_PORT_LED_MAX + 1U] = {
        0x0000U,
        PCA9535_PORT_LED_NUM_1,
        (uint16_t)(PCA9535_PORT_LED_NUM_1 | PCA9535_PORT_LED_NUM_8),
        (uint16_t)(PCA9535_PORT_LED_NUM_1 | PCA9535_PORT_LED_NUM_8 | PCA9535_PORT_LED_NUM_7),
        (uint16_t)(PCA9535_PORT_LED_NUM_1 | PCA9535_PORT_LED_NUM_8 | PCA9535_PORT_LED_NUM_7 | PCA9535_PORT_LED_NUM_6),
        (uint16_t)(PCA9535_PORT_LED_NUM_1 | PCA9535_PORT_LED_NUM_8 | PCA9535_PORT_LED_NUM_7 | PCA9535_PORT_LED_NUM_6 | PCA9535_PORT_LED_NUM_5),
        (uint16_t)(PCA9535_PORT_LED_NUM_1 | PCA9535_PORT_LED_NUM_8 | PCA9535_PORT_LED_NUM_7 | PCA9535_PORT_LED_NUM_6 | PCA9535_PORT_LED_NUM_5 | PCA9535_PORT_LED_NUM_4),
        (uint16_t)(PCA9535_PORT_LED_NUM_1 | PCA9535_PORT_LED_NUM_8 | PCA9535_PORT_LED_NUM_7 | PCA9535_PORT_LED_NUM_6 | PCA9535_PORT_LED_NUM_5 | PCA9535_PORT_LED_NUM_4 | PCA9535_PORT_LED_NUM_3),
        (uint16_t)(PCA9535_PORT_LED_NUM_1 | PCA9535_PORT_LED_NUM_8 | PCA9535_PORT_LED_NUM_7 | PCA9535_PORT_LED_NUM_6 | PCA9535_PORT_LED_NUM_5 | PCA9535_PORT_LED_NUM_4 | PCA9535_PORT_LED_NUM_3 | PCA9535_PORT_LED_NUM_2),
    };
    uint16_t lMask;

    if (num > PCA9535_PORT_LED_MAX) {
        return DRV_STATUS_INVALID_PARAM;
    }

    lMask = gPca9535PortShowMask;
    lMask &= (uint16_t)~(PCA9535_PORT_LED_NUM_1 |
                         PCA9535_PORT_LED_NUM_2 |
                         PCA9535_PORT_LED_NUM_3 |
                         PCA9535_PORT_LED_NUM_4 |
                         PCA9535_PORT_LED_NUM_5 |
                         PCA9535_PORT_LED_NUM_6 |
                         PCA9535_PORT_LED_NUM_7 |
                         PCA9535_PORT_LED_NUM_8);
    lMask |= gPca9535PortNumMap[num];
    return pca9535PortSetShowMask(lMask);
}

eDrvStatus pca9535PortLedPowerShow(bool isRedOn, bool isGreenOn, bool isBlueOn)
{
    uint16_t lMask;

    lMask = gPca9535PortShowMask;
    lMask &= (uint16_t)~(PCA9535_PORT_LED_POWER_RED |
                         PCA9535_PORT_LED_POWER_GREEN |
                         PCA9535_PORT_LED_POWER_BLUE);

    if (isRedOn) {
        lMask |= PCA9535_PORT_LED_POWER_RED;
    }
    if (isGreenOn) {
        lMask |= PCA9535_PORT_LED_POWER_GREEN;
    }
    if (isBlueOn) {
        lMask |= PCA9535_PORT_LED_POWER_BLUE;
    }

    return pca9535PortSetShowMask(lMask);
}

eDrvStatus pca9535PortLedPressShow(bool isRedOn, bool isGreenOn, bool isBlueOn)
{
    uint16_t lMask;

    lMask = gPca9535PortShowMask;
    lMask &= (uint16_t)~(PCA9535_PORT_LED_PRESS_RED |
                         PCA9535_PORT_LED_PRESS_GREEN |
                         PCA9535_PORT_LED_PRESS_BLUE);

    if (isRedOn) {
        lMask |= PCA9535_PORT_LED_PRESS_RED;
    }
    if (isGreenOn) {
        lMask |= PCA9535_PORT_LED_PRESS_GREEN;
    }
    if (isBlueOn) {
        lMask |= PCA9535_PORT_LED_PRESS_BLUE;
    }

    return pca9535PortSetShowMask(lMask);
}

eDrvStatus pca9535PortSetShowMask(uint16_t mask)
{
    eDrvStatus lStatus;

    lStatus = pca9535PortEnsureReady();
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    lStatus = pca9535PortApplyShowMask(mask);
    if (lStatus == DRV_STATUS_OK) {
        gPca9535PortShowMask = mask;
    }
    return lStatus;
}

eDrvStatus pca9535PortGetShowMask(uint16_t *mask)
{
    if (mask == NULL) {
        return DRV_STATUS_INVALID_PARAM;
    }

    *mask = gPca9535PortShowMask;
    return DRV_STATUS_OK;
}

eDrvStatus pca9535PortReadInputPort(uint16_t *value)
{
    eDrvStatus lStatus;

    if (value == NULL) {
        return DRV_STATUS_INVALID_PARAM;
    }

    lStatus = pca9535PortEnsureReady();
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    return pca9535ReadInputPort(PCA9535_DEV0, value);
}

void pca9535PortResetInit(void)
{
    pca9535PlatformResetInit();
}

void pca9535PortResetWrite(bool assertReset)
{
    pca9535PlatformResetWrite(assertReset);
}

void pca9535PortDelayMs(uint32_t delayMs)
{
    pca9535PlatformDelayMs(delayMs);
}

static void pca9535PortEnableCycleCnt(void)
{
    if (gPca9535PortCycleCntReady) {
        return;
    }

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    gPca9535PortCycleCntReady = true;
}

static void pca9535PortEnableGpioClock(GPIO_TypeDef *gpioPort)
{
    if (gpioPort == GPIOA) {
        __HAL_RCC_GPIOA_CLK_ENABLE();
        return;
    }

    if (gpioPort == GPIOB) {
        __HAL_RCC_GPIOB_CLK_ENABLE();
        return;
    }

    if (gpioPort == GPIOC) {
        __HAL_RCC_GPIOC_CLK_ENABLE();
        return;
    }

#ifdef GPIOD
    if (gpioPort == GPIOD) {
        __HAL_RCC_GPIOD_CLK_ENABLE();
        return;
    }
#endif

#ifdef GPIOE
    if (gpioPort == GPIOE) {
        __HAL_RCC_GPIOE_CLK_ENABLE();
        return;
    }
#endif
}

static eDrvStatus pca9535PortApplyShowMask(uint16_t mask)
{
    eDrvStatus lStatus;
    uint16_t lRawPortMask;

    lRawPortMask = (uint16_t)(~mask);

    lStatus = pca9535SetOutputPort(PCA9535_DEV0, lRawPortMask);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    lStatus = pca9535SetDirectionPort(PCA9535_DEV0, lRawPortMask);
    if (lStatus != DRV_STATUS_OK) {
        return lStatus;
    }

    return pca9535SetPolarityPort(PCA9535_DEV0, 0U);
}

static eDrvStatus pca9535PortEnsureReady(void)
{
    if (pca9535PortIsReady()) {
        return DRV_STATUS_OK;
    }

    return pca9535PortInit();
}

static eDrvStatus pca9535PortSoftIicInitAdpt(uint8_t bus)
{
    if (bus >= (uint8_t)DRVANLOGIIC_MAX) {
        return DRV_STATUS_INVALID_PARAM;
    }

    return drvAnlogIicInit(bus);
}

static eDrvStatus pca9535PortSoftIicWriteRegAdpt(uint8_t bus, uint8_t address, const uint8_t *regBuf, uint16_t regLen, const uint8_t *buffer, uint16_t length)
{
    if (bus >= (uint8_t)DRVANLOGIIC_MAX) {
        return DRV_STATUS_INVALID_PARAM;
    }

    return drvAnlogIicWriteRegister(bus, address, regBuf, regLen, buffer, length);
}

static eDrvStatus pca9535PortSoftIicReadRegAdpt(uint8_t bus, uint8_t address, const uint8_t *regBuf, uint16_t regLen, uint8_t *buffer, uint16_t length)
{
    if (bus >= (uint8_t)DRVANLOGIIC_MAX) {
        return DRV_STATUS_INVALID_PARAM;
    }

    return drvAnlogIicReadRegister(bus, address, regBuf, regLen, buffer, length);
}
