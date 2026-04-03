/***********************************************************************************
* @file     : systask.h
* @brief    : System task callback declarations.
* @details  : Exposes task callback entry points used by the application startup.
* @author   : GitHub Copilot
* @date     : 2026-03-31
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef SYSTASK_H
#define SYSTASK_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"

#ifndef SENSOR_TASK_MPU6050_LOG_SUPPORT
#define SENSOR_TASK_MPU6050_LOG_SUPPORT    1
#endif


#define SENSOR_TASK_INIT_RETRY_PERIOD_MS   1000U

#define DEFAULT_TASK_STACK_SIZE     configMINIMAL_STACK_SIZE
#define DEFAULT_TASK_PRIORITY       (tskIDLE_PRIORITY + 1U)
#define DEFAULT_TASK_PERIOD_MS      500U

#define SYSTEM_TASK_STACK_SIZE      (configMINIMAL_STACK_SIZE * 2U)
#define SYSTEM_TASK_PRIORITY        (tskIDLE_PRIORITY + 2U)
#define SYSTEM_TASK_PERIOD_MS       10U

#define SENSOR_TASK_STACK_SIZE      (configMINIMAL_STACK_SIZE * 4U)
#define SENSOR_TASK_PRIORITY        (tskIDLE_PRIORITY + 5U)
#define SENSOR_TASK_PERIOD_MS       100U

#define CONSOLE_TASK_STACK_SIZE     (configMINIMAL_STACK_SIZE * 4U)
#define CONSOLE_TASK_PRIORITY       (tskIDLE_PRIORITY + 1U)
#define CONSOLE_TASK_PERIOD_MS      20U

#define GUARD_TASK_STACK_SIZE       configMINIMAL_STACK_SIZE
#define GUARD_TASK_PRIORITY         (tskIDLE_PRIORITY + 1U)
#define GUARD_TASK_PERIOD_MS        1000U

#define POWER_TASK_STACK_SIZE       configMINIMAL_STACK_SIZE
#define POWER_TASK_PRIORITY         (tskIDLE_PRIORITY + 1U)
#define POWER_TASK_PERIOD_MS        1000U

#define APPCOMM_TASK_STACK_SIZE     (configMINIMAL_STACK_SIZE * 4U)
#define APPCOMM_TASK_PRIORITY       (tskIDLE_PRIORITY + 3U)
#define APPCOMM_TASK_PERIOD_MS      10U

#define MEMORY_TASK_STACK_SIZE      configMINIMAL_STACK_SIZE
#define MEMORY_TASK_PRIORITY        (tskIDLE_PRIORITY + 1U)
#define MEMORY_TASK_PERIOD_MS       1000U

#ifdef __cplusplus
extern "C" {
#endif

void defaultTaskCallback(void *parameter);
void systemTaskCallback(void *parameter);

#ifdef __cplusplus
}
#endif
#endif  // SYSTASK_H
/**************************End of file********************************/
