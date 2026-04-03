/***********************************************************************************
* @file     : systask.c
* @brief    : System task callback implementation.
* @details  : Contains task callback bodies used by the application startup.
* @author   : GitHub Copilot
* @date     : 2026-03-31
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "systask.h"

#include <string.h>

#include "console.h"
#include "appcomm/appcomm.h"
#include "drvlayer/DrvGpio/drvgpio.h"
#include "log.h"
#include "FreeRTOS.h"
#include "task.h"
#include "gd32f4xx_gpio.h"
#include "system.h"
#include "system_debug.h"
#include "sys_int.h"
#include "drvanlogiic_debug.h"
#include "drviic_debug.h"
#include "Rep/drvlayer/drvspi/drvspi_debug.h"
#include "drvgpio_debug.h"
#include "drvuart_debug.h"
#include "Rep/module/mpu6050/mpu6050_debug.h"
#include "Rep/module/w25qxxx/w25qxxx_debug.h"
#include "Rep/module/w25qxxx/w25qxxx.h"

#define SENSOR_TASK_TAG "SensorTask"
#define SENSOR_TASK_FLASH_TEST_ADDRESS       0U
#define SENSOR_TASK_FLASH_IDLE_PERIOD_MS     1000U
#define SENSOR_TASK_FLASH_BUFFER_SIZE        16U
#define SENSOR_TASK_W25Q128_CAPACITY_ID      0x18U

static TaskHandle_t gSensorTaskHandle = NULL;
static TaskHandle_t gConsoleTaskHandle = NULL;
static TaskHandle_t gGuardTaskHandle = NULL;
static TaskHandle_t gPowerTaskHandle = NULL;
static TaskHandle_t gAppCommTaskHandle = NULL;
static TaskHandle_t gMemoryTaskHandle = NULL;
static const uint8_t gSensorTaskW25q128Name[] = "W25Q128";
static bool gDrvGpioReady = false;

static void process(void);
static BaseType_t createTask(TaskFunction_t taskFunction, const char *taskName, configSTACK_DEPTH_TYPE stackDepth, UBaseType_t taskPriority, TaskHandle_t *taskHandle);
static void sensorTaskCallback(void *parameter);
static void consoleTaskCallback(void *parameter);
static void guardTaskCallback(void *parameter);
static void powerTaskCallback(void *parameter);
static void appCommTaskCallback(void *parameter);
static void memoryTaskCallback(void *parameter);
static bool createTasks(void);
static bool initializeDrvGpio(void);
static bool initializeConsole(void);
static bool initializeAppComm(void);
static const char *sensorTaskGetW25qxxxStatusString(eW25qxxxStatus status);
static bool sensorTaskPrepareFlashDevice(eW25qxxxMapType device, eDrvSpiPortMap spi);
static bool sensorTaskVerifyFlashDevice(eW25qxxxMapType device, eDrvSpiPortMap spi, const uint8_t *name, uint32_t nameLength, uint8_t expectedCapacityId);
static bool sensorTaskRunFlashDemo(void);

static void process(void)
{
    switch (systemGetMode()) {
        case eSYSTEM_INIT_MODE:
            LOG_I(SYSTEM_TAG, "System initialized");
            systemSetMode(eSYSTEM_SELF_CHECK_MODE);
            break;
        case eSYSTEM_SELF_CHECK_MODE:
            if (!createTasks()) {
                break;
            }
            LOG_I(SYSTEM_TAG, "Self-check passed");
            systemSetMode(eSYSTEM_STANDBY_MODE);
            break;
        case eSYSTEM_STANDBY_MODE:
            break;
        case eSYSTEM_NORMAL_MODE:
        case eSYSTEM_UPDATE_MODE:
        case eSYSTEM_DIAGNOSTIC_MODE:
            break;
        default:
            LOG_W(SYSTEM_TAG, "Unknown system mode: %d", (int)systemGetMode());
            systemSetMode(eSYSTEM_INIT_MODE);
            break;
    }
}
/**
* @brief : Default task callback.
* @param : parameter - task parameter, unused.
* @return: None
**/
void defaultTaskCallback(void *parameter)
{
    (void)parameter;

    for (;;) {
        gpio_bit_toggle(STATUS_LED_GPIO_PORT, STATUS_LED_PIN);
        vTaskDelay(pdMS_TO_TICKS(DEFAULT_TASK_PERIOD_MS));
    }
}

/**
* @brief : System task callback.
* @param : parameter - task parameter, unused.
* @return: None
**/
void systemTaskCallback(void *parameter)
{
    (void)parameter;

    for (;;) {
        process();
        vTaskDelay(pdMS_TO_TICKS(SYSTEM_TASK_PERIOD_MS));
    }
}

static BaseType_t createTask(TaskFunction_t taskFunction, const char *taskName, configSTACK_DEPTH_TYPE stackDepth, UBaseType_t taskPriority, TaskHandle_t *taskHandle)
{
    BaseType_t lReturn;

    if ((NULL == taskFunction) || (NULL == taskName) || (NULL == taskHandle)) {
        return pdFAIL;
    }

    if (NULL != *taskHandle) {
        return pdPASS;
    }

    lReturn = xTaskCreate(taskFunction,
        taskName,
        stackDepth,
        NULL,
        taskPriority,
        taskHandle);
    if (pdPASS != lReturn) {
        LOG_E(SYSTEM_TAG, "Create task failed: %s", taskName);
    }

    return lReturn;
}

static bool createTasks(void)
{
    bool lResult = true;

    if (!initializeDrvGpio()) {
        return false;
    }

    if (initializeConsole()) {
        if (pdPASS != createTask(consoleTaskCallback,
            "ConsoleTask",
            CONSOLE_TASK_STACK_SIZE,
            CONSOLE_TASK_PRIORITY,
            &gConsoleTaskHandle)) {
            lResult = false;
        }
    } else {
        lResult = false;
    }

    if (pdPASS != createTask(sensorTaskCallback,
        "SensorTask",
        SENSOR_TASK_STACK_SIZE,
        SENSOR_TASK_PRIORITY,
        &gSensorTaskHandle)) {
        lResult = false;
    }

    if (pdPASS != createTask(guardTaskCallback,
        "GuardTask",
        GUARD_TASK_STACK_SIZE,
        GUARD_TASK_PRIORITY,
        &gGuardTaskHandle)) {
        lResult = false;
    }

    if (pdPASS != createTask(powerTaskCallback,
        "PowerTask",
        POWER_TASK_STACK_SIZE,
        POWER_TASK_PRIORITY,
        &gPowerTaskHandle)) {
        lResult = false;
    }

    if (initializeAppComm()) {
        if (pdPASS != createTask(appCommTaskCallback,
            "AppCommTask",
            APPCOMM_TASK_STACK_SIZE,
            APPCOMM_TASK_PRIORITY,
            &gAppCommTaskHandle)) {
            lResult = false;
        }
    } else {
        lResult = false;
    }

    if (pdPASS != createTask(memoryTaskCallback,
        "MemoryTask",
        MEMORY_TASK_STACK_SIZE,
        MEMORY_TASK_PRIORITY,
        &gMemoryTaskHandle)) {
        lResult = false;
    }

    return lResult;
}

static bool initializeDrvGpio(void)
{
    if (gDrvGpioReady) {
        return true;
    }

    drvGpioInit();
    gDrvGpioReady = true;
    LOG_I(SYSTEM_TAG, "DrvGpio initialized");
    return true;
}

static bool initializeConsole(void)
{
    static bool gConsoleReady = false;

    if (gConsoleReady) {
        return true;
    }

    if (!consoleInit()) {
        LOG_E(SYSTEM_TAG, "Console init failed");
        return false;
    }

    if (!systemDebugConsoleRegister()) {
        LOG_E(SYSTEM_TAG, "Register system console command failed");
        return false;
    }

    if (!drvGpioDebugConsoleRegister()) {
        LOG_E(SYSTEM_TAG, "Register GPIO console command failed");
        return false;
    }

    if (!drvAnlogIicDebugConsoleRegister()) {
        LOG_E(SYSTEM_TAG, "Register software IIC console command failed");
        return false;
    }

    if (!drvIicDebugConsoleRegister()) {
        LOG_E(SYSTEM_TAG, "Register hardware IIC console command failed");
        return false;
    }

    if (!drvSpiDebugConsoleRegister()) {
        LOG_E(SYSTEM_TAG, "Register SPI console command failed");
        return false;
    }

    if (!drvUartDebugConsoleRegister()) {
        LOG_E(SYSTEM_TAG, "Register UART console command failed");
        return false;
    }

    if (!mpu6050DebugConsoleRegister()) {
        LOG_E(SYSTEM_TAG, "Register MPU6050 console command failed");
        return false;
    }

    if (!w25qxxxDebugConsoleRegister()) {
        LOG_E(SYSTEM_TAG, "Register W25QXXX console command failed");
        return false;
    }

    gConsoleReady = true;
    LOG_I(SYSTEM_TAG, "Console initialized");
    return true;
}

static bool initializeAppComm(void)
{
    if (!appCommInit()) {
        LOG_E(SYSTEM_TAG, "AppComm init failed");
        return false;
    }

    LOG_I(SYSTEM_TAG, "AppComm initialized on debug uart");
    return true;
}

static const char *sensorTaskGetW25qxxxStatusString(eW25qxxxStatus status)
{
    switch (status) {
        case W25QXXX_STATUS_OK:
            return "ok";
        case W25QXXX_STATUS_INVALID_PARAM:
            return "invalid_param";
        case W25QXXX_STATUS_NOT_READY:
            return "not_ready";
        case W25QXXX_STATUS_BUSY:
            return "busy";
        case W25QXXX_STATUS_TIMEOUT:
            return "timeout";
        case W25QXXX_STATUS_UNSUPPORTED:
            return "unsupported";
        case W25QXXX_STATUS_DEVICE_ID_MISMATCH:
            return "device_id_mismatch";
        case W25QXXX_STATUS_OUT_OF_RANGE:
            return "out_of_range";
        default:
            return "error";
    }
}

static bool sensorTaskPrepareFlashDevice(eW25qxxxMapType device, eDrvSpiPortMap spi)
{
    eW25qxxxStatus lStatus;

    lStatus = w25qxxxGetDefCfg(device);
    if (lStatus != W25QXXX_STATUS_OK) {
        return false;
    }

    lStatus = w25qxxxSetHardSpi(device, spi);
    if (lStatus != W25QXXX_STATUS_OK) {
        return false;
    }

    return true;
}

static bool sensorTaskVerifyFlashDevice(eW25qxxxMapType device, eDrvSpiPortMap spi, const uint8_t *name, uint32_t nameLength, uint8_t expectedCapacityId)
{
    uint8_t lReadBuffer[SENSOR_TASK_FLASH_BUFFER_SIZE];
    const stW25qxxxInfo *lInfo;
    eW25qxxxStatus lStatus;

    if ((name == NULL) || (nameLength == 0U) || (nameLength >= SENSOR_TASK_FLASH_BUFFER_SIZE)) {
        LOG_E(SENSOR_TASK_TAG, "Invalid flash test config on spi=%d", (int)spi);
        return false;
    }

    if (!sensorTaskPrepareFlashDevice(device, spi)) {
        LOG_E(SENSOR_TASK_TAG, "Prepare flash config failed on spi=%d", (int)spi);
        return false;
    }

    lStatus = w25qxxxInit(device);
    if (lStatus != W25QXXX_STATUS_OK) {
        LOG_E(SENSOR_TASK_TAG,
              "%s init failed on spi=%d: %s(%d)",
              (const char *)name,
              (int)spi,
              sensorTaskGetW25qxxxStatusString(lStatus),
              (int)lStatus);
        return false;
    }

    lInfo = w25qxxxGetInfo(device);
    if ((lInfo == NULL) || (lInfo->capacityId != expectedCapacityId)) {
        LOG_E(SENSOR_TASK_TAG,
              "%s capacity mismatch on spi=%d: expect=0x%02X actual=0x%02X",
              (const char *)name,
              (int)spi,
              (unsigned int)expectedCapacityId,
              (unsigned int)((lInfo != NULL) ? lInfo->capacityId : 0U));
        return false;
    }

    LOG_I(SENSOR_TASK_TAG,
          "%s jedec=%02X %02X %02X size=%luB addrWidth=%u spi=%d",
          (const char *)name,
          (unsigned int)lInfo->manufacturerId,
          (unsigned int)lInfo->memoryType,
          (unsigned int)lInfo->capacityId,
          (unsigned long)lInfo->totalSizeBytes,
          (unsigned int)lInfo->addressWidth,
          (int)spi);

    lStatus = w25qxxxEraseSector(device, SENSOR_TASK_FLASH_TEST_ADDRESS);
    if (lStatus != W25QXXX_STATUS_OK) {
        LOG_E(SENSOR_TASK_TAG,
              "%s erase failed on spi=%d: %s(%d)",
              (const char *)name,
              (int)spi,
              sensorTaskGetW25qxxxStatusString(lStatus),
              (int)lStatus);
        return false;
    }

    lStatus = w25qxxxWrite(device, SENSOR_TASK_FLASH_TEST_ADDRESS, name, nameLength);
    if (lStatus != W25QXXX_STATUS_OK) {
        LOG_E(SENSOR_TASK_TAG,
              "%s write failed on spi=%d: %s(%d)",
              (const char *)name,
              (int)spi,
              sensorTaskGetW25qxxxStatusString(lStatus),
              (int)lStatus);
        return false;
    }

    (void)memset(lReadBuffer, 0, sizeof(lReadBuffer));
    lStatus = w25qxxxRead(device, SENSOR_TASK_FLASH_TEST_ADDRESS, lReadBuffer, nameLength);
    if (lStatus != W25QXXX_STATUS_OK) {
        LOG_E(SENSOR_TASK_TAG,
              "%s read failed on spi=%d: %s(%d)",
              (const char *)name,
              (int)spi,
              sensorTaskGetW25qxxxStatusString(lStatus),
              (int)lStatus);
        return false;
    }

    lReadBuffer[nameLength] = '\0';
    if (memcmp(lReadBuffer, name, nameLength) != 0) {
        LOG_E(SENSOR_TASK_TAG,
              "%s readback mismatch on spi=%d: expect=%s actual=%s",
              (const char *)name,
              (int)spi,
              (const char *)name,
              (const char *)lReadBuffer);
        return false;
    }

    LOG_I(SENSOR_TASK_TAG,
          "%s verify ok on spi=%d readback=%s",
          (const char *)name,
          (int)spi,
          (const char *)lReadBuffer);
    return true;
}

static bool sensorTaskRunFlashDemo(void)
{
    bool lW25q128Ok;

    LOG_I(SENSOR_TASK_TAG,
          "Start single flash demo: W25Q128 PB10/PB14/PB15 CS=PE15");

    lW25q128Ok = sensorTaskVerifyFlashDevice(W25QXXX_DEV0,
                                             DRVSPI_BUS0,
                                             gSensorTaskW25q128Name,
                                             sizeof(gSensorTaskW25q128Name) - 1U,
                                             SENSOR_TASK_W25Q128_CAPACITY_ID);

    if (lW25q128Ok) {
        LOG_I(SENSOR_TASK_TAG, "Single flash demo completed");
        return true;
    }

    LOG_E(SENSOR_TASK_TAG,
          "Single flash demo failed: w25q128=%d",
          (int)lW25q128Ok);
    return false;
}

static void sensorTaskCallback(void *parameter)
{
    TickType_t lLastWakeTime;
    bool lFlashReady = false;
    uint32_t lDelayMs = SENSOR_TASK_INIT_RETRY_PERIOD_MS;

    (void)parameter;
    lLastWakeTime = xTaskGetTickCount();

    for (;;) {
        if (!lFlashReady) {
            lFlashReady = sensorTaskRunFlashDemo();
            lDelayMs = lFlashReady ? SENSOR_TASK_FLASH_IDLE_PERIOD_MS : SENSOR_TASK_INIT_RETRY_PERIOD_MS;
        } else {
            lDelayMs = SENSOR_TASK_FLASH_IDLE_PERIOD_MS;
        }

        vTaskDelayUntil(&lLastWakeTime, pdMS_TO_TICKS(lDelayMs));
    }
}

static void consoleTaskCallback(void *parameter)
{
    (void)parameter;

    for (;;) {
        consoleProcess();
        vTaskDelay(pdMS_TO_TICKS(CONSOLE_TASK_PERIOD_MS));
    }
}

static void guardTaskCallback(void *parameter)
{
    (void)parameter;

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(GUARD_TASK_PERIOD_MS));
    }
}

static void powerTaskCallback(void *parameter)
{
    (void)parameter;

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(POWER_TASK_PERIOD_MS));
    }
}

static void appCommTaskCallback(void *parameter)
{
    (void)parameter;

    for (;;) {
        appCommProcess();
        vTaskDelay(pdMS_TO_TICKS(APPCOMM_TASK_PERIOD_MS));
    }
}

static void memoryTaskCallback(void *parameter)
{
    (void)parameter;

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(MEMORY_TASK_PERIOD_MS));
    }
}
/**************************End of file********************************/

