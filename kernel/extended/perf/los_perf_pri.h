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

#ifndef _LOS_PERF_PRI_H
#define _LOS_PERF_PRI_H

#include "los_perf.h"
#include "perf.h"
#include "los_mp.h"
#include "los_task_pri.h"
#include "los_exc_pri.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#define PERF_EVENT_TO_CODE       0
#define PERF_CODE_TO_EVENT       1
#define PERF_DATA_MAGIC_WORD     0xEFEFEF00

#define SMP_CALL_PERF_FUNC(func)  OsMpFuncCall(OS_MP_CPU_ALL, (SMP_FUNC_CALL)func, NULL)

enum PmuStatus {
    PERF_PMU_STOPED,
    PERF_PMU_STARTED,
};

typedef struct {
    UINTPTR pc;
    UINTPTR fp;
} PerfRegs;

typedef struct {
    UINT32  ipNr;
    IpInfo  ip[PERF_MAX_CALLCHAIN_DEPTH];
} PerfBackTrace;

typedef struct {
    UINT32        cpuid;                /* cpu id */
    UINT32        taskId;               /* task id */
    UINT32        processId;            /* process id */
    UINT32        eventId;              /* record type */
    UINT32        period;               /* record period */
    UINT64        time;                 /* record time */
    IpInfo        pc;                   /* instruction pointer */
    PerfBackTrace callChain;            /* number of callChain ips */
} PerfSampleData;

typedef struct {
    UINT32      magic;           /* magic number */
    UINT32      eventType;       /* enum PerfEventType */
    UINT32      len;             /* sample data file length */
    UINT32      sampleType;      /* IP | TID | TIMESTAMP... */
    UINT32      sectionId;       /* section id */
} PerfDataHdr;

typedef struct {
    UINT32 counter;
    UINT32 eventId;
    UINT32 period;
    UINT64 count[LOSCFG_KERNEL_CORE_NUM];
} Event;

typedef struct {
    Event per[PERF_MAX_EVENT];
    UINT8 nr;
    UINT8 cntDivided;
} PerfEvent;

typedef struct {
    enum PerfEventType type;
    PerfEvent events;
    UINT32 (*config)(VOID);
    UINT32 (*start)(VOID);
    UINT32 (*stop)(VOID);
    CHAR *(*getName)(Event *event);
} Pmu;

typedef struct {
    /* time info */
    UINT64                  startTime;
    UINT64                  endTime;

    /* instrumentation status */
    enum PerfStatus         status;
    enum PmuStatus          pmuStatusPerCpu[LOSCFG_KERNEL_CORE_NUM];

    /* configuration info */
    UINT32                  sampleType;
    UINT32                  taskIds[PERF_MAX_FILTER_TSKS];
    UINT8                   taskIdsNr;
    UINT32                  processIds[PERF_MAX_FILTER_TSKS];
    UINT8                   processIdsNr;
    UINT8                   needSample;
} PerfCB;

#ifndef OsPerfArchFetchIrqRegs
STATIC INLINE VOID OsPerfArchFetchIrqRegs(PerfRegs *regs, LosTaskCB *curTask) {}
#endif

STATIC INLINE VOID OsPerfFetchIrqRegs(PerfRegs *regs)
{
    LosTaskCB *curTask = OsCurrTaskGet();
    OsPerfArchFetchIrqRegs(regs, curTask);
    PRINT_DEBUG("pc = 0x%x, fp = 0x%x\n", regs->pc, regs->fp);
}

#ifndef OsPerfArchFetchCallerRegs
STATIC INLINE VOID OsPerfArchFetchCallerRegs(PerfRegs *regs) {}
#endif

STATIC INLINE VOID OsPerfFetchCallerRegs(PerfRegs *regs)
{
    OsPerfArchFetchCallerRegs(regs);
    PRINT_DEBUG("pc = 0x%x, fp = 0x%x\n", regs->pc, regs->fp);
}

extern VOID OsPerfSetIrqRegs(UINTPTR pc, UINTPTR fp);
extern VOID OsPerfUpdateEventCount(Event *event, UINT32 value);
extern VOID OsPerfHandleOverFlow(Event *event, PerfRegs *regs);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_PERF_PRI_H */
