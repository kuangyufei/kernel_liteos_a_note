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

#include "los_sched_pri.h"
#include "los_task_pri.h"
#include "los_process_pri.h"
#include "los_hook.h"
#include "los_tick_pri.h"
#include "los_mp.h"

#define OS_SCHED_FIFO_TIMEOUT      0x7FFFFFFF
#define PRIQUEUE_PRIOR0_BIT        0x80000000U
#define OS_SCHED_TIME_SLICES_MIN   ((5000 * OS_SYS_NS_PER_US) / OS_NS_PER_CYCLE)  /* 5ms */
#define OS_SCHED_TIME_SLICES_MAX   ((LOSCFG_BASE_CORE_TIMESLICE_TIMEOUT * OS_SYS_NS_PER_US) / OS_NS_PER_CYCLE)
#define OS_SCHED_TIME_SLICES_DIFF  (OS_SCHED_TIME_SLICES_MAX - OS_SCHED_TIME_SLICES_MIN)
#define OS_SCHED_READY_MAX         30
#define OS_TIME_SLICE_MIN          (INT32)((50 * OS_SYS_NS_PER_US) / OS_NS_PER_CYCLE) /* 50us */

STATIC HPFRunqueue g_schedHPF;

STATIC VOID HPFDequeue(SchedRunqueue *rq, LosTaskCB *taskCB);
STATIC VOID HPFEnqueue(SchedRunqueue *rq, LosTaskCB *taskCB);
STATIC UINT32 HPFWait(LosTaskCB *runTask, LOS_DL_LIST *list, UINT32 ticks);
STATIC VOID HPFWake(LosTaskCB *resumedTask);
STATIC BOOL HPFSchedParamModify(LosTaskCB *taskCB, const SchedParam *param);
STATIC UINT32 HPFSchedParamGet(const LosTaskCB *taskCB, SchedParam *param);
STATIC UINT32 HPFDelay(LosTaskCB *runTask, UINT64 waitTime);
STATIC VOID HPFYield(LosTaskCB *runTask);
STATIC VOID HPFStartToRun(SchedRunqueue *rq, LosTaskCB *taskCB);
STATIC VOID HPFExit(LosTaskCB *taskCB);
STATIC UINT32 HPFSuspend(LosTaskCB *taskCB);
STATIC UINT32 HPFResume(LosTaskCB *taskCB, BOOL *needSched);
STATIC UINT64 HPFTimeSliceGet(const LosTaskCB *taskCB);
STATIC VOID HPFTimeSliceUpdate(SchedRunqueue *rq, LosTaskCB *taskCB, UINT64 currTime);
STATIC INT32 HPFParamCompare(const SchedPolicy *sp1, const SchedPolicy *sp2);
STATIC VOID HPFPriorityInheritance(LosTaskCB *owner, const SchedParam *param);
STATIC VOID HPFPriorityRestore(LosTaskCB *owner, const LOS_DL_LIST *list, const SchedParam *param);

const STATIC SchedOps g_priorityOps = {
    .dequeue = HPFDequeue,
    .enqueue = HPFEnqueue,
    .wait = HPFWait,
    .wake = HPFWake,
    .schedParamModify = HPFSchedParamModify,
    .schedParamGet = HPFSchedParamGet,
    .delay = HPFDelay,
    .yield = HPFYield,
    .start = HPFStartToRun,
    .exit = HPFExit,
    .suspend = HPFSuspend,
    .resume = HPFResume,
    .deadlineGet = HPFTimeSliceGet,
    .timeSliceUpdate = HPFTimeSliceUpdate,
    .schedParamCompare = HPFParamCompare,
    .priorityInheritance = HPFPriorityInheritance,
    .priorityRestore = HPFPriorityRestore,
};

STATIC VOID HPFTimeSliceUpdate(SchedRunqueue *rq, LosTaskCB *taskCB, UINT64 currTime)
{
    SchedHPF *sched = (SchedHPF *)&taskCB->sp;
    LOS_ASSERT(currTime >= taskCB->startTime);

    INT32 incTime = (currTime - taskCB->startTime - taskCB->irqUsedTime);

    LOS_ASSERT(incTime >= 0);

    if (sched->policy == LOS_SCHED_RR) {
        taskCB->timeSlice -= incTime;
#ifdef LOSCFG_SCHED_DEBUG
        taskCB->schedStat.timeSliceRealTime += incTime;
#endif
    }
    taskCB->irqUsedTime = 0;
    taskCB->startTime = currTime;
    if (taskCB->timeSlice <= OS_TIME_SLICE_MIN) {
        rq->schedFlag |= INT_PEND_RESCH;
    }

#ifdef LOSCFG_SCHED_DEBUG
    taskCB->schedStat.allRuntime += incTime;
#endif
}

STATIC UINT64 HPFTimeSliceGet(const LosTaskCB *taskCB)
{
    SchedHPF *sched = (SchedHPF *)&taskCB->sp;
    INT32 timeSlice = taskCB->timeSlice;

    timeSlice = (timeSlice <= OS_TIME_SLICE_MIN) ? sched->initTimeSlice : timeSlice;
    return (taskCB->startTime + timeSlice);
}

STATIC INLINE UINT32 TimeSliceCalculate(HPFRunqueue *rq, UINT16 basePrio, UINT16 priority)
{
    UINT32 time;
    UINT32 readyTasks;

    HPFQueue *queueList = &rq->queueList[basePrio];
    readyTasks = queueList->readyTasks[priority];
    if (readyTasks > OS_SCHED_READY_MAX) {
        return OS_SCHED_TIME_SLICES_MIN;
    }
    time = ((OS_SCHED_READY_MAX - readyTasks) * OS_SCHED_TIME_SLICES_DIFF) / OS_SCHED_READY_MAX;
    return (time + OS_SCHED_TIME_SLICES_MIN);
}

STATIC INLINE VOID PriQueHeadInsert(HPFRunqueue *rq, UINT32 basePrio, LOS_DL_LIST *priQue, UINT32 priority)
{
    HPFQueue *queueList = &rq->queueList[basePrio];
    LOS_DL_LIST *priQueList = &queueList->priQueList[0];
    UINT32 *bitmap = &queueList->queueBitmap;

    /*
     * Task control blocks are inited as zero. And when task is deleted,
     * and at the same time would be deleted from priority queue or
     * other lists, task pend node will restored as zero.
     */
    LOS_ASSERT(priQue->pstNext == NULL);

    if (*bitmap == 0) {
        rq->queueBitmap |= PRIQUEUE_PRIOR0_BIT >> basePrio;
    }

    if (LOS_ListEmpty(&priQueList[priority])) {
        *bitmap |= PRIQUEUE_PRIOR0_BIT >> priority;
    }

    LOS_ListHeadInsert(&priQueList[priority], priQue);
    queueList->readyTasks[priority]++;
}

STATIC INLINE VOID PriQueTailInsert(HPFRunqueue *rq, UINT32 basePrio, LOS_DL_LIST *priQue, UINT32 priority)
{
    HPFQueue *queueList = &rq->queueList[basePrio];
    LOS_DL_LIST *priQueList = &queueList->priQueList[0];
    UINT32 *bitmap = &queueList->queueBitmap;

    /*
     * Task control blocks are inited as zero. And when task is deleted,
     * and at the same time would be deleted from priority queue or
     * other lists, task pend node will restored as zero.
     */
    LOS_ASSERT(priQue->pstNext == NULL);

    if (*bitmap == 0) {
        rq->queueBitmap |= PRIQUEUE_PRIOR0_BIT >> basePrio;
    }

    if (LOS_ListEmpty(&priQueList[priority])) {
        *bitmap |= PRIQUEUE_PRIOR0_BIT >> priority;
    }

    LOS_ListTailInsert(&priQueList[priority], priQue);
    queueList->readyTasks[priority]++;
}

STATIC INLINE VOID PriQueDelete(HPFRunqueue *rq, UINT32 basePrio, LOS_DL_LIST *priQue, UINT32 priority)
{
    HPFQueue *queueList = &rq->queueList[basePrio];
    LOS_DL_LIST *priQueList = &queueList->priQueList[0];
    UINT32 *bitmap = &queueList->queueBitmap;

    LOS_ListDelete(priQue);
    queueList->readyTasks[priority]--;
    if (LOS_ListEmpty(&priQueList[priority])) {
        *bitmap &= ~(PRIQUEUE_PRIOR0_BIT >> priority);
    }

    if (*bitmap == 0) {
        rq->queueBitmap &= ~(PRIQUEUE_PRIOR0_BIT >> basePrio);
    }
}

STATIC INLINE VOID PriQueInsert(HPFRunqueue *rq, LosTaskCB *taskCB)
{
    LOS_ASSERT(!(taskCB->taskStatus & OS_TASK_STATUS_READY));
    SchedHPF *sched = (SchedHPF *)&taskCB->sp;

    switch (sched->policy) {
        case LOS_SCHED_RR: {
            if (taskCB->timeSlice > OS_TIME_SLICE_MIN) {
                PriQueHeadInsert(rq, sched->basePrio, &taskCB->pendList, sched->priority);
            } else {
                sched->initTimeSlice = TimeSliceCalculate(rq, sched->basePrio, sched->priority);
                taskCB->timeSlice = sched->initTimeSlice;
                PriQueTailInsert(rq, sched->basePrio, &taskCB->pendList, sched->priority);
#ifdef LOSCFG_SCHED_DEBUG
                taskCB->schedStat.timeSliceTime = taskCB->schedStat.timeSliceRealTime;
                taskCB->schedStat.timeSliceCount++;
#endif
            }
            break;
        }
        case LOS_SCHED_FIFO: {
            /* The time slice of FIFO is always greater than 0 unless the yield is called */
            if ((taskCB->timeSlice > OS_TIME_SLICE_MIN) && (taskCB->taskStatus & OS_TASK_STATUS_RUNNING)) {
                PriQueHeadInsert(rq, sched->basePrio, &taskCB->pendList, sched->priority);
            } else {
                sched->initTimeSlice = OS_SCHED_FIFO_TIMEOUT;
                taskCB->timeSlice = sched->initTimeSlice;
                PriQueTailInsert(rq, sched->basePrio, &taskCB->pendList, sched->priority);
            }
            break;
        }
        default:
            LOS_ASSERT(0);
            break;
    }

    taskCB->taskStatus &= ~OS_TASK_STATUS_BLOCKED;
    taskCB->taskStatus |= OS_TASK_STATUS_READY;
}

STATIC VOID HPFEnqueue(SchedRunqueue *rq, LosTaskCB *taskCB)
{
#ifdef LOSCFG_SCHED_DEBUG
    if (!(taskCB->taskStatus & OS_TASK_STATUS_RUNNING)) {
        taskCB->startTime = OsGetCurrSchedTimeCycle();
    }
#endif
    PriQueInsert(rq->hpfRunqueue, taskCB);
}

STATIC VOID HPFDequeue(SchedRunqueue *rq, LosTaskCB *taskCB)
{
    SchedHPF *sched = (SchedHPF *)&taskCB->sp;

    if (taskCB->taskStatus & OS_TASK_STATUS_READY) {
        PriQueDelete(rq->hpfRunqueue, sched->basePrio, &taskCB->pendList, sched->priority);
        taskCB->taskStatus &= ~OS_TASK_STATUS_READY;
    }
}

STATIC VOID HPFStartToRun(SchedRunqueue *rq, LosTaskCB *taskCB)
{
    HPFDequeue(rq, taskCB);
}

STATIC VOID HPFExit(LosTaskCB *taskCB)
{
    if (taskCB->taskStatus & OS_TASK_STATUS_READY) {
        HPFDequeue(OsSchedRunqueue(), taskCB);
    } else if (taskCB->taskStatus & OS_TASK_STATUS_PENDING) {
        LOS_ListDelete(&taskCB->pendList);
        taskCB->taskStatus &= ~OS_TASK_STATUS_PENDING;
    }

    if (taskCB->taskStatus & (OS_TASK_STATUS_DELAY | OS_TASK_STATUS_PEND_TIME)) {
        OsSchedTimeoutQueueDelete(taskCB);
        taskCB->taskStatus &= ~(OS_TASK_STATUS_DELAY | OS_TASK_STATUS_PEND_TIME);
    }
}

STATIC VOID HPFYield(LosTaskCB *runTask)
{
    SchedRunqueue *rq = OsSchedRunqueue();
    runTask->timeSlice = 0;

    runTask->startTime = OsGetCurrSchedTimeCycle();
    HPFEnqueue(rq, runTask);
    OsSchedResched();
}

STATIC UINT32 HPFDelay(LosTaskCB *runTask, UINT64 waitTime)
{
    runTask->taskStatus |= OS_TASK_STATUS_DELAY;
    runTask->waitTime = waitTime;

    OsSchedResched();
    return LOS_OK;
}

STATIC UINT32 HPFWait(LosTaskCB *runTask, LOS_DL_LIST *list, UINT32 ticks)
{
    runTask->taskStatus |= OS_TASK_STATUS_PENDING;
    LOS_ListTailInsert(list, &runTask->pendList);

    if (ticks != LOS_WAIT_FOREVER) {
        runTask->taskStatus |= OS_TASK_STATUS_PEND_TIME;
        runTask->waitTime = OS_SCHED_TICK_TO_CYCLE(ticks);
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

STATIC VOID HPFWake(LosTaskCB *resumedTask)
{
    LOS_ListDelete(&resumedTask->pendList);
    resumedTask->taskStatus &= ~OS_TASK_STATUS_PENDING;

    if (resumedTask->taskStatus & OS_TASK_STATUS_PEND_TIME) {
        OsSchedTimeoutQueueDelete(resumedTask);
        resumedTask->taskStatus &= ~OS_TASK_STATUS_PEND_TIME;
    }

    if (!(resumedTask->taskStatus & OS_TASK_STATUS_SUSPENDED)) {
#ifdef LOSCFG_SCHED_DEBUG
        resumedTask->schedStat.pendTime += OsGetCurrSchedTimeCycle() - resumedTask->startTime;
        resumedTask->schedStat.pendCount++;
#endif
        HPFEnqueue(OsSchedRunqueue(), resumedTask);
    }
}

STATIC BOOL BasePriorityModify(SchedRunqueue *rq, LosTaskCB *taskCB, UINT16 priority)
{
    LosProcessCB *processCB = OS_PCB_FROM_PID(taskCB->processID);
    BOOL needSched = FALSE;

    LOS_DL_LIST_FOR_EACH_ENTRY(taskCB, &processCB->threadSiblingList, LosTaskCB, threadList) {
        SchedHPF *sched = (SchedHPF *)&taskCB->sp;
        if (taskCB->taskStatus & OS_TASK_STATUS_READY) {
            taskCB->ops->dequeue(rq, taskCB);
            sched->basePrio = priority;
            taskCB->ops->enqueue(rq, taskCB);
        } else {
            sched->basePrio = priority;
        }
        if (taskCB->taskStatus & (OS_TASK_STATUS_READY | OS_TASK_STATUS_RUNNING)) {
            needSched = TRUE;
        }
    }

    return needSched;
}

STATIC BOOL HPFSchedParamModify(LosTaskCB *taskCB, const SchedParam *param)
{
    SchedRunqueue *rq = OsSchedRunqueue();
    BOOL needSched = FALSE;
    SchedHPF *sched = (SchedHPF *)&taskCB->sp;

    if (sched->policy != param->policy) {
        sched->policy = param->policy;
        taskCB->timeSlice = 0;
    }

    if (sched->basePrio != param->basePrio) {
        needSched = BasePriorityModify(rq, taskCB, param->basePrio);
    }

    if (taskCB->taskStatus & OS_TASK_STATUS_READY) {
        HPFDequeue(rq, taskCB);
        sched->priority = param->priority;
        HPFEnqueue(rq, taskCB);
        return TRUE;
    }

    sched->priority = param->priority;
    OsHookCall(LOS_HOOK_TYPE_TASK_PRIMODIFY, taskCB, sched->priority);
    if (taskCB->taskStatus & OS_TASK_STATUS_INIT) {
        HPFEnqueue(rq, taskCB);
        return TRUE;
    }

    if (taskCB->taskStatus & OS_TASK_STATUS_RUNNING) {
        return TRUE;
    }

    return needSched;
}

STATIC UINT32 HPFSchedParamGet(const LosTaskCB *taskCB, SchedParam *param)
{
    SchedHPF *sched = (SchedHPF *)&taskCB->sp;
    param->policy = sched->policy;
    param->basePrio = sched->basePrio;
    param->priority = sched->priority;
    param->timeSlice = sched->initTimeSlice;
    return LOS_OK;
}

STATIC UINT32 HPFSuspend(LosTaskCB *taskCB)
{
    if (taskCB->taskStatus & OS_TASK_STATUS_READY) {
        HPFDequeue(OsSchedRunqueue(), taskCB);
    }

    SchedTaskFreeze(taskCB);

    taskCB->taskStatus |= OS_TASK_STATUS_SUSPENDED;
    OsHookCall(LOS_HOOK_TYPE_MOVEDTASKTOSUSPENDEDLIST, taskCB);
    if (taskCB == OsCurrTaskGet()) {
        OsSchedResched();
    }
    return LOS_OK;
}

STATIC UINT32 HPFResume(LosTaskCB *taskCB, BOOL *needSched)
{
    *needSched = FALSE;

    SchedTaskUnfreeze(taskCB);

    taskCB->taskStatus &= ~OS_TASK_STATUS_SUSPENDED;
    if (!OsTaskIsBlocked(taskCB)) {
        HPFEnqueue(OsSchedRunqueue(), taskCB);
        *needSched = TRUE;
    }

    return LOS_OK;
}

STATIC INT32 HPFParamCompare(const SchedPolicy *sp1, const SchedPolicy *sp2)
{
    SchedHPF *param1 = (SchedHPF *)sp1;
    SchedHPF *param2 = (SchedHPF *)sp2;

    if (param1->basePrio != param2->basePrio) {
        return (param1->basePrio - param2->basePrio);
    }

    return (param1->priority - param2->priority);
}

STATIC VOID HPFPriorityInheritance(LosTaskCB *owner, const SchedParam *param)
{
    SchedHPF *sp = (SchedHPF *)&owner->sp;

    if ((param->policy != LOS_SCHED_RR) && (param->policy != LOS_SCHED_FIFO)) {
        return;
    }

    if (sp->priority <= param->priority) {
        return;
    }

    LOS_BitmapSet(&sp->priBitmap, sp->priority);
    sp->priority = param->priority;
}

STATIC VOID HPFPriorityRestore(LosTaskCB *owner, const LOS_DL_LIST *list, const SchedParam *param)
{
    UINT16 priority;
    LosTaskCB *pendedTask = NULL;

    if ((param->policy != LOS_SCHED_RR) && (param->policy != LOS_SCHED_FIFO)) {
        return;
    }

    SchedHPF *sp = (SchedHPF *)&owner->sp;
    if (sp->priority < param->priority) {
        if (LOS_HighBitGet(sp->priBitmap) != param->priority) {
            LOS_BitmapClr(&sp->priBitmap, param->priority);
        }
        return;
    }

    if (sp->priBitmap == 0) {
        return;
    }

    if ((list != NULL) && !LOS_ListEmpty((LOS_DL_LIST *)list)) {
        priority = LOS_HighBitGet(sp->priBitmap);
        LOS_DL_LIST_FOR_EACH_ENTRY(pendedTask, list, LosTaskCB, pendList) {
            SchedHPF *pendSp = (SchedHPF *)&pendedTask->sp;
            if ((pendedTask->ops == owner->ops) && (priority != pendSp->priority)) {
                LOS_BitmapClr(&sp->priBitmap, pendSp->priority);
            }
        }
    }

    priority = LOS_LowBitGet(sp->priBitmap);
    if (priority != LOS_INVALID_BIT_INDEX) {
        LOS_BitmapClr(&sp->priBitmap, priority);
        sp->priority = priority;
    }
}

VOID HPFTaskSchedParamInit(LosTaskCB *taskCB, UINT16 policy,
                           const SchedParam *parentParam,
                           const TSK_INIT_PARAM_S *param)
{
    SchedHPF *sched = (SchedHPF *)&taskCB->sp;

    sched->policy = policy;
    if (param != NULL) {
        sched->priority = param->usTaskPrio;
    } else {
        sched->priority = parentParam->priority;
    }
    sched->basePrio = parentParam->basePrio;

    sched->initTimeSlice = 0;
    taskCB->timeSlice = sched->initTimeSlice;
    taskCB->ops = &g_priorityOps;
}

VOID HPFProcessDefaultSchedParamGet(SchedParam *param)
{
    param->basePrio = OS_USER_PROCESS_PRIORITY_HIGHEST;
}

VOID HPFSchedPolicyInit(SchedRunqueue *rq)
{
    if (ArchCurrCpuid() > 0) {
        rq->hpfRunqueue = &g_schedHPF;
        return;
    }

    for (UINT16 index = 0; index < OS_PRIORITY_QUEUE_NUM; index++) {
        HPFQueue *queueList = &g_schedHPF.queueList[index];
        LOS_DL_LIST *priQue = &queueList->priQueList[0];
        for (UINT16 prio = 0; prio < OS_PRIORITY_QUEUE_NUM; prio++) {
            LOS_ListInit(&priQue[prio]);
        }
    }

    rq->hpfRunqueue = &g_schedHPF;
}
