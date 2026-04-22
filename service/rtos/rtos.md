# RTOS Mapping Service

This component provides a reusable RTOS facade for `rep/` and project-side code.

## Layout

- `rep/service/rtos`: core facade and normalized RTOS API
- `User/port/rtos_port.*`: project-side OS assembly and vendor API binding

## Responsibility Split

- Core: exports stable functions such as scheduler state query, delay, tick, and yield
- Port: maps the selected concrete OS (`FreeRTOS`, CubeMX CMSIS FreeRTOS, `uC/OS-II`, or bare metal) to the core interface

## Usage

Project and reusable code should call `repRtosDelayMs()`, `repRtosGetTickMs()`, `repRtosTaskCreate()`, `repRtosTaskDelayUntilMs()`, `repRtosStatsInit()`, and `repRtosIsSchedulerRunning()` instead of directly including vendor RTOS headers.

Native RTOS headers and APIs are only allowed inside `User/port/rtos_port.*` or equivalent project-side RTOS port files. Project code under `User/system`、`User/manager`、`User/bsp` and reusable code under `rep/` must not call `OSTime*`、`OSTask*`、`OSSem*`、`OSMutex*`、`OSQ*`、`OSFlag*` or other vendor RTOS APIs directly.

When a new RTOS is introduced, extend only `User/port/rtos_port.c` unless the normalized interface itself needs a new capability.