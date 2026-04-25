/************************************************************************************
* @file     : trace.h
* @brief    : Cortex-M fault snapshot capture interface.
* @details  : Captures stacked core registers and SCB fault status registers without
*             binding to any project-specific transport or log implementation.
* @author   : \.rumi
* @date     : 2026-04-24
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef TRACE_H
#define TRACE_H

#include <stdbool.h>
#include <stdint.h>

#include "rep_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct stTraceFaultStackFrame {
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t lr;
    uint32_t pc;
    uint32_t psr;
} stTraceFaultStackFrame;

typedef struct stTraceFaultSnapshot {
    stTraceFaultStackFrame stackFrame;
    uint32_t excReturn;
    uint32_t cfsr;
    uint32_t hfsr;
    uint32_t bfar;
    uint32_t mmfar;
    uint32_t msp;
    uint32_t psp;
    bool hasStackFrame;
} stTraceFaultSnapshot;

typedef enum eTraceFaultType {
    TRACE_FAULT_HARDFAULT = 0,
    TRACE_FAULT_MEMMANAGE,
    TRACE_FAULT_BUSFAULT,
    TRACE_FAULT_USAGEFAULT,
} eTraceFaultType;

void traceFaultCapture(stTraceFaultSnapshot *snapshot, const stTraceFaultStackFrame *frame, uint32_t excReturn);
void traceFaultHandle(eTraceFaultType faultType, const stTraceFaultStackFrame *frame, uint32_t excReturn);

void traceFaultPlatformTransportInit(void);
int32_t traceFaultPlatformTransportWrite(const uint8_t *buffer, uint16_t length);
void traceFaultPlatformHalt(const stTraceFaultSnapshot *snapshot);

void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);

#ifdef __cplusplus
}
#endif
#endif  // TRACE_H
/**************************End of file********************************/
