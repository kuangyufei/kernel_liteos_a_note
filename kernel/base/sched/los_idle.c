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

/**
 * @brief 空闲任务调度操作集
 * @details 实现空闲任务特有的调度策略，所有未实现的接口均设为NULL
 */
const STATIC SchedOps g_idleOps = {
    .dequeue = IdleDequeue,                // 从就绪队列移除任务
    .enqueue = IdleEnqueue,                // 将任务加入就绪队列
    .waitTimeGet = NULL,                   // 未实现：获取等待时间
    .wait = IdleWait,                      // 任务等待操作
    .wake = IdleWake,                      // 唤醒等待中的任务
    .schedParamModify = NULL,              // 未实现：修改调度参数
    .schedParamGet = IdleSchedParamGet,    // 获取调度参数
    .delay = NULL,                         // 未实现：延迟操作
    .yield = IdleYield,                    // 任务主动让出CPU
    .start = IdleStartToRun,               // 开始运行任务
    .exit = NULL,                          // 未实现：任务退出
    .suspend = NULL,                       // 未实现：挂起任务
    .resume = IdleResume,                  // 恢复任务运行
    .deadlineGet = IdleTimeSliceGet,       // 获取时间片（截止时间）
    .timeSliceUpdate = IdleTimeSliceUpdate,// 更新时间片信息
    .schedParamCompare = IdleParamCompare, // 比较调度参数（优先级）
    .priorityInheritance = IdlePriorityInheritance, // 未实现：优先级继承
    .priorityRestore = IdlePriorityRestore, // 未实现：优先级恢复
};

/**
 * @brief 更新空闲任务的时间片开始时间
 * @param rq       调度运行队列指针（未使用）
 * @param taskCB   任务控制块指针
 * @param currTime 当前调度时间（周期数）
 */
STATIC VOID IdleTimeSliceUpdate(SchedRunqueue *rq, LosTaskCB *taskCB, UINT64 currTime)
{
    (VOID)rq;  // 未使用参数

    taskCB->startTime = currTime;  // 更新任务开始时间为当前时间
}

/**
 * @brief 获取空闲任务的时间片长度
 * @param taskCB  任务控制块指针（未使用）
 * @return UINT64 时间片长度（最大响应时间 - 时钟精度补偿）
 */
STATIC UINT64 IdleTimeSliceGet(const LosTaskCB *taskCB)
{
    (VOID)taskCB;  // 未使用参数
    return (OS_SCHED_MAX_RESPONSE_TIME - OS_TICK_RESPONSE_PRECISION);  // 返回计算后的时间片
}

/**
 * @brief 将空闲任务加入就绪队列
 * @param rq      调度运行队列指针（未使用）
 * @param taskCB  任务控制块指针
 */
STATIC VOID IdleEnqueue(SchedRunqueue *rq, LosTaskCB *taskCB)
{
    (VOID)rq;  // 未使用参数

    taskCB->taskStatus &= ~OS_TASK_STATUS_BLOCKED;  // 清除阻塞状态
    taskCB->taskStatus |= OS_TASK_STATUS_READY;     // 设置就绪状态
}

/**
 * @brief 将空闲任务从就绪队列移除
 * @param rq      调度运行队列指针（未使用）
 * @param taskCB  任务控制块指针
 */
STATIC VOID IdleDequeue(SchedRunqueue *rq, LosTaskCB *taskCB)
{
    (VOID)rq;  // 未使用参数

    taskCB->taskStatus &= ~OS_TASK_STATUS_READY;  // 清除就绪状态
}

/**
 * @brief 开始运行空闲任务
 * @param rq      调度运行队列指针
 * @param taskCB  任务控制块指针
 */
STATIC VOID IdleStartToRun(SchedRunqueue *rq, LosTaskCB *taskCB)
{
    IdleDequeue(rq, taskCB);  // 从就绪队列移除（标记为运行中）
}

/**
 * @brief 空闲任务主动让出CPU（空实现）
 * @param runTask  当前运行任务控制块指针（未使用）
 */
STATIC VOID IdleYield(LosTaskCB *runTask)
{
    (VOID)runTask;  // 未使用参数
    return;         // 空闲任务无需让出CPU
}

/**
 * @brief 空闲任务等待操作（不支持）
 * @param runTask  当前运行任务控制块指针（未使用）
 * @param list     等待队列头指针（未使用）
 * @param ticks    等待超时ticks数（未使用）
 * @return UINT32  操作结果，始终返回LOS_NOK表示不支持
 */
STATIC UINT32 IdleWait(LosTaskCB *runTask, LOS_DL_LIST *list, UINT32 ticks)
{
    (VOID)runTask;  // 未使用参数
    (VOID)list;     // 未使用参数
    (VOID)ticks;    // 未使用参数

    return LOS_NOK;  // 空闲任务不支持等待操作
}

/**
 * @brief 唤醒处于等待状态的空闲任务
 * @param resumedTask  待唤醒任务控制块指针
 */
STATIC VOID IdleWake(LosTaskCB *resumedTask)
{
    LOS_ListDelete(&resumedTask->pendList);  // 将任务从等待队列中删除
    resumedTask->taskStatus &= ~OS_TASK_STATUS_PENDING;  // 清除等待状态

    if (resumedTask->taskStatus & OS_TASK_STATUS_PEND_TIME) {  // 检查是否有等待计时
        OsSchedTimeoutQueueDelete(resumedTask);  // 从超时队列中删除
        resumedTask->taskStatus &= ~OS_TASK_STATUS_PEND_TIME;  // 清除等待计时状态
    }

    if (!(resumedTask->taskStatus & OS_TASK_STATUS_SUSPENDED)) {  // 任务未被挂起
#ifdef LOSCFG_SCHED_DEBUG
        resumedTask->schedStat.pendTime += OsGetCurrSchedTimeCycle() - resumedTask->startTime;  // 累加挂起时间
        resumedTask->schedStat.pendCount++;  // 挂起次数+1
#endif
        resumedTask->ops->enqueue(OsSchedRunqueue(), resumedTask);  // 调用enqueue接口加入就绪队列
    }
}

/**
 * @brief 恢复空闲任务运行
 * @param taskCB   任务控制块指针
 * @param needSched 是否需要重新调度的输出指针
 * @return UINT32  操作结果，LOS_OK表示成功
 */
STATIC UINT32 IdleResume(LosTaskCB *taskCB, BOOL *needSched)
{
    *needSched = FALSE;  // 初始化为不需要调度

    taskCB->taskStatus &= ~OS_TASK_STATUS_SUSPENDED;  // 清除挂起状态
    if (!OsTaskIsBlocked(taskCB)) {  // 任务未被阻塞
        taskCB->ops->enqueue(OsSchedRunqueue(), taskCB);  // 加入就绪队列
        *needSched = TRUE;  // 需要重新调度
    }
    return LOS_OK;  // 返回成功
}

/**
 * @brief 获取空闲任务的调度参数
 * @param taskCB  任务控制块指针
 * @param param   输出参数，用于存储获取到的调度参数
 * @return UINT32 操作结果，LOS_OK表示成功
 */
STATIC UINT32 IdleSchedParamGet(const LosTaskCB *taskCB, SchedParam *param)
{
    SchedHPF *sched = (SchedHPF *)&taskCB->sp;  // 转换为HPF调度参数
    param->policy = sched->policy;              // 设置调度策略
    param->basePrio = sched->basePrio;          // 设置基础优先级
    param->priority = sched->priority;          // 设置当前优先级
    param->timeSlice = 0;                       // 空闲任务无时间片
    return LOS_OK;  // 返回成功
}

/**
 * @brief 比较两个空闲任务的调度参数（始终相等）
 * @param sp1  第一个任务的调度参数（未使用）
 * @param sp2  第二个任务的调度参数（未使用）
 * @return INT32 比较结果，始终返回0表示优先级相等
 */
STATIC INT32 IdleParamCompare(const SchedPolicy *sp1, const SchedPolicy *sp2)
{
    return 0;  // 所有空闲任务优先级相同
}

/**
 * @brief 空闲任务优先级继承（空实现）
 * @param owner  资源拥有者任务控制块（未使用）
 * @param param  申请资源的任务调度参数（未使用）
 */
STATIC VOID IdlePriorityInheritance(LosTaskCB *owner, const SchedParam *param)
{
    (VOID)owner;  // 未使用参数
    (VOID)param;  // 未使用参数
    return;       // 空闲任务不支持优先级继承
}

/**
 * @brief 空闲任务优先级恢复（空实现）
 * @param owner  资源拥有者任务控制块（未使用）
 * @param list   等待队列（未使用）
 * @param param  原调度参数（未使用）
 */
STATIC VOID IdlePriorityRestore(LosTaskCB *owner, const LOS_DL_LIST *list, const SchedParam *param)
{
    (VOID)owner;  // 未使用参数
    (VOID)list;   // 未使用参数
    (VOID)param;  // 未使用参数
    return;       // 空闲任务不支持优先级恢复
}

/**
 * @brief 初始化空闲任务的调度参数
 * @param taskCB  任务控制块指针
 */
VOID IdleTaskSchedParamInit(LosTaskCB *taskCB)
{
    SchedHPF *sched = (SchedHPF *)&taskCB->sp;  // 转换为HPF调度参数
    sched->policy = LOS_SCHED_IDLE;             // 设置调度策略为空闲调度
    sched->basePrio = OS_PROCESS_PRIORITY_LOWEST;  // 基础优先级设为最低
    sched->priority = OS_TASK_PRIORITY_LOWEST;  // 任务优先级设为最低
    sched->initTimeSlice = 0;                   // 初始时间片为0（无时间片限制）
    taskCB->timeSlice = sched->initTimeSlice;   // 设置当前时间片
    taskCB->taskStatus = OS_TASK_STATUS_SUSPENDED;  // 初始状态为挂起
    taskCB->ops = &g_idleOps;                   // 绑定空闲调度操作集
}
