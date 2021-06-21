/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd. All rights reserved.
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
#include "los_process_pri.h"
#include "los_arch_mmu.h"
#ifdef LOSCFG_KERNEL_CPUP
#include "los_cpup_pri.h"
#endif
#include "los_hw_tick_pri.h"
#include "los_tick_pri.h"
#if (LOSCFG_BASE_CORE_TSK_MONITOR == YES)
#include "los_stackinfo_pri.h"
#endif
#include "los_mp.h"
#ifdef LOSCFG_SCHED_DEBUG
#include "los_stat_pri.h"
#endif



#define OS_32BIT_MAX               0xFFFFFFFFUL
#define OS_64BIT_MAX               0xFFFFFFFFFFFFFFFFULL
#define OS_SCHED_FIFO_TIMEOUT      0x7FFFFFFF
#define OS_PRIORITY_QUEUE_NUM      32
#define PRIQUEUE_PRIOR0_BIT        0x80000000U
#define OS_SCHED_TIME_SLICES_MIN   ((5000 * OS_SYS_NS_PER_US) / OS_NS_PER_CYCLE)  /* 5ms */
#define OS_SCHED_TIME_SLICES_MAX   ((LOSCFG_BASE_CORE_TIMESLICE_TIMEOUT * OS_SYS_NS_PER_US) / OS_NS_PER_CYCLE)
#define OS_SCHED_TIME_SLICES_DIFF  (OS_SCHED_TIME_SLICES_MAX - OS_SCHED_TIME_SLICES_MIN)
#define OS_SCHED_READY_MAX         30
#define OS_TIME_SLICE_MIN          (INT32)((50 * OS_SYS_NS_PER_US) / OS_NS_PER_CYCLE) /* 50us */
#define OS_SCHED_MAX_RESPONSE_TIME (UINT64)(OS_64BIT_MAX - 1U)	//调度最大响应时间

typedef struct {//进程调度队列
    LOS_DL_LIST priQueueList[OS_PRIORITY_QUEUE_NUM];//各优先级任务调度队列,默认32级
    UINT32      readyTasks[OS_PRIORITY_QUEUE_NUM];//各优先级就绪任务个数
    UINT32      queueBitmap;//任务优先级调度位图
} SchedQueue;

typedef struct {//调度器
    SchedQueue queueList[OS_PRIORITY_QUEUE_NUM];//进程优先级调度队列,默认32级
    UINT32     queueBitmap;//进程优先级调度位图
    SchedScan  taskScan;//函数指针,扫描任务
    SchedScan  swtmrScan;//函数指针,扫描定时器
} Sched;

STATIC Sched *g_sched = NULL;//全局调度器
STATIC UINT64 g_schedTickMaxResponseTime;
UINT64 g_sysSchedStartTime = 0;

#ifdef LOSCFG_SCHED_TICK_DEBUG
#define OS_SCHED_DEBUG_DATA_NUM  1000
typedef struct {
    UINT32 tickResporeTime[OS_SCHED_DEBUG_DATA_NUM];
    UINT32 index;
    UINT32 setTickCount;
    UINT64 oldResporeTime;
} SchedTickDebug;
STATIC SchedTickDebug *g_schedTickDebug = NULL;

STATIC UINT32 OsSchedDebugInit(VOID)
{
    UINT32 size = sizeof(SchedTickDebug) * LOSCFG_KERNEL_CORE_NUM;
    g_schedTickDebug = (SchedTickDebug *)LOS_MemAlloc(m_aucSysMem0, size);
    if (g_schedTickDebug == NULL) {
        return LOS_ERRNO_TSK_NO_MEMORY;
    }

    (VOID)memset_s(g_schedTickDebug, size, 0, size);
    return LOS_OK;
}

VOID OsSchedDebugRecordData(VOID)
{
    SchedTickDebug *schedDebug = &g_schedTickDebug[ArchCurrCpuid()];
    if (schedDebug->index < OS_SCHED_DEBUG_DATA_NUM) {
        UINT64 currTime = OsGerCurrSchedTimeCycle();
        schedDebug->tickResporeTime[schedDebug->index] = currTime - schedDebug->oldResporeTime;
        schedDebug->oldResporeTime = currTime;
        schedDebug->index++;
    }
}

SchedTickDebug *OsSchedDebugGet(VOID)
{
    return g_schedTickDebug;
}

UINT32 OsShellShowTickRespo(VOID)
{
    UINT32 intSave;
    UINT16 cpu;

    UINT32 tickSize = sizeof(SchedTickDebug) * LOSCFG_KERNEL_CORE_NUM;
    SchedTickDebug *schedDebug = (SchedTickDebug *)LOS_MemAlloc(m_aucSysMem1, tickSize);
    if (schedDebug == NULL) {
        return LOS_NOK;
    }

    UINT32 sortLinkNum[LOSCFG_KERNEL_CORE_NUM];
    SCHEDULER_LOCK(intSave);
    (VOID)memcpy_s((CHAR *)schedDebug, tickSize, (CHAR *)OsSchedDebugGet(), tickSize);
    (VOID)memset_s((CHAR *)OsSchedDebugGet(), tickSize, 0, tickSize);
    for (cpu = 0; cpu < LOSCFG_KERNEL_CORE_NUM; cpu++) {
        sortLinkNum[cpu] = OsPercpuGetByID(cpu)->taskSortLink.nodeNum + OsPercpuGetByID(cpu)->swtmrSortLink.nodeNum;
    }
    SCHEDULER_UNLOCK(intSave);

    for (cpu = 0; cpu < LOSCFG_KERNEL_CORE_NUM; cpu++) {
        SchedTickDebug *schedData = &schedDebug[cpu];
        PRINTK("cpu : %u sched data num : %u set time count : %u SortMax : %u\n",
               cpu, schedData->index, schedData->setTickCount, sortLinkNum[cpu]);
        UINT32 *data = schedData->tickResporeTime;
        for (UINT32 i = 0; i < schedData->index; i++) {
            UINT32 timeUs = (data[i] * OS_NS_PER_CYCLE) / OS_SYS_NS_PER_US;
            PRINTK("     %u(%u)", timeUs, timeUs / OS_US_PER_TICK);
            if ((i != 0) && ((i % 5) == 0)) {
                PRINTK("\n");
            }
        }

        PRINTK("\n");
    }

    (VOID)LOS_MemFree(m_aucSysMem1, schedDebug);
    return LOS_OK;
}

#else

UINT32 OsShellShowTickRespo(VOID)
{
    return LOS_NOK;
}
#endif

#ifdef LOSCFG_SCHED_DEBUG
UINT32 OsShellShowSchedParam(VOID)
{
    UINT64 averRunTime;
    UINT64 averTimeSlice;
    UINT64 averSchedWait;
    UINT64 averPendTime;
    UINT32 intSave;
    UINT32 size = g_taskMaxNum * sizeof(LosTaskCB);
    LosTaskCB *taskCBArray = LOS_MemAlloc(m_aucSysMem1, size);
    if (taskCBArray == NULL) {
        return LOS_NOK;
    }

    SCHEDULER_LOCK(intSave);
    (VOID)memcpy_s(taskCBArray, size, g_taskCBArray, size);
    SCHEDULER_UNLOCK(intSave);
    PRINTK("  Tid    AverRunTime(us)    SwitchCount  AverTimeSlice(us)    TimeSliceCount  AverReadyWait(us)  "
           "AverPendTime(us)  TaskName \n");
    for (UINT32 tid = 0; tid < g_taskMaxNum; tid++) {
        LosTaskCB *taskCB = taskCBArray + tid;
        if (OsTaskIsUnused(taskCB)) {
            continue;
        }

        averRunTime = 0;
        averTimeSlice = 0;
        averPendTime = 0;
        averSchedWait = 0;

        if (taskCB->schedStat.switchCount >= 1) {
            averRunTime = taskCB->schedStat.runTime / taskCB->schedStat.switchCount;
            averRunTime = (averRunTime * OS_NS_PER_CYCLE) / OS_SYS_NS_PER_US;
        }

        if (taskCB->schedStat.timeSliceCount > 1) {
            averTimeSlice = taskCB->schedStat.timeSliceTime / (taskCB->schedStat.timeSliceCount - 1);
            averTimeSlice = (averTimeSlice * OS_NS_PER_CYCLE) / OS_SYS_NS_PER_US;
        }

        if (taskCB->schedStat.pendCount > 1) {
            averPendTime = taskCB->schedStat.pendTime / taskCB->schedStat.pendCount;
            averPendTime = (averPendTime * OS_NS_PER_CYCLE) / OS_SYS_NS_PER_US;
        }

        if (taskCB->schedStat.waitSchedCount > 0) {
            averSchedWait = taskCB->schedStat.waitSchedTime / taskCB->schedStat.waitSchedCount;
            averSchedWait = (averSchedWait * OS_NS_PER_CYCLE) / OS_SYS_NS_PER_US;
        }

        PRINTK("%5u%19llu%15llu%19llu%18llu%19llu%18llu  %-32s\n", taskCB->taskID,
               averRunTime, taskCB->schedStat.switchCount,
               averTimeSlice, taskCB->schedStat.timeSliceCount - 1,
               averSchedWait, averPendTime, taskCB->taskName);
    }

    (VOID)LOS_MemFree(m_aucSysMem1, taskCBArray);

    return LOS_OK;
}

#else

UINT32 OsShellShowSchedParam(VOID)
{
    return LOS_NOK;
}
#endif

UINT32 OsSchedSetTickTimerType(UINT32 timerType)
{
    switch (timerType) {
        case 32: /* 32 bit timer */
            g_schedTickMaxResponseTime = OS_32BIT_MAX;
            break;
        case 64: /* 64 bit timer */
            g_schedTickMaxResponseTime = OS_64BIT_MAX;
            break;
        default:
            PRINT_ERR("Unsupported Tick Timer type, The system only supports 32 and 64 bit tick timers\n");
            return LOS_NOK;
    }

    return LOS_OK;
}

STATIC VOID OsSchedSetStartTime(UINT64 currCycle)
{
    if (g_sysSchedStartTime == 0) {
        g_sysSchedStartTime = currCycle;
    }
}

STATIC INLINE VOID OsTimeSliceUpdate(LosTaskCB *taskCB, UINT64 currTime)
{
    LOS_ASSERT(currTime >= taskCB->startTime);

    INT32 incTime = (currTime - taskCB->startTime - taskCB->irqUsedTime);

    LOS_ASSERT(incTime >= 0);

    if (taskCB->policy == LOS_SCHED_RR) {
        taskCB->timeSlice -= incTime;
#ifdef LOSCFG_SCHED_DEBUG
        taskCB->schedStat.timeSliceRealTime += incTime;
#endif
    }
    taskCB->irqUsedTime = 0;
    taskCB->startTime = currTime;

#ifdef LOSCFG_SCHED_DEBUG
    taskCB->schedStat.allRuntime += incTime;
#endif
}

STATIC INLINE VOID OsSchedSetNextExpireTime(UINT64 startTime, UINT32 responseID,
                                            UINT64 taskEndTime, UINT32 oldResponseID)
{
    UINT64 nextExpireTime = OsGetNextExpireTime(startTime);
    Percpu *currCpu = OsPercpuGet();
    UINT64 nextResponseTime;
    BOOL isTimeSlice = FALSE;

    if (currCpu->responseID == oldResponseID) {
        /* This time has expired, and the next time the theory has expired is infinite */
        currCpu->responseTime = OS_SCHED_MAX_RESPONSE_TIME;
    }

    /* The current thread's time slice has been consumed, but the current system lock task cannot
     * trigger the schedule to release the CPU
     */
    if (taskEndTime < nextExpireTime) {
        nextExpireTime = taskEndTime;
        isTimeSlice = TRUE;
    }

    if ((currCpu->responseTime > nextExpireTime) && ((currCpu->responseTime - nextExpireTime) >= OS_CYCLE_PER_TICK)) {
        nextResponseTime = nextExpireTime - startTime;
        if (nextResponseTime < OS_CYCLE_PER_TICK) {
            nextResponseTime = OS_CYCLE_PER_TICK;
            nextExpireTime = startTime + nextResponseTime;
            if (nextExpireTime >= currCpu->responseTime) {
                return;
            }
        } else if (nextResponseTime > g_schedTickMaxResponseTime) {
            nextResponseTime = g_schedTickMaxResponseTime;
            nextExpireTime = startTime + nextResponseTime;
        }
    } else {
        /* There is no point earlier than the current expiration date */
        return;
    }

    if (isTimeSlice) {
        /* The expiration time of the current system is the thread's slice expiration time */
        currCpu->responseID = responseID;
    } else {
        currCpu->responseID = OS_INVALID_VALUE;
    }

    currCpu->responseTime = nextExpireTime;
    HalClockTickTimerReload(nextResponseTime);

#ifdef LOSCFG_SCHED_TICK_DEBUG
    SchedTickDebug *schedDebug = &g_schedTickDebug[ArchCurrCpuid()];
    if (schedDebug->index < OS_SCHED_DEBUG_DATA_NUM) {
        schedDebug->setTickCount++;
    }
#endif
}

VOID OsSchedUpdateExpireTime(UINT64 startTime)
{
    UINT64 endTime;
    LosTaskCB *runTask = OsCurrTaskGet();

    if (runTask->policy == LOS_SCHED_RR) {
        LOS_SpinLock(&g_taskSpin);
        INT32 timeSlice = (runTask->timeSlice <= OS_TIME_SLICE_MIN) ? runTask->initTimeSlice : runTask->timeSlice;
        LOS_SpinUnlock(&g_taskSpin);
        endTime = startTime + timeSlice;
    } else {
        endTime = OS_SCHED_MAX_RESPONSE_TIME;
    }

    OsSchedSetNextExpireTime(startTime, runTask->taskID, endTime, runTask->taskID);
}

STATIC INLINE UINT32 OsSchedCalculateTimeSlice(UINT16 proPriority, UINT16 priority)
{
    UINT32 ratTime, readTasks;

    SchedQueue *queueList = &g_sched->queueList[proPriority];
    readTasks = queueList->readyTasks[priority];
    if (readTasks > OS_SCHED_READY_MAX) {
        return OS_SCHED_TIME_SLICES_MIN;
    }
    ratTime = ((OS_SCHED_READY_MAX - readTasks) * OS_SCHED_TIME_SLICES_DIFF) / OS_SCHED_READY_MAX;
    return (ratTime + OS_SCHED_TIME_SLICES_MIN);
}

STATIC INLINE VOID OsSchedPriQueueEnHead(UINT32 proPriority, LOS_DL_LIST *priqueueItem, UINT32 priority)
{
    SchedQueue *queueList = &g_sched->queueList[proPriority];
    LOS_DL_LIST *priQueueList = &queueList->priQueueList[0];
    UINT32 *bitMap = &queueList->queueBitmap;

    /*
     * Task control blocks are inited as zero. And when task is deleted,
     * and at the same time would be deleted from priority queue or
     * other lists, task pend node will restored as zero.
     */
    LOS_ASSERT(priqueueItem->pstNext == NULL);

    if (*bitMap == 0) {
        g_sched->queueBitmap |= PRIQUEUE_PRIOR0_BIT >> proPriority;
    }

    if (LOS_ListEmpty(&priQueueList[priority])) {
        *bitMap |= PRIQUEUE_PRIOR0_BIT >> priority;
    }

    LOS_ListHeadInsert(&priQueueList[priority], priqueueItem);
    queueList->readyTasks[priority]++;
}

STATIC INLINE VOID OsSchedPriQueueEnTail(UINT32 proPriority, LOS_DL_LIST *priqueueItem, UINT32 priority)
{
    SchedQueue *queueList = &g_sched->queueList[proPriority];
    LOS_DL_LIST *priQueueList = &queueList->priQueueList[0];
    UINT32 *bitMap = &queueList->queueBitmap;

    /*
     * Task control blocks are inited as zero. And when task is deleted,
     * and at the same time would be deleted from priority queue or
     * other lists, task pend node will restored as zero.
     */
    LOS_ASSERT(priqueueItem->pstNext == NULL);

    if (*bitMap == 0) {
        g_sched->queueBitmap |= PRIQUEUE_PRIOR0_BIT >> proPriority;
    }

    if (LOS_ListEmpty(&priQueueList[priority])) {
        *bitMap |= PRIQUEUE_PRIOR0_BIT >> priority;
    }

    LOS_ListTailInsert(&priQueueList[priority], priqueueItem);
    queueList->readyTasks[priority]++;
}

STATIC INLINE VOID OsSchedPriQueueDelete(UINT32 proPriority, LOS_DL_LIST *priqueueItem, UINT32 priority)
{
    SchedQueue *queueList = &g_sched->queueList[proPriority];
    LOS_DL_LIST *priQueueList = &queueList->priQueueList[0];
    UINT32 *bitMap = &queueList->queueBitmap;

    LOS_ListDelete(priqueueItem);
    queueList->readyTasks[priority]--;
    if (LOS_ListEmpty(&priQueueList[priority])) {
        *bitMap &= ~(PRIQUEUE_PRIOR0_BIT >> priority);
    }

    if (*bitMap == 0) {
        g_sched->queueBitmap &= ~(PRIQUEUE_PRIOR0_BIT >> proPriority);
    }
}

STATIC INLINE VOID OsSchedWakePendTimeTask(UINT64 currTime, LosTaskCB *taskCB, BOOL *needSchedule)
{
#ifndef LOSCFG_SCHED_DEBUG
    (VOID)currTime;
#endif

    LOS_SpinLock(&g_taskSpin);
    UINT16 tempStatus = taskCB->taskStatus;
    if (tempStatus & (OS_TASK_STATUS_PENDING | OS_TASK_STATUS_DELAY)) {
        taskCB->taskStatus &= ~(OS_TASK_STATUS_PENDING | OS_TASK_STATUS_PEND_TIME | OS_TASK_STATUS_DELAY);
        if (tempStatus & OS_TASK_STATUS_PENDING) {
#if (LOSCFG_KERNEL_LITEIPC == YES)
            taskCB->ipcStatus &= ~IPC_THREAD_STATUS_PEND;
#endif
            taskCB->taskStatus |= OS_TASK_STATUS_TIMEOUT;
            LOS_ListDelete(&taskCB->pendList);
            taskCB->taskMux = NULL;
            OsTaskWakeClearPendMask(taskCB);
        }

        if (!(tempStatus & OS_TASK_STATUS_SUSPENDED)) {
#ifdef LOSCFG_SCHED_DEBUG
            taskCB->schedStat.pendTime += currTime - taskCB->startTime;
            taskCB->schedStat.pendCount++;
#endif
            OsSchedTaskEnQueue(taskCB);
            *needSchedule = TRUE;
        }
    }

    LOS_SpinUnlock(&g_taskSpin);
}

STATIC INLINE BOOL OsSchedScanTimerList(VOID)
{
    Percpu *cpu = OsPercpuGet();
    BOOL needSchedule = FALSE;
    SortLinkAttribute *taskSortLink = &OsPercpuGet()->taskSortLink;
    LOS_DL_LIST *listObject = &taskSortLink->sortLink;
    /*
     * When task is pended with timeout, the task block is on the timeout sortlink
     * (per cpu) and ipc(mutex,sem and etc.)'s block at the same time, it can be waken
     * up by either timeout or corresponding ipc it's waiting.
     *
     * Now synchronize sortlink preocedure is used, therefore the whole task scan needs
     * to be protected, preventing another core from doing sortlink deletion at same time.
     */
    LOS_SpinLock(&cpu->taskSortLinkSpin);

    if (LOS_ListEmpty(listObject)) {
        LOS_SpinUnlock(&cpu->taskSortLinkSpin);
        return needSchedule;
    }

    SortLinkList *sortList = LOS_DL_LIST_ENTRY(listObject->pstNext, SortLinkList, sortLinkNode);
    UINT64 currTime = OsGerCurrSchedTimeCycle();
    while (sortList->responseTime <= currTime) {
        LosTaskCB *taskCB = LOS_DL_LIST_ENTRY(sortList, LosTaskCB, sortList);
        OsDeleteNodeSortLink(taskSortLink, &taskCB->sortList);
        LOS_SpinUnlock(&cpu->taskSortLinkSpin);

        OsSchedWakePendTimeTask(currTime, taskCB, &needSchedule);

        LOS_SpinLock(&cpu->taskSortLinkSpin);
        if (LOS_ListEmpty(listObject)) {
            break;
        }

        sortList = LOS_DL_LIST_ENTRY(listObject->pstNext, SortLinkList, sortLinkNode);
    }

    LOS_SpinUnlock(&cpu->taskSortLinkSpin);

    return needSchedule;
}

STATIC INLINE VOID OsSchedEnTaskQueue(LosTaskCB *taskCB, LosProcessCB *processCB)
{
    LOS_ASSERT(!(taskCB->taskStatus & OS_TASK_STATUS_READY));

    switch (taskCB->policy) {
        case LOS_SCHED_RR: {
            if (taskCB->timeSlice > OS_TIME_SLICE_MIN) {
                OsSchedPriQueueEnHead(processCB->priority, &taskCB->pendList, taskCB->priority);
            } else {
                taskCB->initTimeSlice = OsSchedCalculateTimeSlice(processCB->priority, taskCB->priority);
                taskCB->timeSlice = taskCB->initTimeSlice;
                OsSchedPriQueueEnTail(processCB->priority, &taskCB->pendList, taskCB->priority);
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
                OsSchedPriQueueEnHead(processCB->priority, &taskCB->pendList, taskCB->priority);
            } else {
                taskCB->initTimeSlice = OS_SCHED_FIFO_TIMEOUT;
                taskCB->timeSlice = taskCB->initTimeSlice;
                OsSchedPriQueueEnTail(processCB->priority, &taskCB->pendList, taskCB->priority);
            }
            break;
        }
        case LOS_SCHED_IDLE:
#ifdef LOSCFG_SCHED_DEBUG
            taskCB->schedStat.timeSliceCount = 1;
#endif
            break;
        default:
            LOS_ASSERT(0);
            break;
    }

    taskCB->taskStatus &= ~OS_TASK_STATUS_BLOCKED;
    taskCB->taskStatus |= OS_TASK_STATUS_READY;

    processCB->processStatus &= ~(OS_PROCESS_STATUS_INIT | OS_PROCESS_STATUS_PENDING);
    processCB->processStatus |= OS_PROCESS_STATUS_READY;
    processCB->readyTaskNum++;
}

STATIC INLINE VOID OsSchedDeTaskQueue(LosTaskCB *taskCB, LosProcessCB *processCB)
{
    if (taskCB->policy != LOS_SCHED_IDLE) {
        OsSchedPriQueueDelete(processCB->priority, &taskCB->pendList, taskCB->priority);
    }
    taskCB->taskStatus &= ~OS_TASK_STATUS_READY;

    processCB->readyTaskNum--;
    if (processCB->readyTaskNum == 0) {
        processCB->processStatus &= ~OS_PROCESS_STATUS_READY;
    }
}

VOID OsSchedTaskDeQueue(LosTaskCB *taskCB)
{
    LosProcessCB *processCB = OS_PCB_FROM_PID(taskCB->processID);

    if (taskCB->taskStatus & OS_TASK_STATUS_READY) {
        OsSchedDeTaskQueue(taskCB, processCB);
    }

    if (processCB->processStatus & OS_PROCESS_STATUS_READY) {
        return;
    }

    /* If the current process has only the current thread running,
     * the process becomes blocked after the thread leaves the scheduling queue
     */
    if (OS_PROCESS_GET_RUNTASK_COUNT(processCB->processStatus) == 1) {
        processCB->processStatus |= OS_PROCESS_STATUS_PENDING;
    }
}

VOID OsSchedTaskEnQueue(LosTaskCB *taskCB)
{
    LosProcessCB *processCB = OS_PCB_FROM_PID(taskCB->processID);
#ifdef LOSCFG_SCHED_DEBUG
    if (!(taskCB->taskStatus & OS_TASK_STATUS_RUNNING)) {
        taskCB->startTime = OsGerCurrSchedTimeCycle();
    }
#endif
    OsSchedEnTaskQueue(taskCB, processCB);
}

VOID OsSchedTaskExit(LosTaskCB *taskCB)
{
    LosProcessCB *processCB = OS_PCB_FROM_PID(taskCB->processID);

    if (taskCB->taskStatus & OS_TASK_STATUS_READY) {
        OsSchedTaskDeQueue(taskCB);
        processCB->processStatus &= ~OS_PROCESS_STATUS_PENDING;
    } else if (taskCB->taskStatus & OS_TASK_STATUS_PENDING) {
        LOS_ListDelete(&taskCB->pendList);
        taskCB->taskStatus &= ~OS_TASK_STATUS_PENDING;
    }

    if (taskCB->taskStatus & (OS_TASK_STATUS_DELAY | OS_TASK_STATUS_PEND_TIME)) {
        OsDeleteSortLink(&taskCB->sortList, OS_SORT_LINK_TASK);
        taskCB->taskStatus &= ~(OS_TASK_STATUS_DELAY | OS_TASK_STATUS_PEND_TIME);
    }
}

VOID OsSchedYield(VOID)
{
    LosTaskCB *runTask = OsCurrTaskGet();

    runTask->timeSlice = 0;

    runTask->startTime = OsGerCurrSchedTimeCycle();
    OsSchedTaskEnQueue(runTask);
    OsSchedResched();
}

VOID OsSchedDelay(LosTaskCB *runTask, UINT32 tick)
{
    OsSchedTaskDeQueue(runTask);
    runTask->taskStatus |= OS_TASK_STATUS_DELAY;
    runTask->waitTimes = tick;

    OsSchedResched();
}

UINT32 OsSchedTaskWait(LOS_DL_LIST *list, UINT32 ticks, BOOL needSched)
{
    LosTaskCB *runTask = OsCurrTaskGet();
    OsSchedTaskDeQueue(runTask);

    runTask->taskStatus |= OS_TASK_STATUS_PENDING;
    LOS_ListTailInsert(list, &runTask->pendList);

    if (ticks != LOS_WAIT_FOREVER) {
        runTask->taskStatus |= OS_TASK_STATUS_PEND_TIME;
        runTask->waitTimes = ticks;
    }

    if (needSched == TRUE) {
        OsSchedResched();
        if (runTask->taskStatus & OS_TASK_STATUS_TIMEOUT) {
            runTask->taskStatus &= ~OS_TASK_STATUS_TIMEOUT;
            return LOS_ERRNO_TSK_TIMEOUT;
        }
    }

    return LOS_OK;
}

VOID OsSchedTaskWake(LosTaskCB *resumedTask)
{
    LOS_ListDelete(&resumedTask->pendList);
    resumedTask->taskStatus &= ~OS_TASK_STATUS_PENDING;

    if (resumedTask->taskStatus & OS_TASK_STATUS_PEND_TIME) {
        OsDeleteSortLink(&resumedTask->sortList, OS_SORT_LINK_TASK);
        resumedTask->taskStatus &= ~OS_TASK_STATUS_PEND_TIME;
    }

    if (!(resumedTask->taskStatus & OS_TASK_STATUS_SUSPENDED)) {
#ifdef LOSCFG_SCHED_DEBUG
        resumedTask->schedStat.pendTime += OsGerCurrSchedTimeCycle() - resumedTask->startTime;
        resumedTask->schedStat.pendCount++;
#endif
        OsSchedTaskEnQueue(resumedTask);
    }
}

BOOL OsSchedModifyTaskSchedParam(LosTaskCB *taskCB, UINT16 policy, UINT16 priority)
{
    if (taskCB->policy != policy) {
        taskCB->policy = policy;
        taskCB->timeSlice = 0;
    }

    if (taskCB->taskStatus & OS_TASK_STATUS_READY) {
        OsSchedTaskDeQueue(taskCB);
        taskCB->priority = priority;
        OsSchedTaskEnQueue(taskCB);
        return TRUE;
    }

    taskCB->priority = priority;
    if (taskCB->taskStatus & OS_TASK_STATUS_INIT) {
        OsSchedTaskEnQueue(taskCB);
        return TRUE;
    }

    if (taskCB->taskStatus & OS_TASK_STATUS_RUNNING) {
        return TRUE;
    }

    return FALSE;
}

BOOL OsSchedModifyProcessSchedParam(LosProcessCB *processCB, UINT16 policy, UINT16 priority)
{
    LosTaskCB *taskCB = NULL;
    BOOL needSched = FALSE;
    (VOID)policy;

    if (processCB->processStatus & OS_PROCESS_STATUS_READY) {
        LOS_DL_LIST_FOR_EACH_ENTRY(taskCB, &processCB->threadSiblingList, LosTaskCB, threadList) {
            if (taskCB->taskStatus & OS_TASK_STATUS_READY) {
                OsSchedPriQueueDelete(processCB->priority, &taskCB->pendList, taskCB->priority);
                OsSchedPriQueueEnTail(priority, &taskCB->pendList, taskCB->priority);
                needSched = TRUE;
            }
        }
    }

    processCB->priority = priority;
    if (processCB->processStatus & OS_PROCESS_STATUS_RUNNING) {
        needSched = TRUE;
    }

    return needSched;
}

VOID OsSchedTick(VOID)
{
    Sched *sched = g_sched;
    Percpu *currCpu = OsPercpuGet();
    BOOL needSched = FALSE;

    if (currCpu->responseID == OS_INVALID_VALUE) {
        if (sched->swtmrScan != NULL) {
            (VOID)sched->swtmrScan();
        }

        needSched = sched->taskScan();
        currCpu->responseTime = OS_SCHED_MAX_RESPONSE_TIME;

        if (needSched) {
            LOS_MpSchedule(OS_MP_CPU_ALL);
            currCpu->schedFlag = INT_PEND_RESCH;
        }
    }
}

VOID OsSchedSetIdleTaskSchedParam(LosTaskCB *idleTask)
{
    idleTask->policy = LOS_SCHED_IDLE;
    idleTask->initTimeSlice = OS_SCHED_FIFO_TIMEOUT;
    idleTask->timeSlice = idleTask->initTimeSlice;
    OsSchedTaskEnQueue(idleTask);
}

UINT32 OsSchedSwtmrScanRegister(SchedScan func)
{
    if (func == NULL) {
        return LOS_NOK;
    }

    g_sched->swtmrScan = func;
    return LOS_OK;
}
//调度初始化
UINT32 OsSchedInit(VOID)
{
    UINT16 index, pri;
    UINT32 ret;

    g_sched = (Sched *)LOS_MemAlloc(m_aucSysMem0, sizeof(Sched));//分配调度器内存
    if (g_sched == NULL) {
        return LOS_ERRNO_TSK_NO_MEMORY;
    }

    (VOID)memset_s(g_sched, sizeof(Sched), 0, sizeof(Sched));

    for (index = 0; index < OS_PRIORITY_QUEUE_NUM; index++) {//初始化进程优先级队列
        SchedQueue *queueList = &g_sched->queueList[index];
        LOS_DL_LIST *priList = &queueList->priQueueList[0];//每个进程优先级都有同样的任务优先级链表
        for (pri = 0; pri < OS_PRIORITY_QUEUE_NUM; pri++) {
            LOS_ListInit(&priList[pri]);//初始化任务优先级链表节点
        }
    }

    for (index = 0; index < LOSCFG_KERNEL_CORE_NUM; index++) {//初始化每个CPU核
        Percpu *cpu = OsPercpuGetByID(index);//获取某个CPU信息
        ret = OsSortLinkInit(&cpu->taskSortLink);//初始化任务排序链表
        if (ret != LOS_OK) {
            return LOS_ERRNO_TSK_NO_MEMORY;
        }
        cpu->responseTime = OS_SCHED_MAX_RESPONSE_TIME;
        LOS_SpinInit(&cpu->taskSortLinkSpin);//自旋锁初始化
        LOS_SpinInit(&cpu->swtmrSortLinkSpin);//操作具体CPU核定时器排序链表
    }

    g_sched->taskScan = OsSchedScanTimerList;//

#ifdef LOSCFG_SCHED_TICK_DEBUG
    ret = OsSchedDebugInit();
    if (ret != LOS_OK) {
        return ret;
    }
#endif
    return LOS_OK;
}

STATIC LosTaskCB *OsGetTopTask(VOID)
{
    UINT32 priority, processPriority;
    UINT32 bitmap;
    LosTaskCB *newTask = NULL;
    UINT32 processBitmap = g_sched->queueBitmap;
#if (LOSCFG_KERNEL_SMP == YES)
    UINT32 cpuid = ArchCurrCpuid();
#endif

    while (processBitmap) {
        processPriority = CLZ(processBitmap);
        SchedQueue *queueList = &g_sched->queueList[processPriority];
        bitmap = queueList->queueBitmap;
            while (bitmap) {
                priority = CLZ(bitmap);
                LOS_DL_LIST_FOR_EACH_ENTRY(newTask, &queueList->priQueueList[priority], LosTaskCB, pendList) {
#if (LOSCFG_KERNEL_SMP == YES)
                    if (newTask->cpuAffiMask & (1U << cpuid)) {
#endif
                        goto FIND_TASK;
#if (LOSCFG_KERNEL_SMP == YES)
                    }
#endif
                }
            bitmap &= ~(1U << (OS_PRIORITY_QUEUE_NUM - priority - 1));
        }
        processBitmap &= ~(1U << (OS_PRIORITY_QUEUE_NUM - processPriority - 1));
    }

    newTask = OS_TCB_FROM_TID(OsPercpuGet()->idleTaskID);

FIND_TASK:
    OsSchedDeTaskQueue(newTask, OS_PCB_FROM_PID(newTask->processID));
    return newTask;
}

VOID OsSchedStart(VOID)
{
    UINT32 cpuid = ArchCurrCpuid();
    UINT32 intSave;

    SCHEDULER_LOCK(intSave);

    OsTickStart();

    LosTaskCB *newTask = OsGetTopTask();
    LosProcessCB *newProcess = OS_PCB_FROM_PID(newTask->processID);

    newTask->taskStatus |= OS_TASK_STATUS_RUNNING;
    newProcess->processStatus |= OS_PROCESS_STATUS_RUNNING;
    newProcess->processStatus = OS_PROCESS_RUNTASK_COUNT_ADD(newProcess->processStatus);

    OsSchedSetStartTime(HalClockGetCycles());
    newTask->startTime = OsGerCurrSchedTimeCycle();

#if (LOSCFG_KERNEL_SMP == YES)
    /*
     * attention: current cpu needs to be set, in case first task deletion
     * may fail because this flag mismatch with the real current cpu.
     */
    newTask->currCpu = cpuid;
#endif

    OsCurrTaskSet((VOID *)newTask);

    /* System start schedule */
    OS_SCHEDULER_SET(cpuid);

    OsPercpuGet()->responseID = OS_INVALID;
    OsSchedSetNextExpireTime(newTask->startTime, newTask->taskID, newTask->startTime + newTask->timeSlice, OS_INVALID);

    PRINTK("cpu %d entering scheduler\n", cpuid);
    OsTaskContextLoad(newTask);
}

#if (LOSCFG_KERNEL_SMP == YES)
VOID OsSchedToUserReleaseLock(VOID)
{
    /* The scheduling lock needs to be released before returning to user mode */
    LOCKDEP_CHECK_OUT(&g_taskSpin);
    ArchSpinUnlock(&g_taskSpin.rawLock);

    OsPercpuGet()->taskLockCnt--;
}
#endif

#if (LOSCFG_BASE_CORE_TSK_MONITOR == YES)
STATIC VOID OsTaskStackCheck(LosTaskCB *runTask, LosTaskCB *newTask)
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

STATIC INLINE VOID OsSchedSwitchCheck(LosTaskCB *runTask, LosTaskCB *newTask)
{
#if (LOSCFG_BASE_CORE_TSK_MONITOR == YES)
    OsTaskStackCheck(runTask, newTask);
#endif /* LOSCFG_BASE_CORE_TSK_MONITOR == YES */

    OsTraceTaskSchedule(newTask, runTask);
}

STATIC INLINE VOID OsSchedSwitchProcess(LosProcessCB *runProcess, LosProcessCB *newProcess)
{
    runProcess->processStatus = OS_PROCESS_RUNTASK_COUNT_DEC(runProcess->processStatus);
    newProcess->processStatus = OS_PROCESS_RUNTASK_COUNT_ADD(newProcess->processStatus);

    LOS_ASSERT(!(OS_PROCESS_GET_RUNTASK_COUNT(newProcess->processStatus) > LOSCFG_KERNEL_CORE_NUM));
    if (OS_PROCESS_GET_RUNTASK_COUNT(runProcess->processStatus) == 0) {
        runProcess->processStatus &= ~OS_PROCESS_STATUS_RUNNING;
    }

    LOS_ASSERT(!(newProcess->processStatus & OS_PROCESS_STATUS_PENDING));
    newProcess->processStatus |= OS_PROCESS_STATUS_RUNNING;

#ifdef LOSCFG_KERNEL_VM
    if (OsProcessIsUserMode(newProcess)) {
        LOS_ArchMmuContextSwitch(&newProcess->vmSpace->archMmu);
    }
#endif

    OsCurrProcessSet(newProcess);
}

STATIC VOID OsSchedTaskSwicth(LosTaskCB *runTask, LosTaskCB *newTask)
{
    UINT64 endTime;

    OsSchedSwitchCheck(runTask, newTask);

    runTask->taskStatus &= ~OS_TASK_STATUS_RUNNING;
    newTask->taskStatus |= OS_TASK_STATUS_RUNNING;

#if (LOSCFG_KERNEL_SMP == YES)
    /* mask new running task's owner processor */
    runTask->currCpu = OS_TASK_INVALID_CPUID;
    newTask->currCpu = ArchCurrCpuid();
#endif

    OsCurrTaskSet((VOID *)newTask);
    LosProcessCB *newProcess = OS_PCB_FROM_PID(newTask->processID);
    LosProcessCB *runProcess = OS_PCB_FROM_PID(runTask->processID);
    if (runProcess != newProcess) {
        OsSchedSwitchProcess(runProcess, newProcess);
    }

    if (OsProcessIsUserMode(newProcess)) {
        OsCurrUserTaskSet(newTask->userArea);
    }

#ifdef LOSCFG_KERNEL_CPUP
    OsCpupCycleEndStart(runTask->taskID, newTask->taskID);
#endif

#ifdef LOSCFG_SCHED_DEBUG
    UINT64 waitStartTime = newTask->startTime;
#endif
    if (runTask->taskStatus & OS_TASK_STATUS_READY) {
        /* When a thread enters the ready queue, its slice of time is updated */
        newTask->startTime = runTask->startTime;
    } else {
        /* The currently running task is blocked */
        newTask->startTime = OsGerCurrSchedTimeCycle();
        /* The task is in a blocking state and needs to update its time slice before pend */
        OsTimeSliceUpdate(runTask, newTask->startTime);

        if (runTask->taskStatus & (OS_TASK_STATUS_PEND_TIME | OS_TASK_STATUS_DELAY)) {
            OsAdd2SortLink(&runTask->sortList, runTask->startTime, runTask->waitTimes, OS_SORT_LINK_TASK);
        }
    }

    if (newTask->policy == LOS_SCHED_RR) {
        endTime = newTask->startTime + newTask->timeSlice;
    } else {
        endTime = OS_SCHED_MAX_RESPONSE_TIME;
    }
    OsSchedSetNextExpireTime(newTask->startTime, newTask->taskID, endTime, runTask->taskID);

#ifdef LOSCFG_SCHED_DEBUG
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
    Percpu *percpu = OsPercpuGet();
    LosTaskCB *runTask = OsCurrTaskGet();

    OsTimeSliceUpdate(runTask, OsGerCurrSchedTimeCycle());
    if (runTask->timeSlice <= OS_TIME_SLICE_MIN) {
        percpu->schedFlag = INT_PEND_RESCH;
    }

    if (OsPreemptable() && (percpu->schedFlag == INT_PEND_RESCH)) {
        percpu->schedFlag = INT_NO_RESCH;

        LOS_SpinLock(&g_taskSpin);

        OsSchedTaskEnQueue(runTask);

        LosTaskCB *newTask = OsGetTopTask();
        if (runTask != newTask) {
            OsSchedTaskSwicth(runTask, newTask);
            LOS_SpinUnlock(&g_taskSpin);
            return;
        }

        LOS_SpinUnlock(&g_taskSpin);
    }

    OsSchedUpdateExpireTime(runTask->startTime);
}

VOID OsSchedResched(VOID)
{
    LOS_ASSERT(LOS_SpinHeld(&g_taskSpin));
#if (LOSCFG_KERNEL_SMP == YES)
    LOS_ASSERT(OsPercpuGet()->taskLockCnt == 1);
#else
    LOS_ASSERT(OsPercpuGet()->taskLockCnt == 0);
#endif

    OsPercpuGet()->schedFlag = INT_NO_RESCH;
    LosTaskCB *runTask = OsCurrTaskGet();
    LosTaskCB *newTask = OsGetTopTask();
    if (runTask == newTask) {
        return;
    }

    OsSchedTaskSwicth(runTask, newTask);
}

VOID LOS_Schedule(VOID)
{
    UINT32 intSave;
    LosTaskCB *runTask = OsCurrTaskGet();

    if (OS_INT_ACTIVE) {
        OsPercpuGet()->schedFlag = INT_PEND_RESCH;
        return;
    }

    if (!OsPreemptable()) {
        return;
    }

    /*
     * trigger schedule in task will also do the slice check
     * if neccessary, it will give up the timeslice more in time.
     * otherwhise, there's no other side effects.
     */
    SCHEDULER_LOCK(intSave);

    OsTimeSliceUpdate(runTask, OsGerCurrSchedTimeCycle());

    /* add run task back to ready queue */
    OsSchedTaskEnQueue(runTask);

    /* reschedule to new thread */
    OsSchedResched();

    SCHEDULER_UNLOCK(intSave);
}

STATIC INLINE LOS_DL_LIST *OsSchedLockPendFindPosSub(const LosTaskCB *runTask, const LOS_DL_LIST *lockList)
{
    LosTaskCB *pendedTask = NULL;
    LOS_DL_LIST *node = NULL;

    LOS_DL_LIST_FOR_EACH_ENTRY(pendedTask, lockList, LosTaskCB, pendList) {
        if (pendedTask->priority < runTask->priority) {
            continue;
        } else if (pendedTask->priority > runTask->priority) {
            node = &pendedTask->pendList;
            break;
        } else {
            node = pendedTask->pendList.pstNext;
            break;
        }
    }

    return node;
}

LOS_DL_LIST *OsSchedLockPendFindPos(const LosTaskCB *runTask, LOS_DL_LIST *lockList)
{
    LOS_DL_LIST *node = NULL;

    if (LOS_ListEmpty(lockList)) {
        node = lockList;
    } else {
        LosTaskCB *pendedTask1 = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(lockList));
        LosTaskCB *pendedTask2 = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_LAST(lockList));
        if (pendedTask1->priority > runTask->priority) {
            node = lockList->pstNext;
        } else if (pendedTask2->priority <= runTask->priority) {
            node = lockList;
        } else {
            node = OsSchedLockPendFindPosSub(runTask, lockList);
        }
    }

    return node;
}

