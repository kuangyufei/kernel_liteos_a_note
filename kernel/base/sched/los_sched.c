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

SchedRunqueue g_schedRunqueue[LOSCFG_KERNEL_CORE_NUM];
/**
 * 设置调度器的下一个到期时间
 * 
 * @param responseID   当前任务的ID
 * @param taskEndTime  当前任务的时间片结束时间
 * @param oldResponseID 上一个任务的ID
 */
STATIC INLINE VOID SchedNextExpireTimeSet(UINT32 responseID, UINT64 taskEndTime, UINT32 oldResponseID)
{
    SchedRunqueue *rq = OsSchedRunqueue();  // 获取当前CPU的运行队列
    BOOL isTimeSlice = FALSE;              // 是否是时间片到期
    UINT64 currTime = OsGetCurrSchedTimeCycle();  // 获取当前调度时间
    UINT64 nextExpireTime = OsGetSortLinkNextExpireTime(&rq->timeoutQueue, currTime, OS_TICK_RESPONSE_PRECISION);  // 获取下一个超时时间

    rq->schedFlag &= ~INT_PEND_TICK;  // 清除tick中断挂起标志

    /* 如果当前响应ID与旧ID相同，说明该时间已过期，将响应时间设置为最大值 */
    if (rq->responseID == oldResponseID) {
        rq->responseTime = OS_SCHED_MAX_RESPONSE_TIME;
    }

    /* 如果下一个超时时间大于任务结束时间，并且差值大于最小周期，则使用任务结束时间 */
    if ((nextExpireTime > taskEndTime) && ((nextExpireTime - taskEndTime) > OS_SCHED_MINI_PERIOD)) {
        nextExpireTime = taskEndTime;
        isTimeSlice = TRUE;  // 标记为时间片到期
    }

    /* 如果当前响应时间小于等于下一个超时时间，或者差值小于响应精度，则直接返回 */
    if ((rq->responseTime <= nextExpireTime) ||
        ((rq->responseTime - nextExpireTime) < OS_TICK_RESPONSE_PRECISION)) {
        return;
    }

    /* 如果是时间片到期，设置响应ID为当前任务ID，否则设置为无效值 */
    if (isTimeSlice) {
        rq->responseID = responseID;
    } else {
        rq->responseID = OS_INVALID_VALUE;
    }

    /* 计算并设置下一个响应时间 */
    UINT64 nextResponseTime = nextExpireTime - currTime;
    rq->responseTime = currTime + HalClockTickTimerReload(nextResponseTime);
}
/**
 * 更新调度器的到期时间
 * 
 * 该函数用于在任务时间片到期或任务超时时，更新调度器的下一个到期时间。
 * 如果调度器未激活或处于中断上下文中，则设置挂起标志并返回。
 */
VOID OsSchedExpireTimeUpdate(VOID)
{
    UINT32 intSave;
    
    /* 检查调度器是否激活或是否处于中断上下文 */
    if (!OS_SCHEDULER_ACTIVE || OS_INT_ACTIVE) {
        OsSchedRunqueuePendingSet();  // 设置调度器挂起标志
        return;
    }

    /* 获取当前运行的任务控制块 */
    LosTaskCB *runTask = OsCurrTaskGet();
    
    /* 加锁获取任务的截止时间 */
    SCHEDULER_LOCK(intSave);
    UINT64 deadline = runTask->ops->deadlineGet(runTask);
    SCHEDULER_UNLOCK(intSave);
    
    /* 设置下一个到期时间 */
    SchedNextExpireTimeSet(runTask->taskID, deadline, runTask->taskID);
}

/**
 * 唤醒超时任务
 * 
 * @param rq       运行队列指针
 * @param currTime 当前时间
 * @param taskCB   任务控制块指针
 * @param needSched 是否需要调度的标志位
 */
STATIC INLINE VOID SchedTimeoutTaskWake(SchedRunqueue *rq, UINT64 currTime, LosTaskCB *taskCB, BOOL *needSched)
{
    LOS_SpinLock(&g_taskSpin);  // 加锁保护任务状态
    
    /* 处理EDF调度策略的任务 */
    if (OsSchedPolicyIsEDF(taskCB)) {
        SchedEDF *sched = (SchedEDF *)&taskCB->sp;
        
        /* 如果任务完成时间已到 */
        if (sched->finishTime <= currTime) {
            if (taskCB->timeSlice >= 0) {
                PrintExcInfo("EDF task: %u name: %s is timeout, timeout for %llu us.\n",
                             taskCB->taskID, taskCB->taskName, OS_SYS_CYCLE_TO_US(currTime - sched->finishTime));
            }
            taskCB->timeSlice = 0;  // 重置时间片
        }
        
        /* 处理永久等待标志 */
        if (sched->flags == EDF_WAIT_FOREVER) {
            taskCB->taskStatus &= ~OS_TASK_STATUS_PEND_TIME;  // 清除等待时间标志
            sched->flags = EDF_UNUSED;  // 设置标志为未使用
        }
    }

    /* 处理任务状态 */
    UINT16 tempStatus = taskCB->taskStatus;
    if (tempStatus & (OS_TASK_STATUS_PEND_TIME | OS_TASK_STATUS_DELAY)) {
        /* 清除相关状态标志 */
        taskCB->taskStatus &= ~(OS_TASK_STATUS_PENDING | OS_TASK_STATUS_PEND_TIME | OS_TASK_STATUS_DELAY);
        
        /* 处理超时状态 */
        if (tempStatus & OS_TASK_STATUS_PEND_TIME) {
            taskCB->taskStatus |= OS_TASK_STATUS_TIMEOUT;  // 设置超时标志
            LOS_ListDelete(&taskCB->pendList);  // 从挂起链表中删除
            taskCB->taskMux = NULL;  // 清除互斥锁指针
            OsTaskWakeClearPendMask(taskCB);  // 清除挂起掩码
        }

        /* 如果任务未被挂起，则将其加入就绪队列 */
        if (!(tempStatus & OS_TASK_STATUS_SUSPENDED)) {
#ifdef LOSCFG_SCHED_HPF_DEBUG
            /* 调试信息：更新挂起时间和计数 */
            taskCB->schedStat.pendTime += currTime - taskCB->startTime;
            taskCB->schedStat.pendCount++;
#endif
            taskCB->ops->enqueue(rq, taskCB);  // 将任务加入就绪队列
            *needSched = TRUE;  // 设置需要调度标志
        }
    }

    LOS_SpinUnlock(&g_taskSpin);  // 解锁
}

/**
 * 扫描超时队列并唤醒超时任务
 * 
 * @param rq 运行队列指针
 * @return 是否需要调度的标志（TRUE需要调度，FALSE不需要）
 */
STATIC INLINE BOOL SchedTimeoutQueueScan(SchedRunqueue *rq)
{
    BOOL needSched = FALSE;  // 是否需要调度的标志
    SortLinkAttribute *timeoutQueue = &rq->timeoutQueue;  // 超时队列
    LOS_DL_LIST *listObject = &timeoutQueue->sortLink;  // 排序链表

    /* 加锁保护超时队列，防止多核同时操作 */
    LOS_SpinLock(&timeoutQueue->spinLock);

    /* 如果超时队列为空，直接返回 */
    if (LOS_ListEmpty(listObject)) {
        LOS_SpinUnlock(&timeoutQueue->spinLock);
        return needSched;
    }

    /* 获取队列中的第一个任务 */
    SortLinkList *sortList = LOS_DL_LIST_ENTRY(listObject->pstNext, SortLinkList, sortLinkNode);
    UINT64 currTime = OsGetCurrSchedTimeCycle();  // 获取当前时间

    /* 遍历超时队列，处理所有已超时的任务 */
    while (sortList->responseTime <= currTime) {
        /* 获取任务控制块 */
        LosTaskCB *taskCB = LOS_DL_LIST_ENTRY(sortList, LosTaskCB, sortList);
        
        /* 从超时队列中删除该任务 */
        OsDeleteNodeSortLink(timeoutQueue, &taskCB->sortList);
        LOS_SpinUnlock(&timeoutQueue->spinLock);

        /* 唤醒超时任务 */
        SchedTimeoutTaskWake(rq, currTime, taskCB, &needSched);

        /* 重新加锁，继续处理下一个任务 */
        LOS_SpinLock(&timeoutQueue->spinLock);
        
        /* 如果队列为空，则退出循环 */
        if (LOS_ListEmpty(listObject)) {
            break;
        }

        /* 获取下一个任务 */
        sortList = LOS_DL_LIST_ENTRY(listObject->pstNext, SortLinkList, sortLinkNode);
    }

    /* 解锁超时队列 */
    LOS_SpinUnlock(&timeoutQueue->spinLock);

    return needSched;  // 返回是否需要调度的标志
}

/**
 * 处理调度器的tick中断
 * 
 * 该函数在每个tick中断时被调用，用于检查超时任务并触发调度。
 */
VOID OsSchedTick(VOID)
{
    SchedRunqueue *rq = OsSchedRunqueue();  // 获取当前CPU的运行队列

    /* 如果当前响应ID为无效值，扫描超时队列 */
    if (rq->responseID == OS_INVALID_VALUE) {
        /* 如果扫描到超时任务并需要调度，则触发多核调度 */
        if (SchedTimeoutQueueScan(rq)) {
            LOS_MpSchedule(OS_MP_CPU_ALL);  // 触发多核调度
            rq->schedFlag |= INT_PEND_RESCH;  // 设置重新调度标志
        }
    }

    /* 设置tick中断挂起标志 */
    rq->schedFlag |= INT_PEND_TICK;
    
    /* 重置响应时间为最大值 */
    rq->responseTime = OS_SCHED_MAX_RESPONSE_TIME;
}

VOID OsSchedResponseTimeReset(UINT64 responseTime)
{
    OsSchedRunqueue()->responseTime = responseTime;
}

VOID OsSchedRunqueueInit(VOID)
{
    if (ArchCurrCpuid() != 0) {
        return;
    }

    for (UINT16 index = 0; index < LOSCFG_KERNEL_CORE_NUM; index++) {
        SchedRunqueue *rq = OsSchedRunqueueByID(index);
        OsSortLinkInit(&rq->timeoutQueue);
        rq->responseTime = OS_SCHED_MAX_RESPONSE_TIME;
    }
}

VOID OsSchedRunqueueIdleInit(LosTaskCB *idleTask)
{
    SchedRunqueue *rq = OsSchedRunqueue();
    rq->idleTask = idleTask;
}

UINT32 OsSchedInit(VOID)
{
    for (UINT16 cpuid = 0; cpuid < LOSCFG_KERNEL_CORE_NUM; cpuid++) {
        SchedRunqueue *rq = OsSchedRunqueueByID(cpuid);
        EDFSchedPolicyInit(rq);
        HPFSchedPolicyInit(rq);
    }

#ifdef LOSCFG_SCHED_TICK_DEBUG
    UINT32 ret = OsSchedDebugInit();
    if (ret != LOS_OK) {
        return ret;
    }
#endif
    return LOS_OK;
}

/*
 * If the return value greater than 0,  task1 has a lower priority than task2.
 * If the return value less than 0, task1 has a higher priority than task2.
 * If the return value is 0, task1 and task2 have the same priority.
 */
INT32 OsSchedParamCompare(const LosTaskCB *task1, const LosTaskCB *task2)
{
    SchedHPF *rp1 = (SchedHPF *)&task1->sp;
    SchedHPF *rp2 = (SchedHPF *)&task2->sp;

    if (rp1->policy == rp2->policy) {
        return task1->ops->schedParamCompare(&task1->sp, &task2->sp);
    }

    if (rp1->policy == LOS_SCHED_IDLE) {
        return 1;
    } else if (rp2->policy == LOS_SCHED_IDLE) {
        return -1;
    }
    return 0;
}

UINT32 OsSchedParamInit(LosTaskCB *taskCB, UINT16 policy, const SchedParam *parentParam, const LosSchedParam *param)
{
    switch (policy) {
        case LOS_SCHED_FIFO:
        case LOS_SCHED_RR:
            HPFTaskSchedParamInit(taskCB, policy, parentParam, param);
            break;
        case LOS_SCHED_DEADLINE:
            return EDFTaskSchedParamInit(taskCB, policy, parentParam, param);
        case LOS_SCHED_IDLE:
            IdleTaskSchedParamInit(taskCB);
            break;
        default:
            return LOS_NOK;
    }

    return LOS_OK;
}

VOID OsSchedProcessDefaultSchedParamGet(UINT16 policy, SchedParam *param)
{
    switch (policy) {
        case LOS_SCHED_FIFO:
        case LOS_SCHED_RR:
            HPFProcessDefaultSchedParamGet(param);
            break;
        case LOS_SCHED_DEADLINE:
            EDFProcessDefaultSchedParamGet(param);
            break;
        case LOS_SCHED_IDLE:
        default:
            PRINT_ERR("Invalid process-level scheduling policy, %u\n", policy);
            break;
    }
    return;
}
STATIC LosTaskCB *TopTaskGet(SchedRunqueue *rq)
{
    // 首先尝试从EDF运行队列中获取最高优先级任务
    LosTaskCB *newTask = EDFRunqueueTopTaskGet(rq->edfRunqueue);
    if (newTask != NULL) {
        goto FIND;  // 如果找到任务，跳转到FIND标签
    }

    // 如果EDF队列中没有任务，尝试从HPF运行队列中获取最高优先级任务
    newTask = HPFRunqueueTopTaskGet(rq->hpfRunqueue);
    if (newTask != NULL) {
        goto FIND;  // 如果找到任务，跳转到FIND标签
    }

    // 如果以上队列都没有任务，则返回空闲任务
    newTask = rq->idleTask;

FIND:
    // 调用任务的start操作，准备运行该任务
    newTask->ops->start(rq, newTask);
    return newTask;  // 返回找到的任务
}

VOID OsSchedStart(VOID)
{
    UINT32 cpuid = ArchCurrCpuid();        // 获取当前CPU核心ID
    UINT32 intSave;                        // 中断状态保存变量

    PRINTK("cpu %d entering scheduler\n", cpuid);  // 打印调度器启动日志

    SCHEDULER_LOCK(intSave);               // 加调度器锁，进入临界区

    OsTickStart();                          // 启动系统tick定时器

    SchedRunqueue *rq = OsSchedRunqueue();  // 获取当前CPU的运行队列
    LosTaskCB *newTask = TopTaskGet(rq);   // 获取最高优先级任务
    newTask->taskStatus |= OS_TASK_STATUS_RUNNING;  // 设置任务为运行状态

#ifdef LOSCFG_KERNEL_SMP
    /* 
     * 设置任务绑定的CPU核心，防止首次任务删除时
     * 当前CPU标志与实际运行CPU不匹配导致问题
     */
    newTask->currCpu = cpuid;              // 设置任务当前运行的CPU核心
#endif

    OsCurrTaskSet((VOID *)newTask);        // 将新任务设置为当前运行任务

    newTask->startTime = OsGetCurrSchedTimeCycle();  // 记录任务启动时间

    OsSwtmrResponseTimeReset(newTask->startTime);  // 重置软件定时器响应时间

    /* 系统正式启动调度 */
    OS_SCHEDULER_SET(cpuid);               // 设置调度器激活标志

    rq->responseID = OS_INVALID;           // 重置运行队列响应ID
    UINT64 deadline = newTask->ops->deadlineGet(newTask);  // 获取任务截止时间
    SchedNextExpireTimeSet(newTask->taskID, deadline, OS_INVALID);  // 设置下次到期时间
    OsTaskContextLoad(newTask);            // 加载任务上下文，开始执行任务
}


#ifdef LOSCFG_KERNEL_SMP
VOID OsSchedToUserReleaseLock(VOID)
{
    /* The scheduling lock needs to be released before returning to user mode */
    LOCKDEP_CHECK_OUT(&g_taskSpin);
    ArchSpinUnlock(&g_taskSpin.rawLock);

    OsSchedUnlock();
}
#endif

#ifdef LOSCFG_BASE_CORE_TSK_MONITOR
STATIC VOID TaskStackCheck(LosTaskCB *runTask, LosTaskCB *newTask)
{
    if (!OS_STACK_MAGIC_CHECK(runTask->topOfStack)) {
        LOS_Panic("CURRENT task ID: %s:%d stack overflow!\n", runTask->taskName, runTask->taskID);
    }

    if (((UINTPTR)(newTask->stackPointer) <= newTask->topOfStack) ||
        ((UINTPTR)(newTask->stackPointer) > (newTask->topOfStack + newTask->stackSize))) {
        LOS_Panic("HIGHEST task ID: %s:%u SP error! StackPointer: %p TopOfStack: %p\n",
                  newTask->taskName, newTask->taskID, newTask->stackPointer, newTask->topOfStack);
    }
}
#endif

STATIC INLINE VOID SchedSwitchCheck(LosTaskCB *runTask, LosTaskCB *newTask)
{
#ifdef LOSCFG_BASE_CORE_TSK_MONITOR
    TaskStackCheck(runTask, newTask);
#endif /* LOSCFG_BASE_CORE_TSK_MONITOR */
    OsHookCall(LOS_HOOK_TYPE_TASK_SWITCHEDIN, newTask, runTask);
}

/**
 * 执行任务切换
 * 
 * @param rq       运行队列指针
 * @param runTask  当前正在运行的任务
 * @param newTask  将要切换到的任务
 */
STATIC VOID SchedTaskSwitch(SchedRunqueue *rq, LosTaskCB *runTask, LosTaskCB *newTask)
{
    /* 检查任务栈并调用任务切换钩子函数 */
    SchedSwitchCheck(runTask, newTask);

    /* 更新任务状态：当前任务停止运行，新任务开始运行 */
    runTask->taskStatus &= ~OS_TASK_STATUS_RUNNING;
    newTask->taskStatus |= OS_TASK_STATUS_RUNNING;

#ifdef LOSCFG_KERNEL_SMP
    /* 在多核环境下更新任务的CPU绑定信息 */
    runTask->currCpu = OS_TASK_INVALID_CPUID;
    newTask->currCpu = ArchCurrCpuid();
#endif

    /* 设置新任务为当前任务 */
    OsCurrTaskSet((VOID *)newTask);

#ifdef LOSCFG_KERNEL_VM
    /* 如果新任务的内存管理单元不同，则进行上下文切换 */
    if (newTask->archMmu != runTask->archMmu) {
        LOS_ArchMmuContextSwitch((LosArchMmu *)newTask->archMmu);
    }
#endif

#ifdef LOSCFG_KERNEL_CPUP
    /* 更新CPU使用率统计 */
    OsCpupCycleEndStart(runTask, newTask);
#endif

#ifdef LOSCFG_SCHED_HPF_DEBUG
    /* 记录任务等待调度的时间 */
    UINT64 waitStartTime = newTask->startTime;
#endif

    /* 处理任务时间片 */
    if (runTask->taskStatus & OS_TASK_STATUS_READY) {
        /* 如果当前任务仍在就绪状态，继承其开始时间 */
        newTask->startTime = runTask->startTime;
    } else {
        /* 如果当前任务被阻塞，更新开始时间并处理时间片 */
        newTask->startTime = OsGetCurrSchedTimeCycle();
        runTask->ops->timeSliceUpdate(rq, runTask, newTask->startTime);

        /* 如果任务处于超时等待状态，将其加入超时队列 */
        if (runTask->taskStatus & (OS_TASK_STATUS_PEND_TIME | OS_TASK_STATUS_DELAY)) {
            OsSchedTimeoutQueueAdd(runTask, runTask->ops->waitTimeGet(runTask));
        }
    }

    /* 设置下一个到期时间 */
    UINT64 deadline = newTask->ops->deadlineGet(newTask);
    SchedNextExpireTimeSet(newTask->taskID, deadline, runTask->taskID);

#ifdef LOSCFG_SCHED_HPF_DEBUG
    /* 更新调度统计信息 */
    newTask->schedStat.waitSchedTime += newTask->startTime - waitStartTime;
    newTask->schedStat.waitSchedCount++;
    runTask->schedStat.runTime = runTask->schedStat.allRuntime;
    runTask->schedStat.switchCount++;
#endif

    /* 执行任务上下文切换 */
    OsTaskSchedule(newTask, runTask);
}

/**
 * 在中断结束时检查是否需要调度
 * 
 * 该函数在中断处理结束时被调用，用于检查当前任务是否需要被调度出去，
 * 并处理相关的调度逻辑。
 */
VOID OsSchedIrqEndCheckNeedSched(VOID)
{
    SchedRunqueue *rq = OsSchedRunqueue();  // 获取当前CPU的运行队列
    LosTaskCB *runTask = OsCurrTaskGet();   // 获取当前运行的任务

    /* 更新当前任务的时间片 */
    runTask->ops->timeSliceUpdate(rq, runTask, OsGetCurrSchedTimeCycle());

    /* 如果当前任务可被抢占且设置了重新调度标志 */
    if (OsPreemptable() && (rq->schedFlag & INT_PEND_RESCH)) {
        rq->schedFlag &= ~INT_PEND_RESCH;  // 清除重新调度标志

        /* 加锁保护任务队列 */
        LOS_SpinLock(&g_taskSpin);

        /* 将当前任务重新加入就绪队列 */
        runTask->ops->enqueue(rq, runTask);

        /* 获取下一个要运行的任务 */
        LosTaskCB *newTask = TopTaskGet(rq);
        
        /* 如果当前任务与下一个任务不同，则进行任务切换 */
        if (runTask != newTask) {
            SchedTaskSwitch(rq, runTask, newTask);
            LOS_SpinUnlock(&g_taskSpin);
            return;
        }

        /* 解锁任务队列 */
        LOS_SpinUnlock(&g_taskSpin);
    }

    /* 如果设置了tick中断挂起标志，则更新调度器的到期时间 */
    if (rq->schedFlag & INT_PEND_TICK) {
        OsSchedExpireTimeUpdate();
    }
}


VOID OsSchedResched(VOID)
{
    LOS_ASSERT(LOS_SpinHeld(&g_taskSpin));  // 确保当前持有任务自旋锁
    SchedRunqueue *rq = OsSchedRunqueue();  // 获取当前CPU的运行队列
#ifdef LOSCFG_KERNEL_SMP
    LOS_ASSERT(rq->taskLockCnt == 1);  // 在SMP模式下，任务锁计数应为1
#else
    LOS_ASSERT(rq->taskLockCnt == 0);  // 在单核模式下，任务锁计数应为0
#endif

    rq->schedFlag &= ~INT_PEND_RESCH;  // 清除重新调度标志
    LosTaskCB *runTask = OsCurrTaskGet();  // 获取当前正在运行的任务
    LosTaskCB *newTask = TopTaskGet(rq);  // 从运行队列中获取最高优先级的任务
    if (runTask == newTask) {
        return;  // 如果当前任务仍然是最高优先级任务，则直接返回
    }

    SchedTaskSwitch(rq, runTask, newTask);  // 执行任务切换
}

/**
 * 触发任务调度
 * 
 * 该函数是调度器的核心函数，用于在任务主动放弃CPU或需要调度时触发任务切换。
 * 它会更新当前任务的时间片，将其重新加入就绪队列，并选择下一个要运行的任务。
 */
VOID LOS_Schedule(VOID)
{
    UINT32 intSave;  // 用于保存中断状态
    LosTaskCB *runTask = OsCurrTaskGet();  // 获取当前正在运行的任务
    SchedRunqueue *rq = OsSchedRunqueue();  // 获取当前CPU的运行队列

    /* 如果当前处于中断上下文，设置挂起标志并返回 */
    if (OS_INT_ACTIVE) {
        OsSchedRunqueuePendingSet();
        return;
    }

    /* 如果当前任务不可被抢占，直接返回 */
    if (!OsPreemptable()) {
        return;
    }

    /*
     * 在任务中触发调度时也会进行时间片检查
     * 如果需要，它会及时放弃时间片
     * 否则，不会有其他副作用
     */
    SCHEDULER_LOCK(intSave);  // 加锁保护调度器

    /* 更新当前任务的时间片 */
    runTask->ops->timeSliceUpdate(rq, runTask, OsGetCurrSchedTimeCycle());

    /* 将当前任务重新加入就绪队列 */
    runTask->ops->enqueue(rq, runTask);

    /* 重新调度到新线程 */
    OsSchedResched();

    SCHEDULER_UNLOCK(intSave);  // 解锁调度器
}

/**
 * 在挂起链表中查找合适的位置插入当前任务（子函数）
 * 
 * @param runTask  当前任务控制块
 * @param lockList 挂起链表
 * @return 返回合适位置的链表节点指针，如果未找到则返回NULL
 */
STATIC INLINE LOS_DL_LIST *SchedLockPendFindPosSub(const LosTaskCB *runTask, const LOS_DL_LIST *lockList)
{
    LosTaskCB *pendedTask = NULL;  // 用于遍历的挂起任务指针

    /* 遍历挂起链表中的每个任务 */
    LOS_DL_LIST_FOR_EACH_ENTRY(pendedTask, lockList, LosTaskCB, pendList) {
        /* 比较当前任务与挂起任务的优先级 */
        INT32 ret = OsSchedParamCompare(pendedTask, runTask);
        
        /* 如果当前任务优先级低于挂起任务，继续查找 */
        if (ret < 0) {
            continue;
        } 
        /* 如果当前任务优先级高于挂起任务，返回该任务的位置 */
        else if (ret > 0) {
            return &pendedTask->pendList;
        } 
        /* 如果优先级相同，返回该任务的下一个位置 */
        else {
            return pendedTask->pendList.pstNext;
        }
    }
    
    /* 如果没有找到合适的位置，返回NULL */
    return NULL;
}


/**
 * 在挂起链表中找到合适的位置插入当前任务
 * 
 * @param runTask 当前任务控制块
 * @param lockList 挂起链表
 * @return 返回合适位置的链表节点指针
 */
LOS_DL_LIST *OsSchedLockPendFindPos(const LosTaskCB *runTask, LOS_DL_LIST *lockList)
{
    /* 如果挂起链表为空，直接返回链表头 */
    if (LOS_ListEmpty(lockList)) {
        return lockList;
    }

    /* 获取挂起链表中的第一个任务 */
    LosTaskCB *pendedTask1 = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(lockList));
    
    /* 比较当前任务与第一个任务的优先级 */
    INT32 ret = OsSchedParamCompare(pendedTask1, runTask);
    
    /* 如果当前任务优先级高于第一个任务，则插入到链表头部 */
    if (ret > 0) {
        return lockList->pstNext;
    }

    /* 获取挂起链表中的最后一个任务 */
    LosTaskCB *pendedTask2 = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_LAST(lockList));
    
    /* 比较当前任务与最后一个任务的优先级 */
    ret = OsSchedParamCompare(pendedTask2, runTask);
    
    /* 如果当前任务优先级低于或等于最后一个任务，则插入到链表尾部 */
    if (ret <= 0) {
        return lockList;
    }

    /* 如果以上条件都不满足，则在链表中查找合适的位置 */
    return SchedLockPendFindPosSub(runTask, lockList);
}

