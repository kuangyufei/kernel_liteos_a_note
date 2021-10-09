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

#include <unistd.h>
#include <securec.h>
#include <sys/wait.h>
#include "perf.h"
#include "option.h"
#include "perf_stat.h"

static PerfConfigAttr g_statAttr;

static inline int GetEvents(const char *argv)
{
    return ParseEvents(argv, &g_statAttr.eventsCfg, &g_statAttr.eventsCfg.eventsNr);
}

static inline int GetTids(const char *argv)
{
    return ParseIds(argv, (int *)g_statAttr.taskIds, &g_statAttr.taskIdsNr);
}

static inline int GetPids(const char *argv)
{
    return ParseIds(argv, (int *)g_statAttr.processIds, &g_statAttr.processIdsNr);
}

static PerfOption g_statOpts[] = {
    OPTION_CALLBACK("-e", GetEvents),
    OPTION_CALLBACK("-t", GetTids),
    OPTION_CALLBACK("-P", GetPids),
    OPTION_UINT("-p", &g_statAttr.eventsCfg.events[0].period),
    OPTION_UINT("-s", &g_statAttr.sampleType),
    OPTION_UINT("-d", &g_statAttr.eventsCfg.predivided),
};

static int PerfStatAttrInit(void)
{
    PerfConfigAttr attr = {
        .eventsCfg = {
#ifdef LOSCFG_PERF_HW_PMU
            .type = PERF_EVENT_TYPE_HW,
            .events = {
                [0] = {PERF_COUNT_HW_CPU_CYCLES, 0xFFFF},
                [1] = {PERF_COUNT_HW_INSTRUCTIONS, 0xFFFFFF00},
                [2] = {PERF_COUNT_HW_ICACHE_REFERENCES, 0xFFFF},
                [3] = {PERF_COUNT_HW_DCACHE_REFERENCES, 0xFFFF},
            },
            .eventsNr = 4, /* 4 events */
#elif defined LOSCFG_PERF_TIMED_PMU
            .type = PERF_EVENT_TYPE_TIMED,
            .events = {
                [0] = {PERF_COUNT_CPU_CLOCK, 100},
            },
            .eventsNr = 1, /* 1 event */
#elif defined LOSCFG_PERF_SW_PMU
            .type = PERF_EVENT_TYPE_SW,
            .events = {
                [0] = {PERF_COUNT_SW_TASK_SWITCH, 1},
                [1] = {PERF_COUNT_SW_IRQ_RESPONSE, 1},
                [2] = {PERF_COUNT_SW_MEM_ALLOC, 1},
                [3] = {PERF_COUNT_SW_MUX_PEND, 1},
            },
            .eventsNr = 4, /* 4 events */
#endif
            .predivided = 0,
        },
        .taskIds = {0},
        .taskIdsNr = 0,
        .processIds = {0},
        .processIdsNr = 0,
        .needSample = 0,
        .sampleType = 0,
    };

    return memcpy_s(&g_statAttr, sizeof(PerfConfigAttr), &attr, sizeof(PerfConfigAttr)) != EOK ? -1 : 0;
}

void PerfStat(int fd, int argc, char **argv)
{
    int ret;
    int child;
    SubCmd cmd = {0};

    if (argc < 3) { /* perf stat argc is at least 3 */
        return;
    }

    ret = PerfStatAttrInit();
    if (ret != 0) {
        printf("perf stat attr init failed\n");
        return;
    }

    ret = ParseOptions(argc - 2, &argv[2], g_statOpts, &cmd); /* parse option and cmd begin at index 2 */
    if (ret != 0) {
        printf("parse error\n");
        return;
    }

    ret = PerfConfig(fd, &g_statAttr);
    if (ret != 0) {
        printf("perf config failed\n");
        return;
    }

    PerfStart(fd, 0);
    child = fork();
    if (child < 0) {
        printf("fork error\n");
        goto EXIT;
    } else if (child == 0) {
        (void)execve(cmd.path, cmd.params, NULL);
        exit(0);
    }

    (void)waitpid(child, 0, 0);
EXIT:
    PerfStop(fd);
}

