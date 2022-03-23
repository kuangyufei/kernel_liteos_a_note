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
#ifdef LOSCFG_SCHED_DEBUG
#include "los_stat_pri.h"
#endif
#include "los_pm_pri.h"

#define OS_32BIT_MAX               0xFFFFFFFFUL
#define OS_SCHED_FIFO_TIMEOUT      0x7FFFFFFF
#define OS_PRIORITY_QUEUE_NUM      32	///< 就绪队列数量
#define PRIQUEUE_PRIOR0_BIT        0x80000000U
#define OS_SCHED_TIME_SLICES_MIN   ((5000 * OS_SYS_NS_PER_US) / OS_NS_PER_CYCLE)  /* 5ms 产生的时钟周期数 调度最小时间片 */
#define OS_SCHED_TIME_SLICES_MAX   ((LOSCFG_BASE_CORE_TIMESLICE_TIMEOUT * OS_SYS_NS_PER_US) / OS_NS_PER_CYCLE) ///< 调度最大时间片 20ms产生的时钟周期数
#define OS_SCHED_TIME_SLICES_DIFF  (OS_SCHED_TIME_SLICES_MAX - OS_SCHED_TIME_SLICES_MIN) ///< 最大,最小二者差
#define OS_SCHED_READY_MAX         30 ///< 某个任务优先级上挂的最大就绪任务数量
#define OS_TIME_SLICE_MIN          (INT32)((50 * OS_SYS_NS_PER_US) / OS_NS_PER_CYCLE) /* 50us */

#define OS_TASK_STATUS_BLOCKED     (OS_TASK_STATUS_INIT | OS_TASK_STATUS_PENDING | \
                                    OS_TASK_STATUS_DELAY | OS_TASK_STATUS_PEND_TIME)

typedef struct {
    LOS_DL_LIST priQueueList[OS_PRIORITY_QUEUE_NUM];///< 各优先级任务调度队列,默认32级
    UINT32      readyTasks[OS_PRIORITY_QUEUE_NUM]; ///< 各优先级就绪任务数
    UINT32      queueBitmap; ///< 任务优先级调度位图
} SchedQueue;

/// 调度器
typedef struct {
    SchedQueue queueList[OS_PRIORITY_QUEUE_NUM];///< 进程优先级调度队列,默认32级
    UINT32     queueBitmap;///< 进程优先级调度位图
} Sched;

SchedRunQue g_schedRunQue[LOSCFG_KERNEL_CORE_NUM];
STATIC Sched g_sched;

#ifdef LOSCFG_SCHED_TICK_DEBUG
#define OS_SCHED_DEBUG_DATA_NUM  1000
typedef struct {//调试调度,每个CPU核一个
    UINT32 tickResporeTime[OS_SCHED_DEBUG_DATA_NUM];
    UINT32 index;
    UINT32 setTickCount;
    UINT64 oldResporeTime;
} SchedTickDebug;
STATIC SchedTickDebug *g_schedTickDebug = NULL;///< 全局调度调试池
/// 调试调度器初始化
STATIC UINT32 OsSchedDebugInit(VOID)
{
    UINT32 size = sizeof(SchedTickDebug) * LOSCFG_KERNEL_CORE_NUM;//见核有份
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
        SchedRunQue *rq = OsSchedRunQueByID(cpu);
        sortLinkNum[cpu] = OsGetSortLinkNodeNum(&rq->taskSortLink);
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
#endif

#ifdef LOSCFG_SCHED_DEBUG
STATIC VOID SchedDataGet(LosTaskCB *taskCB, UINT64 *runTime, UINT64 *timeSlice, UINT64 *pendTime, UINT64 *schedWait)
{
    if (taskCB->schedStat.switchCount >= 1) {
        UINT64 averRunTime = taskCB->schedStat.runTime / taskCB->schedStat.switchCount;
        *runTime = (averRunTime * OS_NS_PER_CYCLE) / OS_SYS_NS_PER_US;
    }

    if (taskCB->schedStat.timeSliceCount > 1) {
        UINT64 averTimeSlice = taskCB->schedStat.timeSliceTime / (taskCB->schedStat.timeSliceCount - 1);
        *timeSlice = (averTimeSlice * OS_NS_PER_CYCLE) / OS_SYS_NS_PER_US;
    }

    if (taskCB->schedStat.pendCount > 1) {
        UINT64 averPendTime = taskCB->schedStat.pendTime / taskCB->schedStat.pendCount;
        *pendTime = (averPendTime * OS_NS_PER_CYCLE) / OS_SYS_NS_PER_US;
    }

    if (taskCB->schedStat.waitSchedCount > 0) {
        UINT64 averSchedWait = taskCB->schedStat.waitSchedTime / taskCB->schedStat.waitSchedCount;
        *schedWait = (averSchedWait * OS_NS_PER_CYCLE) / OS_SYS_NS_PER_US;
    }
}

UINT32 OsShellShowSchedParam(VOID)
{
    UINT64 averRunTime;
    UINT64 averTimeSlice;
    UINT64 averSchedWait;
    UINT64 averPendTime;
    UINT32 taskLinkNum[LOSCFG_KERNEL_CORE_NUM];
    UINT32 intSave;
    UINT32 size = g_taskMaxNum * sizeof(LosTaskCB);
    LosTaskCB *taskCBArray = LOS_MemAlloc(m_aucSysMem1, size);
    if (taskCBArray == NULL) {
        return LOS_NOK;
    }

    SCHEDULER_LOCK(intSave);
    (VOID)memcpy_s(taskCBArray, size, g_taskCBArray, size);
    for (UINT16 cpu = 0; cpu < LOSCFG_KERNEL_CORE_NUM; cpu++) {
        SchedRunQue *rq = OsSchedRunQueByID(cpu);
        taskLinkNum[cpu] = OsGetSortLinkNodeNum(&rq->taskSortLink);
    }
    SCHEDULER_UNLOCK(intSave);

    for (UINT16 cpu = 0; cpu < LOSCFG_KERNEL_CORE_NUM; cpu++) {
        PRINTK("cpu: %u Task SortMax: %u\n", cpu, taskLinkNum[cpu]);
    }

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

        SchedDataGet(taskCB, &averRunTime, &averTimeSlice, &averPendTime, &averSchedWait);

        PRINTK("%5u%19llu%15llu%19llu%18llu%19llu%18llu  %-32s\n", taskCB->taskID,
               averRunTime, taskCB->schedStat.switchCount,
               averTimeSlice, taskCB->schedStat.timeSliceCount - 1,
               averSchedWait, averPendTime, taskCB->taskName);
    }

    (VOID)LOS_MemFree(m_aucSysMem1, taskCBArray);

    return LOS_OK;
}
#endif

/// 更新时间片
STATIC INLINE VOID TimeSliceUpdate(LosTaskCB *taskCB, UINT64 currTime)
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

STATIC INLINE VOID SchedSetNextExpireTime(UINT32 responseID, UINT64 taskEndTime, UINT32 oldResponseID)
{
    SchedRunQue *rq = OsSchedRunQue();
    BOOL isTimeSlice = FALSE;
    UINT64 currTime = OsGetCurrSchedTimeCycle();
    UINT64 nextExpireTime = OsGetSortLinkNextExpireTime(&rq->taskSortLink, currTime, OS_TICK_RESPONSE_PRECISION);

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

#ifdef LOSCFG_SCHED_TICK_DEBUG
    SchedTickDebug *schedDebug = &g_schedTickDebug[ArchCurrCpuid()];
    if (schedDebug->index < OS_SCHED_DEBUG_DATA_NUM) {
        schedDebug->setTickCount++;
    }
#endif
}

/// 更新过期时间
VOID OsSchedUpdateExpireTime(VOID)
{
    UINT64 endTime;
    LosTaskCB *runTask = OsCurrTaskGet();

    if (!OS_SCHEDULER_ACTIVE || OS_INT_ACTIVE) {//如果中断发生中或调度未发生
        OsSchedRunQuePendingSet();
        return;
    }

    if (runTask->policy == LOS_SCHED_RR) {//抢占式调度计算出当前任务的到期时间,时间到了就需要切换到其他任务运行
        LOS_SpinLock(&g_taskSpin);
        INT32 timeSlice = (runTask->timeSlice <= OS_TIME_SLICE_MIN) ? runTask->initTimeSlice : runTask->timeSlice;//修改时间片,以时钟周期为单位
        endTime = runTask->startTime + timeSlice;
        LOS_SpinUnlock(&g_taskSpin);
    } else {
        endTime = OS_SCHED_MAX_RESPONSE_TIME - OS_TICK_RESPONSE_PRECISION;
    }

    SchedSetNextExpireTime(runTask->taskID, endTime, runTask->taskID);
}
/// 计算时间片,参数为进程优先级和任务优先级, 结论是同一个优先级下 任务越多,时间片越短
STATIC INLINE UINT32 SchedCalculateTimeSlice(UINT16 proPriority, UINT16 priority)
{
    UINT32 retTime;
    UINT32 readyTasks;

    SchedQueue *queueList = &g_sched.queueList[proPriority];
    readyTasks = queueList->readyTasks[priority];//获取指定优先级的任务数量
    if (readyTasks > OS_SCHED_READY_MAX) {//如果就绪任务很多
        return OS_SCHED_TIME_SLICES_MIN;//给个最小时间片 5ms
    }// 0:15 | 1: 14.5 | 2:14 | 30: 0  
    retTime = ((OS_SCHED_READY_MAX - readyTasks) * OS_SCHED_TIME_SLICES_DIFF) / OS_SCHED_READY_MAX;//时间片的算法
    return (retTime + OS_SCHED_TIME_SLICES_MIN);// 20, 19.5, ... 5
}

STATIC INLINE VOID SchedPriQueueEnHead(UINT32 proPriority, LOS_DL_LIST *priqueueItem, UINT32 priority)
{
    SchedQueue *queueList = &g_sched.queueList[proPriority];
    LOS_DL_LIST *priQueueList = &queueList->priQueueList[0];
    UINT32 *bitMap = &queueList->queueBitmap;

    /*
     * Task control blocks are inited as zero. And when task is deleted,
     * and at the same time would be deleted from priority queue or
     * other lists, task pend node will restored as zero.
     */
    LOS_ASSERT(priqueueItem->pstNext == NULL);

    if (*bitMap == 0) {
        g_sched.queueBitmap |= PRIQUEUE_PRIOR0_BIT >> proPriority;
    }

    if (LOS_ListEmpty(&priQueueList[priority])) { //链表为空
        *bitMap |= PRIQUEUE_PRIOR0_BIT >> priority;//对应位 置0
    }

    LOS_ListHeadInsert(&priQueueList[priority], priqueueItem);//从就绪任务链表头部插入
    queueList->readyTasks[priority]++;//就绪任务数增加
}
/// 从就绪任务优先级链表尾部插入
STATIC INLINE VOID SchedPriQueueEnTail(UINT32 proPriority, LOS_DL_LIST *priqueueItem, UINT32 priority)
{
    SchedQueue *queueList = &g_sched.queueList[proPriority];
    LOS_DL_LIST *priQueueList = &queueList->priQueueList[0];
    UINT32 *bitMap = &queueList->queueBitmap;

    /*
     * Task control blocks are inited as zero. And when task is deleted,
     * and at the same time would be deleted from priority queue or
     * other lists, task pend node will restored as zero.
     */
    LOS_ASSERT(priqueueItem->pstNext == NULL);

    if (*bitMap == 0) {
        g_sched.queueBitmap |= PRIQUEUE_PRIOR0_BIT >> proPriority;
    }

    if (LOS_ListEmpty(&priQueueList[priority])) {
        *bitMap |= PRIQUEUE_PRIOR0_BIT >> priority;
    }

    LOS_ListTailInsert(&priQueueList[priority], priqueueItem);
    queueList->readyTasks[priority]++;
}
/// 1.找出指定优先级的调度队列 2.摘除指定任务优先级链表中的指定任务节点
STATIC INLINE VOID SchedPriQueueDelete(UINT32 proPriority, LOS_DL_LIST *priqueueItem, UINT32 priority)
{
    SchedQueue *queueList = &g_sched.queueList[proPriority];
    LOS_DL_LIST *priQueueList = &queueList->priQueueList[0];//获取首个链表
    UINT32 *bitMap = &queueList->queueBitmap;//获取位图

    LOS_ListDelete(priqueueItem);//摘除参数结点
    queueList->readyTasks[priority]--;//对应任务数减少
    if (LOS_ListEmpty(&priQueueList[priority])) {//如果该条线上没有了任务了
        *bitMap &= ~(PRIQUEUE_PRIOR0_BIT >> priority);//将任务对应优先级的位 置0
    }// ...111001110 --> ...111000110 

    if (*bitMap == 0) {//如果变量变成0, 000000000
        g_sched.queueBitmap &= ~(PRIQUEUE_PRIOR0_BIT >> proPriority);
    }
}

STATIC INLINE VOID SchedEnTaskQueue(LosTaskCB *taskCB)
{
    LOS_ASSERT(!(taskCB->taskStatus & OS_TASK_STATUS_READY));

    switch (taskCB->policy) {
        case LOS_SCHED_RR: {
            if (taskCB->timeSlice > OS_TIME_SLICE_MIN) {
                SchedPriQueueEnHead(taskCB->basePrio, &taskCB->pendList, taskCB->priority);
            } else {
                taskCB->initTimeSlice = SchedCalculateTimeSlice(taskCB->basePrio, taskCB->priority);
                taskCB->timeSlice = taskCB->initTimeSlice;
                SchedPriQueueEnTail(taskCB->basePrio, &taskCB->pendList, taskCB->priority);
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
                SchedPriQueueEnHead(taskCB->basePrio, &taskCB->pendList, taskCB->priority);
            } else {
                taskCB->initTimeSlice = OS_SCHED_FIFO_TIMEOUT;
                taskCB->timeSlice = taskCB->initTimeSlice;
                SchedPriQueueEnTail(taskCB->basePrio, &taskCB->pendList, taskCB->priority);
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
}

VOID OsSchedTaskDeQueue(LosTaskCB *taskCB)
{
    if (taskCB->taskStatus & OS_TASK_STATUS_READY) {
        if (taskCB->policy != LOS_SCHED_IDLE) {
            SchedPriQueueDelete(taskCB->basePrio, &taskCB->pendList, taskCB->priority);
        }
        taskCB->taskStatus &= ~OS_TASK_STATUS_READY;
    }
}

VOID OsSchedTaskEnQueue(LosTaskCB *taskCB)
{
#ifdef LOSCFG_SCHED_DEBUG
    if (!(taskCB->taskStatus & OS_TASK_STATUS_RUNNING)) {
        taskCB->startTime = OsGetCurrSchedTimeCycle();
    }
#endif
    SchedEnTaskQueue(taskCB);
}

VOID OsSchedTaskExit(LosTaskCB *taskCB)
{
    if (taskCB->taskStatus & OS_TASK_STATUS_READY) {
        OsSchedTaskDeQueue(taskCB);
    } else if (taskCB->taskStatus & OS_TASK_STATUS_PENDING) {
        LOS_ListDelete(&taskCB->pendList);
        taskCB->taskStatus &= ~OS_TASK_STATUS_PENDING;
    }

    if (taskCB->taskStatus & (OS_TASK_STATUS_DELAY | OS_TASK_STATUS_PEND_TIME)) {
        OsSchedDeTaskFromTimeList(taskCB);
        taskCB->taskStatus &= ~(OS_TASK_STATUS_DELAY | OS_TASK_STATUS_PEND_TIME);
    }
}

VOID OsSchedYield(VOID)
{
    LosTaskCB *runTask = OsCurrTaskGet();

    runTask->timeSlice = 0;

    runTask->startTime = OsGetCurrSchedTimeCycle();
    OsSchedTaskEnQueue(runTask);
    OsSchedResched();
}

VOID OsSchedDelay(LosTaskCB *runTask, UINT64 waitTime)
{
    runTask->taskStatus |= OS_TASK_STATUS_DELAY;
    runTask->waitTime = waitTime;

    OsSchedResched();
}

UINT32 OsSchedTaskWait(LOS_DL_LIST *list, UINT32 ticks, BOOL needSched)
{
    LosTaskCB *runTask = OsCurrTaskGet();

    runTask->taskStatus |= OS_TASK_STATUS_PENDING;
    LOS_ListTailInsert(list, &runTask->pendList);

    if (ticks != LOS_WAIT_FOREVER) {
        runTask->taskStatus |= OS_TASK_STATUS_PEND_TIME;
        runTask->waitTime = OS_SCHED_TICK_TO_CYCLE(ticks);
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
        OsSchedDeTaskFromTimeList(resumedTask);
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

BOOL OsSchedModifyProcessSchedParam(UINT32 pid, UINT16 policy, UINT16 priority)
{
    LosProcessCB *processCB = OS_PCB_FROM_PID(pid);
    LosTaskCB *taskCB = NULL;
    BOOL needSched = FALSE;
    (VOID)policy;

    LOS_DL_LIST_FOR_EACH_ENTRY(taskCB, &processCB->threadSiblingList, LosTaskCB, threadList) {
        if (taskCB->taskStatus & OS_TASK_STATUS_READY) {
            SchedPriQueueDelete(taskCB->basePrio, &taskCB->pendList, taskCB->priority);
            SchedPriQueueEnTail(priority, &taskCB->pendList, taskCB->priority);
            needSched = TRUE;
        } else if (taskCB->taskStatus & OS_TASK_STATUS_RUNNING) {
            needSched = TRUE;
        }
        taskCB->basePrio = priority;
    }

    return needSched;
}

STATIC VOID SchedFreezeTask(LosTaskCB *taskCB)
{
    UINT64 responseTime;

    if (!OsIsPmMode()) {
        return;
    }

    if (!(taskCB->taskStatus & (OS_TASK_STATUS_PEND_TIME | OS_TASK_STATUS_DELAY))) {
        return;
    }

    responseTime = GET_SORTLIST_VALUE(&taskCB->sortList);
    OsSchedDeTaskFromTimeList(taskCB);
    SET_SORTLIST_VALUE(&taskCB->sortList, responseTime);
    taskCB->taskStatus |= OS_TASK_FLAG_FREEZE;
    return;
}

STATIC VOID SchedUnfreezeTask(LosTaskCB *taskCB)
{
    UINT64 currTime, responseTime;

    if (!(taskCB->taskStatus & OS_TASK_FLAG_FREEZE)) {
        return;
    }

    taskCB->taskStatus &= ~OS_TASK_FLAG_FREEZE;
    currTime = OsGetCurrSchedTimeCycle();
    responseTime = GET_SORTLIST_VALUE(&taskCB->sortList);
    if (responseTime > currTime) {
        OsSchedAddTask2TimeList(taskCB, responseTime);
        return;
    }

    SET_SORTLIST_VALUE(&taskCB->sortList, OS_SORT_LINK_INVALID_TIME);
    if (taskCB->taskStatus & OS_TASK_STATUS_PENDING) {
        LOS_ListDelete(&taskCB->pendList);
    }
    taskCB->taskStatus &= ~(OS_TASK_STATUS_DELAY | OS_TASK_STATUS_PEND_TIME | OS_TASK_STATUS_PENDING);
    return;
}

VOID OsSchedSuspend(LosTaskCB *taskCB)
{
    if (taskCB->taskStatus & OS_TASK_STATUS_READY) {
        OsSchedTaskDeQueue(taskCB);
    }

    SchedFreezeTask(taskCB);

    taskCB->taskStatus |= OS_TASK_STATUS_SUSPENDED;
    OsHookCall(LOS_HOOK_TYPE_MOVEDTASKTOSUSPENDEDLIST, taskCB);
    if (taskCB == OsCurrTaskGet()) {
        OsSchedResched();
    }
}

BOOL OsSchedResume(LosTaskCB *taskCB)
{
    BOOL needSched = FALSE;

    SchedUnfreezeTask(taskCB);

    taskCB->taskStatus &= ~OS_TASK_STATUS_SUSPENDED;
    if (!OsTaskIsBlocked(taskCB)) {
        OsSchedTaskEnQueue(taskCB);
        needSched = TRUE;
    }

    return needSched;
}

STATIC INLINE VOID SchedWakePendTimeTask(UINT64 currTime, LosTaskCB *taskCB, BOOL *needSchedule)
{
#ifndef LOSCFG_SCHED_DEBUG
    (VOID)currTime;
#endif

    LOS_SpinLock(&g_taskSpin);
    UINT16 tempStatus = taskCB->taskStatus;
    if (tempStatus & (OS_TASK_STATUS_PENDING | OS_TASK_STATUS_DELAY)) {//任务被挂起或被延迟执行
        taskCB->taskStatus &= ~(OS_TASK_STATUS_PENDING | OS_TASK_STATUS_PEND_TIME | OS_TASK_STATUS_DELAY);//去掉这些标签
        if (tempStatus & OS_TASK_STATUS_PENDING) {//如果贴有挂起/待办标签
            taskCB->taskStatus |= OS_TASK_STATUS_TIMEOUT;//贴上时间到了的标签
            LOS_ListDelete(&taskCB->pendList);//从挂起/待办链表中删除
            taskCB->taskMux = NULL;
            OsTaskWakeClearPendMask(taskCB);
        }

        if (!(tempStatus & OS_TASK_STATUS_SUSPENDED)) {
#ifdef LOSCFG_SCHED_DEBUG
            taskCB->schedStat.pendTime += currTime - taskCB->startTime;
            taskCB->schedStat.pendCount++;
#endif
            OsSchedTaskEnQueue(taskCB);//将任务加入就绪队列
            *needSchedule = TRUE;
        }
    }

    LOS_SpinUnlock(&g_taskSpin);
}
///扫描那些处于等待状态的任务是否时间到了
STATIC INLINE BOOL SchedScanTaskTimeList(SchedRunQue *rq)
{
    BOOL needSchedule = FALSE;
    SortLinkAttribute *taskSortLink = &rq->taskSortLink;
    LOS_DL_LIST *listObject = &taskSortLink->sortLink;
    /*
     * When task is pended with timeout, the task block is on the timeout sortlink
     * (per cpu) and ipc(mutex,sem and etc.)'s block at the same time, it can be waken
     * up by either timeout or corresponding ipc it's waiting.
     *
     * Now synchronize sortlink procedure is used, therefore the whole task scan needs
     * to be protected, preventing another core from doing sortlink deletion at same time.
     */
    LOS_SpinLock(&taskSortLink->spinLock);

    if (LOS_ListEmpty(listObject)) {
        LOS_SpinUnlock(&taskSortLink->spinLock);
        return needSchedule;
    }

    SortLinkList *sortList = LOS_DL_LIST_ENTRY(listObject->pstNext, SortLinkList, sortLinkNode);//获取每个优先级上的任务链表头节点
    UINT64 currTime = OsGetCurrSchedTimeCycle();//获取当前时间
    while (sortList->responseTime <= currTime) {//到达时间小于当前时间,说明任务时间过了,需要去执行了
        LosTaskCB *taskCB = LOS_DL_LIST_ENTRY(sortList, LosTaskCB, sortList);//获取任务实体
        OsDeleteNodeSortLink(taskSortLink, &taskCB->sortList);//从排序链表中删除
        LOS_SpinUnlock(&taskSortLink->spinLock);

        SchedWakePendTimeTask(currTime, taskCB, &needSchedule);

        LOS_SpinLock(&taskSortLink->spinLock);
        if (LOS_ListEmpty(listObject)) {//因为上面已经执行了删除操作,所以此处可能有空
            break;//为空则直接退出循环
        }

        sortList = LOS_DL_LIST_ENTRY(listObject->pstNext, SortLinkList, sortLinkNode);//处理下一个节点
    }

    LOS_SpinUnlock(&taskSortLink->spinLock);

    return needSchedule;//返回是否需要调度
}

VOID OsSchedTick(VOID)
{
    SchedRunQue *rq = OsSchedRunQue();

    if (rq->responseID == OS_INVALID_VALUE) {
        if (SchedScanTaskTimeList(rq)) {
            LOS_MpSchedule(OS_MP_CPU_ALL);
            rq->schedFlag |= INT_PEND_RESCH;
        }
    }
    rq->schedFlag |= INT_PEND_TICK;
    rq->responseTime = OS_SCHED_MAX_RESPONSE_TIME;
}

VOID OsSchedSetIdleTaskSchedParam(LosTaskCB *idleTask)
{
    idleTask->basePrio = OS_TASK_PRIORITY_LOWEST;
    idleTask->policy = LOS_SCHED_IDLE;
    idleTask->initTimeSlice = OS_SCHED_FIFO_TIMEOUT;
    idleTask->timeSlice = idleTask->initTimeSlice;
    OsSchedTaskEnQueue(idleTask);
}

VOID OsSchedResetSchedResponseTime(UINT64 responseTime)
{
    OsSchedRunQue()->responseTime = responseTime;
}

VOID OsSchedRunQueInit(VOID)
{
    if (ArchCurrCpuid() != 0) {
        return;
    }

    for (UINT16 index = 0; index < LOSCFG_KERNEL_CORE_NUM; index++) {
        SchedRunQue *rq = OsSchedRunQueByID(index);
        OsSortLinkInit(&rq->taskSortLink);
        rq->responseTime = OS_SCHED_MAX_RESPONSE_TIME;
    }
}

VOID OsSchedRunQueIdleInit(UINT32 idleTaskID)
{
    SchedRunQue *rq = OsSchedRunQue();
    rq->idleTaskID = idleTaskID;
}

UINT32 OsSchedInit(VOID)
{
    for (UINT16 index = 0; index < OS_PRIORITY_QUEUE_NUM; index++) {
        SchedQueue *queueList = &g_sched.queueList[index];
        LOS_DL_LIST *priList = &queueList->priQueueList[0];
        for (UINT16 pri = 0; pri < OS_PRIORITY_QUEUE_NUM; pri++) {
            LOS_ListInit(&priList[pri]);
        }
    }

#ifdef LOSCFG_SCHED_TICK_DEBUG
    UINT32 ret = OsSchedDebugInit();
    if (ret != LOS_OK) {
        return ret;
    }
#endif
    return LOS_OK;
}

STATIC LosTaskCB *GetTopTask(SchedRunQue *rq)
{
    UINT32 priority, processPriority;
    UINT32 bitmap;
    LosTaskCB *newTask = NULL;
    UINT32 processBitmap = g_sched.queueBitmap;
#ifdef LOSCFG_KERNEL_SMP
    UINT32 cpuid = ArchCurrCpuid();
#endif

    while (processBitmap) {
        processPriority = CLZ(processBitmap);
        SchedQueue *queueList = &g_sched.queueList[processPriority];
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

    newTask = OS_TCB_FROM_TID(rq->idleTaskID);

FIND_TASK:
    OsSchedTaskDeQueue(newTask);
    return newTask;
}
///CPU的调度开始,每个CPU核都会执行这个函数一次.
VOID OsSchedStart(VOID)
{
    UINT32 cpuid = ArchCurrCpuid();//从系统寄存器上获取当前执行的CPU核编号
    UINT32 intSave;

    PRINTK("cpu %d entering scheduler\n", cpuid);

    SCHEDULER_LOCK(intSave);

    OsTickStart();//开始了属于本核的tick 

    SchedRunQue *rq = OsSchedRunQue();
    LosTaskCB *newTask = GetTopTask(rq);
    newTask->taskStatus |= OS_TASK_STATUS_RUNNING;

#ifdef LOSCFG_KERNEL_SMP //注意：需要设置当前cpu，以防第一个任务删除可能会失败，因为此标志与实际当前 cpu 不匹配。
    /*
     * attention: current cpu needs to be set, in case first task deletion
     * may fail because this flag mismatch with the real current cpu.
     */
    newTask->currCpu = cpuid;//设置当前CPU,确保第一个任务由本CPU核执行
#endif

    OsCurrTaskSet((VOID *)newTask);

    newTask->startTime = OsGetCurrSchedTimeCycle();

    OsSwtmrResponseTimeReset(newTask->startTime);

    /* System start schedule */
    OS_SCHEDULER_SET(cpuid);

    rq->responseID = OS_INVALID;
    SchedSetNextExpireTime(newTask->taskID, newTask->startTime + newTask->timeSlice, OS_INVALID);
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


/*!
 * @brief OsSchedTaskSwitch	实现新老两个任务切换
 *
 * @param newTask 	
 * @param runTask	
 * @return	
 *
 * @see
 */
STATIC VOID SchedTaskSwitch(LosTaskCB *runTask, LosTaskCB *newTask)
{
    UINT64 endTime;

    SchedSwitchCheck(runTask, newTask);

    runTask->taskStatus &= ~OS_TASK_STATUS_RUNNING; //当前任务去掉正在运行的标签
    newTask->taskStatus |= OS_TASK_STATUS_RUNNING;	//新任务贴上正在运行的标签,虽标签贴上了,但目前还是在老任务中跑.

#ifdef LOSCFG_KERNEL_SMP
    /* mask new running task's owner processor */
    runTask->currCpu = OS_TASK_INVALID_CPUID;//褫夺当前任务的CPU使用权
    newTask->currCpu = ArchCurrCpuid(); //标记新任务获取当前CPU使用权
#endif

    OsCurrTaskSet((VOID *)newTask);//设置新任务为当前任务
#ifdef LOSCFG_KERNEL_VM
    if (newTask->archMmu != runTask->archMmu) {
        LOS_ArchMmuContextSwitch((LosArchMmu *)newTask->archMmu);
    }
#endif

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
        TimeSliceUpdate(runTask, newTask->startTime);

        if (runTask->taskStatus & (OS_TASK_STATUS_PEND_TIME | OS_TASK_STATUS_DELAY)) {
            OsSchedAddTask2TimeList(runTask, runTask->startTime + runTask->waitTime);
        }
    }

    if (newTask->policy == LOS_SCHED_RR) {
        endTime = newTask->startTime + newTask->timeSlice;
    } else {
        endTime = OS_SCHED_MAX_RESPONSE_TIME - OS_TICK_RESPONSE_PRECISION;
    }
    SchedSetNextExpireTime(newTask->taskID, endTime, runTask->taskID);

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
    SchedRunQue *rq = OsSchedRunQue();
    LosTaskCB *runTask = OsCurrTaskGet();

    TimeSliceUpdate(runTask, OsGetCurrSchedTimeCycle());
    if (runTask->timeSlice <= OS_TIME_SLICE_MIN) {
        rq->schedFlag |= INT_PEND_RESCH;
    }

    if (OsPreemptable() && (rq->schedFlag & INT_PEND_RESCH)) {
        rq->schedFlag &= ~INT_PEND_RESCH;

        LOS_SpinLock(&g_taskSpin);

        OsSchedTaskEnQueue(runTask);

        LosTaskCB *newTask = GetTopTask(rq);
        if (runTask != newTask) {
            SchedTaskSwitch(runTask, newTask);
            LOS_SpinUnlock(&g_taskSpin);
            return;
        }

        LOS_SpinUnlock(&g_taskSpin);
    }

    if (rq->schedFlag & INT_PEND_TICK) {
        OsSchedUpdateExpireTime();
    }
}

VOID OsSchedResched(VOID)
{
    LOS_ASSERT(LOS_SpinHeld(&g_taskSpin));
    SchedRunQue *rq = OsSchedRunQue();
#ifdef LOSCFG_KERNEL_SMP
    LOS_ASSERT(rq->taskLockCnt == 1);
#else
    LOS_ASSERT(rq->taskLockCnt == 0);
#endif

    rq->schedFlag &= ~INT_PEND_RESCH;
    LosTaskCB *runTask = OsCurrTaskGet();
    LosTaskCB *newTask = GetTopTask(rq);
    if (runTask == newTask) {
        return;
    }

    SchedTaskSwitch(runTask, newTask);
}

VOID LOS_Schedule(VOID)
{
    UINT32 intSave;
    LosTaskCB *runTask = OsCurrTaskGet();

    if (OS_INT_ACTIVE) {
        OsSchedRunQuePendingSet();
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

    TimeSliceUpdate(runTask, OsGetCurrSchedTimeCycle());

    /* add run task back to ready queue | 添加任务到就绪队列*/
    OsSchedTaskEnQueue(runTask); 

    /* reschedule to new thread | 申请调度,CPU可能将换任务执行*/
    OsSchedResched();

    SCHEDULER_UNLOCK(intSave);
}
/// 找到合适的位置 将当前任务插入到 读/写锁链表中
STATIC INLINE LOS_DL_LIST *OsSchedLockPendFindPosSub(const LosTaskCB *runTask, const LOS_DL_LIST *lockList)
{
    LosTaskCB *pendedTask = NULL;
    LOS_DL_LIST *node = NULL;

    LOS_DL_LIST_FOR_EACH_ENTRY(pendedTask, lockList, LosTaskCB, pendList) {//遍历阻塞链表
        if (pendedTask->priority < runTask->priority) {//当前优先级更小,位置不宜,继续
            continue;
        } else if (pendedTask->priority > runTask->priority) {//找到合适位置
            node = &pendedTask->pendList;
            break;
        } else {//找到合适位置
            node = pendedTask->pendList.pstNext;
            break;
        }
    }

    return node;
}
/// 找到合适的位置 将当前任务插入到 读/写锁链表中
LOS_DL_LIST *OsSchedLockPendFindPos(const LosTaskCB *runTask, LOS_DL_LIST *lockList)
{
    LOS_DL_LIST *node = NULL;

    if (LOS_ListEmpty(lockList)) {
        node = lockList;
    } else {
        LosTaskCB *pendedTask1 = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(lockList));//找到首任务
        LosTaskCB *pendedTask2 = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_LAST(lockList)); //找到尾任务
        if (pendedTask1->priority > runTask->priority) { //首任务比当前任务优先级低 ,所以runTask可以排第一
            node = lockList->pstNext;//下一个,注意priority越大 优先级越低
        } else if (pendedTask2->priority <= runTask->priority) {//尾任务比当前任务优先级高 ,所以runTask排最后
            node = lockList;
        } else {//两头比较没结果,陷入中间比较
            node = OsSchedLockPendFindPosSub(runTask, lockList);//进入子级循环找
        }
    }

    return node;
}

