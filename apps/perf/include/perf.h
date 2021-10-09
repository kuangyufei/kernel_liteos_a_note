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


#ifndef _PERF_H
#define _PERF_H

#include <stdlib.h>

#ifdef  __cplusplus
#if  __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#define PERF_MAX_EVENT          7
#define PERF_MAX_FILTER_TSKS    32

#ifdef PERF_DEBUG
#define printf_debug(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define printf_debug(fmt, ...)
#endif

/*
 * Perf types
 */
enum PerfEventType {
    PERF_EVENT_TYPE_HW,      /* boards common hw events */
    PERF_EVENT_TYPE_TIMED,   /* hrtimer timed events */
    PERF_EVENT_TYPE_SW,      /* software trace events */
    PERF_EVENT_TYPE_RAW,     /* boards special hw events, see enum PmuEventType in corresponding arch headfile */

    PERF_EVENT_TYPE_MAX
};

/*
 * Common hardware pmu events
 */
enum PmuHwId {
    PERF_COUNT_HW_CPU_CYCLES = 0,      /* cpu cycle event */
    PERF_COUNT_HW_INSTRUCTIONS,        /* instruction event */
    PERF_COUNT_HW_DCACHE_REFERENCES,   /* dcache access event */
    PERF_COUNT_HW_DCACHE_MISSES,       /* dcache miss event */
    PERF_COUNT_HW_ICACHE_REFERENCES,   /* icache access event */
    PERF_COUNT_HW_ICACHE_MISSES,       /* icache miss event */
    PERF_COUNT_HW_BRANCH_INSTRUCTIONS, /* software change of pc event */
    PERF_COUNT_HW_BRANCH_MISSES,       /* branch miss event */

    PERF_COUNT_HW_MAX,
};

/*
 * Common hrtimer timed events
 */
enum PmuTimedId {
    PERF_COUNT_CPU_CLOCK = 0,      /* hrtimer timed event */
};

/*
 * Common software pmu events
 */
enum PmuSwId {
    PERF_COUNT_SW_TASK_SWITCH = 1, /* task switch event */
    PERF_COUNT_SW_IRQ_RESPONSE,    /* irq response event */
    PERF_COUNT_SW_MEM_ALLOC,       /* memory alloc event */
    PERF_COUNT_SW_MUX_PEND,        /* mutex pend event */

    PERF_COUNT_SW_MAX,
};

/*
 * perf sample data types
 * Config it through PerfConfigAttr->sampleType.
 */
enum PerfSampleType {
    PERF_RECORD_CPU       = 1U << 0, /* record current cpuid */
    PERF_RECORD_TID       = 1U << 1, /* record current task id */
    PERF_RECORD_TYPE      = 1U << 2, /* record event type */
    PERF_RECORD_PERIOD    = 1U << 3, /* record event period */
    PERF_RECORD_TIMESTAMP = 1U << 4, /* record timestamp */
    PERF_RECORD_IP        = 1U << 5, /* record instruction pointer */
    PERF_RECORD_CALLCHAIN = 1U << 6, /* record backtrace */
    PERF_RECORD_PID       = 1U << 7, /* record current process id */
};

/*
 * perf configuration sub event information
 *
 * This structure is used to config specific events attributes.
 */
typedef struct {
    unsigned int type;              /* enum PerfEventType */
    struct {
        unsigned int eventId;       /* the specific event corresponds to the PerfEventType */
        unsigned int period;        /* event period, for every "period"th occurrence of the event a
                                        sample will be recorded */
    } events[PERF_MAX_EVENT];       /* perf event list */
    unsigned int eventsNr;          /* total perf event number */
    size_t predivided;              /* whether to prescaler (once every 64 counts),
                                        which only take effect on cpu cycle hardware event */
} PerfEventConfig;

/*
 * perf configuration main information
 *
 * This structure is used to set perf sampling attributes, including events, tasks and other information.
 */
typedef struct {
    PerfEventConfig         eventsCfg;                      /* perf event config */
    unsigned int            taskIds[PERF_MAX_FILTER_TSKS];  /* perf task filter list (allowlist) */
    unsigned int            taskIdsNr;                      /* task numbers of task filter allowlist,
                                                                 if set 0 perf will sample all tasks */
    unsigned int            processIds[PERF_MAX_FILTER_TSKS];  /* perf process filter list (allowlist) */
    unsigned int            processIdsNr;                      /* process numbers of process filter allowlist,
                                                                 if set 0 perf will sample all processes */
    unsigned int            sampleType;                     /* type of data to sample defined in PerfSampleType */
    size_t                  needSample;                     /* whether to sample data */
} PerfConfigAttr;

void PerfUsage(void);
void PerfDumpAttr(PerfConfigAttr *attr);
int PerfConfig(int fd, PerfConfigAttr *attr);
void PerfStart(int fd, size_t sectionId);
void PerfStop(int fd);
ssize_t PerfRead(int fd, char *buf, size_t size);
void PerfPrintBuffer(const char *buf, ssize_t num);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif /* _PERF_H */
