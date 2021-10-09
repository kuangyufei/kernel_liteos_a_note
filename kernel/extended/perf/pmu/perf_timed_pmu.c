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

#define US_PER_SECOND               1000000
#define HRTIMER_DEFAULT_PERIOD_US   1000

STATIC SwPmu g_perfTimed;

STATIC BOOL OsPerfTimedPeriodValid(UINT32 period)
{
    return period >= TIMER_PERIOD_LOWER_BOUND_US;
}

STATIC UINT32 OsPerfTimedStart(VOID)
{
    UINT32 i;
    UINT32 cpuid = ArchCurrCpuid();
    PerfEvent *events = &g_perfTimed.pmu.events;
    UINT32 eventNum = events->nr;

    for (i = 0; i < eventNum; i++) {
        Event *event = &(events->per[i]);
        event->count[cpuid] = 0;
    }

    if (cpuid != 0) { /* only need start on one core */
        return LOS_OK;
    }

    if (hrtimer_start(&g_perfTimed.hrtimer, g_perfTimed.time, HRTIMER_MODE_REL) != 0) {
        PRINT_ERR("Hrtimer start failed\n");
        return LOS_NOK;
    }

    if (hrtimer_forward(&g_perfTimed.hrtimer, g_perfTimed.cfgTime) == 0) {
        PRINT_ERR("Hrtimer forward failed\n");
        return LOS_NOK;
    }

    g_perfTimed.time = g_perfTimed.cfgTime;
    return LOS_OK;
}

STATIC UINT32 OsPerfTimedConfig(VOID)
{
    UINT32 i;
    PerfEvent *events = &g_perfTimed.pmu.events;
    UINT32 eventNum = events->nr;

    for (i = 0; i < eventNum; i++) {
        Event *event = &(events->per[i]);
        UINT32 period = event->period;
        if (event->eventId == PERF_COUNT_CPU_CLOCK) {
            if (!OsPerfTimedPeriodValid(period)) {
                period = TIMER_PERIOD_LOWER_BOUND_US;
                PRINT_ERR("config period invalid, should be >= 100, use default period:%u us\n", period);
            }

            g_perfTimed.cfgTime = (union ktime) {
                .tv.sec  = period / US_PER_SECOND,
                .tv.usec = period % US_PER_SECOND
            };
            PRINT_INFO("hrtimer config period - sec:%d, usec:%d\n", g_perfTimed.cfgTime.tv.sec,
                g_perfTimed.cfgTime.tv.usec);
            return LOS_OK;
        }
    }
    return LOS_NOK;
}

STATIC UINT32 OsPerfTimedStop(VOID)
{
    UINT32 ret;

    if (ArchCurrCpuid() != 0) { /* only need stop on one core */
        return LOS_OK;
    }

    ret = hrtimer_cancel(&g_perfTimed.hrtimer);
    if (ret != 1) {
        PRINT_ERR("Hrtimer stop failed!, 0x%x\n", ret);
        return LOS_NOK;
    }
    return LOS_OK;
}

STATIC VOID OsPerfTimedHandle(VOID)
{
    UINT32 index;
    PerfRegs regs;

    PerfEvent *events = &g_perfTimed.pmu.events;
    UINT32 eventNum = events->nr;

    (VOID)memset_s(&regs, sizeof(PerfRegs), 0, sizeof(PerfRegs));
    OsPerfFetchIrqRegs(&regs);

    for (index = 0; index < eventNum; index++) {
        Event *event = &(events->per[index]);
        OsPerfUpdateEventCount(event, 1); /* eventCount += 1 every once */
        OsPerfHandleOverFlow(event, &regs);
    }
}

STATIC enum hrtimer_restart OsPerfHrtimer(struct hrtimer *hrtimer)
{
    SMP_CALL_PERF_FUNC(OsPerfTimedHandle); /* send to all cpu to collect data */
    return HRTIMER_RESTART;
}

STATIC CHAR *OsPerfGetEventName(Event *event)
{
    if (event->eventId == PERF_COUNT_CPU_CLOCK) {
        return "timed";
    } else {
        return "unknown";
    }
}

UINT32 OsTimedPmuInit(VOID)
{
    UINT32 ret;

    g_perfTimed.time = (union ktime) {
        .tv.sec = 0,
        .tv.usec = HRTIMER_DEFAULT_PERIOD_US,
    };

    hrtimer_init(&g_perfTimed.hrtimer, 1, HRTIMER_MODE_REL);

    ret = hrtimer_create(&g_perfTimed.hrtimer, g_perfTimed.time, OsPerfHrtimer);
    if (ret != LOS_OK) {
        return ret;
    }

    g_perfTimed.pmu = (Pmu) {
        .type    = PERF_EVENT_TYPE_TIMED,
        .config  = OsPerfTimedConfig,
        .start   = OsPerfTimedStart,
        .stop    = OsPerfTimedStop,
        .getName = OsPerfGetEventName,
    };

    (VOID)memset_s(&g_perfTimed.pmu.events, sizeof(PerfEvent), 0, sizeof(PerfEvent));
    ret = OsPerfPmuRegister(&g_perfTimed.pmu);
    return ret;
}
