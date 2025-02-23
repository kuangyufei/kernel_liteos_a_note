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

//基于优先数调度算法 Highest-Priority-First (HPF)

STATIC HPFRunqueue g_schedHPF;

STATIC VOID HPFDequeue(SchedRunqueue *rq, LosTaskCB *taskCB);
STATIC VOID HPFEnqueue(SchedRunqueue *rq, LosTaskCB *taskCB);
STATIC UINT64 HPFWaitTimeGet(LosTaskCB *taskCB);
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
//优先级调度算法操作
const STATIC SchedOps g_priorityOps = {
    .dequeue = HPFDequeue,
    .enqueue = HPFEnqueue,
    .waitTimeGet = HPFWaitTimeGet,
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

// 更新时间片
STATIC VOID HPFTimeSliceUpdate(SchedRunqueue *rq, LosTaskCB *taskCB, UINT64 currTime)
{
    SchedHPF *sched = (SchedHPF *)&taskCB->sp;
    LOS_ASSERT(currTime >= taskCB->startTime);  // 断言当前时间不小于任务开始时间

    INT32 incTime = (currTime - taskCB->startTime - taskCB->irqUsedTime);  // 计算任务实际运行时间

    LOS_ASSERT(incTime >= 0);  // 断言运行时间不小于0

    if (sched->policy == LOS_SCHED_RR) {  // 如果是RR调度策略
        taskCB->timeSlice -= incTime;  // 减少时间片
#ifdef LOSCFG_SCHED_HPF_DEBUG
        taskCB->schedStat.timeSliceRealTime += incTime;  // 调试模式下记录实际运行时间
#endif
    }
#ifdef LOSCFG_KERNEL_SCHED_PLIMIT
    OsSchedLimitUpdateRuntime(taskCB, currTime, incTime);  // 更新任务运行时间限制
#endif
    taskCB->irqUsedTime = 0;  // 重置中断使用时间
    taskCB->startTime = currTime;  // 更新任务开始时间为当前时间
    if (taskCB->timeSlice <= OS_TIME_SLICE_MIN) {  // 如果时间片小于等于最小时间片
        rq->schedFlag |= INT_PEND_RESCH;  // 设置重新调度标志
    }

#ifdef LOSCFG_SCHED_HPF_DEBUG
    taskCB->schedStat.allRuntime += incTime;  // 调试模式下记录总运行时间
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

// 将任务插入到优先级队列的头部
STATIC INLINE VOID PriQueHeadInsert(HPFRunqueue *rq, UINT32 basePrio, LOS_DL_LIST *priQue, UINT32 priority)
{
    HPFQueue *queueList = &rq->queueList[basePrio];  // 获取指定基础优先级的队列
    LOS_DL_LIST *priQueList = &queueList->priQueList[0];  // 获取优先级队列列表
    UINT32 *bitmap = &queueList->queueBitmap;  // 获取队列位图

    /*
     * 任务控制块初始化为零。当任务被删除时，
     * 同时会从优先级队列或其他列表中删除，
     * 任务挂起节点将被重置为零。
     */
    LOS_ASSERT(priQue->pstNext == NULL);  // 断言任务节点未链接到其他列表

    if (*bitmap == 0) {  // 如果队列位图为空
        rq->queueBitmap |= PRIQUEUE_PRIOR0_BIT >> basePrio;  // 设置基础优先级位
    }

    if (LOS_ListEmpty(&priQueList[priority])) {  // 如果指定优先级的队列为空
        *bitmap |= PRIQUEUE_PRIOR0_BIT >> priority;  // 设置优先级位
    }

    LOS_ListHeadInsert(&priQueList[priority], priQue);  // 将任务插入到队列头部
    queueList->readyTasks[priority]++;  // 增加就绪任务计数
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
// 将任务插入到就绪队列
STATIC INLINE VOID PriQueInsert(HPFRunqueue *rq, LosTaskCB *taskCB)
{
    LOS_ASSERT(!(taskCB->taskStatus & OS_TASK_STATUS_READY));  // 断言任务当前不处于就绪状态
    SchedHPF *sched = (SchedHPF *)&taskCB->sp;  // 获取任务的调度参数

    switch (sched->policy) {  // 根据调度策略进行处理
        case LOS_SCHED_RR: {  // 如果是RR调度策略
            if (taskCB->timeSlice > OS_TIME_SLICE_MIN) {  // 如果时间片大于最小时间片
                PriQueHeadInsert(rq, sched->basePrio, &taskCB->pendList, sched->priority);  // 插入到队列头部
            } else {  // 如果时间片不足
                sched->initTimeSlice = TimeSliceCalculate(rq, sched->basePrio, sched->priority);  // 重新计算时间片
                taskCB->timeSlice = sched->initTimeSlice;  // 重置时间片
                PriQueTailInsert(rq, sched->basePrio, &taskCB->pendList, sched->priority);  // 插入到队列尾部
#ifdef LOSCFG_SCHED_HPF_DEBUG
                taskCB->schedStat.timeSliceTime = taskCB->schedStat.timeSliceRealTime;  // 调试模式下记录时间片时间
                taskCB->schedStat.timeSliceCount++;  // 增加时间片计数
#endif
            }
            break;
        }
        case LOS_SCHED_FIFO: {  // 如果是FIFO调度策略
            /* FIFO的时间片总是大于0，除非调用了yield */
            if ((taskCB->timeSlice > OS_TIME_SLICE_MIN) && (taskCB->taskStatus & OS_TASK_STATUS_RUNNING)) {  // 如果时间片足够且任务正在运行
                PriQueHeadInsert(rq, sched->basePrio, &taskCB->pendList, sched->priority);  // 插入到队列头部
            } else {  // 如果时间片不足或任务未运行
                sched->initTimeSlice = OS_SCHED_FIFO_TIMEOUT;  // 设置FIFO的超时时间片
                taskCB->timeSlice = sched->initTimeSlice;  // 重置时间片
                PriQueTailInsert(rq, sched->basePrio, &taskCB->pendList, sched->priority);  // 插入到队列尾部
            }
            break;
        }
        default:
            LOS_ASSERT(0);  // 其他调度策略断言失败
            break;
    }

    taskCB->taskStatus &= ~OS_TASK_STATUS_BLOCKED;  // 清除阻塞状态
    taskCB->taskStatus |= OS_TASK_STATUS_READY;  // 设置为就绪状态
}

//入就绪队列
STATIC VOID HPFEnqueue(SchedRunqueue *rq, LosTaskCB *taskCB)
{
#ifdef LOSCFG_SCHED_HPF_DEBUG
    if (!(taskCB->taskStatus & OS_TASK_STATUS_RUNNING)) {
        taskCB->startTime = OsGetCurrSchedTimeCycle();
    }
#endif
    PriQueInsert(rq->hpfRunqueue, taskCB);
}
//出就绪队列
STATIC VOID HPFDequeue(SchedRunqueue *rq, LosTaskCB *taskCB)
{
    SchedHPF *sched = (SchedHPF *)&taskCB->sp;

    if (taskCB->taskStatus & OS_TASK_STATUS_READY) {//是否有就绪状态
        PriQueDelete(rq->hpfRunqueue, sched->basePrio, &taskCB->pendList, sched->priority);
        taskCB->taskStatus &= ~OS_TASK_STATUS_READY;//更新成非就绪状态
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

STATIC UINT64 HPFWaitTimeGet(LosTaskCB *taskCB)
{
    taskCB->waitTime += taskCB->startTime;
    return taskCB->waitTime;
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
#ifdef LOSCFG_SCHED_HPF_DEBUG
        resumedTask->schedStat.pendTime += OsGetCurrSchedTimeCycle() - resumedTask->startTime;
        resumedTask->schedStat.pendCount++;
#endif
        HPFEnqueue(OsSchedRunqueue(), resumedTask);
    }
}

STATIC BOOL BasePriorityModify(SchedRunqueue *rq, LosTaskCB *taskCB, UINT16 priority)
{
    LosProcessCB *processCB = OS_PCB_FROM_TCB(taskCB);
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

// 修改任务的调度参数
STATIC BOOL HPFSchedParamModify(LosTaskCB *taskCB, const SchedParam *param)
{
    SchedRunqueue *rq = OsSchedRunqueue();  // 获取当前运行队列
    BOOL needSched = FALSE;  // 是否需要重新调度的标志
    SchedHPF *sched = (SchedHPF *)&taskCB->sp;  // 获取任务的调度参数

    if (sched->policy != param->policy) {  // 如果调度策略发生变化
        sched->policy = param->policy;  // 更新调度策略
        taskCB->timeSlice = 0;  // 重置时间片
    }

    if (sched->basePrio != param->basePrio) {  // 如果基础优先级发生变化
        needSched = BasePriorityModify(rq, taskCB, param->basePrio);  // 修改基础优先级
    }

    if (taskCB->taskStatus & OS_TASK_STATUS_READY) {  // 如果任务处于就绪状态
        HPFDequeue(rq, taskCB);  // 将任务从队列中移除
        sched->priority = param->priority;  // 更新优先级
        HPFEnqueue(rq, taskCB);  // 将任务重新加入队列
        return TRUE;  // 返回需要重新调度
    }

    sched->priority = param->priority;  // 更新优先级
    OsHookCall(LOS_HOOK_TYPE_TASK_PRIMODIFY, taskCB, sched->priority);  // 调用优先级修改钩子函数
    if (taskCB->taskStatus & OS_TASK_STATUS_INIT) {  // 如果任务处于初始化状态
        HPFEnqueue(rq, taskCB);  // 将任务加入队列
        return TRUE;  // 返回需要重新调度
    }

    if (taskCB->taskStatus & OS_TASK_STATUS_RUNNING) {  // 如果任务正在运行
        return TRUE;  // 返回需要重新调度
    }

    return needSched;  // 返回是否需要重新调度
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
/// 恢复任务优先级
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
        priority = LOS_HighBitGet(sp->priBitmap);//获取在历史调度中最高优先级
        LOS_DL_LIST_FOR_EACH_ENTRY(pendedTask, list, LosTaskCB, pendList) {//遍历链表
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
                           const LosSchedParam *param)
{
    SchedHPF *sched = (SchedHPF *)&taskCB->sp;

    sched->policy = policy;
    if (param != NULL) {
        sched->priority = param->priority;
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
//HPF 调度策略初始化
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
