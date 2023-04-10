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

STATIC INLINE VOID SchedNextExpireTimeSet(UINT32 responseID, UINT64 taskEndTime, UINT32 oldResponseID)
{
    SchedRunqueue *rq = OsSchedRunqueue();
    BOOL isTimeSlice = FALSE;
    UINT64 currTime = OsGetCurrSchedTimeCycle();
    UINT64 nextExpireTime = OsGetSortLinkNextExpireTime(&rq->timeoutQueue, currTime, OS_TICK_RESPONSE_PRECISION);

    rq->schedFlag &= ~INT_PEND_TICK;
    if (rq->responseID == oldResponseID) {
        /* This time has expired, and the next time the theory has expired is infinite */
        rq->responseTime = OS_SCHED_MAX_RESPONSE_TIME;
    }

    /* The current thread's time slice has been consumed, but the current system lock task cannot
     * trigger the schedule to release the CPU
     */
    if ((nextExpireTime > taskEndTime) && ((nextExpireTime - taskEndTime) > OS_SCHED_MINI_PERIOD)) {
        nextExpireTime = taskEndTime;
        isTimeSlice = TRUE;
    }

    if ((rq->responseTime <= nextExpireTime) ||
        ((rq->responseTime - nextExpireTime) < OS_TICK_RESPONSE_PRECISION)) {
        return;
    }

    if (isTimeSlice) {
        /* The expiration time of the current system is the thread's slice expiration time */
        rq->responseID = responseID;
    } else {
        rq->responseID = OS_INVALID_VALUE;
    }

    UINT64 nextResponseTime = nextExpireTime - currTime;
    rq->responseTime = currTime + HalClockTickTimerReload(nextResponseTime);
}

VOID OsSchedExpireTimeUpdate(VOID)
{
    UINT32 intSave;
    if (!OS_SCHEDULER_ACTIVE || OS_INT_ACTIVE) {
        OsSchedRunqueuePendingSet();
        return;
    }

    LosTaskCB *runTask = OsCurrTaskGet();
    SCHEDULER_LOCK(intSave);
    UINT64 deadline = runTask->ops->deadlineGet(runTask);
    SCHEDULER_UNLOCK(intSave);
    SchedNextExpireTimeSet(runTask->taskID, deadline, runTask->taskID);
}

STATIC INLINE VOID SchedTimeoutTaskWake(SchedRunqueue *rq, UINT64 currTime, LosTaskCB *taskCB, BOOL *needSched)
{
    LOS_SpinLock(&g_taskSpin);
    if (OsSchedPolicyIsEDF(taskCB)) {
        SchedEDF *sched = (SchedEDF *)&taskCB->sp;
        if (sched->finishTime <= currTime) {
            if (taskCB->timeSlice >= 0) {
                PrintExcInfo("EDF task: %u name: %s is timeout, timeout for %llu us.\n",
                             taskCB->taskID, taskCB->taskName, OS_SYS_CYCLE_TO_US(currTime - sched->finishTime));
            }
            taskCB->timeSlice = 0;
        }
        if (sched->flags == EDF_WAIT_FOREVER) {
            taskCB->taskStatus &= ~OS_TASK_STATUS_PEND_TIME;
            sched->flags = EDF_UNUSED;
        }
    }

    UINT16 tempStatus = taskCB->taskStatus;
    if (tempStatus & (OS_TASK_STATUS_PEND_TIME | OS_TASK_STATUS_DELAY)) {
        taskCB->taskStatus &= ~(OS_TASK_STATUS_PENDING | OS_TASK_STATUS_PEND_TIME | OS_TASK_STATUS_DELAY);
        if (tempStatus & OS_TASK_STATUS_PEND_TIME) {
            taskCB->taskStatus |= OS_TASK_STATUS_TIMEOUT;
            LOS_ListDelete(&taskCB->pendList);
            taskCB->taskMux = NULL;
            OsTaskWakeClearPendMask(taskCB);
        }

        if (!(tempStatus & OS_TASK_STATUS_SUSPENDED)) {
#ifdef LOSCFG_SCHED_HPF_DEBUG
            taskCB->schedStat.pendTime += currTime - taskCB->startTime;
            taskCB->schedStat.pendCount++;
#endif
            taskCB->ops->enqueue(rq, taskCB);
            *needSched = TRUE;
        }
    }

    LOS_SpinUnlock(&g_taskSpin);
}

STATIC INLINE BOOL SchedTimeoutQueueScan(SchedRunqueue *rq)
{
    BOOL needSched = FALSE;
    SortLinkAttribute *timeoutQueue = &rq->timeoutQueue;
    LOS_DL_LIST *listObject = &timeoutQueue->sortLink;
    /*
     * When task is pended with timeout, the task block is on the timeout sortlink
     * (per cpu) and ipc(mutex,sem and etc.)'s block at the same time, it can be waken
     * up by either timeout or corresponding ipc it's waiting.
     *
     * Now synchronize sortlink procedure is used, therefore the whole task scan needs
     * to be protected, preventing another core from doing sortlink deletion at same time.
     */
    LOS_SpinLock(&timeoutQueue->spinLock);

    if (LOS_ListEmpty(listObject)) {
        LOS_SpinUnlock(&timeoutQueue->spinLock);
        return needSched;
    }

    SortLinkList *sortList = LOS_DL_LIST_ENTRY(listObject->pstNext, SortLinkList, sortLinkNode);
    UINT64 currTime = OsGetCurrSchedTimeCycle();
    while (sortList->responseTime <= currTime) {
        LosTaskCB *taskCB = LOS_DL_LIST_ENTRY(sortList, LosTaskCB, sortList);
        OsDeleteNodeSortLink(timeoutQueue, &taskCB->sortList);
        LOS_SpinUnlock(&timeoutQueue->spinLock);

        SchedTimeoutTaskWake(rq, currTime, taskCB, &needSched);

        LOS_SpinLock(&timeoutQueue->spinLock);
        if (LOS_ListEmpty(listObject)) {
            break;
        }

        sortList = LOS_DL_LIST_ENTRY(listObject->pstNext, SortLinkList, sortLinkNode);
    }

    LOS_SpinUnlock(&timeoutQueue->spinLock);

    return needSched;
}

VOID OsSchedTick(VOID)
{
    SchedRunqueue *rq = OsSchedRunqueue();

    if (rq->responseID == OS_INVALID_VALUE) {
        if (SchedTimeoutQueueScan(rq)) {
            LOS_MpSchedule(OS_MP_CPU_ALL);
            rq->schedFlag |= INT_PEND_RESCH;
        }
    }
    rq->schedFlag |= INT_PEND_TICK;
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
    LosTaskCB *newTask = EDFRunqueueTopTaskGet(rq->edfRunqueue);
    if (newTask != NULL) {
        goto FIND;
    }

    newTask = HPFRunqueueTopTaskGet(rq->hpfRunqueue);
    if (newTask != NULL) {
        goto FIND;
    }

    newTask = rq->idleTask;

FIND:
    newTask->ops->start(rq, newTask);
    return newTask;
}

VOID OsSchedStart(VOID)
{
    UINT32 cpuid = ArchCurrCpuid();
    UINT32 intSave;

    PRINTK("cpu %d entering scheduler\n", cpuid);

    SCHEDULER_LOCK(intSave);

    OsTickStart();

    SchedRunqueue *rq = OsSchedRunqueue();
    LosTaskCB *newTask = TopTaskGet(rq);
    newTask->taskStatus |= OS_TASK_STATUS_RUNNING;

#ifdef LOSCFG_KERNEL_SMP
    /*
     * attention: current cpu needs to be set, in case first task deletion
     * may fail because this flag mismatch with the real current cpu.
     */
    newTask->currCpu = cpuid;
#endif

    OsCurrTaskSet((VOID *)newTask);

    newTask->startTime = OsGetCurrSchedTimeCycle();

    OsSwtmrResponseTimeReset(newTask->startTime);

    /* System start schedule */
    OS_SCHEDULER_SET(cpuid);

    rq->responseID = OS_INVALID;
    UINT64 deadline = newTask->ops->deadlineGet(newTask);
    SchedNextExpireTimeSet(newTask->taskID, deadline, OS_INVALID);
    OsTaskContextLoad(newTask);
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

STATIC VOID SchedTaskSwitch(SchedRunqueue *rq, LosTaskCB *runTask, LosTaskCB *newTask)
{
    SchedSwitchCheck(runTask, newTask);

    runTask->taskStatus &= ~OS_TASK_STATUS_RUNNING;
    newTask->taskStatus |= OS_TASK_STATUS_RUNNING;

#ifdef LOSCFG_KERNEL_SMP
    /* mask new running task's owner processor */
    runTask->currCpu = OS_TASK_INVALID_CPUID;
    newTask->currCpu = ArchCurrCpuid();
#endif

    OsCurrTaskSet((VOID *)newTask);
#ifdef LOSCFG_KERNEL_VM
    if (newTask->archMmu != runTask->archMmu) {
        LOS_ArchMmuContextSwitch((LosArchMmu *)newTask->archMmu);
    }
#endif

#ifdef LOSCFG_KERNEL_CPUP
    OsCpupCycleEndStart(runTask, newTask);
#endif

#ifdef LOSCFG_SCHED_HPF_DEBUG
    UINT64 waitStartTime = newTask->startTime;
#endif
    if (runTask->taskStatus & OS_TASK_STATUS_READY) {
        /* When a thread enters the ready queue, its slice of time is updated */
        newTask->startTime = runTask->startTime;
    } else {
        /* The currently running task is blocked */
        newTask->startTime = OsGetCurrSchedTimeCycle();
        /* The task is in a blocking state and needs to update its time slice before pend */
        runTask->ops->timeSliceUpdate(rq, runTask, newTask->startTime);

        if (runTask->taskStatus & (OS_TASK_STATUS_PEND_TIME | OS_TASK_STATUS_DELAY)) {
            OsSchedTimeoutQueueAdd(runTask, runTask->ops->waitTimeGet(runTask));
        }
    }

    UINT64 deadline = newTask->ops->deadlineGet(newTask);
    SchedNextExpireTimeSet(newTask->taskID, deadline, runTask->taskID);

#ifdef LOSCFG_SCHED_HPF_DEBUG
    newTask->schedStat.waitSchedTime += newTask->startTime - waitStartTime;
    newTask->schedStat.waitSchedCount++;
    runTask->schedStat.runTime = runTask->schedStat.allRuntime;
    runTask->schedStat.switchCount++;
#endif
    /* do the task context switch */
    OsTaskSchedule(newTask, runTask);
}

VOID OsSchedIrqEndCheckNeedSched(VOID)
{
    SchedRunqueue *rq = OsSchedRunqueue();
    LosTaskCB *runTask = OsCurrTaskGet();

    runTask->ops->timeSliceUpdate(rq, runTask, OsGetCurrSchedTimeCycle());

    if (OsPreemptable() && (rq->schedFlag & INT_PEND_RESCH)) {
        rq->schedFlag &= ~INT_PEND_RESCH;

        LOS_SpinLock(&g_taskSpin);

        runTask->ops->enqueue(rq, runTask);

        LosTaskCB *newTask = TopTaskGet(rq);
        if (runTask != newTask) {
            SchedTaskSwitch(rq, runTask, newTask);
            LOS_SpinUnlock(&g_taskSpin);
            return;
        }

        LOS_SpinUnlock(&g_taskSpin);
    }

    if (rq->schedFlag & INT_PEND_TICK) {
        OsSchedExpireTimeUpdate();
    }
}

VOID OsSchedResched(VOID)
{
    LOS_ASSERT(LOS_SpinHeld(&g_taskSpin));
    SchedRunqueue *rq = OsSchedRunqueue();
#ifdef LOSCFG_KERNEL_SMP
    LOS_ASSERT(rq->taskLockCnt == 1);
#else
    LOS_ASSERT(rq->taskLockCnt == 0);
#endif

    rq->schedFlag &= ~INT_PEND_RESCH;
    LosTaskCB *runTask = OsCurrTaskGet();
    LosTaskCB *newTask = TopTaskGet(rq);
    if (runTask == newTask) {
        return;
    }

    SchedTaskSwitch(rq, runTask, newTask);
}

VOID LOS_Schedule(VOID)
{
    UINT32 intSave;
    LosTaskCB *runTask = OsCurrTaskGet();
    SchedRunqueue *rq = OsSchedRunqueue();

    if (OS_INT_ACTIVE) {
        OsSchedRunqueuePendingSet();
        return;
    }

    if (!OsPreemptable()) {
        return;
    }

    /*
     * trigger schedule in task will also do the slice check
     * if necessary, it will give up the timeslice more in time.
     * otherwise, there's no other side effects.
     */
    SCHEDULER_LOCK(intSave);

    runTask->ops->timeSliceUpdate(rq, runTask, OsGetCurrSchedTimeCycle());

    /* add run task back to ready queue */
    runTask->ops->enqueue(rq, runTask);

    /* reschedule to new thread */
    OsSchedResched();

    SCHEDULER_UNLOCK(intSave);
}

STATIC INLINE LOS_DL_LIST *SchedLockPendFindPosSub(const LosTaskCB *runTask, const LOS_DL_LIST *lockList)
{
    LosTaskCB *pendedTask = NULL;

    LOS_DL_LIST_FOR_EACH_ENTRY(pendedTask, lockList, LosTaskCB, pendList) {
        INT32 ret = OsSchedParamCompare(pendedTask, runTask);
        if (ret < 0) {
            continue;
        } else if (ret > 0) {
            return &pendedTask->pendList;
        } else {
            return pendedTask->pendList.pstNext;
        }
    }
    return NULL;
}

LOS_DL_LIST *OsSchedLockPendFindPos(const LosTaskCB *runTask, LOS_DL_LIST *lockList)
{
    if (LOS_ListEmpty(lockList)) {
        return lockList;
    }

    LosTaskCB *pendedTask1 = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(lockList));
    INT32 ret = OsSchedParamCompare(pendedTask1, runTask);
    if (ret > 0) {
        return lockList->pstNext;
    }

    LosTaskCB *pendedTask2 = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_LAST(lockList));
    ret = OsSchedParamCompare(pendedTask2, runTask);
    if (ret <= 0) {
        return lockList;
    }

    return SchedLockPendFindPosSub(runTask, lockList);
}
