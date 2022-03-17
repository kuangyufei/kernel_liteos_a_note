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

#ifndef _LOS_SCHED_PRI_H
#define _LOS_SCHED_PRI_H

#include "los_sortlink_pri.h"
#include "los_sys_pri.h"
#include "los_hwi.h"
#include "hal_timer.h"
#ifdef LOSCFG_SCHED_DEBUG
#include "los_stat_pri.h"
#endif
#include "los_stackinfo_pri.h"
#include "los_futex_pri.h"
#include "los_signal.h"
#ifdef LOSCFG_KERNEL_CPUP
#include "los_cpup_pri.h"
#endif
#ifdef LOSCFG_KERNEL_LITEIPC
#include "hm_liteipc.h"
#endif

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#define OS_SCHED_MINI_PERIOD       (OS_SYS_CLOCK / LOSCFG_BASE_CORE_TICK_PER_SECOND_MINI) ///< 1毫秒的时钟周期
#define OS_TICK_RESPONSE_PRECISION (UINT32)((OS_SCHED_MINI_PERIOD * 75) / 100) ///< 不明白为啥是 * 75 就精确了???  @note_thinking
#define OS_SCHED_MAX_RESPONSE_TIME (UINT64)(((UINT64)-1) - 1U)

extern UINT32 g_taskScheduled;
#define OS_SCHEDULER_ACTIVE (g_taskScheduled & (1U << ArchCurrCpuid()))
#define OS_SCHEDULER_ALL_ACTIVE (g_taskScheduled == LOSCFG_KERNEL_CPU_MASK)

typedef BOOL (*SCHED_TL_FIND_FUNC)(UINTPTR, UINTPTR);
//获取当前调度经历了多少个时间周期
STATIC INLINE UINT64 OsGetCurrSchedTimeCycle(VOID)
{
    return HalClockGetCycles();
}

typedef enum {
    INT_NO_RESCH = 0x0,   /* no needs to schedule */
    INT_PEND_RESCH = 0x1, /* pending schedule flag */
    INT_PEND_TICK = 0x2,  /* pending tick */
} SchedFlag;

typedef struct {
    SortLinkAttribute taskSortLink;          /* task sort link */
    SortLinkAttribute swtmrSortLink;         /* swtmr sort link */
    UINT64            responseTime;          /* Response time for current CPU tick interrupts */
    UINT32            responseID;            /* The response ID of the current CPU tick interrupt */
    UINT32            idleTaskID;            /* idle task id */
    UINT32            taskLockCnt;           /* task lock flag */
    UINT32            swtmrTaskID;           /* software timer task id */
    UINT32            swtmrHandlerQueue;     /* software timer timeout queue id */
    UINT32            schedFlag;             /* pending scheduler flag */
} SchedRunQue;

extern SchedRunQue g_schedRunQue[LOSCFG_KERNEL_CORE_NUM];

STATIC INLINE SchedRunQue *OsSchedRunQue(VOID)
{
    return &g_schedRunQue[ArchCurrCpuid()];
}

STATIC INLINE SchedRunQue *OsSchedRunQueByID(UINT16 id)
{
    return &g_schedRunQue[id];
}

STATIC INLINE UINT32 OsSchedLockCountGet(VOID)
{
    return OsSchedRunQue()->taskLockCnt;
}

STATIC INLINE VOID OsSchedLockSet(UINT32 count)
{
    OsSchedRunQue()->taskLockCnt = count;
}

STATIC INLINE VOID OsSchedLock(VOID)
{
    OsSchedRunQue()->taskLockCnt++;
}

STATIC INLINE VOID OsSchedUnlock(VOID)
{
    OsSchedRunQue()->taskLockCnt--;
}

STATIC INLINE BOOL OsSchedUnlockResch(VOID)
{
    SchedRunQue *rq = OsSchedRunQue();
    if (rq->taskLockCnt > 0) {
        rq->taskLockCnt--;
        if ((rq->taskLockCnt == 0) && (rq->schedFlag & INT_PEND_RESCH) && OS_SCHEDULER_ACTIVE) {
            return TRUE;
        }
    }

    return FALSE;
}

STATIC INLINE BOOL OsSchedIsLock(VOID)
{
    return (OsSchedRunQue()->taskLockCnt != 0);
}

/* Check if preemptible with counter flag */
STATIC INLINE BOOL OsPreemptable(VOID)
{
    SchedRunQue *rq = OsSchedRunQue();
    /*
     * Unlike OsPreemptableInSched, the int may be not disabled when OsPreemptable
     * is called, needs manually disable interrupt, to prevent current task from
     * being migrated to another core, and get the wrong preemptable status.
     */
    UINT32 intSave = LOS_IntLock();
    BOOL preemptible = (rq->taskLockCnt == 0);
    if (!preemptible) {
        /* Set schedule flag if preemption is disabled */
        rq->schedFlag |= INT_PEND_RESCH;
    }
    LOS_IntRestore(intSave);
    return preemptible;
}

STATIC INLINE BOOL OsPreemptableInSched(VOID)
{
    BOOL preemptible = FALSE;
    SchedRunQue *rq = OsSchedRunQue();

#ifdef LOSCFG_KERNEL_SMP
    /*
     * For smp systems, schedule must hold the task spinlock, and this counter
     * will increase by 1 in that case.
     */
    preemptible = (rq->taskLockCnt == 1);

#else
    preemptible = (rq->taskLockCnt == 0);
#endif
    if (!preemptible) {
        /* Set schedule flag if preemption is disabled */
        rq->schedFlag |= INT_PEND_RESCH;
    }

    return preemptible;
}

STATIC INLINE UINT32 OsSchedGetRunQueIdle(VOID)
{
    return OsSchedRunQue()->idleTaskID;
}

STATIC INLINE VOID OsSchedRunQuePendingSet(VOID)
{
    OsSchedRunQue()->schedFlag |= INT_PEND_RESCH;
}

#ifdef LOSCFG_KERNEL_SMP
STATIC INLINE VOID FindIdleRunQue(UINT16 *idleCpuID)
{
    SchedRunQue *idleRq = OsSchedRunQueByID(0);
    UINT32 nodeNum = OsGetSortLinkNodeNum(&idleRq->taskSortLink) + OsGetSortLinkNodeNum(&idleRq->swtmrSortLink);
    UINT16 cpuID = 1;
    do {
        SchedRunQue *rq = OsSchedRunQueByID(cpuID);
        UINT32 temp = OsGetSortLinkNodeNum(&rq->taskSortLink) + OsGetSortLinkNodeNum(&rq->swtmrSortLink);
        if (nodeNum > temp) {
            *idleCpuID = cpuID;
            nodeNum = temp;
        }
        cpuID++;
    } while (cpuID < LOSCFG_KERNEL_CORE_NUM);
}
#endif

STATIC INLINE VOID OsSchedAddTask2TimeList(SortLinkList *node, UINT64 startTime, UINT32 waitTicks)
{
    UINT16 idleCpu = 0;
#ifdef LOSCFG_KERNEL_SMP
    FindIdleRunQue(&idleCpu);
#endif
    SchedRunQue *rq = OsSchedRunQueByID(idleCpu);
    UINT64 responseTime = startTime + (UINT64)waitTicks * OS_CYCLE_PER_TICK;
    OsAdd2SortLink(&rq->taskSortLink, node, responseTime, idleCpu);
}

STATIC INLINE UINT32 OsSchedSwtmrHandlerQueueGet(VOID)
{
    return OsSchedRunQue()->swtmrHandlerQueue;
}

STATIC INLINE VOID OsSchedDeTaskFromTimeList(SortLinkList *node)
{
#ifdef LOSCFG_KERNEL_SMP
    SchedRunQue *rq = OsSchedRunQueByID(node->cpuid);
#else
    SchedRunQue *rq = OsSchedRunQueByID(0);
#endif
    OsDeleteFromSortLink(&rq->taskSortLink, node);
}

STATIC INLINE VOID OsSchedAddSwtmr2TimeList(SortLinkList *node, UINT64 startTime, UINT32 waitTicks)
{
    UINT16 idleCpu = 0;
#ifdef LOSCFG_KERNEL_SMP
    FindIdleRunQue(&idleCpu);
#endif
    SchedRunQue *rq = OsSchedRunQueByID(idleCpu);
    UINT64 responseTime = startTime + (UINT64)waitTicks * OS_CYCLE_PER_TICK;
    OsAdd2SortLink(&rq->swtmrSortLink, node, responseTime, idleCpu);
}

STATIC INLINE VOID OsSchedDeSwtmrFromTimeList(SortLinkList *node)
{
#ifdef LOSCFG_KERNEL_SMP
    SchedRunQue *rq = OsSchedRunQueByID(node->cpuid);
#else
    SchedRunQue *rq = OsSchedRunQueByID(0);
#endif
    OsDeleteFromSortLink(&rq->swtmrSortLink, node);
}

VOID OsSchedRunQueIdleInit(UINT32 idleTaskID);
VOID OsSchedRunQueSwtmrInit(UINT32 swtmrTaskID, UINT32 swtmrQueue);
VOID OsSchedRunQueInit(VOID);
BOOL OsSchedSwtmrTimeListFind(SCHED_TL_FIND_FUNC checkFunc, UINTPTR arg);

/**
 * @ingroup los_sched
 * Define a usable task priority.
 *
 * Highest task priority.
 */
#define OS_TASK_PRIORITY_HIGHEST    0

/**
 * @ingroup los_sched
 * Define a usable task priority.
 *
 * Lowest task priority.
 */
#define OS_TASK_PRIORITY_LOWEST     31

/**
 * @ingroup los_sched
 * Flag that indicates the task or task control block status.
 *
 * The task is init.
 */
#define OS_TASK_STATUS_INIT         0x0001U

/**
 * @ingroup los_sched
 * Flag that indicates the task or task control block status.
 *
 * The task is ready.
 */
#define OS_TASK_STATUS_READY        0x0002U

/**
 * @ingroup los_sched
 * Flag that indicates the task or task control block status.
 *
 * The task is running.
 */
#define OS_TASK_STATUS_RUNNING      0x0004U

/**
 * @ingroup los_sched
 * Flag that indicates the task or task control block status.
 *
 * The task is suspended.
 */
#define OS_TASK_STATUS_SUSPENDED    0x0008U

/**
 * @ingroup los_sched
 * Flag that indicates the task or task control block status.
 *
 * The task is blocked.
 */
#define OS_TASK_STATUS_PENDING      0x0010U

/**
 * @ingroup los_sched
 * Flag that indicates the task or task control block status.
 *
 * The task is delayed.
 */
#define OS_TASK_STATUS_DELAY        0x0020U

/**
 * @ingroup los_sched
 * Flag that indicates the task or task control block status.
 *
 * The time for waiting for an event to occur expires.
 */
#define OS_TASK_STATUS_TIMEOUT      0x0040U

/**
 * @ingroup los_sched
 * Flag that indicates the task or task control block status.
 *
 * The task is pend for a period of time.
 */
#define OS_TASK_STATUS_PEND_TIME    0x0080U

/**
 * @ingroup los_sched
 * Flag that indicates the task or task control block status.
 *
 * The task is exit.
 */
#define OS_TASK_STATUS_EXIT         0x0100U

#define OS_TCB_NAME_LEN             32

typedef struct {
    VOID            *stackPointer;      /**< Task stack pointer | 内核栈指针位置(SP)  */	
    UINT16          taskStatus;         /**< Task status | 各种状态标签，可以拥有多种标签，按位标识 */

    /* The scheduling */
    UINT16          basePrio;
    UINT16          priority;           /**< Task priority | 任务优先级[0:31],默认是31级  */
    UINT16          policy;				///< 任务的调度方式(三种 .. LOS_SCHED_RR   		  LOS_SCHED_FIFO .. )
    UINT64          startTime;          /**< The start time of each phase of task | 任务开始时间  */
    UINT64          irqStartTime;       /**< Interrupt start time | 任务中断开始时间  */ 
    UINT32          irqUsedTime;        /**< Interrupt consumption time | 任务中断消耗时间  */ 
    UINT32          initTimeSlice;      /**< Task init time slice | 任务初始的时间片  */ 
    INT32           timeSlice;          /**< Task remaining time slice | 任务剩余时间片  */ 
    UINT32          waitTimes;          /**< Task delay time, tick number | 设置任务调度延期时间  */ 
    SortLinkList    sortList;           /**< Task sortlink node | 跟CPU捆绑的任务排序链表节点,上面挂的是就绪队列的下一个阶段,进入CPU要执行的任务队列  */	
    UINT32          stackSize;          /**< Task stack size | 内核态栈大小,内存来自内核空间  */		
    UINTPTR         topOfStack;         /**< Task stack top | 内核态栈顶 bottom = top + size */		
    UINT32          taskID;             /**< Task ID | 任务ID，任务池本质是一个大数组，ID就是数组的索引，默认 < 128 */				
    TSK_ENTRY_FUNC  taskEntry;          /**< Task entrance function | 任务执行入口地址 */	
    VOID            *joinRetval;        /**< pthread adaption | 用来存储join线程的入口地址 */	
    VOID            *taskMux;           /**< Task-held mutex | task在等哪把锁 */		
    VOID            *taskEvent;         /**< Task-held event | task在等哪个事件 */		
    UINTPTR         args[4];            /**< Parameter, of which the maximum number is 4 | 入口函数的参数 例如 main (int argc,char *argv[]) */	
    CHAR            taskName[OS_TCB_NAME_LEN]; /**< Task name | 任务的名称 */	
    LOS_DL_LIST     pendList;           /**< Task pend node | 如果任务阻塞时就通过它挂到各种阻塞情况的链表上,比如OsTaskWait时 */		
    LOS_DL_LIST     threadList;         /**< thread list | 挂到所属进程的线程链表上 */
    UINT32          eventMask;          /**< Event mask | 任务对哪些事件进行屏蔽 */
    UINT32          eventMode;          /**< Event mode | 事件三种模式(LOS_WAITMODE_AND,LOS_WAITMODE_OR,LOS_WAITMODE_CLR) */	
    UINT32          priBitMap;          /**< BitMap for recording the change of task priority,the priority can not be greater than 31 
    										| 任务在执行过程中优先级会经常变化，这个变量用来记录所有曾经变化过的优先级，例如 ..01001011 曾经有过 0,1,3,6 优先级 */
#ifdef LOSCFG_KERNEL_CPUP
    OsCpupBase      taskCpup;           /**< task cpu usage | CPU 使用统计 */
#endif
    INT32           errorNo;            /**< Error Num | 错误序号 */
    UINT32          signal;             /**< Task signal | 任务信号类型,(SIGNAL_NONE,SIGNAL_KILL,SIGNAL_SUSPEND,SIGNAL_AFFI) */
    sig_cb          sig;				///< 信号控制块，用于异步通信,类似于 linux singal模块
#ifdef LOSCFG_KERNEL_SMP
    UINT16          currCpu;            /**< CPU core number of this task is running on | 正在运行此任务的CPU内核号 */
    UINT16          lastCpu;            /**< CPU core number of this task is running on last time | 上次运行此任务的CPU内核号 */
    UINT16          cpuAffiMask;        /**< CPU affinity mask, support up to 16 cores | CPU亲和力掩码，最多支持16核，亲和力很重要，多核情况下尽量一个任务在一个CPU核上运行，提高效率 */
#ifdef LOSCFG_KERNEL_SMP_TASK_SYNC	//多核情况下的任务同步开关,采用信号量实现
    UINT32          syncSignal;         /**< Synchronization for signal handling | 用于CPU之间同步信号量 */
#endif
#ifdef LOSCFG_KERNEL_SMP_LOCKDEP //SMP死锁检测开关
    LockDep         lockDep;	///< 死锁依赖检测
#endif
#endif
#ifdef LOSCFG_SCHED_DEBUG //调试调度开关
    SchedStat       schedStat;          /**< Schedule statistics | 调度统计 */
#endif
#ifdef LOSCFG_KERNEL_VM
    UINTPTR         archMmu;
    UINTPTR         userArea;			///< 用户空间的堆区开始位置
    UINTPTR         userMapBase;		///< 用户空间的栈顶位置,内存来自用户空间,和topOfStack有本质的区别.
    UINT32          userMapSize;        /**< user thread stack size ,real size : userMapSize + USER_STACK_MIN_SIZE | 用户栈大小 */
    FutexNode       futex;				///< 指明任务在等待哪把快锁，一次只等一锁，锁和任务的关系是(1:N)关系
#endif
    UINT32          processID;          /**< Which belong process */
    LOS_DL_LIST     joinList;           /**< join list | 联结链表,允许任务之间相互释放彼此 */
    LOS_DL_LIST     lockList;           /**< Hold the lock list | 该链表上挂的都是已持有的锁 */
    UINTPTR         waitID;             /**< Wait for the PID or GID of the child process | 等待子进程的PID或GID */
    UINT16          waitFlag;           /**< The type of child process that is waiting, belonging to a group or parent,
                                             a specific child process, or any child process | 任务在等待什么信息 ? (OS_TASK_WAIT_PROCESS | OS_TASK_WAIT_GID | OS_TASK_WAIT_LITEIPC  ..) 
                                        往往用于被其他任务查看该任务在等待什么事件,如果事件到了就可以唤醒任务*/
#ifdef LOSCFG_KERNEL_LITEIPC //轻量级进程间通信开关
    IpcTaskInfo     *ipcTaskInfo;	///< 任务间通讯信息结构体
#endif
#ifdef LOSCFG_KERNEL_PERF
    UINTPTR         pc;	///< pc寄存器
    UINTPTR         fp; ///< fp寄存器
#endif
} LosTaskCB;


STATIC INLINE BOOL OsTaskIsRunning(const LosTaskCB *taskCB)
{
    return ((taskCB->taskStatus & OS_TASK_STATUS_RUNNING) != 0);
}

STATIC INLINE BOOL OsTaskIsReady(const LosTaskCB *taskCB)
{
    return ((taskCB->taskStatus & OS_TASK_STATUS_READY) != 0);
}

STATIC INLINE BOOL OsTaskIsInactive(const LosTaskCB *taskCB)
{
    return ((taskCB->taskStatus & (OS_TASK_STATUS_INIT | OS_TASK_STATUS_EXIT)) != 0);
}

STATIC INLINE BOOL OsTaskIsPending(const LosTaskCB *taskCB)
{
    return ((taskCB->taskStatus & OS_TASK_STATUS_PENDING) != 0);
}

STATIC INLINE BOOL OsTaskIsSuspended(const LosTaskCB *taskCB)
{
    return ((taskCB->taskStatus & OS_TASK_STATUS_SUSPENDED) != 0);
}

STATIC INLINE BOOL OsTaskIsBlocked(const LosTaskCB *taskCB)
{
    return ((taskCB->taskStatus & (OS_TASK_STATUS_SUSPENDED | OS_TASK_STATUS_PENDING | OS_TASK_STATUS_DELAY)) != 0);
}

STATIC INLINE LosTaskCB *OsCurrTaskGet(VOID)
{
    return (LosTaskCB *)ArchCurrTaskGet();
}

STATIC INLINE VOID OsCurrTaskSet(LosTaskCB *task)
{
    ArchCurrTaskSet(task);
}

STATIC INLINE VOID OsCurrUserTaskSet(UINTPTR thread)
{
    ArchCurrUserTaskSet(thread);
}

STATIC INLINE VOID OsSchedIrqUpdateUsedTime(VOID)
{
    LosTaskCB *runTask = OsCurrTaskGet();
    runTask->irqUsedTime = OsGetCurrSchedTimeCycle() - runTask->irqStartTime;//获取时间差
}
/// 获取中断开始时间
STATIC INLINE VOID OsSchedIrqStartTime(VOID)
{
    LosTaskCB *runTask = OsCurrTaskGet();
    runTask->irqStartTime = OsGetCurrSchedTimeCycle(); //获取当前时间
}

/*
 * Schedule flag, one bit represents one core.
 * This flag is used to prevent kernel scheduling before OSStartToRun.
 */
#define OS_SCHEDULER_SET(cpuid) do {     \
    g_taskScheduled |= (1U << (cpuid));  \
} while (0);
//清楚调度标识位,对应位设置为0
#define OS_SCHEDULER_CLR(cpuid) do {     \
    g_taskScheduled &= ~(1U << (cpuid)); \
} while (0);

VOID OsSchedSetIdleTaskSchedParam(LosTaskCB *idleTask);
VOID OsSchedResetSchedResponseTime(UINT64 responseTime);
VOID OsSchedUpdateExpireTime(VOID);
VOID OsSchedToUserReleaseLock(VOID);
VOID OsSchedTaskDeQueue(LosTaskCB *taskCB);
VOID OsSchedTaskEnQueue(LosTaskCB *taskCB);
UINT32 OsSchedTaskWait(LOS_DL_LIST *list, UINT32 timeout, BOOL needSched);
VOID OsSchedTaskWake(LosTaskCB *resumedTask);
BOOL OsSchedModifyTaskSchedParam(LosTaskCB *taskCB, UINT16 policy, UINT16 priority);
BOOL OsSchedModifyProcessSchedParam(UINT32 pid, UINT16 policy, UINT16 priority);
VOID OsSchedSuspend(LosTaskCB *taskCB);
BOOL OsSchedResume(LosTaskCB *taskCB);
VOID OsSchedDelay(LosTaskCB *runTask, UINT32 tick);
VOID OsSchedYield(VOID);
VOID OsSchedTaskExit(LosTaskCB *taskCB);
VOID OsSchedTick(VOID);
UINT32 OsSchedInit(VOID);
VOID OsSchedStart(VOID);

/*
 * This function simply picks the next task and switches to it.
 * Current task needs to already be in the right state or the right
 * queues it needs to be in.
 */
VOID OsSchedResched(VOID);
VOID OsSchedIrqEndCheckNeedSched(VOID);

/*
* This function inserts the runTask to the lock pending list based on the
* task priority.
*/
LOS_DL_LIST *OsSchedLockPendFindPos(const LosTaskCB *runTask, LOS_DL_LIST *lockList);

#ifdef LOSCFG_SCHED_TICK_DEBUG
VOID OsSchedDebugRecordData(VOID);
#endif

UINT32 OsShellShowTickRespo(VOID);

UINT32 OsShellShowSchedParam(VOID);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_SCHED_PRI_H */
