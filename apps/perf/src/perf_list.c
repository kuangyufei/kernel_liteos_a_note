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

#include <stdio.h>
#include "perf.h"
#include "perf_list.h"

static const char *g_eventTypeStr[] = {
    "[Hardware event]",
    "[Timed event]",
    "[Software event]",
};

const PerfEvent g_events[] = {
#ifdef LOSCFG_PERF_HW_PMU
    {
        .name = "cycles",
        .event = PERF_COUNT_HW_CPU_CYCLES,
        .type = PERF_EVENT_TYPE_HW,
    },
    {
        .name = "instruction",
        .event = PERF_COUNT_HW_INSTRUCTIONS,
        .type = PERF_EVENT_TYPE_HW,
    },
    {
        .name = "dcache",
        .event = PERF_COUNT_HW_DCACHE_REFERENCES,
        .type = PERF_EVENT_TYPE_HW,
    },
    {
        .name = "dcache-miss",
        .event = PERF_COUNT_HW_DCACHE_MISSES,
        .type = PERF_EVENT_TYPE_HW,
    },
    {
        .name = "icache",
        .event = PERF_COUNT_HW_ICACHE_REFERENCES,
        .type = PERF_EVENT_TYPE_HW,
    },
    {
        .name = "icache-miss",
        .event = PERF_COUNT_HW_ICACHE_MISSES,
        .type = PERF_EVENT_TYPE_HW,
    },
    {
        .name = "branch",
        .event = PERF_COUNT_HW_BRANCH_INSTRUCTIONS,
        .type = PERF_EVENT_TYPE_HW,
    },
    {
        .name = "branch-miss",
        .event = PERF_COUNT_HW_BRANCH_MISSES,
        .type = PERF_EVENT_TYPE_HW,
    },
#endif
#ifdef LOSCFG_PERF_TIMED_PMU
    {
        .name = "clock",
        .event = PERF_COUNT_CPU_CLOCK,
        .type = PERF_EVENT_TYPE_TIMED,
    },
#endif
#ifdef LOSCFG_PERF_SW_PMU
    {
        .name = "task-switch",
        .event = PERF_COUNT_SW_TASK_SWITCH,
        .type = PERF_EVENT_TYPE_SW,
    },
    {
        .name = "irq-in",
        .event = PERF_COUNT_SW_IRQ_RESPONSE,
        .type = PERF_EVENT_TYPE_SW,
    },
    {
        .name = "mem-alloc",
        .event = PERF_COUNT_SW_MEM_ALLOC,
        .type = PERF_EVENT_TYPE_SW,
    },
    {
        .name = "mux-pend",
        .event = PERF_COUNT_SW_MUX_PEND,
        .type = PERF_EVENT_TYPE_SW,
    },
#endif
    {
        .name = "",
        .event = -1,
        .type = PERF_EVENT_TYPE_MAX,
    }
};

void PerfList(void)
{
    const PerfEvent *evt = &g_events[0];
    printf("\n");
    for (; evt->event != -1; evt++) {
        printf("\t %-25s%30s\n", evt->name, g_eventTypeStr[evt->type]);
    }
    printf("\n");
}
