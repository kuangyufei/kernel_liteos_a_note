/*
 * Copyright (c) 2023-2023 Huawei Device Co., Ltd. All rights reserved.
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

#include "los_sched_pri.h"
#include "los_task_pri.h"
#include "los_process_pri.h"
#include "los_hook.h"
#include "los_tick_pri.h"
#include "los_sys_pri.h"

STATIC EDFRunqueue g_schedEDF;

STATIC VOID EDFDequeue(SchedRunqueue *rq, LosTaskCB *taskCB);
STATIC VOID EDFEnqueue(SchedRunqueue *rq, LosTaskCB *taskCB);
STATIC UINT64 EDFWaitTimeGet(LosTaskCB *taskCB);
STATIC UINT32 EDFWait(LosTaskCB *runTask, LOS_DL_LIST *list, UINT32 ticks);
STATIC VOID EDFWake(LosTaskCB *resumedTask);
STATIC BOOL EDFSchedParamModify(LosTaskCB *taskCB, const SchedParam *param);
STATIC UINT32 EDFSchedParamGet(const LosTaskCB *taskCB, SchedParam *param);
STATIC UINT32 EDFDelay(LosTaskCB *runTask, UINT64 waitTime);
STATIC VOID EDFYield(LosTaskCB *runTask);
STATIC VOID EDFExit(LosTaskCB *taskCB);
STATIC UINT32 EDFSuspend(LosTaskCB *taskCB);
STATIC UINT32 EDFResume(LosTaskCB *taskCB, BOOL *needSched);
STATIC UINT64 EDFTimeSliceGet(const LosTaskCB *taskCB);
STATIC VOID EDFTimeSliceUpdate(SchedRunqueue *rq, LosTaskCB *taskCB, UINT64 currTime);
STATIC INT32 EDFParamCompare(const SchedPolicy *sp1, const SchedPolicy *sp2);
STATIC VOID EDFPriorityInheritance(LosTaskCB *owner, const SchedParam *param);
STATIC VOID EDFPriorityRestore(LosTaskCB *owner, const LOS_DL_LIST *list, const SchedParam *param);

const STATIC SchedOps g_deadlineOps = {
    .dequeue = EDFDequeue,
    .enqueue = EDFEnqueue,
    .waitTimeGet = EDFWaitTimeGet,
    .wait = EDFWait,
    .wake = EDFWake,
    .schedParamModify = EDFSchedParamModify,
    .schedParamGet = EDFSchedParamGet,
    .delay = EDFDelay,
    .yield = EDFYield,
    .start = EDFDequeue,
    .exit = EDFExit,
    .suspend = EDFSuspend,
    .resume = EDFResume,
    .deadlineGet = EDFTimeSliceGet,
    .timeSliceUpdate = EDFTimeSliceUpdate,
    .schedParamCompare = EDFParamCompare,
    .priorityInheritance = EDFPriorityInheritance,
    .priorityRestore = EDFPriorityRestore,
};

STATIC VOID EDFTimeSliceUpdate(SchedRunqueue *rq, LosTaskCB *taskCB, UINT64 currTime)
{
    SchedEDF *sched = (SchedEDF *)&taskCB->sp;

    LOS_ASSERT(currTime >= taskCB->startTime);

    if (taskCB->timeSlice <= 0) {
        taskCB->irqUsedTime = 0;
        return;
    }

    INT32 incTime = (currTime - taskCB->startTime - taskCB->irqUsedTime);
    LOS_ASSERT(incTime >= 0);

#ifdef LOSCFG_SCHED_EDF_DEBUG
    taskCB->schedStat.timeSliceRealTime += incTime;
    taskCB->schedStat.allRuntime += (currTime - taskCB->startTime);
#endif

    taskCB->timeSlice -= incTime;
    taskCB->irqUsedTime = 0;
    taskCB->startTime = currTime;

    if ((sched->finishTime > currTime) && (taskCB->timeSlice > 0)) {
        return;
    }

    rq->schedFlag |= INT_PEND_RESCH;
    if (sched->finishTime <= currTime) {
#ifdef LOSCFG_SCHED_EDF_DEBUG
        EDFDebugRecord((UINTPTR *)taskCB, sched->finishTime);
#endif

        taskCB->timeSlice = 0;
        PrintExcInfo("EDF task %u is timeout, runTime: %d us period: %llu us\n", taskCB->taskID,
                     (INT32)OS_SYS_CYCLE_TO_US((UINT64)sched->runTime), OS_SYS_CYCLE_TO_US(sched->period));
    }
}

STATIC UINT64 EDFTimeSliceGet(const LosTaskCB *taskCB)
{
    SchedEDF *sched = (SchedEDF *)&taskCB->sp;
    UINT64 endTime = taskCB->startTime + taskCB->timeSlice;
    return (endTime > sched->finishTime) ? sched->finishTime : endTime;
}

STATIC VOID DeadlineQueueInsert(EDFRunqueue *rq, LosTaskCB *taskCB)
{
    LOS_DL_LIST *root = &rq->root;
    if (LOS_ListEmpty(root)) {
        LOS_ListTailInsert(root, &taskCB->pendList);
        return;
    }

    LOS_DL_LIST *list = root->pstNext;
    do {
        LosTaskCB *readyTask = LOS_DL_LIST_ENTRY(list, LosTaskCB, pendList);
        if (EDFParamCompare(&readyTask->sp, &taskCB->sp) > 0) {
            LOS_ListHeadInsert(list, &taskCB->pendList);
            return;
        }
        list = list->pstNext;
    } while (list != root);

    LOS_ListHeadInsert(list, &taskCB->pendList);
}

STATIC VOID EDFEnqueue(SchedRunqueue *rq, LosTaskCB *taskCB)
{
    LOS_ASSERT(!(taskCB->taskStatus & OS_TASK_STATUS_READY));

    EDFRunqueue *erq = rq->edfRunqueue;
    SchedEDF *sched = (SchedEDF *)&taskCB->sp;
    if (taskCB->timeSlice <= 0) {
#ifdef LOSCFG_SCHED_EDF_DEBUG
        UINT64 oldFinish = sched->finishTime;
#endif
        UINT64 currTime = OsGetCurrSchedTimeCycle();
        if (sched->flags == EDF_INIT) {
            sched->finishTime = currTime;
        } else if (sched->flags != EDF_NEXT_PERIOD) {
            /* The start time of the next period */
            sched->finishTime = (sched->finishTime - sched->deadline) + sched->period;
        }

        /* Calculate the start time of the next period */
        while (1) {
            /* The deadline of the next period */
            UINT64 finishTime = sched->finishTime + sched->deadline;
            if ((finishTime <= currTime) || ((sched->finishTime + sched->runTime) > finishTime)) {
                /* This period cannot meet the minimum running time, so it is migrated to the next period */
                sched->finishTime += sched->period;
                continue;
            }
            break;
        }

        if (sched->finishTime > currTime) {
            /* Wait for the next period to start */
            LOS_ListTailInsert(&erq->waitList, &taskCB->pendList);
            taskCB->waitTime = OS_SCHED_MAX_RESPONSE_TIME;
            if (!OsTaskIsRunning(taskCB)) {
                OsSchedTimeoutQueueAdd(taskCB, sched->finishTime);
            }
#ifdef LOSCFG_SCHED_EDF_DEBUG
            if (oldFinish != sched->finishTime) {
                EDFDebugRecord((UINTPTR *)taskCB, oldFinish);
                taskCB->schedStat.allRuntime = 0;
                taskCB->schedStat.timeSliceRealTime = 0;
                taskCB->schedStat.pendTime = 0;
            }
#endif
            taskCB->taskStatus |= OS_TASK_STATUS_PEND_TIME;
            sched->flags = EDF_NEXT_PERIOD;
            return;
        }

        sched->finishTime += sched->deadline;
        taskCB->timeSlice = sched->runTime;
        sched->flags = EDF_UNUSED;
    }

    DeadlineQueueInsert(erq, taskCB);
    taskCB->taskStatus &= ~(OS_TASK_STATUS_BLOCKED | OS_TASK_STATUS_TIMEOUT);
    taskCB->taskStatus |= OS_TASK_STATUS_READY;
}

STATIC VOID EDFDequeue(SchedRunqueue *rq, LosTaskCB *taskCB)
{
    (VOID)rq;
    LOS_ListDelete(&taskCB->pendList);
    taskCB->taskStatus &= ~OS_TASK_STATUS_READY;
}

STATIC VOID EDFExit(LosTaskCB *taskCB)
{
    if (taskCB->taskStatus & OS_TASK_STATUS_READY) {
        EDFDequeue(OsSchedRunqueue(), taskCB);
    } else if (taskCB->taskStatus & OS_TASK_STATUS_PENDING) {
        LOS_ListDelete(&taskCB->pendList);
        taskCB->taskStatus &= ~OS_TASK_STATUS_PENDING;
    }
    if (taskCB->taskStatus & (OS_TASK_STATUS_DELAY | OS_TASK_STATUS_PEND_TIME)) {
        OsSchedTimeoutQueueDelete(taskCB);
        taskCB->taskStatus &= ~(OS_TASK_STATUS_DELAY | OS_TASK_STATUS_PEND_TIME);
    }
}

STATIC VOID EDFYield(LosTaskCB *runTask)
{
    SchedRunqueue *rq = OsSchedRunqueue();
    runTask->timeSlice = 0;

    runTask->startTime = OsGetCurrSchedTimeCycle();
    EDFEnqueue(rq, runTask);
    OsSchedResched();
}

STATIC UINT32 EDFDelay(LosTaskCB *runTask, UINT64 waitTime)
{
    runTask->taskStatus |= OS_TASK_STATUS_DELAY;
    runTask->waitTime = waitTime;
    OsSchedResched();
    return LOS_OK;
}

STATIC UINT64 EDFWaitTimeGet(LosTaskCB *taskCB)
{
    const SchedEDF *sched = (const SchedEDF *)&taskCB->sp;
    if (sched->flags != EDF_WAIT_FOREVER) {
        taskCB->waitTime += taskCB->startTime;
    }
    return (taskCB->waitTime >= sched->finishTime) ? sched->finishTime : taskCB->waitTime;
}

STATIC UINT32 EDFWait(LosTaskCB *runTask, LOS_DL_LIST *list, UINT32 ticks)
{
    SchedEDF *sched = (SchedEDF *)&runTask->sp;
    runTask->taskStatus |= (OS_TASK_STATUS_PENDING | OS_TASK_STATUS_PEND_TIME);
    LOS_ListTailInsert(list, &runTask->pendList);

    if (ticks != LOS_WAIT_FOREVER) {
        runTask->waitTime = OS_SCHED_TICK_TO_CYCLE(ticks);
    } else {
        sched->flags = EDF_WAIT_FOREVER;
        runTask->waitTime = OS_SCHED_MAX_RESPONSE_TIME;
    }

    if (OsPreemptableInSched()) {
        OsSchedResched();
        if (runTask->taskStatus & OS_TASK_STATUS_TIMEOUT) {
            runTask->taskStatus &= ~OS_TASK_STATUS_TIMEOUT;
            return LOS_ERRNO_TSK_TIMEOUT;
        }
    }

    return LOS_OK;
}

STATIC VOID EDFWake(LosTaskCB *resumedTask)
{
    LOS_ListDelete(&resumedTask->pendList);
    resumedTask->taskStatus &= ~OS_TASK_STATUS_PENDING;

    if (resumedTask->taskStatus & OS_TASK_STATUS_PEND_TIME) {
        OsSchedTimeoutQueueDelete(resumedTask);
        resumedTask->taskStatus &= ~OS_TASK_STATUS_PEND_TIME;
    }

    if (!(resumedTask->taskStatus & OS_TASK_STATUS_SUSPENDED)) {
#ifdef LOSCFG_SCHED_EDF_DEBUG
        resumedTask->schedStat.pendTime += OsGetCurrSchedTimeCycle() - resumedTask->startTime;
        resumedTask->schedStat.pendCount++;
#endif
        EDFEnqueue(OsSchedRunqueue(), resumedTask);
    }
}

STATIC BOOL EDFSchedParamModify(LosTaskCB *taskCB, const SchedParam *param)
{
    SchedRunqueue *rq = OsSchedRunqueue();
    SchedEDF *sched = (SchedEDF *)&taskCB->sp;

    taskCB->timeSlice = 0;
    sched->runTime = (INT32)OS_SYS_US_TO_CYCLE(param->runTimeUs);
    sched->period = OS_SYS_US_TO_CYCLE(param->periodUs);

    if (taskCB->taskStatus & OS_TASK_STATUS_READY) {
        EDFDequeue(rq, taskCB);
        sched->deadline = OS_SYS_US_TO_CYCLE(param->deadlineUs);
        EDFEnqueue(rq, taskCB);
        return TRUE;
    }

    sched->deadline = OS_SYS_US_TO_CYCLE(param->deadlineUs);
    if (taskCB->taskStatus & OS_TASK_STATUS_INIT) {
        EDFEnqueue(rq, taskCB);
        return TRUE;
    }

    if (taskCB->taskStatus & OS_TASK_STATUS_RUNNING) {
        return TRUE;
    }

    return FALSE;
}

STATIC UINT32 EDFSchedParamGet(const LosTaskCB *taskCB, SchedParam *param)
{
    SchedEDF *sched = (SchedEDF *)&taskCB->sp;
    param->policy = sched->policy;
    param->runTimeUs = (INT32)OS_SYS_CYCLE_TO_US((UINT64)sched->runTime);
    param->deadlineUs = OS_SYS_CYCLE_TO_US(sched->deadline);
    param->periodUs = OS_SYS_CYCLE_TO_US(sched->period);
    return LOS_OK;
}

STATIC UINT32 EDFSuspend(LosTaskCB *taskCB)
{
    return LOS_EOPNOTSUPP;
}

STATIC UINT32 EDFResume(LosTaskCB *taskCB, BOOL *needSched)
{
    return LOS_EOPNOTSUPP;
}

STATIC INT32 EDFParamCompare(const SchedPolicy *sp1, const SchedPolicy *sp2)
{
    const SchedEDF *param1 = (const SchedEDF *)sp1;
    const SchedEDF *param2 = (const SchedEDF *)sp2;

    return (INT32)(param1->finishTime - param2->finishTime);
}

STATIC VOID EDFPriorityInheritance(LosTaskCB *owner, const SchedParam *param)
{
    (VOID)owner;
    (VOID)param;
}

STATIC VOID EDFPriorityRestore(LosTaskCB *owner, const LOS_DL_LIST *list, const SchedParam *param)
{
    (VOID)owner;
    (VOID)list;
    (VOID)param;
}

UINT32 EDFTaskSchedParamInit(LosTaskCB *taskCB, UINT16 policy,
                             const SchedParam *parentParam,
                             const LosSchedParam *param)
{
    (VOID)parentParam;
    SchedEDF *sched = (SchedEDF *)&taskCB->sp;
    sched->flags = EDF_INIT;
    sched->policy = policy;
    sched->runTime = (INT32)OS_SYS_US_TO_CYCLE((UINT64)param->runTimeUs);
    sched->deadline = OS_SYS_US_TO_CYCLE(param->deadlineUs);
    sched->period = OS_SYS_US_TO_CYCLE(param->periodUs);
    sched->finishTime = 0;
    taskCB->timeSlice = 0;
    taskCB->ops = &g_deadlineOps;
    return LOS_OK;
}

VOID EDFProcessDefaultSchedParamGet(SchedParam *param)
{
    param->runTimeUs = OS_SCHED_EDF_MIN_RUNTIME;
    param->deadlineUs = OS_SCHED_EDF_MAX_DEADLINE;
    param->periodUs = param->deadlineUs;
}

VOID EDFSchedPolicyInit(SchedRunqueue *rq)
{
    if (ArchCurrCpuid() > 0) {
        rq->edfRunqueue = &g_schedEDF;
        return;
    }

    EDFRunqueue *erq = &g_schedEDF;
    erq->period = OS_SCHED_MAX_RESPONSE_TIME;
    LOS_ListInit(&erq->root);
    LOS_ListInit(&erq->waitList);
    rq->edfRunqueue = erq;
}
