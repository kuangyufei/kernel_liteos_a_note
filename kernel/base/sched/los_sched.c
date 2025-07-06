/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2023 Huawei Device Co., Ltd. All rights reserved.
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
#include "los_hw_pri.h"
#include "los_task_pri.h"
#include "los_swtmr_pri.h"
#include "los_process_pri.h"
#include "los_arch_mmu.h"
#include "los_hook.h"
#ifdef LOSCFG_KERNEL_CPUP
#include "los_cpup_pri.h"
#endif
#include "los_hw_tick_pri.h"
#include "los_tick_pri.h"
#ifdef LOSCFG_BASE_CORE_TSK_MONITOR
#include "los_stackinfo_pri.h"
#endif
#include "los_mp.h"
/**
 * @brief 调度运行队列数组，每个CPU核心对应一个调度运行队列
 */
SchedRunqueue g_schedRunqueue[LOSCFG_KERNEL_CORE_NUM];

/**
 * @brief 设置下一个到期时间
 * @details 根据当前任务的截止时间和超时队列中的下一个到期时间，计算并更新调度运行队列的响应时间
 * @param responseID 当前任务的响应ID
 * @param taskEndTime 当前任务的结束时间（截止时间）
 * @param oldResponseID 旧的响应ID
 */
STATIC INLINE VOID SchedNextExpireTimeSet(UINT32 responseID, UINT64 taskEndTime, UINT32 oldResponseID)
{
    SchedRunqueue *rq = OsSchedRunqueue();  // 获取当前CPU的调度运行队列
    BOOL isTimeSlice = FALSE;  // 是否为时间片到期标志
    UINT64 currTime = OsGetCurrSchedTimeCycle();  // 获取当前调度时间周期
    UINT64 nextExpireTime = OsGetSortLinkNextExpireTime(&rq->timeoutQueue, currTime, OS_TICK_RESPONSE_PRECISION);  // 获取超时队列中下一个到期时间

    rq->schedFlag &= ~INT_PEND_TICK;  // 清除调度标志中的滴答 pending 位
    if (rq->responseID == oldResponseID) {  // 如果当前响应ID与旧响应ID匹配
        /* 本次已到期，理论上下一次到期时间为无限大 */
        rq->responseTime = OS_SCHED_MAX_RESPONSE_TIME;  // 设置响应时间为最大响应时间
    }

    /* 当前线程时间片已耗尽，但当前系统锁定任务无法触发调度释放CPU */
    if ((nextExpireTime > taskEndTime) && ((nextExpireTime - taskEndTime) > OS_SCHED_MINI_PERIOD)) {  // 如果下一个超时时间大于任务结束时间且差值超过最小周期
        nextExpireTime = taskEndTime;  // 更新下一个到期时间为任务结束时间
        isTimeSlice = TRUE;  // 设置时间片到期标志
    }

    if ((rq->responseTime <= nextExpireTime) ||  // 如果当前响应时间小于等于下一个到期时间
        ((rq->responseTime - nextExpireTime) < OS_TICK_RESPONSE_PRECISION)) {  // 或者两者差值小于滴答响应精度
        return;  // 无需更新，直接返回
    }

    if (isTimeSlice) {  // 如果是时间片到期
        /* 当前系统的到期时间为线程的时间片到期时间 */
        rq->responseID = responseID;  // 更新响应ID为当前任务ID
    } else {  // 否则
        rq->responseID = OS_INVALID_VALUE;  // 设置响应ID为无效值
    }

    UINT64 nextResponseTime = nextExpireTime - currTime;  // 计算下一个响应时间间隔
    rq->responseTime = currTime + HalClockTickTimerReload(nextResponseTime);  // 更新响应时间为当前时间加上重载时钟滴答定时器后的时间
}

/**
 * @brief 更新调度到期时间
 * @details 获取当前运行任务的截止时间，并调用 SchedNextExpireTimeSet 更新调度运行队列的响应时间
 */
VOID OsSchedExpireTimeUpdate(VOID)
{
    UINT32 intSave;  // 中断状态保存变量
    if (!OS_SCHEDULER_ACTIVE || OS_INT_ACTIVE) {  // 如果调度器未激活或处于中断上下文中
        OsSchedRunqueuePendingSet();  // 设置调度运行队列 pending 标志
        return;  // 返回
    }

    LosTaskCB *runTask = OsCurrTaskGet();  // 获取当前运行任务
    SCHEDULER_LOCK(intSave);  // 锁定调度器并保存中断状态
    UINT64 deadline = runTask->ops->deadlineGet(runTask);  // 获取任务的截止时间
    SCHEDULER_UNLOCK(intSave);  // 解锁调度器并恢复中断状态
    SchedNextExpireTimeSet(runTask->taskID, deadline, runTask->taskID);  // 设置下一个到期时间
}

/**
 * @brief 唤醒超时任务
 * @details 处理超时任务的状态更新，将超时任务从阻塞状态唤醒并加入就绪队列
 * @param rq 调度运行队列指针
 * @param currTime 当前时间
 * @param taskCB 任务控制块指针
 * @param needSched 是否需要调度的标志指针
 */
STATIC INLINE VOID SchedTimeoutTaskWake(SchedRunqueue *rq, UINT64 currTime, LosTaskCB *taskCB, BOOL *needSched)
{
    LOS_SpinLock(&g_taskSpin);  // 获取任务自旋锁
    if (OsSchedPolicyIsEDF(taskCB)) {  // 如果任务是EDF调度策略
        SchedEDF *sched = (SchedEDF *)&taskCB->sp;  // 获取EDF调度参数
        if (sched->finishTime <= currTime) {  // 如果任务的完成时间小于等于当前时间
            if (taskCB->timeSlice >= 0) {  // 如果时间片有效
                PrintExcInfo("EDF task: %u name: %s is timeout, timeout for %llu us.\n",  // 打印超时信息
                             taskCB->taskID, taskCB->taskName, OS_SYS_CYCLE_TO_US(currTime - sched->finishTime));
            }
            taskCB->timeSlice = 0;  // 将时间片设置为0
        }
        if (sched->flags == EDF_WAIT_FOREVER) {  // 如果标志为无限等待
            taskCB->taskStatus &= ~OS_TASK_STATUS_PEND_TIME;  // 清除任务的时间等待状态
            sched->flags = EDF_UNUSED;  // 重置EDF标志为未使用
        }
    }

    UINT16 tempStatus = taskCB->taskStatus;  // 保存当前任务状态
    if (tempStatus & (OS_TASK_STATUS_PEND_TIME | OS_TASK_STATUS_DELAY)) {  // 如果任务处于时间等待或延迟状态
        taskCB->taskStatus &= ~(OS_TASK_STATUS_PENDING | OS_TASK_STATUS_PEND_TIME | OS_TASK_STATUS_DELAY);  // 清除相关状态位
        if (tempStatus & OS_TASK_STATUS_PEND_TIME) {  // 如果任务处于时间等待状态
            taskCB->taskStatus |= OS_TASK_STATUS_TIMEOUT;  // 设置任务超时状态
            LOS_ListDelete(&taskCB->pendList);  // 从等待列表中删除任务
            taskCB->taskMux = NULL;  // 重置任务互斥锁
            OsTaskWakeClearPendMask(taskCB);  // 清除任务的等待掩码
        }

        if (!(tempStatus & OS_TASK_STATUS_SUSPENDED)) {  // 如果任务未被挂起
#ifdef LOSCFG_SCHED_HPF_DEBUG
            taskCB->schedStat.pendTime += currTime - taskCB->startTime;  // 更新等待时间统计
            taskCB->schedStat.pendCount++;  // 增加等待次数统计
#endif
            taskCB->ops->enqueue(rq, taskCB);  // 将任务加入就绪队列
            *needSched = TRUE;  // 设置需要调度标志
        }
    }

    LOS_SpinUnlock(&g_taskSpin);  // 释放任务自旋锁
}

/**
 * @brief 扫描超时队列
 * @details 扫描调度运行队列的超时队列，唤醒所有已超时的任务
 * @param rq 调度运行队列指针
 * @return 是否需要调度，TRUE表示需要，FALSE表示不需要
 */
STATIC INLINE BOOL SchedTimeoutQueueScan(SchedRunqueue *rq)
{
    BOOL needSched = FALSE;  // 是否需要调度标志
    SortLinkAttribute *timeoutQueue = &rq->timeoutQueue;  // 获取超时队列
    LOS_DL_LIST *listObject = &timeoutQueue->sortLink;  // 获取超时队列的双向链表
    /*
     * 当任务因超时而阻塞时，任务块同时位于每个CPU的超时排序链表和IPC（互斥锁、信号量等）的阻塞链表上，
     * 可以通过超时或对应的IPC唤醒
     *
     * 现在使用同步排序链表过程，因此整个任务扫描需要保护，防止另一个核心同时进行排序链表删除操作
     */
    LOS_SpinLock(&timeoutQueue->spinLock);  // 获取超时队列的自旋锁

    if (LOS_ListEmpty(listObject)) {  // 如果超时队列为空
        LOS_SpinUnlock(&timeoutQueue->spinLock);  // 释放自旋锁
        return needSched;  // 返回不需要调度
    }

    SortLinkList *sortList = LOS_DL_LIST_ENTRY(listObject->pstNext, SortLinkList, sortLinkNode);  // 获取第一个排序链表节点
    UINT64 currTime = OsGetCurrSchedTimeCycle();  // 获取当前调度时间周期
    while (sortList->responseTime <= currTime) {  // 当排序链表节点的响应时间小于等于当前时间
        LosTaskCB *taskCB = LOS_DL_LIST_ENTRY(sortList, LosTaskCB, sortList);  // 获取任务控制块
        OsDeleteNodeSortLink(timeoutQueue, &taskCB->sortList);  // 从超时队列中删除节点
        LOS_SpinUnlock(&timeoutQueue->spinLock);  // 释放自旋锁

        SchedTimeoutTaskWake(rq, currTime, taskCB, &needSched);  // 唤醒超时任务

        LOS_SpinLock(&timeoutQueue->spinLock);  // 重新获取自旋锁
        if (LOS_ListEmpty(listObject)) {  // 如果超时队列为空
            break;  // 跳出循环
        }

        sortList = LOS_DL_LIST_ENTRY(listObject->pstNext, SortLinkList, sortLinkNode);  // 获取下一个排序链表节点
    }

    LOS_SpinUnlock(&timeoutQueue->spinLock);  // 释放自旋锁

    return needSched;  // 返回是否需要调度
}

/**
 * @brief 调度滴答处理函数
 * @details 处理调度滴答事件，扫描超时队列并更新调度标志
 */
VOID OsSchedTick(VOID)
{
    SchedRunqueue *rq = OsSchedRunqueue();  // 获取当前CPU的调度运行队列

    if (rq->responseID == OS_INVALID_VALUE) {  // 如果响应ID无效
        if (SchedTimeoutQueueScan(rq)) {  // 扫描超时队列，如果有超时任务
            LOS_MpSchedule(OS_MP_CPU_ALL);  // 多处理器调度所有CPU
            rq->schedFlag |= INT_PEND_RESCH;  // 设置调度标志为需要重新调度
        }
    }
    rq->schedFlag |= INT_PEND_TICK;  // 设置调度标志为滴答 pending
    rq->responseTime = OS_SCHED_MAX_RESPONSE_TIME;  // 设置响应时间为最大响应时间
}

/**
 * @brief 重置调度响应时间
 * @details 设置调度运行队列的响应时间为指定值
 * @param responseTime 新的响应时间
 */
VOID OsSchedResponseTimeReset(UINT64 responseTime)
{
    OsSchedRunqueue()->responseTime = responseTime;  // 设置当前调度运行队列的响应时间
}

/**
 * @brief 初始化调度运行队列
 * @details 初始化所有CPU核心的调度运行队列，包括超时队列和响应时间
 */
VOID OsSchedRunqueueInit(VOID)
{
    if (ArchCurrCpuid() != 0) {  // 如果当前CPU不是0号核心
        return;  // 仅在0号核心执行初始化
    }

    for (UINT16 index = 0; index < LOSCFG_KERNEL_CORE_NUM; index++) {  // 遍历所有CPU核心
        SchedRunqueue *rq = OsSchedRunqueueByID(index);  // 获取指定CPU的调度运行队列
        OsSortLinkInit(&rq->timeoutQueue);  // 初始化超时队列
        rq->responseTime = OS_SCHED_MAX_RESPONSE_TIME;  // 设置响应时间为最大响应时间
    }
}

/**
 * @brief 初始化调度运行队列的空闲任务
 * @details 设置调度运行队列的空闲任务指针
 * @param idleTask 空闲任务控制块指针
 */
VOID OsSchedRunqueueIdleInit(LosTaskCB *idleTask)
{
    SchedRunqueue *rq = OsSchedRunqueue();  // 获取当前CPU的调度运行队列
    rq->idleTask = idleTask;  // 设置空闲任务
}

/**
 * @brief 初始化调度器
 * @details 初始化所有CPU核心的调度策略（EDF和HPF），并在调试模式下初始化调度调试
 * @return 操作结果，LOS_OK表示成功，其他值表示失败
 */
UINT32 OsSchedInit(VOID)
{
    for (UINT16 cpuid = 0; cpuid < LOSCFG_KERNEL_CORE_NUM; cpuid++) {  // 遍历所有CPU核心
        SchedRunqueue *rq = OsSchedRunqueueByID(cpuid);  // 获取指定CPU的调度运行队列
        EDFSchedPolicyInit(rq);  // 初始化EDF调度策略
        HPFSchedPolicyInit(rq);  // 初始化HPF调度策略
    }

#ifdef LOSCFG_SCHED_TICK_DEBUG
    UINT32 ret = OsSchedDebugInit();  // 初始化调度调试
    if (ret != LOS_OK) {  // 如果初始化失败
        return ret;  // 返回错误码
    }
#endif
    return LOS_OK;  // 返回成功
}

/**
 * @brief 比较两个任务的调度参数
 * @details 根据任务的调度策略和参数比较任务优先级，返回值大于0表示task1优先级低于task2，小于0表示task1优先级高于task2，等于0表示优先级相同
 * @param task1 第一个任务控制块指针
 * @param task2 第二个任务控制块指针
 * @return 比较结果
 */
INT32 OsSchedParamCompare(const LosTaskCB *task1, const LosTaskCB *task2)
{
    SchedHPF *rp1 = (SchedHPF *)&task1->sp;  // 获取第一个任务的HPF调度参数
    SchedHPF *rp2 = (SchedHPF *)&task2->sp;  // 获取第二个任务的HPF调度参数

    if (rp1->policy == rp2->policy) {  // 如果调度策略相同
        return task1->ops->schedParamCompare(&task1->sp, &task2->sp);  // 调用调度参数比较函数
    }

    if (rp1->policy == LOS_SCHED_IDLE) {  // 如果第一个任务是空闲策略
        return 1;  // 返回1表示优先级低
    } else if (rp2->policy == LOS_SCHED_IDLE) {  // 如果第二个任务是空闲策略
        return -1;  // 返回-1表示第一个任务优先级高
    }
    return 0;  // 其他情况返回0
}

/**
 * @brief 初始化任务的调度参数
 * @details 根据指定的调度策略初始化任务的调度参数
 * @param taskCB 任务控制块指针
 * @param policy 调度策略
 * @param parentParam 父任务的调度参数
 * @param param 任务的调度参数
 * @return 操作结果，LOS_OK表示成功，LOS_NOK表示失败
 */
UINT32 OsSchedParamInit(LosTaskCB *taskCB, UINT16 policy, const SchedParam *parentParam, const LosSchedParam *param)
{
    switch (policy) {  // 根据调度策略选择初始化函数
        case LOS_SCHED_FIFO:  // FIFO调度策略
        case LOS_SCHED_RR:  // RR调度策略
            HPFTaskSchedParamInit(taskCB, policy, parentParam, param);  // 初始化HPF调度参数
            break;
        case LOS_SCHED_DEADLINE:  // 截止时间调度策略
            return EDFTaskSchedParamInit(taskCB, policy, parentParam, param);  // 初始化EDF调度参数并返回结果
        case LOS_SCHED_IDLE:  // 空闲调度策略
            IdleTaskSchedParamInit(taskCB);  // 初始化空闲任务调度参数
            break;
        default:  // 无效调度策略
            return LOS_NOK;  // 返回失败
    }

    return LOS_OK;  // 返回成功
}

/**
 * @brief 获取进程的默认调度参数
 * @details 根据指定的调度策略获取默认的进程调度参数
 * @param policy 调度策略
 * @param param 调度参数指针，用于存储获取到的默认参数
 */
VOID OsSchedProcessDefaultSchedParamGet(UINT16 policy, SchedParam *param)
{
    switch (policy) {  // 根据调度策略选择获取默认参数的函数
        case LOS_SCHED_FIFO:  // FIFO调度策略
        case LOS_SCHED_RR:  // RR调度策略
            HPFProcessDefaultSchedParamGet(param);  // 获取HPF默认调度参数
            break;
        case LOS_SCHED_DEADLINE:  // 截止时间调度策略
            EDFProcessDefaultSchedParamGet(param);  // 获取EDF默认调度参数
            break;
        case LOS_SCHED_IDLE:  // 空闲调度策略
        default:  // 无效调度策略
            PRINT_ERR("Invalid process-level scheduling policy, %u\n", policy);  // 打印错误信息
            break;
    }
    return;  // 返回
}

/**
 * @brief 获取最高优先级任务
 * @details 从EDF和HPF就绪队列中获取最高优先级的任务，如果没有则返回空闲任务
 * @param rq 调度运行队列指针
 * @return 最高优先级任务控制块指针
 */
STATIC LosTaskCB *TopTaskGet(SchedRunqueue *rq)
{
    LosTaskCB *newTask = EDFRunqueueTopTaskGet(rq->edfRunqueue);  // 从EDF就绪队列获取最高优先级任务
    if (newTask != NULL) {  // 如果EDF队列有任务
        goto FIND;  // 跳转到FIND标签
    }

    newTask = HPFRunqueueTopTaskGet(rq->hpfRunqueue);  // 从HPF就绪队列获取最高优先级任务
    if (newTask != NULL) {  // 如果HPF队列有任务
        goto FIND;  // 跳转到FIND标签
    }

    newTask = rq->idleTask;  // 如果没有就绪任务，使用空闲任务

FIND:
    newTask->ops->start(rq, newTask);  // 调用任务的开始调度函数
    return newTask;  // 返回最高优先级任务
}

/**
 * @brief 启动调度器
 * @details 初始化调度器并启动第一个任务
 */
VOID OsSchedStart(VOID)
{
    UINT32 cpuid = ArchCurrCpuid();  // 获取当前CPU ID
    UINT32 intSave;  // 中断状态保存变量

    PRINTK("cpu %d entering scheduler\n", cpuid);  // 打印CPU进入调度器信息

    SCHEDULER_LOCK(intSave);  // 锁定调度器并保存中断状态

    OsTickStart();  // 启动系统滴答定时器

    SchedRunqueue *rq = OsSchedRunqueue();  // 获取当前CPU的调度运行队列
    LosTaskCB *newTask = TopTaskGet(rq);  // 获取最高优先级任务
    newTask->taskStatus |= OS_TASK_STATUS_RUNNING;  // 设置任务状态为运行中

#ifdef LOSCFG_KERNEL_SMP
    /*
     * 注意：需要设置当前CPU，以防第一个任务删除时因该标志与实际当前CPU不匹配而失败
     */
    newTask->currCpu = cpuid;  // 设置任务当前运行的CPU
#endif

    OsCurrTaskSet((VOID *)newTask);  // 设置当前运行任务

    newTask->startTime = OsGetCurrSchedTimeCycle();  // 设置任务开始时间

    OsSwtmrResponseTimeReset(newTask->startTime);  // 重置软件定时器响应时间

    /* 系统开始调度 */
    OS_SCHEDULER_SET(cpuid);  // 设置调度器激活标志

    rq->responseID = OS_INVALID;  // 设置响应ID为无效
    UINT64 deadline = newTask->ops->deadlineGet(newTask);  // 获取任务截止时间
    SchedNextExpireTimeSet(newTask->taskID, deadline, OS_INVALID);  // 设置下一个到期时间
    OsTaskContextLoad(newTask);  // 加载任务上下文
}

#ifdef LOSCFG_KERNEL_SMP
/**
 * @brief 调度到用户态前释放锁
 * @details 在返回用户态前释放调度锁和任务自旋锁
 */
VOID OsSchedToUserReleaseLock(VOID)
{
    /* 返回用户态前需要释放调度锁 */
    LOCKDEP_CHECK_OUT(&g_taskSpin);  // 检查锁依赖并退出
    ArchSpinUnlock(&g_taskSpin.rawLock);  // 释放任务自旋锁

    OsSchedUnlock();  // 解锁调度器
}
#endif

#ifdef LOSCFG_BASE_CORE_TSK_MONITOR
/**
 * @brief 任务栈检查
 * @details 检查当前运行任务和新任务的栈是否溢出或栈指针是否有效
 * @param runTask 当前运行任务控制块指针
 * @param newTask 新任务控制块指针
 */
STATIC VOID TaskStackCheck(LosTaskCB *runTask, LosTaskCB *newTask)
{
    if (!OS_STACK_MAGIC_CHECK(runTask->topOfStack)) {  // 检查当前任务栈魔术字
        LOS_Panic("CURRENT task ID: %s:%d stack overflow!\n", runTask->taskName, runTask->taskID);  // 栈溢出，触发Panic
    }

    if (((UINTPTR)(newTask->stackPointer) <= newTask->topOfStack) ||  // 检查新任务栈指针是否小于等于栈顶
        ((UINTPTR)(newTask->stackPointer) > (newTask->topOfStack + newTask->stackSize))) {  // 或者大于栈底
        LOS_Panic("HIGHEST task ID: %s:%u SP error! StackPointer: %p TopOfStack: %p\n",  // 栈指针错误，触发Panic
                  newTask->taskName, newTask->taskID, newTask->stackPointer, newTask->topOfStack);
    }
}
#endif

/**
 * @brief 调度切换检查
 * @details 在任务切换前进行栈检查和钩子函数调用
 * @param runTask 当前运行任务控制块指针
 * @param newTask 新任务控制块指针
 */
STATIC INLINE VOID SchedSwitchCheck(LosTaskCB *runTask, LosTaskCB *newTask)
{
#ifdef LOSCFG_BASE_CORE_TSK_MONITOR
    TaskStackCheck(runTask, newTask);  // 检查任务栈
#endif /* LOSCFG_BASE_CORE_TSK_MONITOR */
    OsHookCall(LOS_HOOK_TYPE_TASK_SWITCHEDIN, newTask, runTask);  // 调用任务切换钩子函数
}

/**
 * @brief 调度任务切换
 * @details 执行任务切换操作，包括状态更新、上下文切换等
 * @param rq 调度运行队列指针
 * @param runTask 当前运行任务控制块指针
 * @param newTask 新任务控制块指针
 */
STATIC VOID SchedTaskSwitch(SchedRunqueue *rq, LosTaskCB *runTask, LosTaskCB *newTask)
{
    SchedSwitchCheck(runTask, newTask);  // 执行切换前检查

    runTask->taskStatus &= ~OS_TASK_STATUS_RUNNING;  // 清除当前任务的运行状态
    newTask->taskStatus |= OS_TASK_STATUS_RUNNING;  // 设置新任务的运行状态

#ifdef LOSCFG_KERNEL_SMP
    /* 标记新运行任务的所属处理器 */
    runTask->currCpu = OS_TASK_INVALID_CPUID;  // 重置当前任务的CPU ID
    newTask->currCpu = ArchCurrCpuid();  // 设置新任务的CPU ID
#endif

    OsCurrTaskSet((VOID *)newTask);  // 更新当前运行任务
#ifdef LOSCFG_KERNEL_VM
    if (newTask->archMmu != runTask->archMmu) {  // 如果新任务和当前任务的MMU上下文不同
        LOS_ArchMmuContextSwitch((LosArchMmu *)newTask->archMmu);  // 切换MMU上下文
    }
#endif

#ifdef LOSCFG_KERNEL_CPUP
    OsCpupCycleEndStart(runTask, newTask);  // 更新CPU性能计数
#endif

#ifdef LOSCFG_SCHED_HPF_DEBUG
    UINT64 waitStartTime = newTask->startTime;  // 保存新任务的开始等待时间
#endif
    if (runTask->taskStatus & OS_TASK_STATUS_READY) {  // 如果当前任务处于就绪状态
        /* 当线程进入就绪队列时，更新其时间片 */
        newTask->startTime = runTask->startTime;  // 设置新任务的开始时间为当前任务的开始时间
    } else {  // 如果当前任务处于阻塞状态
        newTask->startTime = OsGetCurrSchedTimeCycle();  // 设置新任务的开始时间为当前调度时间
        /* 任务处于阻塞状态，需要在阻塞前更新其时间片 */
        runTask->ops->timeSliceUpdate(rq, runTask, newTask->startTime);  // 更新当前任务的时间片

        if (runTask->taskStatus & (OS_TASK_STATUS_PEND_TIME | OS_TASK_STATUS_DELAY)) {  // 如果当前任务处于时间等待或延迟状态
            OsSchedTimeoutQueueAdd(runTask, runTask->ops->waitTimeGet(runTask));  // 将任务加入超时队列
        }
    }

    UINT64 deadline = newTask->ops->deadlineGet(newTask);  // 获取新任务的截止时间
    SchedNextExpireTimeSet(newTask->taskID, deadline, runTask->taskID);  // 设置下一个到期时间

#ifdef LOSCFG_SCHED_HPF_DEBUG
    newTask->schedStat.waitSchedTime += newTask->startTime - waitStartTime;  // 更新新任务的等待调度时间
    newTask->schedStat.waitSchedCount++;  // 增加新任务的等待调度次数
    runTask->schedStat.runTime = runTask->schedStat.allRuntime;  // 更新当前任务的运行时间
    runTask->schedStat.switchCount++;  // 增加当前任务的切换次数
#endif
    /* 执行任务上下文切换 */
    OsTaskSchedule(newTask, runTask);  // 切换到新任务的上下文
}
/**
 * @brief 中断结束时检查是否需要调度
 * @details 在中断处理结束时更新当前任务时间片，并根据调度标志决定是否触发任务切换
 */
VOID OsSchedIrqEndCheckNeedSched(VOID)
{
    SchedRunqueue *rq = OsSchedRunqueue();  // 获取当前CPU的调度运行队列
    LosTaskCB *runTask = OsCurrTaskGet();  // 获取当前运行任务

    runTask->ops->timeSliceUpdate(rq, runTask, OsGetCurrSchedTimeCycle());  // 更新当前任务的时间片

    if (OsPreemptable() && (rq->schedFlag & INT_PEND_RESCH)) {  // 如果系统可抢占且需要重新调度
        rq->schedFlag &= ~INT_PEND_RESCH;  // 清除重新调度标志

        LOS_SpinLock(&g_taskSpin);  // 获取任务自旋锁

        runTask->ops->enqueue(rq, runTask);  // 将当前任务重新加入就绪队列

        LosTaskCB *newTask = TopTaskGet(rq);  // 获取最高优先级任务
        if (runTask != newTask) {  // 如果最高优先级任务不是当前任务
            SchedTaskSwitch(rq, runTask, newTask);  // 执行任务切换
            LOS_SpinUnlock(&g_taskSpin);  // 释放任务自旋锁
            return;  // 返回
        }

        LOS_SpinUnlock(&g_taskSpin);  // 释放任务自旋锁
    }

    if (rq->schedFlag & INT_PEND_TICK) {  // 如果存在滴答 pending 标志
        OsSchedExpireTimeUpdate();  // 更新调度到期时间
    }
}

/**
 * @brief 执行重新调度
 * @details 从就绪队列中选择最高优先级任务，如果与当前任务不同则执行切换
 */
VOID OsSchedResched(VOID)
{
    LOS_ASSERT(LOS_SpinHeld(&g_taskSpin));  // 断言任务自旋锁已被持有
    SchedRunqueue *rq = OsSchedRunqueue();  // 获取当前CPU的调度运行队列
#ifdef LOSCFG_KERNEL_SMP
    LOS_ASSERT(rq->taskLockCnt == 1);  // SMP模式下断言任务锁计数为1
#else
    LOS_ASSERT(rq->taskLockCnt == 0);  // 非SMP模式下断言任务锁计数为0
#endif

    rq->schedFlag &= ~INT_PEND_RESCH;  // 清除重新调度标志
    LosTaskCB *runTask = OsCurrTaskGet();  // 获取当前运行任务
    LosTaskCB *newTask = TopTaskGet(rq);  // 获取最高优先级任务
    if (runTask == newTask) {  // 如果最高优先级任务是当前任务
        return;  // 无需切换，直接返回
    }

    SchedTaskSwitch(rq, runTask, newTask);  // 执行任务切换
}

/**
 * @brief 触发任务调度
 * @details 检查调度条件，更新当前任务时间片并将其重新加入就绪队列，然后执行重新调度
 */
VOID LOS_Schedule(VOID)
{
    UINT32 intSave;  // 中断状态保存变量
    LosTaskCB *runTask = OsCurrTaskGet();  // 获取当前运行任务
    SchedRunqueue *rq = OsSchedRunqueue();  // 获取当前CPU的调度运行队列

    if (OS_INT_ACTIVE) {  // 如果处于中断上下文中
        OsSchedRunqueuePendingSet();  // 设置调度运行队列pending标志
        return;  // 返回
    }

    if (!OsPreemptable()) {  // 如果系统不可抢占
        return;  // 返回
    }

    /*
     * 任务中触发调度也会进行时间片检查
     * 如有必要，会更及时地放弃时间片
     * 否则无其他副作用
     */
    SCHEDULER_LOCK(intSave);  // 锁定调度器并保存中断状态

    runTask->ops->timeSliceUpdate(rq, runTask, OsGetCurrSchedTimeCycle());  // 更新当前任务的时间片

    /* 将运行任务重新加入就绪队列 */
    runTask->ops->enqueue(rq, runTask);  // 将当前任务加入就绪队列

    /* 重新调度到新线程 */
    OsSchedResched();  // 执行重新调度

    SCHEDULER_UNLOCK(intSave);  // 解锁调度器并恢复中断状态
}

/**
 * @brief 查找锁等待队列中的插入位置（内部辅助函数）
 * @details 遍历等待队列，根据任务优先级比较结果确定新任务的插入位置
 * @param runTask 当前运行任务控制块指针
 * @param lockList 锁等待队列头指针
 * @return 插入位置的链表节点指针，NULL表示插入队尾
 */
STATIC INLINE LOS_DL_LIST *SchedLockPendFindPosSub(const LosTaskCB *runTask, const LOS_DL_LIST *lockList)
{
    LosTaskCB *pendedTask = NULL;  // 等待队列中的任务控制块指针

    LOS_DL_LIST_FOR_EACH_ENTRY(pendedTask, lockList, LosTaskCB, pendList) {  // 遍历等待队列
        INT32 ret = OsSchedParamCompare(pendedTask, runTask);  // 比较任务优先级
        if (ret < 0) {  // 如果等待任务优先级高于当前任务
            continue;  // 继续查找下一个任务
        } else if (ret > 0) {  // 如果等待任务优先级低于当前任务
            return &pendedTask->pendList;  // 返回当前等待任务前的插入位置
        } else {  // 如果优先级相同
            return pendedTask->pendList.pstNext;  // 返回当前等待任务后的插入位置
        }
    }
    return NULL;  // 遍历完成，返回NULL表示插入队尾
}

/**
 * @brief 查找锁等待队列中的插入位置
 * @details 根据任务优先级在等待队列中找到合适的插入位置，确保队列按优先级排序
 * @param runTask 当前运行任务控制块指针
 * @param lockList 锁等待队列头指针
 * @return 插入位置的链表节点指针
 */
LOS_DL_LIST *OsSchedLockPendFindPos(const LosTaskCB *runTask, LOS_DL_LIST *lockList)
{
    if (LOS_ListEmpty(lockList)) {  // 如果等待队列为空
        return lockList;  // 返回队列头作为插入位置
    }

    LosTaskCB *pendedTask1 = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(lockList));  // 获取队列第一个任务
    INT32 ret = OsSchedParamCompare(pendedTask1, runTask);  // 比较与第一个任务的优先级
    if (ret > 0) {  // 如果第一个任务优先级低于当前任务
        return lockList->pstNext;  // 返回队列头之后的位置（队首）
    }

    LosTaskCB *pendedTask2 = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_LAST(lockList));  // 获取队列最后一个任务
    ret = OsSchedParamCompare(pendedTask2, runTask);  // 比较与最后一个任务的优先级
    if (ret <= 0) {  // 如果最后一个任务优先级高于或等于当前任务
        return lockList;  // 返回队列头作为插入位置（队尾）
    }

    return SchedLockPendFindPosSub(runTask, lockList);  // 调用辅助函数查找中间插入位置
}