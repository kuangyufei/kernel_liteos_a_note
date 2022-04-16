/*
 * Copyright (c) 2022-2022 Huawei Device Co., Ltd. All rights reserved.
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

#include "los_statistics_pri.h"
#include "los_task_pri.h"
#include "los_process_pri.h"

#ifdef LOSCFG_SCHED_DEBUG
#ifdef LOSCFG_SCHED_TICK_DEBUG
typedef struct {
    UINT64 responseTime;
    UINT64 responseTimeMax;
    UINT64 count;
} SchedTickDebug;
STATIC SchedTickDebug *g_schedTickDebug = NULL;

UINT32 OsSchedDebugInit(VOID)
{
    UINT32 size = sizeof(SchedTickDebug) * LOSCFG_KERNEL_CORE_NUM;
    g_schedTickDebug = (SchedTickDebug *)LOS_MemAlloc(m_aucSysMem0, size);
    if (g_schedTickDebug == NULL) {
        return LOS_ERRNO_TSK_NO_MEMORY;
    }

    (VOID)memset_s(g_schedTickDebug, size, 0, size);
    return LOS_OK;
}

VOID OsSchedDebugRecordData(VOID)
{
    SchedRunqueue *rq = OsSchedRunqueue();
    SchedTickDebug *schedDebug = &g_schedTickDebug[ArchCurrCpuid()];
    UINT64 currTime = OsGetCurrSchedTimeCycle();
    LOS_ASSERT(currTime >= rq->responseTime);
    UINT64 usedTime = currTime - rq->responseTime;
    schedDebug->responseTime += usedTime;
    if (usedTime > schedDebug->responseTimeMax) {
        schedDebug->responseTimeMax = usedTime;
    }
    schedDebug->count++;
}

UINT32 OsShellShowTickResponse(VOID)
{
    UINT32 intSave;
    UINT16 cpu;

    UINT32 tickSize = sizeof(SchedTickDebug) * LOSCFG_KERNEL_CORE_NUM;
    SchedTickDebug *schedDebug = (SchedTickDebug *)LOS_MemAlloc(m_aucSysMem1, tickSize);
    if (schedDebug == NULL) {
        return LOS_NOK;
    }

    SCHEDULER_LOCK(intSave);
    (VOID)memcpy_s((CHAR *)schedDebug, tickSize, (CHAR *)g_schedTickDebug, tickSize);
    SCHEDULER_UNLOCK(intSave);

    PRINTK("cpu   ATRTime(us) ATRTimeMax(us)  TickCount\n");
    for (cpu = 0; cpu < LOSCFG_KERNEL_CORE_NUM; cpu++) {
        SchedTickDebug *schedData = &schedDebug[cpu];
        UINT64 averTime = 0;
        if (schedData->count > 0) {
            averTime = schedData->responseTime / schedData->count;
            averTime = (averTime * OS_NS_PER_CYCLE) / OS_SYS_NS_PER_US;
        }
        UINT64 timeMax = (schedData->responseTimeMax * OS_NS_PER_CYCLE) / OS_SYS_NS_PER_US;
        PRINTK("%3u%14llu%15llu%11llu\n", cpu, averTime, timeMax, schedData->count);
    }

    (VOID)LOS_MemFree(m_aucSysMem1, schedDebug);
    return LOS_OK;
}
#endif

STATIC VOID SchedDataGet(const LosTaskCB *taskCB, UINT64 *runTime, UINT64 *timeSlice,
                         UINT64 *pendTime, UINT64 *schedWait)
{
    if (taskCB->schedStat.switchCount >= 1) {
        UINT64 averRunTime = taskCB->schedStat.runTime / taskCB->schedStat.switchCount;
        *runTime = (averRunTime * OS_NS_PER_CYCLE) / OS_SYS_NS_PER_US;
    }

    if (taskCB->schedStat.timeSliceCount > 1) {
        UINT64 averTimeSlice = taskCB->schedStat.timeSliceTime / (taskCB->schedStat.timeSliceCount - 1);
        *timeSlice = (averTimeSlice * OS_NS_PER_CYCLE) / OS_SYS_NS_PER_US;
    }

    if (taskCB->schedStat.pendCount > 1) {
        UINT64 averPendTime = taskCB->schedStat.pendTime / taskCB->schedStat.pendCount;
        *pendTime = (averPendTime * OS_NS_PER_CYCLE) / OS_SYS_NS_PER_US;
    }

    if (taskCB->schedStat.waitSchedCount > 0) {
        UINT64 averSchedWait = taskCB->schedStat.waitSchedTime / taskCB->schedStat.waitSchedCount;
        *schedWait = (averSchedWait * OS_NS_PER_CYCLE) / OS_SYS_NS_PER_US;
    }
}

UINT32 OsShellShowSchedStatistics(VOID)
{
    UINT32 taskLinkNum[LOSCFG_KERNEL_CORE_NUM];
    UINT32 intSave;
    LosTaskCB task;

    SCHEDULER_LOCK(intSave);
    for (UINT16 cpu = 0; cpu < LOSCFG_KERNEL_CORE_NUM; cpu++) {
        SchedRunqueue *rq = OsSchedRunqueueByID(cpu);
        taskLinkNum[cpu] = OsGetSortLinkNodeNum(&rq->timeoutQueue);
    }
    SCHEDULER_UNLOCK(intSave);

    for (UINT16 cpu = 0; cpu < LOSCFG_KERNEL_CORE_NUM; cpu++) {
        PRINTK("cpu: %u Task SortMax: %u\n", cpu, taskLinkNum[cpu]);
    }

    PRINTK("  Tid    AverRunTime(us)    SwitchCount  AverTimeSlice(us)    TimeSliceCount  AverReadyWait(us)  "
           "AverPendTime(us)  TaskName \n");
    for (UINT32 tid = 0; tid < g_taskMaxNum; tid++) {
        LosTaskCB *taskCB = g_taskCBArray + tid;
        SCHEDULER_LOCK(intSave);
        if (OsTaskIsUnused(taskCB) || (taskCB->processID == OsGetIdleProcessID())) {
            SCHEDULER_UNLOCK(intSave);
            continue;
        }

        (VOID)memcpy_s(&task, sizeof(LosTaskCB), taskCB, sizeof(LosTaskCB));
        SCHEDULER_UNLOCK(intSave);

        UINT64 averRunTime = 0;
        UINT64 averTimeSlice = 0;
        UINT64 averPendTime = 0;
        UINT64 averSchedWait = 0;

        SchedDataGet(&task, &averRunTime, &averTimeSlice, &averPendTime, &averSchedWait);

        PRINTK("%5u%19llu%15llu%19llu%18llu%19llu%18llu  %-32s\n", taskCB->taskID,
               averRunTime, taskCB->schedStat.switchCount,
               averTimeSlice, taskCB->schedStat.timeSliceCount - 1,
               averSchedWait, averPendTime, taskCB->taskName);
    }

    return LOS_OK;
}
#endif

