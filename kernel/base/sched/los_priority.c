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

/**
 * @brief 优先级调度策略操作集定义
 * @details 封装HPF（最高优先级优先）调度策略的核心操作函数，包括任务入队/出队、时间片管理、优先级调整等
 */
const STATIC SchedOps g_priorityOps = {
    .dequeue = HPFDequeue,               // 任务出队操作
    .enqueue = HPFEnqueue,               // 任务入队操作
    .waitTimeGet = HPFWaitTimeGet,       // 获取等待时间
    .wait = HPFWait,                     // 任务等待操作
    .wake = HPFWake,                     // 任务唤醒操作
    .schedParamModify = HPFSchedParamModify, // 修改调度参数
    .schedParamGet = HPFSchedParamGet,   // 获取调度参数
    .delay = HPFDelay,                   // 任务延迟操作
    .yield = HPFYield,                   // 任务让出CPU
    .start = HPFStartToRun,              // 任务开始运行
    .exit = HPFExit,                     // 任务退出处理
    .suspend = HPFSuspend,               // 任务挂起
    .resume = HPFResume,                 // 任务恢复
    .deadlineGet = HPFTimeSliceGet,      // 获取时间片截止时间
    .timeSliceUpdate = HPFTimeSliceUpdate, // 更新时间片
    .schedParamCompare = HPFParamCompare, // 比较调度参数
    .priorityInheritance = HPFPriorityInheritance, // 优先级继承
    .priorityRestore = HPFPriorityRestore  // 优先级恢复
};


/**
 * @brief 更新任务时间片
 * @param rq 运行队列指针
 * @param taskCB 任务控制块指针
 * @param currTime 当前时间
 * @details 根据任务实际运行时间调整时间片，处理调试统计和运行时限制
 */
STATIC VOID HPFTimeSliceUpdate(SchedRunqueue *rq, LosTaskCB *taskCB, UINT64 currTime)
{
    SchedHPF *sched = (SchedHPF *)&taskCB->sp;  // 获取HPF调度参数
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


/**
 * @brief 获取任务时间片截止时间
 * @param taskCB 任务控制块指针
 * @return 时间片截止时间
 * @details 根据当前时间片和初始时间片计算任务的时间片截止时间
 */
STATIC UINT64 HPFTimeSliceGet(const LosTaskCB *taskCB)
{
    SchedHPF *sched = (SchedHPF *)&taskCB->sp;  // 获取HPF调度参数
    INT32 timeSlice = taskCB->timeSlice;  // 获取当前时间片

    timeSlice = (timeSlice <= OS_TIME_SLICE_MIN) ? sched->initTimeSlice : timeSlice;  // 确定有效时间片
    return (taskCB->startTime + timeSlice);  // 返回时间片截止时间
}

/**
 * @brief 计算时间片大小
 * @param rq HPF运行队列指针
 * @param basePrio 基础优先级
 * @param priority 优先级
 * @return 计算得到的时间片大小
 * @details 根据就绪任务数量动态调整时间片大小
 */
STATIC INLINE UINT32 TimeSliceCalculate(HPFRunqueue *rq, UINT16 basePrio, UINT16 priority)
{
    UINT32 time;  // 计算得到的时间片
    UINT32 readyTasks;  // 就绪任务数量

    HPFQueue *queueList = &rq->queueList[basePrio];  // 获取基础优先级队列
    readyTasks = queueList->readyTasks[priority];  // 获取该优先级下的就绪任务数
    if (readyTasks > OS_SCHED_READY_MAX) {  // 如果就绪任务数超过最大值
        return OS_SCHED_TIME_SLICES_MIN;  // 返回最小时间片
    }
    time = ((OS_SCHED_READY_MAX - readyTasks) * OS_SCHED_TIME_SLICES_DIFF) / OS_SCHED_READY_MAX;  // 计算时间片
    return (time + OS_SCHED_TIME_SLICES_MIN);  // 返回计算后的时间片
}


/**
 * @brief 优先级队列头部插入
 * @param rq HPF运行队列指针
 * @param basePrio 基础优先级
 * @param priQue 优先级队列节点
 * @param priority 优先级
 * @details 将任务节点插入到优先级队列头部，并更新位图和就绪任务计数
 */
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


/**
 * @brief 优先级队列尾部插入
 * @param rq HPF运行队列指针
 * @param basePrio 基础优先级
 * @param priQue 优先级队列节点
 * @param priority 优先级
 * @details 将任务节点插入到优先级队列尾部，并更新位图和就绪任务计数
 */
STATIC INLINE VOID PriQueTailInsert(HPFRunqueue *rq, UINT32 basePrio, LOS_DL_LIST *priQue, UINT32 priority)
{
    HPFQueue *queueList = &rq->queueList[basePrio];  // 获取指定基础优先级的队列
    LOS_DL_LIST *priQueList = &queueList->priQueList[0];  // 获取优先级队列列表
    UINT32 *bitmap = &queueList->queueBitmap;  // 获取队列位图

    /*
     * Task control blocks are inited as zero. And when task is deleted,
     * and at the same time would be deleted from priority queue or
     * other lists, task pend node will restored as zero.
     */
    LOS_ASSERT(priQue->pstNext == NULL);  // 断言任务节点未链接到其他列表

    if (*bitmap == 0) {  // 如果队列位图为空
        rq->queueBitmap |= PRIQUEUE_PRIOR0_BIT >> basePrio;  // 设置基础优先级位
    }

    if (LOS_ListEmpty(&priQueList[priority])) {  // 如果指定优先级的队列为空
        *bitmap |= PRIQUEUE_PRIOR0_BIT >> priority;  // 设置优先级位
    }

    LOS_ListTailInsert(&priQueList[priority], priQue);  // 将任务插入到队列尾部
    queueList->readyTasks[priority]++;  // 增加就绪任务计数
}

/**
 * @brief 从优先级队列删除节点
 * @param rq HPF运行队列指针
 * @param basePrio 基础优先级
 * @param priQue 优先级队列节点
 * @param priority 优先级
 * @details 从优先级队列中删除指定任务节点，并更新位图和就绪任务计数
 */
STATIC INLINE VOID PriQueDelete(HPFRunqueue *rq, UINT32 basePrio, LOS_DL_LIST *priQue, UINT32 priority)
{
    HPFQueue *queueList = &rq->queueList[basePrio];  // 获取指定基础优先级的队列
    LOS_DL_LIST *priQueList = &queueList->priQueList[0];  // 获取优先级队列列表
    UINT32 *bitmap = &queueList->queueBitmap;  // 获取队列位图

    LOS_ListDelete(priQue);  // 从队列中删除节点
    queueList->readyTasks[priority]--;  // 减少就绪任务计数
    if (LOS_ListEmpty(&priQueList[priority])) {  // 如果队列已空
        *bitmap &= ~(PRIQUEUE_PRIOR0_BIT >> priority);  // 清除优先级位
    }

    if (*bitmap == 0) {  // 如果位图已空
        rq->queueBitmap &= ~(PRIQUEUE_PRIOR0_BIT >> basePrio);  // 清除基础优先级位
    }
}

/**
 * @brief 优先级队列插入任务
 * @param rq HPF运行队列指针
 * @param taskCB 任务控制块指针
 * @details 根据任务调度策略（RR/FIFO）将任务插入到优先级队列的头部或尾部
 */
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


/**
 * @brief HPF调度策略任务入队
 * @param rq 运行队列指针
 * @param taskCB 任务控制块指针
 * @details 将任务插入到HPF优先级队列，并在调试模式下更新开始时间
 */
STATIC VOID HPFEnqueue(SchedRunqueue *rq, LosTaskCB *taskCB)
{
#ifdef LOSCFG_SCHED_HPF_DEBUG
    if (!(taskCB->taskStatus & OS_TASK_STATUS_RUNNING)) {  // 如果任务未运行
        taskCB->startTime = OsGetCurrSchedTimeCycle();  // 设置开始时间为当前调度时间
    }
#endif
    PriQueInsert(rq->hpfRunqueue, taskCB);  // 插入到优先级队列
}

/**
 * @brief HPF调度策略任务出队
 * @param rq 运行队列指针
 * @param taskCB 任务控制块指针
 * @details 将任务从HPF优先级队列中移除，并更新任务状态
 */
STATIC VOID HPFDequeue(SchedRunqueue *rq, LosTaskCB *taskCB)
{
    SchedHPF *sched = (SchedHPF *)&taskCB->sp;  // 获取HPF调度参数

    if (taskCB->taskStatus & OS_TASK_STATUS_READY) {  // 如果任务处于就绪状态
        PriQueDelete(rq->hpfRunqueue, sched->basePrio, &taskCB->pendList, sched->priority);  // 从队列中删除
        taskCB->taskStatus &= ~OS_TASK_STATUS_READY;  // 更新成非就绪状态
    }
}

/**
 * @brief 任务开始运行处理
 * @param rq 运行队列指针
 * @param taskCB 任务控制块指针
 * @details 任务开始运行时从队列中移除
 */
STATIC VOID HPFStartToRun(SchedRunqueue *rq, LosTaskCB *taskCB)
{
    HPFDequeue(rq, taskCB);  // 任务出队
}

/**
 * @brief 任务退出处理
 * @param taskCB 任务控制块指针
 * @details 从就绪队列或阻塞队列中移除任务，清理超时状态
 */
STATIC VOID HPFExit(LosTaskCB *taskCB)
{
    if (taskCB->taskStatus & OS_TASK_STATUS_READY) {  // 如果任务处于就绪状态
        HPFDequeue(OsSchedRunqueue(), taskCB);  // 从运行队列中出队
    } else if (taskCB->taskStatus & OS_TASK_STATUS_PENDING) {  // 如果任务处于挂起状态
        LOS_ListDelete(&taskCB->pendList);  // 从挂起列表中删除
        taskCB->taskStatus &= ~OS_TASK_STATUS_PENDING;  // 清除挂起状态
    }

    if (taskCB->taskStatus & (OS_TASK_STATUS_DELAY | OS_TASK_STATUS_PEND_TIME)) {  // 如果任务处于延迟或等待超时状态
        OsSchedTimeoutQueueDelete(taskCB);  // 从超时队列中删除
        taskCB->taskStatus &= ~(OS_TASK_STATUS_DELAY | OS_TASK_STATUS_PEND_TIME);  // 清除延迟和超时状态
    }
}

/**
 * @brief 任务主动让出CPU
 * @param runTask 当前运行任务控制块指针
 * @details 重置时间片，重新入队并触发调度
 */
STATIC VOID HPFYield(LosTaskCB *runTask)
{
    SchedRunqueue *rq = OsSchedRunqueue();  // 获取运行队列
    runTask->timeSlice = 0;  // 重置时间片

    runTask->startTime = OsGetCurrSchedTimeCycle();  // 更新开始时间
    HPFEnqueue(rq, runTask);  // 重新入队
    OsSchedResched();  // 触发调度
}

/**
 * @brief 任务延迟
 * @param runTask 当前运行任务控制块指针
 * @param waitTime 延迟时间
 * @return 操作结果
 * @details 设置任务延迟状态并触发调度
 */
STATIC UINT32 HPFDelay(LosTaskCB *runTask, UINT64 waitTime)
{
    runTask->taskStatus |= OS_TASK_STATUS_DELAY;  // 设置延迟状态
    runTask->waitTime = waitTime;  // 设置等待时间

    OsSchedResched();  // 触发调度
    return LOS_OK;  // 返回成功
}

/**
 * @brief 获取任务等待时间
 * @param taskCB 任务控制块指针
 * @return 任务等待时间
 * @details 计算并返回任务的等待时间
 */
STATIC UINT64 HPFWaitTimeGet(LosTaskCB *taskCB)
{
    taskCB->waitTime += taskCB->startTime;  // 计算等待时间
    return taskCB->waitTime;  // 返回等待时间
}

/**
 * @brief 任务等待操作
 * @param runTask 当前运行任务控制块指针
 * @param list 等待列表
 * @param ticks 等待超时时间
 * @return 操作结果
 * @details 将任务加入等待列表，设置超时（如果需要），并触发调度
 */
STATIC UINT32 HPFWait(LosTaskCB *runTask, LOS_DL_LIST *list, UINT32 ticks)
{
    runTask->taskStatus |= OS_TASK_STATUS_PENDING;  // 设置挂起状态
    LOS_ListTailInsert(list, &runTask->pendList);  // 将任务插入等待列表尾部

    if (ticks != LOS_WAIT_FOREVER) {  // 如果不是永久等待
        runTask->taskStatus |= OS_TASK_STATUS_PEND_TIME;  // 设置等待超时状态
        runTask->waitTime = OS_SCHED_TICK_TO_CYCLE(ticks);  // 转换超时时间为周期
    }

    if (OsPreemptableInSched()) {  // 如果可抢占
        OsSchedResched();  // 触发调度
        if (runTask->taskStatus & OS_TASK_STATUS_TIMEOUT) {  // 如果超时
            runTask->taskStatus &= ~OS_TASK_STATUS_TIMEOUT;  // 清除超时状态
            return LOS_ERRNO_TSK_TIMEOUT;  // 返回超时错误
        }
    }

    return LOS_OK;  // 返回成功
}

/**
 * @brief 任务唤醒操作
 * @param resumedTask 被唤醒任务控制块指针
 * @details 将任务从等待列表中移除，清理超时状态，并根据挂起状态决定是否入队
 */
STATIC VOID HPFWake(LosTaskCB *resumedTask)
{
    LOS_ListDelete(&resumedTask->pendList);  // 从等待列表中删除
    resumedTask->taskStatus &= ~OS_TASK_STATUS_PENDING;  // 清除挂起状态

    if (resumedTask->taskStatus & OS_TASK_STATUS_PEND_TIME) {  // 如果处于等待超时状态
        OsSchedTimeoutQueueDelete(resumedTask);  // 从超时队列中删除
        resumedTask->taskStatus &= ~OS_TASK_STATUS_PEND_TIME;  // 清除等待超时状态
    }

    if (!(resumedTask->taskStatus & OS_TASK_STATUS_SUSPENDED)) {  // 如果未被挂起
#ifdef LOSCFG_SCHED_HPF_DEBUG
        resumedTask->schedStat.pendTime += OsGetCurrSchedTimeCycle() - resumedTask->startTime;  // 记录等待时间
        resumedTask->schedStat.pendCount++;  // 增加等待计数
#endif
        HPFEnqueue(OsSchedRunqueue(), resumedTask);  // 加入运行队列
    }
}

/**
 * @brief 修改基础优先级
 * @param rq 运行队列指针
 * @param taskCB 任务控制块指针
 * @param priority 新的基础优先级
 * @return 是否需要重新调度
 * @details 修改进程中所有线程的基础优先级，并决定是否需要调度
 */
STATIC BOOL BasePriorityModify(SchedRunqueue *rq, LosTaskCB *taskCB, UINT16 priority)
{
    LosProcessCB *processCB = OS_PCB_FROM_TCB(taskCB);  // 从任务控制块获取进程控制块
    BOOL needSched = FALSE;  // 是否需要调度的标志

    LOS_DL_LIST_FOR_EACH_ENTRY(taskCB, &processCB->threadSiblingList, LosTaskCB, threadList) {  // 遍历进程中的所有线程
        SchedHPF *sched = (SchedHPF *)&taskCB->sp;  // 获取调度参数
        if (taskCB->taskStatus & OS_TASK_STATUS_READY) {  // 如果任务就绪
            taskCB->ops->dequeue(rq, taskCB);  // 出队
            sched->basePrio = priority;  // 更新基础优先级
            taskCB->ops->enqueue(rq, taskCB);  // 重新入队
        } else {  // 如果任务未就绪
            sched->basePrio = priority;  // 直接更新基础优先级
        }
        if (taskCB->taskStatus & (OS_TASK_STATUS_READY | OS_TASK_STATUS_RUNNING)) {  // 如果任务就绪或运行中
            needSched = TRUE;  // 需要重新调度
        }
    }

    return needSched;  // 返回是否需要调度
}

/**
 * @brief 修改任务调度参数
 * @param taskCB 任务控制块指针
 * @param param 新的调度参数
 * @return 是否需要重新调度
 * @details 修改任务的调度策略、基础优先级和优先级，并决定是否需要调度
 */
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


/**
 * @brief 获取任务调度参数
 * @param taskCB 任务控制块指针
 * @param param 输出参数，用于存储调度参数
 * @return 操作结果
 * @details 获取任务的调度策略、基础优先级、优先级和初始时间片
 */
STATIC UINT32 HPFSchedParamGet(const LosTaskCB *taskCB, SchedParam *param)
{
    SchedHPF *sched = (SchedHPF *)&taskCB->sp;  // 获取调度参数
    param->policy = sched->policy;  // 获取调度策略
    param->basePrio = sched->basePrio;  // 获取基础优先级
    param->priority = sched->priority;  // 获取优先级
    param->timeSlice = sched->initTimeSlice;  // 获取初始时间片
    return LOS_OK;  // 返回成功
}

/**
 * @brief 任务挂起
 * @param taskCB 任务控制块指针
 * @return 操作结果
 * @details 将任务从就绪队列中移除，设置挂起状态，必要时触发调度
 */
STATIC UINT32 HPFSuspend(LosTaskCB *taskCB)
{
    if (taskCB->taskStatus & OS_TASK_STATUS_READY) {  // 如果任务就绪
        HPFDequeue(OsSchedRunqueue(), taskCB);  // 从运行队列中出队
    }

    SchedTaskFreeze(taskCB);  // 冻结任务

    taskCB->taskStatus |= OS_TASK_STATUS_SUSPENDED;  // 设置挂起状态
    OsHookCall(LOS_HOOK_TYPE_MOVEDTASKTOSUSPENDEDLIST, taskCB);  // 调用挂起钩子
    if (taskCB == OsCurrTaskGet()) {  // 如果是当前运行任务
        OsSchedResched();  // 触发调度
    }
    return LOS_OK;  // 返回成功
}

/**
 * @brief 任务恢复
 * @param taskCB 任务控制块指针
 * @param needSched 输出参数，是否需要调度
 * @return 操作结果
 * @details 解冻任务，清除挂起状态，必要时将任务入队
 */
STATIC UINT32 HPFResume(LosTaskCB *taskCB, BOOL *needSched)
{
    *needSched = FALSE;  // 初始化是否需要调度标志

    SchedTaskUnfreeze(taskCB);  // 解冻任务

    taskCB->taskStatus &= ~OS_TASK_STATUS_SUSPENDED;  // 清除挂起状态
    if (!OsTaskIsBlocked(taskCB)) {  // 如果任务未被阻塞
        HPFEnqueue(OsSchedRunqueue(), taskCB);  // 加入运行队列
        *needSched = TRUE;  // 需要调度
    }

    return LOS_OK;  // 返回成功
}

/**
 * @brief 比较两个调度参数
 * @param sp1 第一个调度参数
 * @param sp2 第二个调度参数
 * @return 比较结果（正数表示sp1优先级高，负数表示sp2优先级高，0表示相等）
 * @details 先比较基础优先级，再比较优先级
 */
STATIC INT32 HPFParamCompare(const SchedPolicy *sp1, const SchedPolicy *sp2)
{
    SchedHPF *param1 = (SchedHPF *)sp1;  // 转换为HPF调度参数
    SchedHPF *param2 = (SchedHPF *)sp2;  // 转换为HPF调度参数

    if (param1->basePrio != param2->basePrio) {  // 先比较基础优先级
        return (param1->basePrio - param2->basePrio);  // 返回基础优先级差值
    }

    return (param1->priority - param2->priority);  // 再比较优先级
}

/**
 * @brief 优先级继承
 * @param owner 资源拥有者任务控制块指针
 * @param param 请求者的调度参数
 * @details 实现优先级继承协议，提高资源拥有者的优先级
 */
STATIC VOID HPFPriorityInheritance(LosTaskCB *owner, const SchedParam *param)
{
    SchedHPF *sp = (SchedHPF *)&owner->sp;  // 获取拥有者的调度参数

    if ((param->policy != LOS_SCHED_RR) && (param->policy != LOS_SCHED_FIFO)) {  // 如果请求者不是RR或FIFO策略
        return;  // 不进行优先级继承
    }

    if (sp->priority <= param->priority) {  // 如果拥有者优先级低于请求者
        return;  // 不进行优先级继承
    }

    LOS_BitmapSet(&sp->priBitmap, sp->priority);  // 记录原始优先级
    sp->priority = param->priority;  // 提高优先级到请求者的优先级
}

/**
 * @brief 优先级恢复
 * @param owner 资源拥有者任务控制块指针
 * @param list 等待列表
 * @param param 请求者的调度参数
 * @details 当资源释放后，恢复拥有者的原始优先级
 */
STATIC VOID HPFPriorityRestore(LosTaskCB *owner, const LOS_DL_LIST *list, const SchedParam *param)
{
    UINT16 priority;  // 优先级
    LosTaskCB *pendedTask = NULL;  // 等待任务

    if ((param->policy != LOS_SCHED_RR) && (param->policy != LOS_SCHED_FIFO)) {  // 如果请求者不是RR或FIFO策略
        return;  // 不进行优先级恢复
    }

    SchedHPF *sp = (SchedHPF *)&owner->sp;  // 获取拥有者的调度参数
    if (sp->priority < param->priority) {  // 如果当前优先级低于请求者优先级
        if (LOS_HighBitGet(sp->priBitmap) != param->priority) {  // 如果最高记录优先级不是请求者优先级
            LOS_BitmapClr(&sp->priBitmap, param->priority);  // 清除该优先级记录
        }
        return;  // 不进行优先级恢复
    }

    if (sp->priBitmap == 0) {  // 如果没有记录的优先级
        return;  // 不进行优先级恢复
    }

    if ((list != NULL) && !LOS_ListEmpty((LOS_DL_LIST *)list)) {  // 如果等待列表不为空
        priority = LOS_HighBitGet(sp->priBitmap);  // 获取历史最高优先级
        LOS_DL_LIST_FOR_EACH_ENTRY(pendedTask, list, LosTaskCB, pendList) {  // 遍历等待列表
            SchedHPF *pendSp = (SchedHPF *)&pendedTask->sp;  // 获取等待任务的调度参数
            if ((pendedTask->ops == owner->ops) && (priority != pendSp->priority)) {  // 如果操作集相同且优先级不同
                LOS_BitmapClr(&sp->priBitmap, pendSp->priority);  // 清除该优先级记录
            }
        }
    }

    priority = LOS_LowBitGet(sp->priBitmap);  // 获取最低记录优先级
    if (priority != LOS_INVALID_BIT_INDEX) {  // 如果有效
        LOS_BitmapClr(&sp->priBitmap, priority);  // 清除该优先级记录
        sp->priority = priority;  // 恢复到该优先级
    }
}

/**
 * @brief HPF任务调度参数初始化
 * @param taskCB 任务控制块指针
 * @param policy 调度策略
 * @param parentParam 父任务调度参数
 * @param param 任务调度参数
 * @details 初始化任务的HPF调度参数，包括策略、优先级和基础优先级
 */
VOID HPFTaskSchedParamInit(LosTaskCB *taskCB, UINT16 policy,
                           const SchedParam *parentParam,
                           const LosSchedParam *param)
{
    SchedHPF *sched = (SchedHPF *)&taskCB->sp;  // 获取HPF调度参数

    sched->policy = policy;  // 设置调度策略
    if (param != NULL) {  // 如果提供了任务调度参数
        sched->priority = param->priority;  // 使用提供的优先级
    } else {  // 如果未提供
        sched->priority = parentParam->priority;  // 使用父任务的优先级
    }
    sched->basePrio = parentParam->basePrio;  // 使用父任务的基础优先级

    sched->initTimeSlice = 0;  // 初始化时间片
    taskCB->timeSlice = sched->initTimeSlice;  // 设置初始时间片
    taskCB->ops = &g_priorityOps;  // 设置调度操作集
}

/**
 * @brief 获取HPF进程默认调度参数
 * @param param 输出参数，用于存储默认调度参数
 * @details 设置进程的默认基础优先级
 */
VOID HPFProcessDefaultSchedParamGet(SchedParam *param)
{
    param->basePrio = OS_USER_PROCESS_PRIORITY_HIGHEST;  // 设置默认基础优先级为用户进程最高优先级
}

/**
 * @brief HPF调度策略初始化
 * @param rq 运行队列指针
 * @details 初始化HPF调度的运行队列和优先级队列
 */
VOID HPFSchedPolicyInit(SchedRunqueue *rq)
{
    if (ArchCurrCpuid() > 0) {  // 如果不是0号CPU
        rq->hpfRunqueue = &g_schedHPF;  // 共享全局HPF运行队列
        return;
    }

    for (UINT16 index = 0; index < OS_PRIORITY_QUEUE_NUM; index++) {  // 遍历所有基础优先级
        HPFQueue *queueList = &g_schedHPF.queueList[index];  // 获取基础优先级队列
        LOS_DL_LIST *priQue = &queueList->priQueList[0];  // 获取优先级队列列表
        for (UINT16 prio = 0; prio < OS_PRIORITY_QUEUE_NUM; prio++) {  // 遍历所有优先级
            LOS_ListInit(&priQue[prio]);  // 初始化优先级队列
        }
    }

    rq->hpfRunqueue = &g_schedHPF;  // 设置运行队列的HPF运行队列指针
}
