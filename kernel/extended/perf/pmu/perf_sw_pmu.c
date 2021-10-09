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
#include "los_hook.h"

STATIC SwPmu g_perfSw;
STATIC CHAR *g_eventName[PERF_COUNT_SW_MAX] = {
    [PERF_COUNT_SW_TASK_SWITCH]  = "task switch",
    [PERF_COUNT_SW_IRQ_RESPONSE] = "irq response",
    [PERF_COUNT_SW_MEM_ALLOC]    = "mem alloc",
    [PERF_COUNT_SW_MUX_PEND]     = "mux pend",
};

STATIC UINT32 g_traceEventMap[PERF_COUNT_SW_MAX] = {
    [PERF_COUNT_SW_TASK_SWITCH]  = LOS_HOOK_TYPE_TASK_SWITCHEDIN,
    [PERF_COUNT_SW_IRQ_RESPONSE] = LOS_HOOK_TYPE_ISR_ENTER,
    [PERF_COUNT_SW_MEM_ALLOC]    = LOS_HOOK_TYPE_MEM_ALLOC,
    [PERF_COUNT_SW_MUX_PEND]     = LOS_HOOK_TYPE_MUX_PEND,
};

VOID OsPerfHook(UINT32 eventType)
{
    if (!g_perfSw.enable) {
        return;
    }

    PerfEvent *events = &g_perfSw.pmu.events;
    UINT32 eventNum = events->nr;

    UINT32 i;
    PerfRegs regs;

    (VOID)memset_s(&regs, sizeof(PerfRegs), 0, sizeof(PerfRegs));

    for (i = 0; i < eventNum; i++) {
        Event *event = &(events->per[i]);
        if (event->counter == eventType) {
            OsPerfUpdateEventCount(event, 1);
            if (event->count[ArchCurrCpuid()] % event->period == 0) {
                OsPerfFetchCallerRegs(&regs);
                OsPerfHandleOverFlow(event, &regs);
            }
            return;
        }
    }
}

STATIC VOID LOS_PerfMemAlloc(VOID *pool, VOID *ptr, UINT32 size)
{
    OsPerfHook(LOS_HOOK_TYPE_MEM_ALLOC);
}

STATIC VOID LOS_PerfMuxPend(const LosMux *muxCB, UINT32 timeout)
{
    OsPerfHook(LOS_HOOK_TYPE_MUX_PEND);
}

STATIC VOID LOS_PerfIsrEnter(UINT32 hwiNum)
{
    OsPerfHook(LOS_HOOK_TYPE_ISR_ENTER);
}

STATIC VOID LOS_PerfTaskSwitchedIn(const LosTaskCB *newTask, const LosTaskCB *runTask)
{
    OsPerfHook(LOS_HOOK_TYPE_TASK_SWITCHEDIN);
}

STATIC VOID OsPerfCnvInit(VOID)
{
    LOS_HookReg(LOS_HOOK_TYPE_MEM_ALLOC, LOS_PerfMemAlloc);
    LOS_HookReg(LOS_HOOK_TYPE_MUX_PEND, LOS_PerfMuxPend);
    LOS_HookReg(LOS_HOOK_TYPE_ISR_ENTER, LOS_PerfIsrEnter);
    LOS_HookReg(LOS_HOOK_TYPE_TASK_SWITCHEDIN, LOS_PerfTaskSwitchedIn);
}

STATIC UINT32 OsPerfSwConfig(VOID)
{
    UINT32 i;
    PerfEvent *events = &g_perfSw.pmu.events;
    UINT32 eventNum = events->nr;

    for (i = 0; i < eventNum; i++) {
        Event *event = &(events->per[i]);
        if ((event->eventId < PERF_COUNT_SW_TASK_SWITCH) || (event->eventId >= PERF_COUNT_SW_MAX) ||
            (event->period == 0)) {
            return LOS_NOK;
        }
        event->counter = g_traceEventMap[event->eventId];
    }
    return LOS_OK;
}

STATIC UINT32 OsPerfSwStart(VOID)
{
    UINT32 i;
    UINT32 cpuid = ArchCurrCpuid();
    PerfEvent *events = &g_perfSw.pmu.events;
    UINT32 eventNum = events->nr;

    for (i = 0; i < eventNum; i++) {
        Event *event = &(events->per[i]);
        event->count[cpuid] = 0;
    }

    g_perfSw.enable = TRUE;
    return LOS_OK;
}

STATIC UINT32 OsPerfSwStop(VOID)
{
    g_perfSw.enable = FALSE;
    return LOS_OK;
}

STATIC CHAR *OsPerfGetEventName(Event *event)
{
    UINT32 eventId = event->eventId;
    if (eventId < PERF_COUNT_SW_MAX) {
        return g_eventName[eventId];
    }
    return "unknown";
}

UINT32 OsSwPmuInit(VOID)
{
    g_perfSw.pmu = (Pmu) {
        .type    = PERF_EVENT_TYPE_SW,
        .config  = OsPerfSwConfig,
        .start   = OsPerfSwStart,
        .stop    = OsPerfSwStop,
        .getName = OsPerfGetEventName,
    };

    g_perfSw.enable = FALSE;

    OsPerfCnvInit();

    (VOID)memset_s(&g_perfSw.pmu.events, sizeof(PerfEvent), 0, sizeof(PerfEvent));
    return OsPerfPmuRegister(&g_perfSw.pmu);
}
