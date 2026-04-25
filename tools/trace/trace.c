/************************************************************************************
* @file     : trace.c
* @brief    : Cortex-M fault snapshot capture implementation.
* @details  : Provides reusable Cortex-M fault handlers, captures exception context,
*             formats diagnostic output, and delegates transport binding through
*             weak platform hooks.
* @author   : \.rumi
* @date     : 2026-04-24
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#include "trace.h"

#if defined(__IAR_SYSTEMS_ICC__)
#include <intrinsics.h>
#endif

static uint32_t traceGetMainStackPointer(void);
static uint32_t traceGetProcessStackPointer(void);
static void traceDisableInterrupts(void);
static uint32_t traceReadSystemRegister32(uint32_t address);
static void traceWriteText(const char *text);
static void traceWriteHex32(uint32_t value);
static void traceWriteLabelHex32(const char *label, uint32_t value);
static const char *traceGetFaultTypeLabel(eTraceFaultType faultType);
static void traceFaultReport(eTraceFaultType faultType, const stTraceFaultSnapshot *snapshot);
void traceHardFaultHandlerC(const stTraceFaultStackFrame *frame, uint32_t excReturn);
void traceMemManageHandlerC(const stTraceFaultStackFrame *frame, uint32_t excReturn);
void traceBusFaultHandlerC(const stTraceFaultStackFrame *frame, uint32_t excReturn);
void traceUsageFaultHandlerC(const stTraceFaultStackFrame *frame, uint32_t excReturn);

#if defined(__CC_ARM)
__asm static uint32_t traceGetMainStackPointer(void)
{
    MRS R0, MSP
    BX LR
}

__asm static uint32_t traceGetProcessStackPointer(void)
{
    MRS R0, PSP
    BX LR
}

__asm static void traceDisableInterrupts(void)
{
    CPSID I
    BX LR
}
#elif defined(__GNUC__) || defined(__clang__)
static uint32_t traceGetMainStackPointer(void)
{
    uint32_t lValue;

    __asm volatile ("mrs %0, msp" : "=r" (lValue));
    return lValue;
}

static uint32_t traceGetProcessStackPointer(void)
{
    uint32_t lValue;

    __asm volatile ("mrs %0, psp" : "=r" (lValue));
    return lValue;
}

static void traceDisableInterrupts(void)
{
    __asm volatile ("cpsid i" : : : "memory");
}
#elif defined(__IAR_SYSTEMS_ICC__)
static uint32_t traceGetMainStackPointer(void)
{
    return __get_MSP();
}

static uint32_t traceGetProcessStackPointer(void)
{
    return __get_PSP();
}

static void traceDisableInterrupts(void)
{
    __disable_interrupt();
}
#else
#error "trace.c requires compiler support for MSP/PSP and interrupt control intrinsics"
#endif

static uint32_t traceReadSystemRegister32(uint32_t address)
{
    return *((volatile const uint32_t *)address);
}

__attribute__((weak)) void traceFaultPlatformTransportInit(void)
{
}

__attribute__((weak)) int32_t traceFaultPlatformTransportWrite(const uint8_t *buffer, uint16_t length)
{
    (void)buffer;
    (void)length;
    return 0;
}

__attribute__((weak)) void traceFaultPlatformHalt(const stTraceFaultSnapshot *snapshot)
{
    (void)snapshot;

    while (1) {
    }
}

static void traceWriteText(const char *text)
{
    const char *lpText;
    uint16_t lLength;

    if (text == NULL) {
        return;
    }

    lpText = text;
    lLength = 0U;
    while (*lpText != '\0') {
        lpText++;
        lLength++;
    }

    if (lLength == 0U) {
        return;
    }

    (void)traceFaultPlatformTransportWrite((const uint8_t *)text, lLength);
}

static void traceWriteHex32(uint32_t value)
{
    static const char lHexChars[] = "0123456789ABCDEF";
    char lBuffer[10];
    uint32_t lIndex;

    lBuffer[0] = '0';
    lBuffer[1] = 'x';
    for (lIndex = 0U; lIndex < 8U; lIndex++) {
        lBuffer[2U + lIndex] = lHexChars[(value >> (28U - (lIndex * 4U))) & 0x0FU];
    }

    (void)traceFaultPlatformTransportWrite((const uint8_t *)lBuffer, sizeof(lBuffer));
}

static void traceWriteLabelHex32(const char *label, uint32_t value)
{
    traceWriteText(label);
    traceWriteHex32(value);
    traceWriteText("\r\n");
}

static const char *traceGetFaultTypeLabel(eTraceFaultType faultType)
{
    switch (faultType) {
        case TRACE_FAULT_MEMMANAGE:
            return "MemManage";
        case TRACE_FAULT_BUSFAULT:
            return "BusFault";
        case TRACE_FAULT_USAGEFAULT:
            return "UsageFault";
        case TRACE_FAULT_HARDFAULT:
        default:
            return "HardFault";
    }
}

static void traceFaultReport(eTraceFaultType faultType, const stTraceFaultSnapshot *snapshot)
{
    traceWriteText("\r\n[");
    traceWriteText(traceGetFaultTypeLabel(faultType));
    traceWriteText("]\r\n");

    if (snapshot == NULL) {
        return;
    }

    traceWriteLabelHex32("EXC_RETURN=", snapshot->excReturn);
    traceWriteLabelHex32("PC=", snapshot->hasStackFrame ? snapshot->stackFrame.pc : 0U);
    traceWriteLabelHex32("LR=", snapshot->hasStackFrame ? snapshot->stackFrame.lr : 0U);
    traceWriteLabelHex32("R0=", snapshot->hasStackFrame ? snapshot->stackFrame.r0 : 0U);
    traceWriteLabelHex32("R1=", snapshot->hasStackFrame ? snapshot->stackFrame.r1 : 0U);
    traceWriteLabelHex32("R2=", snapshot->hasStackFrame ? snapshot->stackFrame.r2 : 0U);
    traceWriteLabelHex32("R3=", snapshot->hasStackFrame ? snapshot->stackFrame.r3 : 0U);
    traceWriteLabelHex32("R12=", snapshot->hasStackFrame ? snapshot->stackFrame.r12 : 0U);
    traceWriteLabelHex32("CFSR=", snapshot->cfsr);
    traceWriteLabelHex32("HFSR=", snapshot->hfsr);
    traceWriteLabelHex32("BFAR=", snapshot->bfar);
    traceWriteLabelHex32("MMFAR=", snapshot->mmfar);
    traceWriteLabelHex32("MSP=", snapshot->msp);
    traceWriteLabelHex32("PSP=", snapshot->psp);
    traceWriteLabelHex32("PSR=", snapshot->hasStackFrame ? snapshot->stackFrame.psr : 0U);
}

void traceFaultCapture(stTraceFaultSnapshot *snapshot, const stTraceFaultStackFrame *frame, uint32_t excReturn)
{
    if (snapshot == NULL) {
        return;
    }

    snapshot->stackFrame.r0 = 0U;
    snapshot->stackFrame.r1 = 0U;
    snapshot->stackFrame.r2 = 0U;
    snapshot->stackFrame.r3 = 0U;
    snapshot->stackFrame.r12 = 0U;
    snapshot->stackFrame.lr = 0U;
    snapshot->stackFrame.pc = 0U;
    snapshot->stackFrame.psr = 0U;
    snapshot->excReturn = excReturn;
    snapshot->cfsr = traceReadSystemRegister32(0xE000ED28UL);
    snapshot->hfsr = traceReadSystemRegister32(0xE000ED2CUL);
    snapshot->bfar = traceReadSystemRegister32(0xE000ED38UL);
    snapshot->mmfar = traceReadSystemRegister32(0xE000ED34UL);
    snapshot->msp = traceGetMainStackPointer();
    snapshot->psp = traceGetProcessStackPointer();
    snapshot->hasStackFrame = false;

    if (frame == NULL) {
        return;
    }

    snapshot->stackFrame.r0 = frame->r0;
    snapshot->stackFrame.r1 = frame->r1;
    snapshot->stackFrame.r2 = frame->r2;
    snapshot->stackFrame.r3 = frame->r3;
    snapshot->stackFrame.r12 = frame->r12;
    snapshot->stackFrame.lr = frame->lr;
    snapshot->stackFrame.pc = frame->pc;
    snapshot->stackFrame.psr = frame->psr;
    snapshot->hasStackFrame = true;
}

void traceFaultHandle(eTraceFaultType faultType, const stTraceFaultStackFrame *frame, uint32_t excReturn)
{
    stTraceFaultSnapshot lSnapshot;

    traceDisableInterrupts();
    traceFaultCapture(&lSnapshot, frame, excReturn);
    traceFaultPlatformTransportInit();
    traceFaultReport(faultType, &lSnapshot);
    traceFaultPlatformHalt(&lSnapshot);
}

void traceHardFaultHandlerC(const stTraceFaultStackFrame *frame, uint32_t excReturn)
{
    traceFaultHandle(TRACE_FAULT_HARDFAULT, frame, excReturn);
}

void traceMemManageHandlerC(const stTraceFaultStackFrame *frame, uint32_t excReturn)
{
    traceFaultHandle(TRACE_FAULT_MEMMANAGE, frame, excReturn);
}

void traceBusFaultHandlerC(const stTraceFaultStackFrame *frame, uint32_t excReturn)
{
    traceFaultHandle(TRACE_FAULT_BUSFAULT, frame, excReturn);
}

void traceUsageFaultHandlerC(const stTraceFaultStackFrame *frame, uint32_t excReturn)
{
    traceFaultHandle(TRACE_FAULT_USAGEFAULT, frame, excReturn);
}

__asm void HardFault_Handler(void)
{
    IMPORT  traceHardFaultHandlerC
    TST     LR, #4
    ITE     EQ
    MRSEQ   R0, MSP
    MRSNE   R0, PSP
    MOV     R1, LR
    B       traceHardFaultHandlerC
}

__asm void MemManage_Handler(void)
{
    IMPORT  traceMemManageHandlerC
    TST     LR, #4
    ITE     EQ
    MRSEQ   R0, MSP
    MRSNE   R0, PSP
    MOV     R1, LR
    B       traceMemManageHandlerC
}

__asm void BusFault_Handler(void)
{
    IMPORT  traceBusFaultHandlerC
    TST     LR, #4
    ITE     EQ
    MRSEQ   R0, MSP
    MRSNE   R0, PSP
    MOV     R1, LR
    B       traceBusFaultHandlerC
}

__asm void UsageFault_Handler(void)
{
    IMPORT  traceUsageFaultHandlerC
    TST     LR, #4
    ITE     EQ
    MRSEQ   R0, MSP
    MRSNE   R0, PSP
    MOV     R1, LR
    B       traceUsageFaultHandlerC
}

/**************************End of file********************************/
