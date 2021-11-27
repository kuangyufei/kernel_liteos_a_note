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
#ifdef LOSCFG_SCHED_DEBUG
#include "los_stat_pri.h"
#endif



#define OS_32BIT_MAX               0xFFFFFFFFUL
#define OS_SCHED_FIFO_TIMEOUT      0x7FFFFFFF
#define OS_PRIORITY_QUEUE_NUM      32	///< 就绪队列数量
#define PRIQUEUE_PRIOR0_BIT        0x80000000U
#define OS_SCHED_TIME_SLICES_MIN   ((5000 * OS_SYS_NS_PER_US) / OS_NS_PER_CYCLE)  /* 5ms 调度最小时间片 */
#define OS_SCHED_TIME_SLICES_MAX   ((LOSCFG_BASE_CORE_TIMESLICE_TIMEOUT * OS_SYS_NS_PER_US) / OS_NS_PER_CYCLE) ///< 调度最大时间片 
#define OS_SCHED_TIME_SLICES_DIFF  (OS_SCHED_TIME_SLICES_MAX - OS_SCHED_TIME_SLICES_MIN) ///< 最大,最小二者差
#define OS_SCHED_READY_MAX         30
#define OS_TIME_SLICE_MIN          (INT32)((50 * OS_SYS_NS_PER_US) / OS_NS_PER_CYCLE) /* 50us */

/// 调度队列
typedef struct {
    LOS_DL_LIST priQueueList[OS_PRIORITY_QUEUE_NUM];///< 各优先级任务调度队列,默认32级
    UINT32      readyTasks[OS_PRIORITY_QUEUE_NUM]; ///< 各优先级就绪任务数
    UINT32      queueBitmap; ///< 任务优先级调度位图
} SchedQueue;

/// 调度器
typedef struct {
    SchedQueue queueList[OS_PRIORITY_QUEUE_NUM];//进程优先级调度队列,默认32级
    UINT32     queueBitmap;//进程优先级调度位图
    SchedScan  taskScan;//函数指针,扫描任务的回调函数
    SchedScan  swtmrScan;//函数指针,扫描定时器的回调函数
} Sched;

STATIC Sched *g_sched = NULL;//全局调度器
STATIC UINT64 g_schedTickMaxResponseTime;
UINT64 g_sysSchedStartTime = OS_64BIT_MAX;

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
        UINT64 currTime = OsGetCurrSchedTimeCycle();
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
    UINT64 allTime;

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
        allTime = 0;
        for (UINT32 i = 1; i < schedData->index; i++) {
            allTime += data[i];
            UINT32 timeUs = (data[i] * OS_NS_PER_CYCLE) / OS_SYS_NS_PER_US;
            PRINTK("     %u(%u)", timeUs, timeUs / OS_US_PER_TICK);
            if ((i != 0) && ((i % 5) == 0)) { /* A row of 5 data */
                PRINTK("\n");
            }
        }

        allTime = (allTime * OS_NS_PER_CYCLE) / OS_SYS_NS_PER_US;
        PRINTK("\nTick Indicates the average response period: %llu(us)\n", allTime / (schedData->index - 1));
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
///< 设置节拍器类型
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
/// 设置调度开始时间
STATIC VOID OsSchedSetStartTime(UINT64 currCycle)
{
    if (g_sysSchedStartTime == OS_64BIT_MAX) {
        g_sysSchedStartTime = currCycle;
    }
}
/// 更新时间片
STATIC INLINE VOID OsTimeSliceUpdate(LosTaskCB *taskCB, UINT64 currTime)
{
    LOS_ASSERT(currTime >= taskCB->startTime); //断言参数时间必须大于开始时间

    INT32 incTime = (currTime - taskCB->startTime - taskCB->irqUsedTime);//计算增加的时间

    LOS_ASSERT(incTime >= 0);

    if (taskCB->policy == LOS_SCHED_RR) {//抢占调度
        taskCB->timeSlice -= incTime; //任务的时间片减少
#ifdef LOSCFG_SCHED_DEBUG
        taskCB->schedStat.timeSliceRealTime += incTime;
#endif
    }
    taskCB->irqUsedTime = 0;//中断时间置0
    taskCB->startTime = currTime;//重新设置开始时间

#ifdef LOSCFG_SCHED_DEBUG
    taskCB->schedStat.allRuntime += incTime;
#endif
}

STATIC INLINE VOID OsSchedTickReload(Percpu *currCpu, UINT64 nextResponseTime, UINT32 responseID, BOOL isTimeSlice)
{
    UINT64 currTime, nextExpireTime;
    UINT32 usedTime;

    currTime = OsGetCurrSchedTimeCycle();
    if (currCpu->tickStartTime != 0) {
        usedTime = currTime - currCpu->tickStartTime;
        currCpu->tickStartTime = 0;
    } else {
        usedTime = 0;
    }

    if ((nextResponseTime > usedTime) && ((nextResponseTime - usedTime) > OS_TICK_RESPONSE_PRECISION)) {
        nextResponseTime -= usedTime;
    } else {
        nextResponseTime = OS_TICK_RESPONSE_PRECISION;
    }

    nextExpireTime = currTime + nextResponseTime;
    if (nextExpireTime >= currCpu->responseTime) {
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
/// 设置下一个到期时间
STATIC INLINE VOID OsSchedSetNextExpireTime(UINT64 startTime, UINT32 responseID,
                                            UINT64 taskEndTime, UINT32 oldResponseID)
{
    UINT64 nextExpireTime = OsGetNextExpireTime(startTime);
    Percpu *currCpu = OsPercpuGet();
    UINT64 nextResponseTime;
    BOOL isTimeSlice = FALSE;

    currCpu->schedFlag &= ~INT_PEND_TICK;
    if (currCpu->responseID == oldResponseID) {
        /* This time has expired, and the next time the theory has expired is infinite */
        currCpu->responseTime = OS_SCHED_MAX_RESPONSE_TIME;
    }

    /* The current thread's time slice has been consumed, but the current system lock task cannot
     * trigger the schedule to release the CPU
     */
    if ((nextExpireTime > taskEndTime) && ((nextExpireTime - taskEndTime) > OS_SCHED_MINI_PERIOD)) {
        nextExpireTime = taskEndTime;
        isTimeSlice = TRUE;
    }

    if ((currCpu->responseTime > nextExpireTime) &&
        ((currCpu->responseTime - nextExpireTime) >= OS_TICK_RESPONSE_PRECISION)) {
        nextResponseTime = nextExpireTime - startTime;
        if (nextResponseTime > g_schedTickMaxResponseTime) {
            nextResponseTime = g_schedTickMaxResponseTime;
        }
    } else {
        /* There is no point earlier than the current expiration date */
        currCpu->tickStartTime = 0;
        return;
    }

    OsSchedTickReload(currCpu, nextResponseTime, responseID, isTimeSlice);
}

VOID OsSchedUpdateExpireTime(UINT64 startTime)
{
    UINT64 endTime;
    Percpu *cpu = OsPercpuGet();
    LosTaskCB *runTask = OsCurrTaskGet();

    if (!OS_SCHEDULER_ACTIVE || OS_INT_ACTIVE) {
        cpu->schedFlag |= INT_PEND_TICK;
        return;
    }

    if (runTask->policy == LOS_SCHED_RR) {
        LOS_SpinLock(&g_taskSpin);
        INT32 timeSlice = (runTask->timeSlice <= OS_TIME_SLICE_MIN) ? runTask->initTimeSlice : runTask->timeSlice;
        LOS_SpinUnlock(&g_taskSpin);
        endTime = startTime + timeSlice;
    } else {
        endTime = OS_SCHED_MAX_RESPONSE_TIME - OS_TICK_RESPONSE_PRECISION;
    }

    OsSchedSetNextExpireTime(startTime, runTask->taskID, endTime, runTask->taskID);
}
/// 计算时间片
STATIC INLINE UINT32 OsSchedCalculateTimeSlice(UINT16 proPriority, UINT16 priority)
{
    UINT32 retTime;
    UINT32 readyTasks;

    SchedQueue *queueList = &g_sched->queueList[proPriority];//拿到优先级调度队列
    readyTasks = queueList->readyTasks[priority];
    if (readyTasks > OS_SCHED_READY_MAX) {
        return OS_SCHED_TIME_SLICES_MIN;
    }
    retTime = ((OS_SCHED_READY_MAX - readyTasks) * OS_SCHED_TIME_SLICES_DIFF) / OS_SCHED_READY_MAX;
    return (retTime + OS_SCHED_TIME_SLICES_MIN);
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
///扫描那些处于等待状态的任务是否时间到了
STATIC INLINE BOOL OsSchedScanTimerList(VOID)
{
    Percpu *cpu = OsPercpuGet();
    BOOL needSchedule = FALSE;
    SortLinkAttribute *taskSortLink = &OsPercpuGet()->taskSortLink;//获取本CPU核上挂的所有等待的任务排序链表
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

    SortLinkList *sortList = LOS_DL_LIST_ENTRY(listObject->pstNext, SortLinkList, sortLinkNode);//获取每个优先级上的任务链表头节点
    UINT64 currTime = OsGetCurrSchedTimeCycle();
    while (sortList->responseTime <= currTime) {//
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

/*!
 * @brief OsSchedEnTaskQueue	
 * 添加任务到进程的就绪队列中
 * @param processCB	
 * @param taskCB	
 * @return	
 *
 * @see
 */
STATIC INLINE VOID OsSchedEnTaskQueue(LosTaskCB *taskCB, LosProcessCB *processCB)
{
    LOS_ASSERT(!(taskCB->taskStatus & OS_TASK_STATUS_READY));//必须是就绪状态,因为只有就绪状态才能入就绪队列

    switch (taskCB->policy) {
        case LOS_SCHED_RR: {//抢占式跳读
            if (taskCB->timeSlice > OS_TIME_SLICE_MIN) {//时间片大于最小的时间片 50微妙
                OsSchedPriQueueEnHead(processCB->priority, &taskCB->pendList, taskCB->priority);//插入对应优先级的就绪队列中
            } else {//如果时间片不够了,咋办?
                taskCB->initTimeSlice = OsSchedCalculateTimeSlice(processCB->priority, taskCB->priority);//重新计算时间片
                taskCB->timeSlice = taskCB->initTimeSlice;//
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
/// 将任务从就绪队列中删除
VOID OsSchedTaskDeQueue(LosTaskCB *taskCB)
{
    LosProcessCB *processCB = OS_PCB_FROM_PID(taskCB->processID);//通过任务获取所在进程

    if (taskCB->taskStatus & OS_TASK_STATUS_READY) {//任务处于就绪状态
        OsSchedDeTaskQueue(taskCB, processCB);//从该进程的就绪任务队列中删除
    }

    if (processCB->processStatus & OS_PROCESS_STATUS_READY) { //进程处于就绪状态不能删除任务,这是为什么呢 ? @note_thinking 
        return;
    }

    /* If the current process has only the current thread running,
     * the process becomes blocked after the thread leaves the scheduling queue
     * 如果当前进程只有当前任务在运行,任务离开调度队列后进程设置为阻塞状态
     * 注意:一个进程下的任务可能同时被多个CPU在并行运行.
     */
    if (OS_PROCESS_GET_RUNTASK_COUNT(processCB->processStatus) == 1) { //根据数量来这只状态,注意 processStatus 承载了两重含义
        processCB->processStatus |= OS_PROCESS_STATUS_PENDING; //将进程状态设为阻塞状态
    }
}

VOID OsSchedTaskEnQueue(LosTaskCB *taskCB)
{
    LosProcessCB *processCB = OS_PCB_FROM_PID(taskCB->processID);
#ifdef LOSCFG_SCHED_DEBUG
    if (!(taskCB->taskStatus & OS_TASK_STATUS_RUNNING)) {
        taskCB->startTime = OsGetCurrSchedTimeCycle();
    }
#endif
    OsSchedEnTaskQueue(taskCB, processCB);
}
/// 任务退出
VOID OsSchedTaskExit(LosTaskCB *taskCB)
{
    LosProcessCB *processCB = OS_PCB_FROM_PID(taskCB->processID);

    if (taskCB->taskStatus & OS_TASK_STATUS_READY) {//就绪状态
        OsSchedTaskDeQueue(taskCB);//从就绪队列中删除
        processCB->processStatus &= ~OS_PROCESS_STATUS_PENDING;//进程贴上非挂起标签
    } else if (taskCB->taskStatus & OS_TASK_STATUS_PENDING) { //挂起状态
        LOS_ListDelete(&taskCB->pendList);	//从任务挂起链表中摘除
        taskCB->taskStatus &= ~OS_TASK_STATUS_PENDING;////任务贴上非挂起标签
    }

    if (taskCB->taskStatus & (OS_TASK_STATUS_DELAY | OS_TASK_STATUS_PEND_TIME)) {
        OsDeleteSortLink(&taskCB->sortList, OS_SORT_LINK_TASK);
        taskCB->taskStatus &= ~(OS_TASK_STATUS_DELAY | OS_TASK_STATUS_PEND_TIME);
    }
}
///通过本函数可以看出 yield 的真正含义是主动让出CPU,那怎么安置自己呢? 跑到末尾重新排队. 真是个活雷锋,好同志啊!!!
VOID OsSchedYield(VOID)
{
    LosTaskCB *runTask = OsCurrTaskGet();

    runTask->timeSlice = 0;//时间片变成0,代表主动让出运行时间.

    runTask->startTime = OsGetCurrSchedTimeCycle();//重新获取开始时间
    OsSchedTaskEnQueue(runTask);//跑队列尾部排队
    OsSchedResched();//发起调度
}
///延期调度
VOID OsSchedDelay(LosTaskCB *runTask, UINT32 tick)
{
    OsSchedTaskDeQueue(runTask);//将任务从就绪队列中删除
    runTask->taskStatus |= OS_TASK_STATUS_DELAY;//任务状态改成延期
    runTask->waitTimes = tick;//延期节拍数

    OsSchedResched();//既然本任务延期,就需要发起新的调度.
}
///任务进入等待链表
UINT32 OsSchedTaskWait(LOS_DL_LIST *list, UINT32 ticks, BOOL needSched)
{
    LosTaskCB *runTask = OsCurrTaskGet();//获取当前任务
    OsSchedTaskDeQueue(runTask);//将任务从就绪队列中删除

    runTask->taskStatus |= OS_TASK_STATUS_PENDING;//任务状态改成阻塞
    LOS_ListTailInsert(list, &runTask->pendList);//挂入阻塞链表

    if (ticks != LOS_WAIT_FOREVER) {//如果不是永久的等待
        runTask->taskStatus |= OS_TASK_STATUS_PEND_TIME;//标记为有时间的阻塞
        runTask->waitTimes = ticks;//要阻塞多久
    }

    if (needSched == TRUE) {//是否需要调度
        OsSchedResched();//申请调度,将切换任务上下文
        if (runTask->taskStatus & OS_TASK_STATUS_TIMEOUT) {
            runTask->taskStatus &= ~OS_TASK_STATUS_TIMEOUT;
            return LOS_ERRNO_TSK_TIMEOUT;
        }
    }

    return LOS_OK;
}
///任务从等待链表中恢复,并从链表中摘除.
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
        resumedTask->schedStat.pendTime += OsGetCurrSchedTimeCycle() - resumedTask->startTime;
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
    OsHookCall(LOS_HOOK_TYPE_TASK_PRIMODIFY, taskCB, taskCB->priority); 
    if (taskCB->taskStatus & OS_TASK_STATUS_INIT) {
        OsSchedTaskEnQueue(taskCB);
        return TRUE;
    }

    if (taskCB->taskStatus & OS_TASK_STATUS_RUNNING) {
        return TRUE;
    }

    return FALSE;
}
///修改进程调度参数
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
//由时钟发起的调度
VOID OsSchedTick(VOID)
{
    Sched *sched = g_sched;	//获取全局调度器
    Percpu *currCpu = OsPercpuGet(); //获取当前CPU
    BOOL needSched = FALSE;
    LosTaskCB *runTask = OsCurrTaskGet(); //获取当前任务

    currCpu->tickStartTime = runTask->irqStartTime;//将任务的中断开始时间给CPU的tick开始时间,这个做的目的是什么呢 ? @note_thinking 
    if (currCpu->responseID == OS_INVALID_VALUE) {
        if (sched->swtmrScan != NULL) {
            (VOID)sched->swtmrScan();//扫描软件定时器 实体函数是: OsSwtmrScan
        }

        needSched = sched->taskScan();//扫描任务, 实体函数是: OsSchedScanTimerList

        if (needSched) {//若需调度
            LOS_MpSchedule(OS_MP_CPU_ALL);//当前CPU向所有cpu发起核间中断,让其发生一次调度
            currCpu->schedFlag |= INT_PEND_RESCH; //内因触发的调度
        }
    }
    currCpu->schedFlag |= INT_PEND_TICK; //贴上外因触发的调度,这个外因指的就是tick时间到了
    currCpu->responseTime = OS_SCHED_MAX_RESPONSE_TIME;//响应时间默认设最大
}

VOID OsSchedSetIdleTaskSchedParam(LosTaskCB *idleTask)
{
    idleTask->policy = LOS_SCHED_IDLE;
    idleTask->initTimeSlice = OS_SCHED_FIFO_TIMEOUT;
    idleTask->timeSlice = idleTask->initTimeSlice;
    OsSchedTaskEnQueue(idleTask);
}
/// 向全局调度器注册扫描软件定时器的回调函数
UINT32 OsSchedSwtmrScanRegister(SchedScan func)
{
    if (func == NULL) {
        return LOS_NOK;
    }

    g_sched->swtmrScan = func;//正式注册, func 将在 OsSchedTick 中被回调
    return LOS_OK;
}
///调度初始化
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

    g_sched->taskScan = OsSchedScanTimerList;// 注册回调函数,扫描那些处于等待状态的任务是否时间到了,将在 OsSchedTick 中回调

#ifdef LOSCFG_SCHED_TICK_DEBUG
    ret = OsSchedDebugInit();
    if (ret != LOS_OK) {
        return ret;
    }
#endif
    return LOS_OK;
}
/// 获取就绪队列中优先级最高的任务
STATIC LosTaskCB *OsGetTopTask(VOID)
{
    UINT32 priority, processPriority;
    UINT32 bitmap;
    LosTaskCB *newTask = NULL;
    UINT32 processBitmap = g_sched->queueBitmap;
#ifdef LOSCFG_KERNEL_SMP
    UINT32 cpuid = ArchCurrCpuid();
#endif

    while (processBitmap) {
        processPriority = CLZ(processBitmap);
        SchedQueue *queueList = &g_sched->queueList[processPriority];
        bitmap = queueList->queueBitmap;
            while (bitmap) {
                priority = CLZ(bitmap);
                LOS_DL_LIST_FOR_EACH_ENTRY(newTask, &queueList->priQueueList[priority], LosTaskCB, pendList) {
#ifdef LOSCFG_KERNEL_SMP
                    if (newTask->cpuAffiMask & (1U << cpuid)) {
#endif
                        goto FIND_TASK;
#ifdef LOSCFG_KERNEL_SMP
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
///CPU的调度开始,每个CPU核都会执行这个函数一次.
VOID OsSchedStart(VOID)
{
    UINT32 cpuid = ArchCurrCpuid();//从系统寄存器上获取当前执行的CPU核编号
    UINT32 intSave;

    SCHEDULER_LOCK(intSave);

    if (cpuid == 0) {
    OsTickStart();//开始了属于本核的tick 
    }

    LosTaskCB *newTask = OsGetTopTask();//拿一个优先级最高的任务
    LosProcessCB *newProcess = OS_PCB_FROM_PID(newTask->processID);//获取该任务的进程实体

    newTask->taskStatus |= OS_TASK_STATUS_RUNNING;//变成运行状态,注意此时该任务还没真正的运行
    newProcess->processStatus |= OS_PROCESS_STATUS_RUNNING;//processStatus 具有两重含义,将它设为正在运行状态,但注意这是在进程的角度,实际上底层还没有切到它运行.
    newProcess->processStatus = OS_PROCESS_RUNTASK_COUNT_ADD(newProcess->processStatus);//当前任务的数量也增加一个

    OsSchedSetStartTime(HalClockGetCycles());//设置调度开始时间
    newTask->startTime = OsGetCurrSchedTimeCycle();

#ifdef LOSCFG_KERNEL_SMP //注意：需要设置当前cpu，以防第一个任务删除可能会失败，因为此标志与实际当前 cpu 不匹配。
    /*
     * attention: current cpu needs to be set, in case first task deletion
     * may fail because this flag mismatch with the real current cpu.
     */
    newTask->currCpu = cpuid;//设置当前CPU,确保第一个任务由本CPU核执行
#endif

    OsCurrTaskSet((VOID *)newTask);

    /* System start schedule */
    OS_SCHEDULER_SET(cpuid);

    OsPercpuGet()->responseID = OS_INVALID;
    OsSchedSetNextExpireTime(newTask->startTime, newTask->taskID, newTask->startTime + newTask->timeSlice, OS_INVALID);

    PRINTK("cpu %d entering scheduler\n", cpuid);
    OsTaskContextLoad(newTask);
}

#ifdef LOSCFG_KERNEL_SMP
VOID OsSchedToUserReleaseLock(VOID)
{
    /* The scheduling lock needs to be released before returning to user mode */
    LOCKDEP_CHECK_OUT(&g_taskSpin);
    ArchSpinUnlock(&g_taskSpin.rawLock);

    OsPercpuGet()->taskLockCnt--;
}
#endif

#ifdef LOSCFG_BASE_CORE_TSK_MONITOR
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
#ifdef LOSCFG_BASE_CORE_TSK_MONITOR
    OsTaskStackCheck(runTask, newTask);
#endif /* LOSCFG_BASE_CORE_TSK_MONITOR */
    OsHookCall(LOS_HOOK_TYPE_TASK_SWITCHEDIN, newTask, runTask);
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

/*!
 * @brief OsSchedTaskSwitch	实现新老两个任务切换
 *
 * @param newTask 	
 * @param runTask	
 * @return	
 *
 * @see
 */
STATIC VOID OsSchedTaskSwitch(LosTaskCB *runTask, LosTaskCB *newTask)
{
    UINT64 endTime;

    OsSchedSwitchCheck(runTask, newTask);//任务内容检查

    runTask->taskStatus &= ~OS_TASK_STATUS_RUNNING; //当前任务去掉正在运行的标签
    newTask->taskStatus |= OS_TASK_STATUS_RUNNING;	//新任务贴上正在运行的标签,虽标签贴上了,但目前还是在老任务中跑.

#ifdef LOSCFG_KERNEL_SMP
    /* mask new running task's owner processor */
    runTask->currCpu = OS_TASK_INVALID_CPUID;//褫夺当前任务的CPU使用权
    newTask->currCpu = ArchCurrCpuid(); //标记新任务获取当前CPU使用权
#endif

    OsCurrTaskSet((VOID *)newTask);//设置新任务为当前任务
    LosProcessCB *newProcess = OS_PCB_FROM_PID(newTask->processID);//获取新任务所在进程实体
    LosProcessCB *runProcess = OS_PCB_FROM_PID(runTask->processID);//获取老任务所在进程实体
    if (runProcess != newProcess) {//如果不是同一个进程,就需要换行进程上下文,也就是切换MMU,切换进程空间
        OsSchedSwitchProcess(runProcess, newProcess);//切换进程上下文
    }

    if (OsProcessIsUserMode(newProcess)) {//如果是用户模式即应用进程
        OsCurrUserTaskSet(newTask->userArea);//设置用户态栈空间
    }

#ifdef LOSCFG_KERNEL_CPUP
    OsCpupCycleEndStart(runTask->taskID, newTask->taskID);
#endif

#ifdef LOSCFG_SCHED_DEBUG
    UINT64 waitStartTime = newTask->startTime;
#endif
    if (runTask->taskStatus & OS_TASK_STATUS_READY) {//注意老任务可不一定是就绪状态
        /* When a thread enters the ready queue, its slice of time is updated 
		当一个线程(任务)进入就绪队列时，它的时间片被更新 */
        newTask->startTime = runTask->startTime;
    } else {
        /* The currently running task is blocked */
        newTask->startTime = OsGetCurrSchedTimeCycle();//重新获取时间
        /* The task is in a blocking state and needs to update its time slice before pend */
        OsTimeSliceUpdate(runTask, newTask->startTime);//更新时间片
		//两种状态下将老任务放入CPU的工作链表中
        if (runTask->taskStatus & (OS_TASK_STATUS_PEND_TIME | OS_TASK_STATUS_DELAY)) {
            OsAdd2SortLink(&runTask->sortList, runTask->startTime, runTask->waitTimes, OS_SORT_LINK_TASK);
        }
    }

    if (newTask->policy == LOS_SCHED_RR) {
        endTime = newTask->startTime + newTask->timeSlice;
    } else {
        endTime = OS_SCHED_MAX_RESPONSE_TIME - OS_TICK_RESPONSE_PRECISION;
    }
    OsSchedSetNextExpireTime(newTask->startTime, newTask->taskID, endTime, runTask->taskID);

#ifdef LOSCFG_SCHED_DEBUG
    newTask->schedStat.waitSchedTime += newTask->startTime - waitStartTime;
    newTask->schedStat.waitSchedCount++;
    runTask->schedStat.runTime = runTask->schedStat.allRuntime;
    runTask->schedStat.switchCount++;
#endif
    /* do the task context switch */
    OsTaskSchedule(newTask, runTask); //执行汇编代码,,注意OsTaskSchedule是一个汇编函数 见于 los_dispatch.s
}

VOID OsSchedIrqEndCheckNeedSched(VOID)
{
    Percpu *percpu = OsPercpuGet();
    LosTaskCB *runTask = OsCurrTaskGet();

    OsTimeSliceUpdate(runTask, OsGetCurrSchedTimeCycle());
    if (runTask->timeSlice <= OS_TIME_SLICE_MIN) {
        percpu->schedFlag |= INT_PEND_RESCH;
    }

    if (OsPreemptable() && (percpu->schedFlag & INT_PEND_RESCH)) {
        percpu->schedFlag &= ~INT_PEND_RESCH;

        LOS_SpinLock(&g_taskSpin);

        OsSchedTaskEnQueue(runTask);

        LosTaskCB *newTask = OsGetTopTask();
        if (runTask != newTask) {
            OsSchedTaskSwitch(runTask, newTask);
            LOS_SpinUnlock(&g_taskSpin);
            return;
        }

        LOS_SpinUnlock(&g_taskSpin);
    }

    if (percpu->schedFlag & INT_PEND_TICK) {
        OsSchedUpdateExpireTime(runTask->startTime);
    }
}
/// 申请一次调度
VOID OsSchedResched(VOID)
{
    LOS_ASSERT(LOS_SpinHeld(&g_taskSpin));
#ifdef LOSCFG_KERNEL_SMP
    LOS_ASSERT(OsPercpuGet()->taskLockCnt == 1); // @note_thinking 为何此处一定得 == 1, 大于1不行吗?  
#else
    LOS_ASSERT(OsPercpuGet()->taskLockCnt == 0);
#endif

    OsPercpuGet()->schedFlag &= ~INT_PEND_RESCH;//去掉标签
    LosTaskCB *runTask = OsCurrTaskGet();
    LosTaskCB *newTask = OsGetTopTask();//获取最高优先级任务
    if (runTask == newTask) {
        return;
    }

    OsSchedTaskSwitch(runTask, newTask);//CPU将真正的换任务执行
}

/*!
 * @brief LOS_Schedule	任务调度主函数,触发系统调度
 *
 * @return	
 *
 * @see
 */
VOID LOS_Schedule(VOID)
{
    UINT32 intSave;
    LosTaskCB *runTask = OsCurrTaskGet();

    if (OS_INT_ACTIVE) { //中断发生中...,需停止调度
        OsPercpuGet()->schedFlag |= INT_PEND_RESCH;//贴上原因
        return;
    }

    if (!OsPreemptable()) {//当不可抢占时直接返回
        return;
    }

    /*
     * trigger schedule in task will also do the slice check
     * if neccessary, it will give up the timeslice more in time.
     * otherwhise, there's no other side effects.
     */
    SCHEDULER_LOCK(intSave);

    OsTimeSliceUpdate(runTask, OsGetCurrSchedTimeCycle());//更新时间片

    /* add run task back to ready queue | 添加任务到就绪队列*/
    OsSchedTaskEnQueue(runTask); 

    /* reschedule to new thread | 申请调度,CPU可能将换任务执行*/
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

