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
// EDF调度策略下获取任务的实际时间片长度
STATIC UINT64 EDFTimeSliceGet(const LosTaskCB *taskCB)  
{
    SchedEDF *sched = (SchedEDF *)&taskCB->sp;          // 将任务控制块中的调度参数转换为EDF专用结构体
    UINT64 endTime = taskCB->startTime + taskCB->timeSlice;  // 计算理论时间片结束时间（开始时间+分配的时间片长度）
    return (endTime > sched->finishTime) ? sched->finishTime : endTime;  // 取理论结束时间和截止时间的较小值，确保不超过截止时间
}

/**
 * @brief EDF调度队列插入函数
 * @param rq EDF运行队列指针，包含待插入的任务队列
 * @param taskCB 待插入的任务控制块指针
 * @details 按照EDF调度策略（最早截止时间优先）将任务插入到合适位置，确保队列始终按截止时间排序
 * @note 该函数是EDF调度器的核心排序实现，通过遍历比较截止时间参数实现有序插入
 */
STATIC VOID DeadlineQueueInsert(EDFRunqueue *rq, LosTaskCB *taskCB)
{
    LOS_DL_LIST *root = &rq->root;  // 获取EDF队列的根节点
    if (LOS_ListEmpty(root)) {      // 判断队列是否为空
        LOS_ListTailInsert(root, &taskCB->pendList);  // 队列为空时直接插入尾部
        return;
    }

    LOS_DL_LIST *list = root->pstNext;  // 从队列第一个元素开始遍历
    do {
        LosTaskCB *readyTask = LOS_DL_LIST_ENTRY(list, LosTaskCB, pendList);  // 将链表节点转换为任务控制块
        if (EDFParamCompare(&readyTask->sp, &taskCB->sp) > 0) {  // 比较当前任务与待插入任务的截止时间参数
            LOS_ListHeadInsert(list, &taskCB->pendList);  // 插入到当前节点之前（保持队列有序）
            return;
        }
        list = list->pstNext;  // 移动到下一个节点继续比较
    } while (list != root);  // 循环直到回到根节点（遍历完成）

    LOS_ListHeadInsert(list, &taskCB->pendList);  // 遍历完成仍未找到插入位置，插入到队列尾部
}

/**
 * @brief 将EDF任务加入调度队列（就绪队列或等待队列）
 * @param rq      调度运行队列指针
 * @param taskCB  待加入队列的任务控制块指针
 * @note 根据任务时间片状态和周期参数决定加入就绪队列或等待队列
 */
STATIC VOID EDFEnqueue(SchedRunqueue *rq, LosTaskCB *taskCB)
{
    LOS_ASSERT(!(taskCB->taskStatus & OS_TASK_STATUS_READY));  // 断言任务当前不处于就绪状态

    EDFRunqueue *erq = rq->edfRunqueue;  // 获取EDF运行队列
    SchedEDF *sched = (SchedEDF *)&taskCB->sp;  // 获取任务的EDF调度参数


    if (taskCB->timeSlice <= 0) {  // 时间片用尽，需要重新计算周期
#ifdef LOSCFG_SCHED_EDF_DEBUG
        UINT64 oldFinish = sched->finishTime;  // 调试模式下记录旧的完成时间
#endif
        UINT64 currTime = OsGetCurrSchedTimeCycle();  // 获取当前调度时间（周期数）

        // 初始化或计算下一个周期的开始时间
        if (sched->flags == EDF_INIT) {  // 首次初始化
            sched->finishTime = currTime;  // 初始化完成时间为当前时间
        } else if (sched->flags != EDF_NEXT_PERIOD) {  // 非下一个周期状态
            // 计算下一个周期的开始时间：(当前完成时间 - 截止时间) + 周期长度
            sched->finishTime = (sched->finishTime - sched->deadline) + sched->period;
        }

        // 计算下一个周期的开始时间，确保满足最小运行时间要求
        while (1) {  // 循环调整周期开始时间
            UINT64 finishTime = sched->finishTime + sched->deadline;  // 计算下一个周期的截止时间
            // 如果当前周期已过或无法满足最小运行时间，迁移到下一个周期
            if ((finishTime <= currTime) || ((sched->finishTime + sched->runTime) > finishTime)) {
                sched->finishTime += sched->period;  // 迁移到下一个周期
                continue;  // 继续检查新周期是否有效
            }
            break;  // 找到有效周期，退出循环
        }

        // 如果下一个周期还未开始，将任务加入等待队列
        if (sched->finishTime > currTime) {
            LOS_ListTailInsert(&erq->waitList, &taskCB->pendList);  // 将任务插入等待队列尾部
            taskCB->waitTime = OS_SCHED_MAX_RESPONSE_TIME;  // 设置最大等待时间
            if (!OsTaskIsRunning(taskCB)) {  // 如果任务未运行
                OsSchedTimeoutQueueAdd(taskCB, sched->finishTime);  // 将任务加入超时队列
            }
#ifdef LOSCFG_SCHED_EDF_DEBUG
            // 调试模式下记录状态变化
            if (oldFinish != sched->finishTime) {  // 完成时间发生变化
                EDFDebugRecord((UINTPTR *)taskCB, oldFinish);  // 记录调试信息
                taskCB->schedStat.allRuntime = 0;  // 重置总运行时间统计
                taskCB->schedStat.timeSliceRealTime = 0;  // 重置实际时间片统计
                taskCB->schedStat.pendTime = 0;  // 重置挂起时间统计
            }
#endif
            taskCB->taskStatus |= OS_TASK_STATUS_PEND_TIME;  // 设置任务状态为等待时间
            sched->flags = EDF_NEXT_PERIOD;  // 设置标志为下一个周期
            return;  // 直接返回，任务已加入等待队列
        }

        // 更新任务的时间片和状态（周期已开始）
        sched->finishTime += sched->deadline;  // 更新完成时间为当前周期截止时间
        taskCB->timeSlice = sched->runTime;  // 重置时间片为任务运行时间
        sched->flags = EDF_UNUSED;  // 设置标志为未使用
    }

    // 将任务插入就绪队列并更新状态
    DeadlineQueueInsert(erq, taskCB);  // 将任务插入EDF就绪队列（按截止时间排序）
    taskCB->taskStatus &= ~(OS_TASK_STATUS_BLOCKED | OS_TASK_STATUS_TIMEOUT);  // 清除阻塞和超时状态
    taskCB->taskStatus |= OS_TASK_STATUS_READY;  // 设置任务状态为就绪
}


/**
 * @brief 将EDF任务从就绪队列中移除
 * @param rq      调度运行队列指针（未使用）
 * @param taskCB  待移除的任务控制块指针
 */
STATIC VOID EDFDequeue(SchedRunqueue *rq, LosTaskCB *taskCB)
{
    (VOID)rq;  // 未使用参数
    LOS_ListDelete(&taskCB->pendList);  // 从链表中删除任务节点
    taskCB->taskStatus &= ~OS_TASK_STATUS_READY;  // 清除就绪状态
}

/**
 * @brief EDF任务退出时的资源清理
 * @param taskCB  待清理的任务控制块指针
 * @note 从各类队列中移除任务并清除相关状态
 */
STATIC VOID EDFExit(LosTaskCB *taskCB)
{
    if (taskCB->taskStatus & OS_TASK_STATUS_READY) {  // 任务处于就绪状态
        EDFDequeue(OsSchedRunqueue(), taskCB);  // 从就绪队列中移除
    } else if (taskCB->taskStatus & OS_TASK_STATUS_PENDING) {  // 任务处于等待状态
        LOS_ListDelete(&taskCB->pendList);  // 从等待队列中删除
        taskCB->taskStatus &= ~OS_TASK_STATUS_PENDING;  // 清除等待状态
    }
    if (taskCB->taskStatus & (OS_TASK_STATUS_DELAY | OS_TASK_STATUS_PEND_TIME)) {  // 任务处于延迟或等待时间状态
        OsSchedTimeoutQueueDelete(taskCB);  // 从超时队列中删除
        taskCB->taskStatus &= ~(OS_TASK_STATUS_DELAY | OS_TASK_STATUS_PEND_TIME);  // 清除相关状态
    }
}

/**
 * @brief EDF任务主动让出CPU
 * @param runTask  当前运行任务控制块指针
 * @note 重置时间片并触发重新调度
 */
STATIC VOID EDFYield(LosTaskCB *runTask)
{
    SchedRunqueue *rq = OsSchedRunqueue();  // 获取当前调度运行队列
    runTask->timeSlice = 0;  // 将任务的时间片置为0，表示主动放弃CPU

    runTask->startTime = OsGetCurrSchedTimeCycle();  // 更新任务开始时间为当前调度周期
    EDFEnqueue(rq, runTask);  // 将任务重新加入EDF就绪队列
    OsSchedResched();  // 触发重新调度
}

/**
 * @brief 将EDF任务加入延迟队列
 * @param runTask  当前运行任务控制块指针
 * @param waitTime 延迟时间（周期数）
 * @return UINT32  操作结果，LOS_OK表示成功
 */
STATIC UINT32 EDFDelay(LosTaskCB *runTask, UINT64 waitTime)
{
    runTask->taskStatus |= OS_TASK_STATUS_DELAY;  // 设置任务为延迟状态
    runTask->waitTime = waitTime;  // 设置等待时间
    OsSchedResched();  // 触发重新调度
    return LOS_OK;  // 返回成功
}

/**
 * @brief 获取EDF任务的等待时间
 * @param taskCB  任务控制块指针
 * @return UINT64  计算后的等待时间（周期数）
 * @note 取任务等待时间和EDF完成时间中的较小值
 */
STATIC UINT64 EDFWaitTimeGet(LosTaskCB *taskCB)
{
    const SchedEDF *sched = (const SchedEDF *)&taskCB->sp;  // 获取EDF调度参数
    if (sched->flags != EDF_WAIT_FOREVER) {  // 非永久等待
        taskCB->waitTime += taskCB->startTime;  // 等待时间加上开始时间
    }
    // 返回等待时间和完成时间中的较小值
    return (taskCB->waitTime >= sched->finishTime) ? sched->finishTime : taskCB->waitTime;
}

/**
 * @brief 处理EDF任务等待操作
 * @param runTask  当前运行任务控制块指针
 * @param list     等待队列头指针
 * @param ticks    等待超时 ticks 数，LOS_WAIT_FOREVER表示永久等待
 * @return UINT32  操作结果，LOS_OK表示成功，LOS_ERRNO_TSK_TIMEOUT表示超时
 */
STATIC UINT32 EDFWait(LosTaskCB *runTask, LOS_DL_LIST *list, UINT32 ticks)
{
    SchedEDF *sched = (SchedEDF *)&runTask->sp;  // 将任务调度参数转换为EDF专用调度参数
    runTask->taskStatus |= (OS_TASK_STATUS_PENDING | OS_TASK_STATUS_PEND_TIME);  // 设置任务为等待状态并标记等待计时
    LOS_ListTailInsert(list, &runTask->pendList);  // 将任务插入等待队列尾部

    if (ticks != LOS_WAIT_FOREVER) {  // 判断是否为有限时间等待
        runTask->waitTime = OS_SCHED_TICK_TO_CYCLE(ticks);  // 将ticks转换为系统周期数
    } else {  // 永久等待处理
        sched->flags = EDF_WAIT_FOREVER;  // 设置永久等待标志
        runTask->waitTime = OS_SCHED_MAX_RESPONSE_TIME;  // 设置最大等待时间
    }

    if (OsPreemptableInSched()) {  // 检查当前是否可抢占调度
        OsSchedResched();  // 触发调度器重新调度
        if (runTask->taskStatus & OS_TASK_STATUS_TIMEOUT) {  // 检查任务是否超时
            runTask->taskStatus &= ~OS_TASK_STATUS_TIMEOUT;  // 清除超时状态
            return LOS_ERRNO_TSK_TIMEOUT;  // 返回超时错误码
        }
    }

    return LOS_OK;  // 返回成功
}

/**
 * @brief 唤醒处于等待状态的EDF任务
 * @param resumedTask  待唤醒任务控制块指针
 */
STATIC VOID EDFWake(LosTaskCB *resumedTask)
{
    LOS_ListDelete(&resumedTask->pendList);  // 将任务从等待队列中删除
    resumedTask->taskStatus &= ~OS_TASK_STATUS_PENDING;  // 清除等待状态

    if (resumedTask->taskStatus & OS_TASK_STATUS_PEND_TIME) {  // 检查是否有等待计时
        OsSchedTimeoutQueueDelete(resumedTask);  // 从超时队列中删除任务
        resumedTask->taskStatus &= ~OS_TASK_STATUS_PEND_TIME;  // 清除等待计时状态
    }

    if (!(resumedTask->taskStatus & OS_TASK_STATUS_SUSPENDED)) {  // 检查任务是否未被挂起
#ifdef LOSCFG_SCHED_EDF_DEBUG
        resumedTask->schedStat.pendTime += OsGetCurrSchedTimeCycle() - resumedTask->startTime;  // 累加挂起时间
        resumedTask->schedStat.pendCount++;  // 挂起次数统计+1
#endif
        EDFEnqueue(OsSchedRunqueue(), resumedTask);  // 将任务重新加入EDF就绪队列
    }
}

/**
 * @brief 修改EDF任务的调度参数
 * @param taskCB  任务控制块指针
 * @param param   新的调度参数
 * @return BOOL   是否需要重新调度
 */
STATIC BOOL EDFSchedParamModify(LosTaskCB *taskCB, const SchedParam *param)
{
    SchedRunqueue *rq = OsSchedRunqueue();  // 获取当前CPU的运行队列
    SchedEDF *sched = (SchedEDF *)&taskCB->sp;  // 转换为EDF调度参数

    taskCB->timeSlice = 0;  // EDF任务不使用时间片机制
    sched->runTime = (INT32)OS_SYS_US_TO_CYCLE(param->runTimeUs);  // 将运行时间从微秒转换为周期数
    sched->period = OS_SYS_US_TO_CYCLE(param->periodUs);  // 将周期从微秒转换为周期数

    if (taskCB->taskStatus & OS_TASK_STATUS_READY) {  // 任务处于就绪状态
        EDFDequeue(rq, taskCB);  // 先从就绪队列中移除
        sched->deadline = OS_SYS_US_TO_CYCLE(param->deadlineUs);  // 更新截止时间
        EDFEnqueue(rq, taskCB);  // 使用新参数重新入队
        return TRUE;  // 需要重新调度
    }

    sched->deadline = OS_SYS_US_TO_CYCLE(param->deadlineUs);  // 更新截止时间
    if (taskCB->taskStatus & OS_TASK_STATUS_INIT) {  // 任务处于初始化状态
        EDFEnqueue(rq, taskCB);  // 将任务加入就绪队列
        return TRUE;  // 需要重新调度
    }

    if (taskCB->taskStatus & OS_TASK_STATUS_RUNNING) {  // 任务正在运行
        return TRUE;  // 需要重新调度
    }

    return FALSE;  // 无需重新调度
}

/**
 * @brief 获取EDF任务的调度参数
 * @param taskCB  任务控制块指针
 * @param param   输出参数，用于存储获取到的调度参数
 * @return UINT32 操作结果，LOS_OK表示成功
 */
STATIC UINT32 EDFSchedParamGet(const LosTaskCB *taskCB, SchedParam *param)
{
    SchedEDF *sched = (SchedEDF *)&taskCB->sp;  // 转换为EDF调度参数
    param->policy = sched->policy;  // 设置调度策略
    param->runTimeUs = (INT32)OS_SYS_CYCLE_TO_US((UINT64)sched->runTime);  // 运行时间从周期数转换为微秒
    param->deadlineUs = OS_SYS_CYCLE_TO_US(sched->deadline);  // 截止时间从周期数转换为微秒
    param->periodUs = OS_SYS_CYCLE_TO_US(sched->period);  // 周期从周期数转换为微秒
    return LOS_OK;  // 返回成功
}

/**
 * @brief 挂起EDF任务（未实现）
 * @param taskCB  任务控制块指针
 * @return UINT32 操作结果，返回LOS_EOPNOTSUPP表示不支持
 */
STATIC UINT32 EDFSuspend(LosTaskCB *taskCB)
{
    return LOS_EOPNOTSUPP;  // EDF调度暂不支持挂起操作
}

/**
 * @brief 恢复EDF任务（未实现）
 * @param taskCB   任务控制块指针
 * @param needSched 是否需要重新调度的输出指针
 * @return UINT32  操作结果，返回LOS_EOPNOTSUPP表示不支持
 */
STATIC UINT32 EDFResume(LosTaskCB *taskCB, BOOL *needSched)
{
    return LOS_EOPNOTSUPP;  // EDF调度暂不支持恢复操作
}

/**
 * @brief 比较两个EDF任务的优先级（基于完成时间）
 * @param sp1  第一个任务的调度参数
 * @param sp2  第二个任务的调度参数
 * @return INT32 比较结果，<0表示sp1优先级高，>0表示sp2优先级高，=0表示优先级相同
 */
STATIC INT32 EDFParamCompare(const SchedPolicy *sp1, const SchedPolicy *sp2)
{
    const SchedEDF *param1 = (const SchedEDF *)sp1;  // 转换为EDF调度参数
    const SchedEDF *param2 = (const SchedEDF *)sp2;  // 转换为EDF调度参数

    return (INT32)(param1->finishTime - param2->finishTime);  // 比较完成时间，较小者优先级高
}

/**
 * @brief EDF优先级继承（空实现）
 * @param owner  资源拥有者任务控制块
 * @param param  申请资源的任务调度参数
 */
STATIC VOID EDFPriorityInheritance(LosTaskCB *owner, const SchedParam *param)
{
    (VOID)owner;  // 未使用参数
    (VOID)param;  // 未使用参数
}

/**
 * @brief EDF优先级恢复（空实现）
 * @param owner  资源拥有者任务控制块
 * @param list   等待队列
 * @param param  原调度参数
 */
STATIC VOID EDFPriorityRestore(LosTaskCB *owner, const LOS_DL_LIST *list, const SchedParam *param)
{
    (VOID)owner;  // 未使用参数
    (VOID)list;   // 未使用参数
    (VOID)param;  // 未使用参数
}

/**
 * @brief EDF任务调度参数初始化
 * @param taskCB 任务控制块指针
 * @param policy 调度策略类型
 * @param parentParam 父任务调度参数（未使用）
 * @param param EDF调度参数（包含运行时间、截止时间、周期）
 * @return 操作结果，LOS_OK表示成功
 * @details 将用户传入的EDF调度参数（微秒单位）转换为内核周期单位，并初始化任务控制块中的EDF专用调度结构体
 */
UINT32 EDFTaskSchedParamInit(LosTaskCB *taskCB, UINT16 policy,
                             const SchedParam *parentParam,
                             const LosSchedParam *param)
{
    (VOID)parentParam;  // 未使用的父任务参数
    SchedEDF *sched = (SchedEDF *)&taskCB->sp;  // 将任务控制块的调度参数转换为EDF专用结构体
    sched->flags = EDF_INIT;  // 设置EDF初始化标志
    sched->policy = policy;  // 设置调度策略类型
    // 将微秒单位的运行时间转换为内核周期数
    sched->runTime = (INT32)OS_SYS_US_TO_CYCLE((UINT64)param->runTimeUs);
    sched->deadline = OS_SYS_US_TO_CYCLE(param->deadlineUs);  // 将微秒单位的截止时间转换为内核周期数
    sched->period = OS_SYS_US_TO_CYCLE(param->periodUs);  // 将微秒单位的周期转换为内核周期数
    sched->finishTime = 0;  // 初始化完成时间为0
    taskCB->timeSlice = 0;  // EDF任务不使用时间片机制，设置为0
    taskCB->ops = &g_deadlineOps;  // 绑定EDF调度操作集
    return LOS_OK;  // 返回成功状态
}

/**
 * @brief 获取EDF调度策略的默认参数
 * @param param 输出参数，用于存储默认调度参数
 * @details 设置EDF任务的默认运行时间、截止时间和周期参数
 */
VOID EDFProcessDefaultSchedParamGet(SchedParam *param)
{
    param->runTimeUs = OS_SCHED_EDF_MIN_RUNTIME;  // 设置默认最小运行时间（微秒）
    param->deadlineUs = OS_SCHED_EDF_MAX_DEADLINE;  // 设置默认最大截止时间（微秒）
    param->periodUs = param->deadlineUs;  // 默认周期等于截止时间（微秒）
}

/**
 * @brief EDF调度策略初始化
 * @param rq 调度运行队列指针
 * @details 初始化EDF运行队列，设置周期并初始化双向链表；多核环境下仅主核初始化全局队列
 */
VOID EDFSchedPolicyInit(SchedRunqueue *rq)
{
    if (ArchCurrCpuid() > 0) {  // 判断是否为非主核（仅主核执行完整初始化）
        rq->edfRunqueue = &g_schedEDF;  // 非主核直接引用全局EDF队列
        return;
    }

    EDFRunqueue *erq = &g_schedEDF;  // 获取全局EDF运行队列
    erq->period = OS_SCHED_MAX_RESPONSE_TIME;  // 设置最大响应时间周期
    LOS_ListInit(&erq->root);  // 初始化就绪任务链表
    LOS_ListInit(&erq->waitList);  // 初始化等待任务链表
    rq->edfRunqueue = erq;  // 绑定EDF队列到调度运行队列
}
