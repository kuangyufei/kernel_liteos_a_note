/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of
 *    conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list
 *    of conditions and the following disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used
 *    to endorse or promote products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "armv7_pmu_pri.h"
#include "perf_pmu_pri.h"
#include "los_hw_cpu.h"
#include "asm/platform.h"

OS_PMU_INTS(LOSCFG_KERNEL_CORE_NUM, g_pmuIrqNr);
STATIC HwPmu g_armv7Pmu;

STATIC INLINE UINT32 Armv7PmncRead(VOID)
{
    UINT32 value = 0;
    __asm__ volatile("mrc p15, 0, %0, c9, c12, 0" : "=r"(value));
    return value;
}

STATIC INLINE VOID Armv7PmncWrite(UINT32 value)
{
    value &= ARMV7_PMNC_MASK;
    __asm__ volatile("mcr p15, 0, %0, c9, c12, 0" : : "r"(value));
    ISB;
}

STATIC INLINE UINT32 Armv7PmuOverflowed(UINT32 pmnc)
{
    return pmnc & ARMV7_OVERFLOWED_MASK;
}

STATIC INLINE UINT32 Armv7PmuCntOverflowed(UINT32 pmnc, UINT32 index)
{
    return pmnc & ARMV7_CNT2BIT(ARMV7_IDX2CNT(index));
}

STATIC INLINE UINT32 Armv7CntValid(UINT32 index)
{
    return index <= ARMV7_IDX_COUNTER_LAST;
}

STATIC INLINE VOID Armv7PmuSelCnt(UINT32 index)
{
    UINT32 counter = ARMV7_IDX2CNT(index);
    __asm__ volatile("mcr p15, 0, %0, c9, c12, 5" : : "r" (counter));
    ISB;
}

STATIC INLINE VOID Armv7PmuSetCntPeriod(UINT32 index, UINT32 period)
{
    if (!Armv7CntValid(index)) {
        PRINT_ERR("CPU writing wrong counter %u\n", index);
    } else if (index == ARMV7_IDX_CYCLE_COUNTER) {
        __asm__ volatile("mcr p15, 0, %0, c9, c13, 0" : : "r" (period));
    } else {
        Armv7PmuSelCnt(index);
        __asm__ volatile("mcr p15, 0, %0, c9, c13, 2" : : "r" (period));
    }
}

STATIC INLINE VOID Armv7BindEvt2Cnt(UINT32 index, UINT32 value)
{
    PRINT_DEBUG("bind event: %u to counter: %u\n", value, index);
    Armv7PmuSelCnt(index);
    value &= ARMV7_EVTYPE_MASK;
    __asm__ volatile("mcr p15, 0, %0, c9, c13, 1" : : "r" (value));
}

STATIC INLINE VOID Armv7EnableCnt(UINT32 index)
{
    UINT32 counter = ARMV7_IDX2CNT(index);
    PRINT_DEBUG("index : %u, counter: %u\n", index, counter);
    __asm__ volatile("mcr p15, 0, %0, c9, c12, 1" : : "r" (ARMV7_CNT2BIT(counter)));
}

STATIC INLINE VOID Armv7DisableCnt(UINT32 index)
{
    UINT32 counter = ARMV7_IDX2CNT(index);
    PRINT_DEBUG("index : %u, counter: %u\n", index, counter);
    __asm__ volatile("mcr p15, 0, %0, c9, c12, 2" : : "r" (ARMV7_CNT2BIT(counter)));
}

STATIC INLINE VOID Armv7EnableCntInterrupt(UINT32 index)
{
    UINT32 counter = ARMV7_IDX2CNT(index);
    __asm__ volatile("mcr p15, 0, %0, c9, c14, 1" : : "r" (ARMV7_CNT2BIT(counter)));
    ISB;
}

STATIC INLINE VOID Armv7DisableCntInterrupt(UINT32 index)
{
    UINT32 counter = ARMV7_IDX2CNT(index);
    __asm__ volatile("mcr p15, 0, %0, c9, c14, 2" : : "r" (ARMV7_CNT2BIT(counter)));
    /* Clear the overflow flag in case an interrupt is pending. */
    __asm__ volatile("mcr p15, 0, %0, c9, c12, 3" : : "r" (ARMV7_CNT2BIT(counter)));
    ISB;
}

STATIC INLINE UINT32 Armv7PmuGetOverflowStatus(VOID)
{
    UINT32 value;

    __asm__ volatile("mrc p15, 0, %0, c9, c12, 3" : "=r" (value));
    value &= ARMV7_FLAG_MASK;
    __asm__ volatile("mcr p15, 0, %0, c9, c12, 3" : : "r" (value));

    return value;
}

STATIC VOID Armv7EnableEvent(Event *event)
{
    UINT32 cnt = event->counter;

    if (!Armv7CntValid(cnt)) {
        PRINT_ERR("CPU enabling wrong PMNC counter IRQ enable %u\n", cnt);
        return;
    }

    if (event->period == 0) {
        PRINT_INFO("event period value not valid, counter: %u\n", cnt);
        return;
    }
    /*
     * Enable counter and interrupt, and set the counter to count
     * the event that we're interested in.
     */
    UINT32 intSave = LOS_IntLock();

    Armv7DisableCnt(cnt);

    /*
     * Set event (if destined for PMNx counters)
     * We only need to set the event for the cycle counter if we
     * have the ability to perform event filtering.
     */
    if (cnt != ARMV7_IDX_CYCLE_COUNTER) {
        Armv7BindEvt2Cnt(cnt, event->eventId);
    }

    /* Enable interrupt for this counter */
    Armv7EnableCntInterrupt(cnt);
    Armv7EnableCnt(cnt);
    LOS_IntRestore(intSave);

    PRINT_DEBUG("enabled event: %u cnt: %u\n", event->eventId, cnt);
}

STATIC VOID Armv7DisableEvent(Event *event)
{
    UINT32 cnt = event->counter;

    if (!Armv7CntValid(cnt)) {
        PRINT_ERR("CPU enabling wrong PMNC counter IRQ enable %u\n", cnt);
        return;
    }

    UINT32 intSave = LOS_IntLock();
    Armv7DisableCnt(cnt);
    Armv7DisableCntInterrupt(cnt);
    LOS_IntRestore(intSave);
}


STATIC VOID Armv7StartAllCnt(VOID)
{
    PRINT_DEBUG("starting pmu...\n");

    /* Enable all counters */
    UINT32 reg = Armv7PmncRead() | ARMV7_PMNC_E;
    if (g_armv7Pmu.cntDivided) {
        reg |= ARMV7_PMNC_D;
    } else {
        reg &= ~ARMV7_PMNC_D;
    }

    Armv7PmncWrite(reg);
    HalIrqUnmask(g_pmuIrqNr[ArchCurrCpuid()]);
}

STATIC VOID Armv7StopAllCnt(VOID)
{
    PRINT_DEBUG("stopping pmu...\n");
    /* Disable all counters */
    Armv7PmncWrite(Armv7PmncRead() & ~ARMV7_PMNC_E);

    HalIrqMask(g_pmuIrqNr[ArchCurrCpuid()]);
}

STATIC VOID Armv7ResetAllCnt(VOID)
{
    UINT32 index;

    /* The counter and interrupt enable registers are unknown at reset. */
    for (index = ARMV7_IDX_CYCLE_COUNTER; index < ARMV7_IDX_MAX_COUNTER; index++) {
        Armv7DisableCnt(index);
        Armv7DisableCntInterrupt(index);
    }

    /* Initialize & Reset PMNC: C and P bits and D bits */
    UINT32 reg = ARMV7_PMNC_P | ARMV7_PMNC_C | (g_armv7Pmu.cntDivided ? ARMV7_PMNC_D : 0);
    Armv7PmncWrite(reg);
}

STATIC VOID Armv7SetEventPeriod(Event *event)
{
    if (event->period != 0) {
        PRINT_INFO("counter: %u, period: 0x%x\n", event->counter, event->period);
        Armv7PmuSetCntPeriod(event->counter, PERIOD_CALC(event->period));
    }
}

STATIC UINTPTR Armv7ReadEventCnt(Event *event)
{
    UINT32 value = 0;
    UINT32 index = event->counter;

    if (!Armv7CntValid(index)) {
        PRINT_ERR("CPU reading wrong counter %u\n", index);
    } else if (index == ARMV7_IDX_CYCLE_COUNTER) {
        __asm__ volatile("mrc p15, 0, %0, c9, c13, 0" : "=r" (value));
    } else {
        Armv7PmuSelCnt(index);
        __asm__ volatile("mrc p15, 0, %0, c9, c13, 2" : "=r" (value));
    }

    if (value < PERIOD_CALC(event->period)) {
        if (Armv7PmuCntOverflowed(Armv7PmuGetOverflowStatus(), event->counter)) {
            value += event->period;
        }
    } else {
        value -= PERIOD_CALC(event->period);
    }
    return value;
}

STATIC const UINT32 g_armv7Map[] = {
    [PERF_COUNT_HW_CPU_CYCLES]          = ARMV7_PERF_HW_CYCLES,
    [PERF_COUNT_HW_INSTRUCTIONS]        = ARMV7_PERF_HW_INSTRUCTIONS,
    [PERF_COUNT_HW_DCACHE_REFERENCES]   = ARMV7_PERF_HW_DCACHES,
    [PERF_COUNT_HW_DCACHE_MISSES]       = ARMV7_PERF_HW_DCACHE_MISSES,
    [PERF_COUNT_HW_ICACHE_REFERENCES]   = ARMV7_PERF_HW_ICACHES,
    [PERF_COUNT_HW_ICACHE_MISSES]       = ARMV7_PERF_HW_ICACHE_MISSES,
    [PERF_COUNT_HW_BRANCH_INSTRUCTIONS] = ARMV7_PERF_HW_BRANCHES,
    [PERF_COUNT_HW_BRANCH_MISSES]       = ARMV7_PERF_HW_BRANCE_MISSES,
};

UINT32 Armv7PmuMapEvent(UINT32 eventType, BOOL reverse)
{
    if (!reverse) {  /* Common event to armv7 real event */
        if (eventType < ARRAY_SIZE(g_armv7Map)) {
            return g_armv7Map[eventType];
        }
        return eventType;
    } else {        /* Armv7 real event to common event */
        UINT32 i;
        for (i = 0; i < ARRAY_SIZE(g_armv7Map); i++) {
            if (g_armv7Map[i] == eventType) {
                return i;
            }
        }
        return PERF_HW_INVALID_EVENT_TYPE;
    }
}

STATIC VOID Armv7PmuIrqHandler(VOID)
{
    UINT32 index;
    PerfRegs regs;

    PerfEvent *events = &(g_armv7Pmu.pmu.events);
    UINT32 eventNum = events->nr;

    /* Get and reset the IRQ flags */
    UINT32 pmnc = Armv7PmuGetOverflowStatus();
    if (!Armv7PmuOverflowed(pmnc)) {
        return;
    }

    (VOID)memset_s(&regs, sizeof(PerfRegs), 0, sizeof(PerfRegs));
    OsPerfFetchIrqRegs(&regs);

    Armv7StopAllCnt();

    for (index = 0; index < eventNum; index++) {
        Event *event = &(events->per[index]);
        /*
         * We have a single interrupt for all counters. Check that
         * each counter has overflowed before we process it.
         */
        if (!Armv7PmuCntOverflowed(pmnc, event->counter) || (event->period == 0)) {
            continue;
        }

        Armv7PmuSetCntPeriod(event->counter, PERIOD_CALC(event->period));

        OsPerfUpdateEventCount(event, event->period);
        OsPerfHandleOverFlow(event, &regs);
    }
    Armv7StartAllCnt();
}

UINT32 OsGetPmuMaxCounter(VOID)
{
    return ARMV7_IDX_MAX_COUNTER;
}

UINT32 OsGetPmuCycleCounter(VOID)
{
    return ARMV7_IDX_CYCLE_COUNTER;
}

UINT32 OsGetPmuCounter0(VOID)
{
    return ARMV7_IDX_COUNTER0;
}

STATIC HwPmu g_armv7Pmu = {
    .canDivided = TRUE,
    .enable     = Armv7EnableEvent,
    .disable    = Armv7DisableEvent,
    .start      = Armv7StartAllCnt,
    .stop       = Armv7StopAllCnt,
    .clear      = Armv7ResetAllCnt,
    .setPeriod  = Armv7SetEventPeriod,
    .readCnt    = Armv7ReadEventCnt,
    .mapEvent   = Armv7PmuMapEvent,
};

UINT32 OsHwPmuInit(VOID)
{
    UINT32 ret;
    UINT32 index;

    for (index = 0; index < LOSCFG_KERNEL_CORE_NUM; index++) {
        ret = LOS_HwiCreate(g_pmuIrqNr[index], 0, 0, Armv7PmuIrqHandler, 0);
        if (ret != LOS_OK) {
            PRINT_ERR("pmu %u irq handler register failed\n", g_pmuIrqNr[index]);
            return ret;
        }
#ifdef LOSCFG_KERNEL_SMP
        HalIrqSetAffinity(g_pmuIrqNr[index], CPUID_TO_AFFI_MASK(index));
#endif
    }
    ret = OsPerfHwInit(&g_armv7Pmu);
    return ret;
}
