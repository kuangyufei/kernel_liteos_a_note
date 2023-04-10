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

#ifdef LOSCFG_SCHED_HPF_DEBUG
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
    SchedEDF *sched = NULL;

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
        if (OsTaskIsUnused(taskCB) || (taskCB->processCB == (UINTPTR)OsGetIdleProcess())) {
            SCHEDULER_UNLOCK(intSave);
            continue;
        }

        sched = (SchedEDF *)&taskCB->sp;
        if (sched->policy == LOS_SCHED_DEADLINE) {
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

#ifdef LOSCFG_SCHED_EDF_DEBUG
#define EDF_DEBUG_NODE 20
typedef struct {
    UINT32 tid;
    INT32 runTimeUs;
    UINT64 deadlineUs;
    UINT64 periodUs;
    UINT64 startTime;
    UINT64 finishTime;
    UINT64 nextfinishTime;
    UINT64 timeSliceUnused;
    UINT64 timeSliceRealTime;
    UINT64 allRuntime;
    UINT64 pendTime;
} EDFDebug;

STATIC EDFDebug g_edfNode[EDF_DEBUG_NODE];
STATIC INT32 g_edfNodePointer = 0;

VOID EDFDebugRecord(UINTPTR *task, UINT64 oldFinish)
{
    LosTaskCB *taskCB = (LosTaskCB *)task;
    SchedEDF *sched = (SchedEDF *)&taskCB->sp;
    SchedParam param;

    // when print edf info, will stop record
    if (g_edfNodePointer == (EDF_DEBUG_NODE + 1)) {
        return;
    }

    taskCB->ops->schedParamGet(taskCB, &param);
    g_edfNode[g_edfNodePointer].tid = taskCB->taskID;
    g_edfNode[g_edfNodePointer].runTimeUs =param.runTimeUs;
    g_edfNode[g_edfNodePointer].deadlineUs =param.deadlineUs;
    g_edfNode[g_edfNodePointer].periodUs =param.periodUs;
    g_edfNode[g_edfNodePointer].startTime = taskCB->startTime;
    if (taskCB->timeSlice <= 0) {
        taskCB->irqUsedTime = 0;
        g_edfNode[g_edfNodePointer].timeSliceUnused = 0;
    } else {
        g_edfNode[g_edfNodePointer].timeSliceUnused = taskCB->timeSlice;
    }
    g_edfNode[g_edfNodePointer].finishTime = oldFinish;
    g_edfNode[g_edfNodePointer].nextfinishTime = sched->finishTime;
    g_edfNode[g_edfNodePointer].timeSliceRealTime = taskCB->schedStat.timeSliceRealTime;
    g_edfNode[g_edfNodePointer].allRuntime = taskCB->schedStat.allRuntime;
    g_edfNode[g_edfNodePointer].pendTime = taskCB->schedStat.pendTime;

    g_edfNodePointer++;
    if (g_edfNodePointer == EDF_DEBUG_NODE) {
        g_edfNodePointer = 0;
    }
}

STATIC VOID EDFInfoPrint(int idx)
{
    INT32 runTimeUs;
    UINT64 deadlineUs;
    UINT64 periodUs;
    UINT64 startTime;
    UINT64 timeSlice;
    UINT64 finishTime;
    UINT64 nextfinishTime;
    UINT64 pendTime;
    UINT64 allRuntime;
    UINT64 timeSliceRealTime;
    CHAR *status = NULL;

    startTime = OS_SYS_CYCLE_TO_US(g_edfNode[idx].startTime);
    timeSlice = OS_SYS_CYCLE_TO_US(g_edfNode[idx].timeSliceUnused);
    finishTime = OS_SYS_CYCLE_TO_US(g_edfNode[idx].finishTime);
    nextfinishTime = OS_SYS_CYCLE_TO_US(g_edfNode[idx].nextfinishTime);
    pendTime = OS_SYS_CYCLE_TO_US(g_edfNode[idx].pendTime);
    allRuntime = OS_SYS_CYCLE_TO_US(g_edfNode[idx].allRuntime);
    timeSliceRealTime = OS_SYS_CYCLE_TO_US(g_edfNode[idx].timeSliceRealTime);
    runTimeUs = g_edfNode[idx].runTimeUs;
    deadlineUs = g_edfNode[idx].deadlineUs;
    periodUs = g_edfNode[idx].periodUs;

    if (timeSlice > 0) {
        status = "TIMEOUT";
    } else if (nextfinishTime == finishTime) {
        status = "NEXT PERIOD";
    } else {
        status = "WAIT RUN";
    }

    PRINTK("%4u%9d%9llu%9llu%12llu%12llu%12llu%9llu%9llu%9llu%9llu %-12s\n",
           g_edfNode[idx].tid, runTimeUs, deadlineUs, periodUs,
           startTime, finishTime, nextfinishTime, allRuntime, timeSliceRealTime,
           timeSlice, pendTime, status);
}

VOID OsEDFDebugPrint(VOID)
{
    INT32 max;
    UINT32 intSave;
    INT32 i;

    SCHEDULER_LOCK(intSave);
    max = g_edfNodePointer;
    g_edfNodePointer = EDF_DEBUG_NODE + 1;
    SCHEDULER_UNLOCK(intSave);

    PRINTK("\r\nlast %d sched is: (in microsecond)\r\n", EDF_DEBUG_NODE);

    PRINTK(" TID  RunTime Deadline   Period   StartTime   "
           "CurPeriod  NextPeriod   AllRun  RealRun  TimeOut WaitTime Status\n");

    for (i = max; i < EDF_DEBUG_NODE; i++) {
        EDFInfoPrint(i);
    }

    for (i = 0; i < max; i++) {
        EDFInfoPrint(i);
    }

    SCHEDULER_LOCK(intSave);
    g_edfNodePointer = max;
    SCHEDULER_UNLOCK(intSave);
}

UINT32 OsShellShowEdfSchedStatistics(VOID)
{
    UINT32 intSave;
    LosTaskCB task;
    UINT64 curTime;
    UINT64 deadline;
    UINT64 finishTime;
    SchedEDF *sched = NULL;

    PRINTK("Now Alive EDF Thread:\n");
    PRINTK("TID        CurTime       DeadTime     FinishTime  taskName\n");

    for (UINT32 tid = 0; tid < g_taskMaxNum; tid++) {
        LosTaskCB *taskCB = g_taskCBArray + tid;
        SCHEDULER_LOCK(intSave);
        if (OsTaskIsUnused(taskCB)) {
            SCHEDULER_UNLOCK(intSave);
            continue;
        }

        sched = (SchedEDF *)&taskCB->sp;
        if (sched->policy != LOS_SCHED_DEADLINE) {
            SCHEDULER_UNLOCK(intSave);
            continue;
        }

        (VOID)memcpy_s(&task, sizeof(LosTaskCB), taskCB, sizeof(LosTaskCB));

        curTime = OS_SYS_CYCLE_TO_US(HalClockGetCycles());
        finishTime = OS_SYS_CYCLE_TO_US(sched->finishTime);
        deadline = OS_SYS_CYCLE_TO_US(taskCB->ops->deadlineGet(taskCB));
        SCHEDULER_UNLOCK(intSave);

        PRINTK("%3u%15llu%15llu%15llu  %-32s\n",
               task.taskID, curTime, deadline, finishTime, task.taskName);
    }

    OsEDFDebugPrint();

    return LOS_OK;
}
#endif
#endif
