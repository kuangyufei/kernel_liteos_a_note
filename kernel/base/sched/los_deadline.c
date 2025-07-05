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

/**
* 定义了一个类型为 SchedOps 的静态常量结构体 g_deadlineOps ，通过函数指针数组的形式，集中封装了 EDF 调度策略所需的所有核心操作。
* 这是内核调度框架的典型设计模式，允许不同调度策略（如 HPF、RR）通过实现统一的 SchedOps 接口实现插拔式替换。
* 接口抽象 ：通过 SchedOps 结构体标准化调度器行为，使内核可无缝切换不同调度策略
* EDF核心逻辑 ：
    排序关键： schedParamCompare 实现基于截止时间的优先级比较
    队列管理： enqueue / dequeue 操作维护按截止时间排序的就绪队列
    实时保障： priorityInheritance / priorityRestore 解决优先级反转问题
    功能完整性 ：覆盖任务生命周期全流程（创建/阻塞/唤醒/退出）及参数管理
* 与其他模块的关联
    排序链表依赖 ： EDFEnqueue 等操作依赖 `los_sortlink.c` 提供的时间有序链表管理
    调度统计 ：调度行为数据会被 `los_statistics.c` 中的 EDFDebugRecord 函数记录，用于调试分析
    任务控制块 ：所有操作最终作用于 LosTaskCB 结构体（任务控制块）中的 EDF 专用字段
* 该结构体是 EDF 调度器的"大脑"，通过统一接口对外提供服务，同时内部实现了 deadline 驱动的调度逻辑，是 LiteOS-A 实时性保障的核心组件之一。
*/
const STATIC SchedOps g_deadlineOps = {
    .dequeue = EDFDequeue,          // 从EDF就绪队列移除任务
    .enqueue = EDFEnqueue,          // 将任务加入EDF就绪队列（按截止时间排序）
    .waitTimeGet = EDFWaitTimeGet,  // 获取任务等待时间
    .wait = EDFWait,                // 任务等待操作（阻塞时调用）
    .wake = EDFWake,                // 任务唤醒操作（解除阻塞时调用）
    .schedParamModify = EDFSchedParamModify, // 修改任务调度参数（周期、截止时间等）
    .schedParamGet = EDFSchedParamGet,       // 获取任务调度参数
    .delay = EDFDelay,              // 任务延迟调度
    .yield = EDFYield,              // 任务主动让出CPU
    .start = EDFDequeue,            // 任务启动调度（复用出队逻辑）
    .exit = EDFExit,                // 任务退出调度
    .suspend = EDFSuspend,          // 任务挂起
    .resume = EDFResume,            // 任务恢复
    .deadlineGet = EDFTimeSliceGet, // 获取任务剩余截止时间（时间片）
    .timeSliceUpdate = EDFTimeSliceUpdate, // 更新任务时间片
    .schedParamCompare = EDFParamCompare,   // 比较两个任务的调度参数（核心排序逻辑）
    .priorityInheritance = EDFPriorityInheritance, // 优先级继承（实时系统关键特性）
    .priorityRestore = EDFPriorityRestore,         // 优先级恢复
};
/**
* 主要功能说明：
* 更新EDF调度任务的时间片状态
* 计算任务实际运行时间
* 处理任务超时情况
* 调试模式下记录运行统计信息
* 设置调度标志，触发重新调度
* 代码执行流程： 获取调度参数 → 检查时间片状态 → 计算运行时间 → 更新任务状态 → 检查超时 → 设置调度标志 → 处理超时情况
*/
STATIC VOID EDFTimeSliceUpdate(SchedRunqueue *rq, LosTaskCB *taskCB, UINT64 currTime)
{
    SchedEDF *sched = (SchedEDF *)&taskCB->sp;  // 获取任务的EDF调度参数

    LOS_ASSERT(currTime >= taskCB->startTime);  // 断言当前时间不小于任务开始时间

    // 如果任务时间片已用完，直接返回
    if (taskCB->timeSlice <= 0) {
        taskCB->irqUsedTime = 0;  // 重置中断使用时间
        return;
    }

    // 计算任务实际运行时间（当前时间 - 开始时间 - 中断时间）
    INT32 incTime = (currTime - taskCB->startTime - taskCB->irqUsedTime);
    LOS_ASSERT(incTime >= 0);  // 断言运行时间不小于0

#ifdef LOSCFG_SCHED_EDF_DEBUG
    // 调试模式下记录实际运行时间和总运行时间
    taskCB->schedStat.timeSliceRealTime += incTime;
    taskCB->schedStat.allRuntime += (currTime - taskCB->startTime);
#endif

    // 更新任务剩余时间片和状态
    taskCB->timeSlice -= incTime;  // 扣除已用时间片
    taskCB->irqUsedTime = 0;       // 重置中断使用时间
    taskCB->startTime = currTime;  // 更新任务开始时间

    // 如果任务未超时且还有剩余时间片，直接返回
    if ((sched->finishTime > currTime) && (taskCB->timeSlice > 0)) {
        return;
    }

    // 设置调度标志，表示需要重新调度
    rq->schedFlag |= INT_PEND_RESCH;

    // 如果任务已超时
    if (sched->finishTime <= currTime) {
#ifdef LOSCFG_SCHED_EDF_DEBUG
        // 调试模式下记录超时信息
        EDFDebugRecord((UINTPTR *)taskCB, sched->finishTime);
#endif

        taskCB->timeSlice = 0;  // 清空剩余时间片
        // 打印任务超时信息
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
/*
主要功能说明：

将任务加入EDF调度队列
处理任务时间片用完的情况
计算并更新任务的周期信息
将任务加入等待队列或就绪队列
更新任务状态和调试信息
代码执行流程： 检查任务状态 → 处理时间片用完 → 计算下一个周期 → 加入等待队列或就绪队列 → 更新任务状态
*/
STATIC VOID EDFEnqueue(SchedRunqueue *rq, LosTaskCB *taskCB)
{
    LOS_ASSERT(!(taskCB->taskStatus & OS_TASK_STATUS_READY));  // 断言任务当前不处于就绪状态

    EDFRunqueue *erq = rq->edfRunqueue;  // 获取EDF运行队列
    SchedEDF *sched = (SchedEDF *)&taskCB->sp;  // 获取任务的EDF调度参数

    // 如果任务时间片已用完
    if (taskCB->timeSlice <= 0) {
#ifdef LOSCFG_SCHED_EDF_DEBUG
        UINT64 oldFinish = sched->finishTime;  // 调试模式下记录旧的完成时间
#endif
        UINT64 currTime = OsGetCurrSchedTimeCycle();  // 获取当前调度时间

        // 初始化或计算下一个周期的开始时间
        if (sched->flags == EDF_INIT) {
            sched->finishTime = currTime;  // 初始化完成时间为当前时间
        } else if (sched->flags != EDF_NEXT_PERIOD) {
            // 计算下一个周期的开始时间
            sched->finishTime = (sched->finishTime - sched->deadline) + sched->period;
        }

        // 计算下一个周期的开始时间，确保满足最小运行时间要求
        while (1) {
            UINT64 finishTime = sched->finishTime + sched->deadline;  // 计算下一个周期的截止时间
            // 如果当前周期已过或无法满足最小运行时间，迁移到下一个周期
            if ((finishTime <= currTime) || ((sched->finishTime + sched->runTime) > finishTime)) {
                sched->finishTime += sched->period;  // 迁移到下一个周期
                continue;
            }
            break;
        }

        // 如果下一个周期还未开始，将任务加入等待队列
        if (sched->finishTime > currTime) {
            LOS_ListTailInsert(&erq->waitList, &taskCB->pendList);  // 将任务插入等待队列
            taskCB->waitTime = OS_SCHED_MAX_RESPONSE_TIME;  // 设置最大等待时间
            if (!OsTaskIsRunning(taskCB)) {
                OsSchedTimeoutQueueAdd(taskCB, sched->finishTime);  // 将任务加入超时队列
            }
#ifdef LOSCFG_SCHED_EDF_DEBUG
            // 调试模式下记录状态变化
            if (oldFinish != sched->finishTime) {
                EDFDebugRecord((UINTPTR *)taskCB, oldFinish);
                taskCB->schedStat.allRuntime = 0;
                taskCB->schedStat.timeSliceRealTime = 0;
                taskCB->schedStat.pendTime = 0;
            }
#endif
            taskCB->taskStatus |= OS_TASK_STATUS_PEND_TIME;  // 设置任务状态为等待时间
            sched->flags = EDF_NEXT_PERIOD;  // 设置标志为下一个周期
            return;
        }

        // 更新任务的时间片和状态
        sched->finishTime += sched->deadline;  // 更新完成时间
        taskCB->timeSlice = sched->runTime;  // 重置时间片
        sched->flags = EDF_UNUSED;  // 设置标志为未使用
    }

    // 将任务插入就绪队列并更新状态
    DeadlineQueueInsert(erq, taskCB);  // 将任务插入EDF就绪队列
    taskCB->taskStatus &= ~(OS_TASK_STATUS_BLOCKED | OS_TASK_STATUS_TIMEOUT);  // 清除阻塞和超时状态
    taskCB->taskStatus |= OS_TASK_STATUS_READY;  // 设置任务状态为就绪
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
/*
该函数用于实现EDF调度策略下的任务主动让出CPU
通过将时间片置0并重新入队，使任务进入就绪状态
最后触发重新调度，让调度器选择下一个要运行的任务
*/
STATIC VOID EDFYield(LosTaskCB *runTask)
{
    SchedRunqueue *rq = OsSchedRunqueue();  // 获取当前调度运行队列
    runTask->timeSlice = 0;  // 将任务的时间片置为0，表示主动放弃CPU

    runTask->startTime = OsGetCurrSchedTimeCycle();  // 更新任务开始时间为当前调度周期
    EDFEnqueue(rq, runTask);  // 将任务重新加入EDF就绪队列
    OsSchedResched();  // 触发重新调度
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
