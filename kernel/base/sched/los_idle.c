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
#include "los_process_pri.h"

STATIC VOID IdleDequeue(SchedRunqueue *rq, LosTaskCB *taskCB);
STATIC VOID IdleEnqueue(SchedRunqueue *rq, LosTaskCB *taskCB);
STATIC UINT32 IdleWait(LosTaskCB *runTask, LOS_DL_LIST *list, UINT32 ticks);
STATIC VOID IdleWake(LosTaskCB *resumedTask);
STATIC UINT32 IdleSchedParamGet(const LosTaskCB *taskCB, SchedParam *param);
STATIC VOID IdleYield(LosTaskCB *runTask);
STATIC VOID IdleStartToRun(SchedRunqueue *rq, LosTaskCB *taskCB);
STATIC UINT32 IdleResume(LosTaskCB *taskCB, BOOL *needSched);
STATIC UINT64 IdleTimeSliceGet(const LosTaskCB *taskCB);
STATIC VOID IdleTimeSliceUpdate(SchedRunqueue *rq, LosTaskCB *taskCB, UINT64 currTime);
STATIC INT32 IdleParamCompare(const SchedPolicy *sp1, const SchedPolicy *sp2);
STATIC VOID IdlePriorityInheritance(LosTaskCB *owner, const SchedParam *param);
STATIC VOID IdlePriorityRestore(LosTaskCB *owner, const LOS_DL_LIST *list, const SchedParam *param);

const STATIC SchedOps g_idleOps = {
    .dequeue = IdleDequeue,
    .enqueue = IdleEnqueue,
    .wait = IdleWait,
    .wake = IdleWake,
    .schedParamModify = NULL,
    .schedParamGet = IdleSchedParamGet,
    .delay = NULL,
    .yield = IdleYield,
    .start = IdleStartToRun,
    .exit = NULL,
    .suspend = NULL,
    .resume = IdleResume,
    .deadlineGet = IdleTimeSliceGet,
    .timeSliceUpdate = IdleTimeSliceUpdate,
    .schedParamCompare = IdleParamCompare,
    .priorityInheritance = IdlePriorityInheritance,
    .priorityRestore = IdlePriorityRestore,
};

STATIC VOID IdleTimeSliceUpdate(SchedRunqueue *rq, LosTaskCB *taskCB, UINT64 currTime)
{
    (VOID)rq;

    taskCB->startTime = currTime;
}

STATIC UINT64 IdleTimeSliceGet(const LosTaskCB *taskCB)
{
    (VOID)taskCB;
    return (OS_SCHED_MAX_RESPONSE_TIME - OS_TICK_RESPONSE_PRECISION);
}

STATIC VOID IdleEnqueue(SchedRunqueue *rq, LosTaskCB *taskCB)
{
    (VOID)rq;

    taskCB->taskStatus &= ~OS_TASK_STATUS_BLOCKED;
    taskCB->taskStatus |= OS_TASK_STATUS_READY;
}

STATIC VOID IdleDequeue(SchedRunqueue *rq, LosTaskCB *taskCB)
{
    (VOID)rq;

    taskCB->taskStatus &= ~OS_TASK_STATUS_READY;
}

STATIC VOID IdleStartToRun(SchedRunqueue *rq, LosTaskCB *taskCB)
{
    IdleDequeue(rq, taskCB);
}

STATIC VOID IdleYield(LosTaskCB *runTask)
{
    (VOID)runTask;
    return;
}

STATIC UINT32 IdleWait(LosTaskCB *runTask, LOS_DL_LIST *list, UINT32 ticks)
{
    (VOID)runTask;
    (VOID)list;
    (VOID)ticks;

    return LOS_NOK;
}

STATIC VOID IdleWake(LosTaskCB *resumedTask)
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
        resumedTask->ops->enqueue(OsSchedRunqueue(), resumedTask);
    }
}

STATIC UINT32 IdleResume(LosTaskCB *taskCB, BOOL *needSched)
{
    *needSched = FALSE;

    taskCB->taskStatus &= ~OS_TASK_STATUS_SUSPENDED;
    if (!OsTaskIsBlocked(taskCB)) {
        taskCB->ops->enqueue(OsSchedRunqueue(), taskCB);
        *needSched = TRUE;
    }
    return LOS_OK;
}

STATIC UINT32 IdleSchedParamGet(const LosTaskCB *taskCB, SchedParam *param)
{
    SchedHPF *sched = (SchedHPF *)&taskCB->sp;
    param->policy = sched->policy;
    param->basePrio = sched->basePrio;
    param->priority = sched->priority;
    param->timeSlice = 0;
    return LOS_OK;
}

STATIC INT32 IdleParamCompare(const SchedPolicy *sp1, const SchedPolicy *sp2)
{
    return 0;
}

STATIC VOID IdlePriorityInheritance(LosTaskCB *owner, const SchedParam *param)
{
    (VOID)owner;
    (VOID)param;
    return;
}

STATIC VOID IdlePriorityRestore(LosTaskCB *owner, const LOS_DL_LIST *list, const SchedParam *param)
{
    (VOID)owner;
    (VOID)list;
    (VOID)param;
    return;
}

VOID IdleTaskSchedParamInit(LosTaskCB *taskCB)
{
    SchedHPF *sched = (SchedHPF *)&taskCB->sp;
    sched->policy = LOS_SCHED_IDLE;
    sched->basePrio = OS_PROCESS_PRIORITY_LOWEST;
    sched->priority = OS_TASK_PRIORITY_LOWEST;
    sched->initTimeSlice = 0;
    taskCB->timeSlice = sched->initTimeSlice;
    taskCB->taskStatus = OS_TASK_STATUS_SUSPENDED;
    taskCB->ops = &g_idleOps;
}
