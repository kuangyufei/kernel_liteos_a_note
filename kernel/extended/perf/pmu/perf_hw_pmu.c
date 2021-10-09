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

#include "perf_pmu_pri.h"

STATIC Pmu *g_perfHw = NULL;

STATIC CHAR *g_eventName[PERF_COUNT_HW_MAX] = {
    [PERF_COUNT_HW_CPU_CYCLES]              = "cycles",
    [PERF_COUNT_HW_INSTRUCTIONS]            = "instructions",
    [PERF_COUNT_HW_ICACHE_REFERENCES]       = "icache",
    [PERF_COUNT_HW_ICACHE_MISSES]           = "icache-misses",
    [PERF_COUNT_HW_DCACHE_REFERENCES]       = "dcache",
    [PERF_COUNT_HW_DCACHE_MISSES]           = "dcache-misses",
    [PERF_COUNT_HW_BRANCH_INSTRUCTIONS]     = "branches",
    [PERF_COUNT_HW_BRANCH_MISSES]           = "branches-misses",
};

/**
 * 1.If config event is PERF_EVENT_TYPE_HW, then map it to the real eventId first, otherwise use the configured
 * eventId directly.
 * 2.Find available counter for each event.
 * 3.Decide whether this hardware pmu need prescaler (once every 64 cycle counts).
 */
STATIC UINT32 OsPerfHwConfig(VOID)
{
    UINT32 i;
    HwPmu *armPmu = GET_HW_PMU(g_perfHw);

    UINT32 maxCounter = OsGetPmuMaxCounter();
    UINT32 counter = OsGetPmuCounter0();
    UINT32 cycleCounter = OsGetPmuCycleCounter();
    UINT32 cycleCode = armPmu->mapEvent(PERF_COUNT_HW_CPU_CYCLES, PERF_EVENT_TO_CODE);
    if (cycleCode == PERF_HW_INVALID_EVENT_TYPE) {
        return LOS_NOK;
    }

    PerfEvent *events = &g_perfHw->events;
    UINT32 eventNum = events->nr;

    for (i = 0; i < eventNum; i++) {
        Event *event = &(events->per[i]);

        if (!VALID_PERIOD(event->period)) {
            PRINT_ERR("Config period: 0x%x invalid, should be in (%#x, %#x)\n", event->period,
                PERIOD_CALC(CCNT_PERIOD_UPPER_BOUND), PERIOD_CALC(CCNT_PERIOD_LOWER_BOUND));
            return LOS_NOK;
        }

        if (g_perfHw->type == PERF_EVENT_TYPE_HW) { /* do map */
            UINT32 eventId = armPmu->mapEvent(event->eventId, PERF_EVENT_TO_CODE);
            if (eventId == PERF_HW_INVALID_EVENT_TYPE) {
                return LOS_NOK;
            }
            event->eventId = eventId;
        }

        if (event->eventId == cycleCode) {
            event->counter = cycleCounter;
        } else {
            event->counter = counter;
            counter++;
        }

        if (counter >= maxCounter) {
            PRINT_ERR("max events: %u excluding cycle event\n", maxCounter - 1);
            return LOS_NOK;
        }

        PRINT_DEBUG("Perf Config %u eventId = 0x%x, counter = 0x%x, period = 0x%x\n", i, event->eventId, event->counter,
            event->period);
    }

    armPmu->cntDivided = events->cntDivided & armPmu->canDivided;
    return LOS_OK;
}

STATIC UINT32 OsPerfHwStart(VOID)
{
    UINT32 i;
    UINT32 cpuid = ArchCurrCpuid();
    HwPmu *armPmu = GET_HW_PMU(g_perfHw);

    PerfEvent *events = &g_perfHw->events;
    UINT32 eventNum = events->nr;

    armPmu->clear();

    for (i = 0; i < eventNum; i++) {
        Event *event = &(events->per[i]);
        armPmu->setPeriod(event);
        armPmu->enable(event);
        event->count[cpuid] = 0;
    }

    armPmu->start();
    return LOS_OK;
}

STATIC UINT32 OsPerfHwStop(VOID)
{
    UINT32 i;
    UINT32 cpuid = ArchCurrCpuid();
    HwPmu *armPmu = GET_HW_PMU(g_perfHw);

    PerfEvent *events = &g_perfHw->events;
    UINT32 eventNum = events->nr;

    armPmu->stop();

    for (i = 0; i < eventNum; i++) {
        Event *event = &(events->per[i]);
        UINTPTR value = armPmu->readCnt(event);
        PRINT_DEBUG("perf stop readCnt value = 0x%x\n", value);
        event->count[cpuid] += value;

        /* multiplier of cycle counter */
        UINT32 eventId = armPmu->mapEvent(event->eventId, PERF_CODE_TO_EVENT);
        if ((eventId == PERF_COUNT_HW_CPU_CYCLES) && (armPmu->cntDivided != 0)) {
            PRINT_DEBUG("perf stop is cycle\n");
            event->count[cpuid] = event->count[cpuid] << 6; /* CCNT counts every 64th cpu cycle */
        }
        PRINT_DEBUG("perf stop eventCount[0x%x] : [%s] = %llu\n", event->eventId, g_eventName[eventId],
            event->count[cpuid]);
    }
    return LOS_OK;
}

STATIC CHAR *OsPerfGetEventName(Event *event)
{
    UINT32 eventId;
    HwPmu *armPmu = GET_HW_PMU(g_perfHw);
    eventId = armPmu->mapEvent(event->eventId, PERF_CODE_TO_EVENT);
    if (eventId < PERF_COUNT_HW_MAX) {
        return g_eventName[eventId];
    } else {
        return "unknown";
    }
}

UINT32 OsPerfHwInit(HwPmu *hwPmu)
{
    UINT32 ret;
    if (hwPmu == NULL) {
        return LOS_NOK;
    }

    hwPmu->pmu.type    = PERF_EVENT_TYPE_HW;
    hwPmu->pmu.config  = OsPerfHwConfig;
    hwPmu->pmu.start   = OsPerfHwStart;
    hwPmu->pmu.stop    = OsPerfHwStop;
    hwPmu->pmu.getName = OsPerfGetEventName;

    (VOID)memset_s(&hwPmu->pmu.events, sizeof(PerfEvent), 0, sizeof(PerfEvent));
    ret = OsPerfPmuRegister(&hwPmu->pmu);

    g_perfHw = OsPerfPmuGet(PERF_EVENT_TYPE_HW);
    return ret;
}
