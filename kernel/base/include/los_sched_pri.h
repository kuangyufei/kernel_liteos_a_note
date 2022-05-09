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
#include "los_statistics_pri.h"
#endif
#include "los_stackinfo_pri.h"
#include "los_futex_pri.h"
#ifdef LOSCFG_KERNEL_PM
#include "los_pm_pri.h"
#endif
#include "los_signal.h"
#ifdef LOSCFG_KERNEL_CPUP
#include "los_cpup_pri.h"
#endif
#ifdef LOSCFG_KERNEL_LITEIPC
#include "hm_liteipc.h"
#endif
#include "los_mp.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#define OS_SCHED_MINI_PERIOD       (OS_SYS_CLOCK / LOSCFG_BASE_CORE_TICK_PER_SECOND_MINI) ///< 1毫秒的时钟周期
#define OS_TICK_RESPONSE_PRECISION (UINT32)((OS_SCHED_MINI_PERIOD * 75) / 100) ///< 不明白为啥是 * 75 就精确了???  @note_thinking
#define OS_SCHED_MAX_RESPONSE_TIME    OS_SORT_LINK_INVALID_TIME
#define OS_SCHED_TICK_TO_CYCLE(ticks) ((UINT64)ticks * OS_CYCLE_PER_TICK)
#define AFFI_MASK_TO_CPUID(mask)      ((UINT16)((mask) - 1))

extern UINT32 g_taskScheduled;
#define OS_SCHEDULER_ACTIVE (g_taskScheduled & (1U << ArchCurrCpuid()))
#define OS_SCHEDULER_ALL_ACTIVE (g_taskScheduled == LOSCFG_KERNEL_CPU_MASK)

typedef struct TagTaskCB LosTaskCB;
typedef BOOL (*SCHED_TL_FIND_FUNC)(UINTPTR, UINTPTR);
//获取当前调度经历了多少个时间周期
STATIC INLINE UINT64 OsGetCurrSchedTimeCycle(VOID)
{
    return HalClockGetCycles();
}

typedef enum {
    INT_NO_RESCH = 0x0,   /* no needs to schedule | 无需调度*/
    INT_PEND_RESCH = 0x1, /* pending schedule flag | 因阻塞而引起的调度*/
    INT_PEND_TICK = 0x2,  /* pending tick | 因Tick而引起的调度*/
} SchedFlag;

#define OS_PRIORITY_QUEUE_NUM 32 //队列优先级
typedef struct {
    LOS_DL_LIST priQueList[OS_PRIORITY_QUEUE_NUM];	//任务
    UINT32      readyTasks[OS_PRIORITY_QUEUE_NUM];  //已就绪任务
    UINT32      queueBitmap;	//位图
} HPFQueue;

typedef struct {
    HPFQueue queueList[OS_PRIORITY_QUEUE_NUM]; //
    UINT32   queueBitmap;
} HPFRunqueue;
//调度运行队列
typedef struct {
    SortLinkAttribute timeoutQueue; /* task timeout queue */
    HPFRunqueue       *hpfRunqueue;
    UINT64            responseTime;          /* Response time for current CPU tick interrupts */
    UINT32            responseID;            /* The response ID of the current CPU tick interrupt */
    UINT32            idleTaskID;            /* idle task id */
    UINT32            taskLockCnt;           /* task lock flag */
    UINT32            schedFlag;             /* pending scheduler flag */
} SchedRunqueue;

extern SchedRunqueue g_schedRunqueue[LOSCFG_KERNEL_CORE_NUM];//每个CPU核都有一个属于自己的调度队列

VOID OsSchedExpireTimeUpdate(VOID);
//获取当前CPU
STATIC INLINE SchedRunqueue *OsSchedRunqueue(VOID)
{
    return &g_schedRunqueue[ArchCurrCpuid()];
}

STATIC INLINE SchedRunqueue *OsSchedRunqueueByID(UINT16 id)
{
    return &g_schedRunqueue[id];
}

STATIC INLINE UINT32 OsSchedLockCountGet(VOID)
{
    return OsSchedRunqueue()->taskLockCnt;
}

STATIC INLINE VOID OsSchedLockSet(UINT32 count)
{
    OsSchedRunqueue()->taskLockCnt = count;
}

STATIC INLINE VOID OsSchedLock(VOID)
{
    OsSchedRunqueue()->taskLockCnt++;
}

STATIC INLINE VOID OsSchedUnlock(VOID)
{
    OsSchedRunqueue()->taskLockCnt--;
}

STATIC INLINE BOOL OsSchedUnlockResch(VOID)
{
    SchedRunqueue *rq = OsSchedRunqueue();
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
    return (OsSchedRunqueue()->taskLockCnt != 0);
}

/* Check if preemptible with counter flag */
STATIC INLINE BOOL OsPreemptable(VOID)
{
    SchedRunqueue *rq = OsSchedRunqueue();
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
    SchedRunqueue *rq = OsSchedRunqueue();

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

STATIC INLINE UINT32 OsSchedRunqueueIdleGet(VOID)
{
    return OsSchedRunqueue()->idleTaskID;
}

STATIC INLINE VOID OsSchedRunqueuePendingSet(VOID)
{
    OsSchedRunqueue()->schedFlag |= INT_PEND_RESCH;
}

#define LOS_SCHED_NORMAL  0U
#define LOS_SCHED_FIFO    1U
#define LOS_SCHED_RR      2U
#define LOS_SCHED_IDLE    3U

typedef struct {
    UINT16 policy;
    UINT16 basePrio;
    UINT16 priority;
    UINT32 timeSlice;
} SchedParam;

typedef struct {//记录任务调度信息
    UINT16  policy; /* This field must be present for all scheduling policies and must be the first in the structure 
					| 所有调度策略都必须存在此字段，并且必须是结构中的第一个字段*/
    UINT16  basePrio; ///< 起始优先级
    UINT16  priority; ///< 当前优先级
    UINT32  initTimeSlice;///< 初始化时间片
    UINT32  priBitmap; /**< Bitmap for recording the change of task priority, the priority can not be greater than 31 
	| 记录任务优先级变化的位图，优先级不能大于31 */
} SchedHPF;

typedef struct { //调度策略
    union {
        SchedHPF hpf; // 目前只支持 优先级策略（Highest-Priority-First，HPF）
    } Policy;
} SchedPolicy;

typedef struct {//调度接口函数
    VOID (*dequeue)(SchedRunqueue *rq, LosTaskCB *taskCB); 	///< 出队列
    VOID (*enqueue)(SchedRunqueue *rq, LosTaskCB *taskCB); 	///< 入队列
    VOID (*start)(SchedRunqueue *rq, LosTaskCB *taskCB); 	///< 开始执行任务
    VOID (*exit)(LosTaskCB *taskCB);	///< 任务退出
    UINT32 (*wait)(LosTaskCB *runTask, LOS_DL_LIST *list, UINT32 timeout); ///< 任务等待
    VOID (*wake)(LosTaskCB *taskCB);///< 任务唤醒
    BOOL (*schedParamModify)(LosTaskCB *taskCB, const SchedParam *param);///< 修改调度参数
    UINT32 (*schedParamGet)(const LosTaskCB *taskCB, SchedParam *param);///< 获取调度参数
    UINT32 (*delay)(LosTaskCB *taskCB, UINT64 waitTime);///< 延时执行
    VOID (*yield)(LosTaskCB *taskCB);///< 让出控制权
    UINT32 (*suspend)(LosTaskCB *taskCB);///< 挂起任务
    UINT32 (*resume)(LosTaskCB *taskCB, BOOL *needSched);///< 恢复任务
    UINT64 (*deadlineGet)(const LosTaskCB *taskCB);///< 获取最后期限
    VOID (*timeSliceUpdate)(SchedRunqueue *rq, LosTaskCB *taskCB, UINT64 currTime);///< 更新时间片
    INT32 (*schedParamCompare)(const SchedPolicy *sp1, const SchedPolicy *sp2); ///< 比较调度参数
    VOID (*priorityInheritance)(LosTaskCB *owner, const SchedParam *param);//继承调度参数
    VOID (*priorityRestore)(LosTaskCB *owner, const LOS_DL_LIST *list, const SchedParam *param);///< 恢复调度参数
} SchedOps;

/**
 * @ingroup los_sched
 * Define a usable task priority.
 *
 * Highest task priority.
 */
#define OS_TASK_PRIORITY_HIGHEST    0	/// 任务最高优先级

/**
 * @ingroup los_sched
 * Define a usable task priority.
 *
 * Lowest task priority.
 */
#define OS_TASK_PRIORITY_LOWEST     31 /// 任务最低优先级

/**
 * @ingroup los_sched
 * Flag that indicates the task or task control block status.
 *
 * The task is init.
 */
#define OS_TASK_STATUS_INIT         0x0001U /// 任务初始状态

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

#define OS_TASK_STATUS_BLOCKED     (OS_TASK_STATUS_INIT | OS_TASK_STATUS_PENDING | \
                                    OS_TASK_STATUS_DELAY | OS_TASK_STATUS_PEND_TIME)

/**
 * @ingroup los_task
 * Flag that indicates the task or task control block status.
 *
 * The delayed operation of this task is frozen.
 */
#define OS_TASK_STATUS_FROZEN       0x0200U
#define OS_TCB_NAME_LEN             32

typedef struct TagTaskCB {
    VOID            *stackPointer;      /**< Task stack pointer | 内核栈指针位置(SP)  */	
    UINT16          taskStatus;         /**< Task status | 各种状态标签，可以拥有多种标签，按位标识 */

    UINT64          startTime;          /**< The start time of each phase of task | 任务开始时间  */
    UINT64          waitTime;          /**< Task delay time, tick number | 设置任务调度延期时间  */ 
    UINT64          irqStartTime;       /**< Interrupt start time | 任务中断开始时间  */ 
    UINT32          irqUsedTime;        /**< Interrupt consumption time | 任务中断消耗时间  */ 
    INT32           timeSlice;          /**< Task remaining time slice | 任务剩余时间片  */ 
    SortLinkList    sortList;           /**< Task sortlink node | 跟CPU捆绑的任务排序链表节点,上面挂的是就绪队列的下一个阶段,进入CPU要执行的任务队列  */	
    const SchedOps  *ops;
    SchedPolicy     sp;

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

STATIC INLINE VOID OsSchedIrqUsedTimeUpdate(VOID)
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

#ifdef LOSCFG_KERNEL_SMP
STATIC INLINE VOID IdleRunqueueFind(UINT16 *idleCpuid)
{
    SchedRunqueue *idleRq = OsSchedRunqueueByID(0);
    UINT32 nodeNum = OsGetSortLinkNodeNum(&idleRq->timeoutQueue);
    UINT16 cpuid = 1;
    do {
        SchedRunqueue *rq = OsSchedRunqueueByID(cpuid);
        UINT32 temp = OsGetSortLinkNodeNum(&rq->timeoutQueue);
        if (nodeNum > temp) {
            *idleCpuid = cpuid;
            nodeNum = temp;
        }
        cpuid++;
    } while (cpuid < LOSCFG_KERNEL_CORE_NUM);
}
#endif

STATIC INLINE VOID OsSchedTimeoutQueueAdd(LosTaskCB *taskCB, UINT64 responseTime)
{
#ifdef LOSCFG_KERNEL_SMP
    UINT16 cpuid = AFFI_MASK_TO_CPUID(taskCB->cpuAffiMask);
    if (cpuid >= LOSCFG_KERNEL_CORE_NUM) {
        cpuid = 0;
        IdleRunqueueFind(&cpuid);
    }
#else
    UINT16 cpuid = 0;
#endif

    SchedRunqueue *rq = OsSchedRunqueueByID(cpuid);
    OsAdd2SortLink(&rq->timeoutQueue, &taskCB->sortList, responseTime, cpuid);
#ifdef LOSCFG_KERNEL_SMP
    if ((cpuid != ArchCurrCpuid()) && (responseTime < rq->responseTime)) {
        rq->schedFlag |= INT_PEND_TICK;
        LOS_MpSchedule(CPUID_TO_AFFI_MASK(cpuid));
    }
#endif
}

STATIC INLINE VOID OsSchedTimeoutQueueDelete(LosTaskCB *taskCB)
{
    SortLinkList *node = &taskCB->sortList;
#ifdef LOSCFG_KERNEL_SMP
    SchedRunqueue *rq = OsSchedRunqueueByID(node->cpuid);
#else
    SchedRunqueue *rq = OsSchedRunqueueByID(0);
#endif
    UINT64 oldResponseTime = GET_SORTLIST_VALUE(node);
    OsDeleteFromSortLink(&rq->timeoutQueue, node);
    if (oldResponseTime <= rq->responseTime) {
        rq->responseTime = OS_SCHED_MAX_RESPONSE_TIME;
    }
}

STATIC INLINE UINT32 OsSchedTimeoutQueueAdjust(LosTaskCB *taskCB, UINT64 responseTime)
{
    UINT32 ret;
    SortLinkList *node = &taskCB->sortList;
#ifdef LOSCFG_KERNEL_SMP
    UINT16 cpuid = node->cpuid;
#else
    UINT16 cpuid = 0;
#endif
    SchedRunqueue *rq = OsSchedRunqueueByID(cpuid);
    ret = OsSortLinkAdjustNodeResponseTime(&rq->timeoutQueue, node, responseTime);
    if (ret == LOS_OK) {
        rq->schedFlag |= INT_PEND_TICK;
    }
    return ret;
}

STATIC INLINE VOID SchedTaskFreeze(LosTaskCB *taskCB)
{
    UINT64 responseTime;

#ifdef LOSCFG_KERNEL_PM
    if (!OsIsPmMode()) {
        return;
    }
#endif

    if (!(taskCB->taskStatus & (OS_TASK_STATUS_PEND_TIME | OS_TASK_STATUS_DELAY))) {
        return;
    }

    responseTime = GET_SORTLIST_VALUE(&taskCB->sortList);
    OsSchedTimeoutQueueDelete(taskCB);
    SET_SORTLIST_VALUE(&taskCB->sortList, responseTime);
    taskCB->taskStatus |= OS_TASK_STATUS_FROZEN;
    return;
}

STATIC INLINE VOID SchedTaskUnfreeze(LosTaskCB *taskCB)
{
    UINT64 currTime, responseTime;

    if (!(taskCB->taskStatus & OS_TASK_STATUS_FROZEN)) {
        return;
    }

    taskCB->taskStatus &= ~OS_TASK_STATUS_FROZEN;
    currTime = OsGetCurrSchedTimeCycle();
    responseTime = GET_SORTLIST_VALUE(&taskCB->sortList);
    if (responseTime > currTime) {
        OsSchedTimeoutQueueAdd(taskCB, responseTime);
        return;
    }

    SET_SORTLIST_VALUE(&taskCB->sortList, OS_SORT_LINK_INVALID_TIME);
    if (taskCB->taskStatus & OS_TASK_STATUS_PENDING) {
        LOS_ListDelete(&taskCB->pendList);
    }
    taskCB->taskStatus &= ~OS_TASK_STATUS_BLOCKED;
    return;
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
//获取最高优先级任务
STATIC INLINE LosTaskCB *HPFRunqueueTopTaskGet(HPFRunqueue *rq)
{
    LosTaskCB *newTask = NULL;
    UINT32 baseBitmap = rq->queueBitmap;
#ifdef LOSCFG_KERNEL_SMP
    UINT32 cpuid = ArchCurrCpuid();
#endif

    while (baseBitmap) {
        UINT32 basePrio = CLZ(baseBitmap);
        HPFQueue *queueList = &rq->queueList[basePrio];
        UINT32 bitmap = queueList->queueBitmap;
        while (bitmap) {
            UINT32 priority = CLZ(bitmap);
            LOS_DL_LIST_FOR_EACH_ENTRY(newTask, &queueList->priQueList[priority], LosTaskCB, pendList) {
#ifdef LOSCFG_KERNEL_SMP
                if (newTask->cpuAffiMask & (1U << cpuid)) {
#endif
                    return newTask;
#ifdef LOSCFG_KERNEL_SMP
                }
#endif
            }
            bitmap &= ~(1U << (OS_PRIORITY_QUEUE_NUM - priority - 1));
        }
        baseBitmap &= ~(1U << (OS_PRIORITY_QUEUE_NUM - basePrio - 1));
    }

    return NULL;
}

VOID HPFSchedPolicyInit(SchedRunqueue *rq);
VOID HPFTaskSchedParamInit(LosTaskCB *taskCB, UINT16 policy,
                           const SchedParam *parentParam, const TSK_INIT_PARAM_S *param);
VOID HPFProcessDefaultSchedParamGet(SchedParam *param);

VOID IdleTaskSchedParamInit(LosTaskCB *taskCB);

INT32 OsSchedParamCompare(const LosTaskCB *task1, const LosTaskCB *task2);
VOID OsSchedPriorityInheritance(LosTaskCB *owner, const SchedParam *param);
UINT32 OsSchedParamInit(LosTaskCB *taskCB, UINT16 policy,
                        const SchedParam *parentParam, const TSK_INIT_PARAM_S *param);
VOID OsSchedProcessDefaultSchedParamGet(UINT16 policy, SchedParam *param);

VOID OsSchedResponseTimeReset(UINT64 responseTime);
VOID OsSchedToUserReleaseLock(VOID);
VOID OsSchedTick(VOID);
UINT32 OsSchedInit(VOID);
VOID OsSchedStart(VOID);

VOID OsSchedRunqueueIdleInit(UINT32 idleTaskID);
VOID OsSchedRunqueueInit(VOID);

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

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_SCHED_PRI_H */
