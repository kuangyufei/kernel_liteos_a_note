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

#ifndef _LOS_TASK_PRI_H
#define _LOS_TASK_PRI_H

#include "los_task.h"
#include "los_percpu_pri.h"
#include "los_spinlock.h"
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

/**
 * @ingroup los_task
 * Define task siginal types.
 *
 * Task siginal types.
 */
#define SIGNAL_NONE                 0U			///< 无信号
#define SIGNAL_KILL                 (1U << 0)	///< 干掉
#define SIGNAL_SUSPEND              (1U << 1)	///< 挂起
#define SIGNAL_AFFI                 (1U << 2)	///< CPU 亲和性,一个任务被切换后被同一个CPU再次执行,则亲和力高

/* scheduler lock */
extern SPIN_LOCK_S g_taskSpin;//任务自旋锁
#define SCHEDULER_LOCK(state)       LOS_SpinLockSave(&g_taskSpin, &(state))
#define SCHEDULER_UNLOCK(state)     LOS_SpinUnlockRestore(&g_taskSpin, state)

/* default and non-running task's ownership id */
#define OS_TASK_INVALID_CPUID       0xFFFF

/**
 * @ingroup los_task
 * Null task ID
 *
 */
#define OS_TASK_ERRORID             0xFFFFFFFF 

/**
 * @ingroup los_task
 * Define a usable task priority.
 *
 * Highest task priority.
 */
#define OS_TASK_PRIORITY_HIGHEST    0	//任务最高优先级,软时钟任务就是最高级任务,见于 OsSwtmrTaskCreate

/**
 * @ingroup los_task
 * Define a usable task priority.
 *
 * Lowest task priority.
 */
#define OS_TASK_PRIORITY_LOWEST     31 ///< 任务最低优先级

/**
 * @ingroup los_task
 * Flag that indicates the task or task control block status.
 *
 * The task is init.
 */
#define OS_TASK_STATUS_INIT         0x0001U ///< 初始化状态

/**
 * @ingroup los_task
 * Flag that indicates the task or task control block status.
 *
 * The task is ready.
 */
#define OS_TASK_STATUS_READY        0x0002U ///< 就绪状态的任务都将插入就绪队列,注意就绪队列的本质是个双向链表

/**
 * @ingroup los_task
 * Flag that indicates the task or task control block status.
 *
 * The task is running.
 */
#define OS_TASK_STATUS_RUNNING      0x0004U ///< 任务状态: 运行中

/**
 * @ingroup los_task
 * Flag that indicates the task or task control block status.
 *
 * The task is suspended.
 */
#define OS_TASK_STATUS_SUSPENDED    0x0008U ///< 任务状态: 暂停, 意思是发生了不需要等待资源的暂停,例如中断来了

/**
 * @ingroup los_task
 * Flag that indicates the task or task control block status.
 *
 * The task is blocked.
 */
#define OS_TASK_STATUS_PENDING      0x0010U ///< 任务状态: 挂起/待办 ,意思在需等待其他外在资源引起的停止,例如互斥锁/信号量 == 

/**
 * @ingroup los_task
 * Flag that indicates the task or task control block status.
 *
 * The task is delayed.
 */
#define OS_TASK_STATUS_DELAY        0x0020U ///< 延迟状态 ,例如 delay(100),因内部原因引起的等待 见于 OsSchedDelay 延期调度

/**
 * @ingroup los_task
 * Flag that indicates the task or task control block status.
 *
 * The time for waiting for an event to occur expires.
 */
#define OS_TASK_STATUS_TIMEOUT      0x0040U	///< 任务等待事件发生超时

/**
 * @ingroup los_task
 * Flag that indicates the task or task control block status.
 *
 * The task is pend for a period of time.
 */
#define OS_TASK_STATUS_PEND_TIME    0x0080U ///< 任务状态: 有时间限制的挂起/待办,例如等待某个事件时设置为时间到了就不等了

#define OS_TASK_STATUS_BLOCKED      (OS_TASK_STATUS_INIT | OS_TASK_STATUS_PENDING | \
                                     OS_TASK_STATUS_DELAY | OS_TASK_STATUS_PEND_TIME) ///< 任务状态: 阻塞

/**
 * @ingroup los_task
 * Flag that indicates the task or task control block status.
 *
 * The task is exit.
 */
#define OS_TASK_STATUS_EXIT         0x0100U ///< 任务状态:退出

/**
 * @ingroup los_task
 * Flag that indicates the task or task control block status.
 *
 * The task control block is unused.
 */
#define OS_TASK_STATUS_UNUSED       0x0200U ///< 任务状态:未使用

/**
 * @ingroup los_task
 * Flag that indicates the task or task control block status.
 *
 * The task is joinable.
 */
#define OS_TASK_FLAG_PTHREAD_JOIN   0x0400U ///< 主task和子task连在一块不分离

/**
 * @ingroup los_task
 * Flag that indicates the task property.
 *
 * The task is system-level task, like idle, swtmr and etc.
 */
#define OS_TASK_FLAG_SYSTEM_TASK    0x1000U	///< 系统任务

/**
 * @ingroup los_task
 * Flag that indicates the task property.
 *
 * The task is no-delete system task, like resourceTask. 
 */
#define OS_TASK_FLAG_NO_DELETE    0x2000U ///< 该任务是不可删除的系统任务，如资源回收任务

/**
 * @ingroup los_task
 * Flag that indicates the task property.
 *
 * Kills the thread during process exit.
 */
#define OS_TASK_FLAG_EXIT_KILL       0x4000U ///< 在进程退出期间一同被干掉的任务

/**
 * @ingroup los_task
 * Flag that indicates the task or task control block status.
 *
 * The delayed operation of this task is frozen.
 */
#define OS_TASK_FLAG_FREEZE          0x8000U

/**
 * @ingroup los_task
 * Flag that indicates the task property.
 *
 * Specifies the process creation task.
 */
#define OS_TASK_FLAG_SPECIFIES_PROCESS 0x0U ///< 创建指定任务 例如: cat weharmony.net 的实现 

/**
 * @ingroup los_task
 * Boundary on which the stack size is aligned.
 *
 */
#define OS_TASK_STACK_SIZE_ALIGN    16U ///< 堆栈大小对齐

/**
 * @ingroup los_task
 * Boundary on which the stack address is aligned.
 *
 */
#define OS_TASK_STACK_ADDR_ALIGN    8U ///< 堆栈地址对齐

/**
 * @ingroup los_task
 * Number of usable task priorities. | 任务优先级数量
 */
#define OS_TSK_PRINUM               (OS_TASK_PRIORITY_LOWEST - OS_TASK_PRIORITY_HIGHEST + 1)

/**
* @ingroup  los_task
* @brief Check whether a task ID is valid.
*
* @par Description:
* This API is used to check whether a task ID, excluding the idle task ID, is valid.
* @attention None.
*
* @param  taskID [IN] Task ID.
*
* @retval 0 or 1. One indicates that the task ID is invalid, whereas zero indicates that the task ID is valid.
* @par Dependency:
* <ul><li>los_task_pri.h: the header file that contains the API declaration.</li></ul>
* @see
*/
#define OS_TSK_GET_INDEX(taskID) (taskID)

/**
* @ingroup  los_task
* @brief Obtain the pointer to a task control block.
*
* @par Description:
* This API is used to obtain the pointer to a task control block using a corresponding parameter.
* @attention None.
*
* @param  ptr [IN] Parameter used for obtaining the task control block.
*
* @retval Pointer to the task control block.
* @par Dependency:
* <ul><li>los_task_pri.h: the header file that contains the API declaration.</li></ul>
* @see
*/ ///通过pendList取出TCB,用于挂入链表节点时使用 pendList的情况
#define OS_TCB_FROM_PENDLIST(ptr) LOS_DL_LIST_ENTRY(ptr, LosTaskCB, pendList)
 
/**
* @ingroup  los_task
* @brief Obtain the pointer to a task control block.
*
* @par Description:
* This API is used to obtain the pointer to a task control block that has a specified task ID.
* @attention None.
*
* @param  TaskID [IN] Task ID.
*
* @retval Pointer to the task control block.
* @par Dependency:
* <ul><li>los_task_pri.h: the header file that contains the API declaration.</li></ul>
* @see
*/
#define OS_TCB_FROM_TID(taskID) (((LosTaskCB *)g_taskCBArray) + (taskID)) ///< 通过任务ID从任务池中拿到实体

#ifndef LOSCFG_STACK_POINT_ALIGN_SIZE
#define LOSCFG_STACK_POINT_ALIGN_SIZE                       (sizeof(UINTPTR) * 2)
#endif

#define OS_TASK_RESOURCE_STATIC_SIZE    0x1000	///< 4K
#define OS_TASK_RESOURCE_FREE_PRIORITY  5		///< 回收资源任务的优先级
#define OS_RESOURCE_EVENT_MASK          0xFF	///< 资源事件的掩码
#define OS_RESOURCE_EVENT_OOM           0x02	///< 内存溢出事件
#define OS_RESOURCE_EVENT_FREE          0x04	///< 资源释放事件
#define OS_TCB_NAME_LEN 32	///< 任务名称长度上限

typedef struct {
    VOID            *stackPointer;      /**< Task stack pointer | 内核栈指针位置(SP)  */	
    UINT16          taskStatus;         /**< Task status | 各种状态标签，可以拥有多种标签，按位标识 */
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
    UINTPTR         userArea;			///< 用户空间的堆区开始位置
    UINTPTR         userMapBase;		///< 用户空间的栈顶位置,内存来自用户空间,和topOfStack有本质的区别.
    UINT32          userMapSize;        /**< user thread stack size ,real size : userMapSize + USER_STACK_MIN_SIZE | 用户栈大小 */
    UINT32          processID;          /**< Which belong process | 所属进程ID */
    FutexNode       futex;				///< 实现快锁功能
    LOS_DL_LIST     joinList;           /**< join list | 联结链表,允许任务之间相互释放彼此 */
    LOS_DL_LIST     lockList;           /**< Hold the lock list | 该链表上挂的都是已持有的锁 */
    UINTPTR         waitID;             /**< Wait for the PID or GID of the child process | 等待子进程的PID或GID */
    UINT16          waitFlag;           /**< The type of child process that is waiting, belonging to a group or parent,
                                             a specific child process, or any child process | 等待的子进程以什么样的方式结束(OS_TASK_WAIT_PROCESS | OS_TASK_WAIT_GID | ..) */
#ifdef LOSCFG_KERNEL_LITEIPC //轻量级进程间通信开关
    IpcTaskInfo     *ipcTaskInfo;	///< 任务间通讯信息结构体
#endif
#ifdef LOSCFG_KERNEL_PERF
    UINTPTR         pc;	///< pc寄存器
    UINTPTR         fp; ///< fp寄存器
#endif
} LosTaskCB;
///< LosTask结构体是给外部使用的
typedef struct {
    LosTaskCB *runTask;
    LosTaskCB *newTask;
} LosTask;

///< 进程信号描述符
struct ProcessSignalInfo {
    siginfo_t *sigInfo;       /**< Signal to be dispatched |  要发送的信号*/
    LosTaskCB *defaultTcb;    /**< Default TCB | 默认task,默认接收信号的任务. */
    LosTaskCB *unblockedTcb;  /**< The signal unblock on this TCB | 信号在此TCB上解除阻塞  */
    LosTaskCB *awakenedTcb;   /**< This TCB was awakened |  即 任务在等待这个信号,此信号一来任务被唤醒.*/
    LosTaskCB *receivedTcb;   /**< This TCB received the signal | 如果没有屏蔽信号,任务将接收这个信号. */
};

typedef int (*ForEachTaskCB)(LosTaskCB *tcb, void *arg);///< 回调任务函数,例如:进程被kill 9 时,通知所有任务善后处理

/**
 * @ingroup los_task
 * Maximum number of tasks.
 *
 */
extern UINT32 g_taskMaxNum;///< 任务最大数量 默认128个


/**
 * @ingroup los_task
 * Starting address of a task.
 *
 */
extern LosTaskCB *g_taskCBArray;///< 外部变量 任务池 默认128个

/**
 * @ingroup los_task
 * Time slice structure.
 */
typedef struct {//时间片结构体，任务轮询
    LosTaskCB *task; /**< Current running task | 当前运行着的任务*/
    UINT16 time;     /**< Expiration time point | 到期时间点*/
    UINT16 timeout;  /**< Expiration duration | 有效期*/
} OsTaskRobin;
/// 获取当前CPU  core运行的任务
STATIC INLINE LosTaskCB *OsCurrTaskGet(VOID)
{
    return (LosTaskCB *)ArchCurrTaskGet();
}
/// 告诉协处理器当前任务使用范围为内核空间 
STATIC INLINE VOID OsCurrTaskSet(LosTaskCB *task)
{
    ArchCurrTaskSet(task);
}
/// 告诉协处理器当前任务使用范围为 用户空间
STATIC INLINE VOID OsCurrUserTaskSet(UINTPTR thread)
{
    ArchCurrUserTaskSet(thread);
}
/// 通过任务ID获取任务实体，task由任务池分配，本质是个数组，彼此都挨在一块
STATIC INLINE LosTaskCB *OsGetTaskCB(UINT32 taskID)
{
    return OS_TCB_FROM_TID(taskID);
}
/// 任务是否在使用
STATIC INLINE BOOL OsTaskIsUnused(const LosTaskCB *taskCB)
{
    if (taskCB->taskStatus & OS_TASK_STATUS_UNUSED) {//在freelist中的任务都是 OS_TASK_STATUS_UNUSED 状态
        return TRUE;
    }

    return FALSE;
}
/// 任务是否在运行
STATIC INLINE BOOL OsTaskIsRunning(const LosTaskCB *taskCB)
{
    if (taskCB->taskStatus & OS_TASK_STATUS_RUNNING) {//一个CPU core 只能有一个 OS_TASK_STATUS_RUNNING task
        return TRUE;
    }

    return FALSE;
}
/// 任务是否不再活跃
STATIC INLINE BOOL OsTaskIsInactive(const LosTaskCB *taskCB)
{
    if (taskCB->taskStatus & (OS_TASK_STATUS_UNUSED | OS_TASK_STATUS_INIT | OS_TASK_STATUS_EXIT)) {//三个标签有一个 代表不在活动
        return TRUE;
    }

    return FALSE;
}
/// 任务是否挂起
STATIC INLINE BOOL OsTaskIsPending(const LosTaskCB *taskCB)
{
    if (taskCB->taskStatus & OS_TASK_STATUS_PENDING) {
        return TRUE;
    }

    return FALSE;
}
/// 任务是否被干掉
STATIC INLINE BOOL OsTaskIsKilled(const LosTaskCB *taskCB)
{
    if (taskCB->taskStatus & OS_TASK_FLAG_EXIT_KILL) {
        return TRUE;
    }

    return FALSE;
}

#define OS_TID_CHECK_INVALID(taskID) ((UINT32)(taskID) >= g_taskMaxNum)//是否有无效的任务 > 128

/* get task info */
#define OS_ALL_TASK_MASK  0xFFFFFFFF
/// 任务的信号列表
#define OS_TASK_WAIT_ANYPROCESS (1 << 0U)					///< 等待任意进程出现
#define OS_TASK_WAIT_PROCESS    (1 << 1U)					///< 等待指定进程出现
#define OS_TASK_WAIT_GID        (1 << 2U)					///< 等待组ID
#define OS_TASK_WAIT_SEM        (OS_TASK_WAIT_GID + 1)		///< 等待信号量发生
#define OS_TASK_WAIT_QUEUE      (OS_TASK_WAIT_SEM + 1)		///< 等待队列到来
#define OS_TASK_WAIT_JOIN       (OS_TASK_WAIT_QUEUE + 1)	///< 等待
#define OS_TASK_WAIT_SIGNAL     (OS_TASK_WAIT_JOIN + 1) 	///< 
#define OS_TASK_WAIT_LITEIPC    (OS_TASK_WAIT_SIGNAL + 1)	///< 等待liteipc到来
#define OS_TASK_WAIT_MUTEX      (OS_TASK_WAIT_LITEIPC + 1)	///< 等待MUTEX到来
#define OS_TASK_WAIT_FUTEX      (OS_TASK_WAIT_MUTEX + 1)	///< 等待FUTEX到来
#define OS_TASK_WAIT_EVENT      (OS_TASK_WAIT_FUTEX + 1) 	///< 等待事件发生
#define OS_TASK_WAIT_COMPLETE   (OS_TASK_WAIT_EVENT + 1)	///< 等待结束信号

/// 设置事件阻塞掩码,即设置任务的等待事件.
STATIC INLINE VOID OsTaskWaitSetPendMask(UINT16 mask, UINTPTR lockID, UINT32 timeout)
{
    LosTaskCB *runTask = OsCurrTaskGet();
    runTask->waitID = lockID;
    runTask->waitFlag = mask;
    (VOID)timeout;
}

/// 清除事件阻塞掩码,即任务不再等待任何事件.
STATIC INLINE VOID OsTaskWakeClearPendMask(LosTaskCB *resumeTask)
{
    resumeTask->waitID = 0;
    resumeTask->waitFlag = 0;
}

extern UINT32 OsTaskSetDetachUnsafe(LosTaskCB *taskCB);
extern VOID OsTaskJoinPostUnsafe(LosTaskCB *taskCB);
extern UINT32 OsTaskJoinPendUnsafe(LosTaskCB *taskCB);
extern BOOL OsTaskCpuAffiSetUnsafe(UINT32 taskID, UINT16 newCpuAffiMask, UINT16 *oldCpuAffiMask);
extern VOID OsTaskSchedule(LosTaskCB *, LosTaskCB *);
extern VOID OsTaskContextLoad(LosTaskCB *newTask);
extern VOID OsIdleTask(VOID);
extern UINT32 OsIdleTaskCreate(VOID);
extern UINT32 OsTaskInit(VOID);
extern UINT32 OsShellCmdDumpTask(INT32 argc, const CHAR **argv);
extern UINT32 OsShellCmdTskInfoGet(UINT32 taskID, VOID *seqfile, UINT16 flag);
extern LosTaskCB *OsGetMainTask(VOID);
extern VOID OsSetMainTask(VOID);
extern UINT32 OsGetIdleTaskId(VOID);
extern VOID OsTaskEntry(UINT32 taskID);
extern SortLinkAttribute *OsTaskSortLinkGet(VOID);
extern VOID OsTaskProcSignal(VOID);
extern UINT32 OsTaskDeleteUnsafe(LosTaskCB *taskCB, UINT32 status, UINT32 intSave);
extern VOID OsTaskResourcesToFree(LosTaskCB *taskCB);
extern VOID OsRunTaskToDelete(LosTaskCB *taskCB);
extern UINT32 OsCreateUserTask(UINT32 processID, TSK_INIT_PARAM_S *initParam);
extern INT32 OsSetTaskName(LosTaskCB *taskCB, const CHAR *name, BOOL setPName);
extern VOID OsTaskCBRecycleToFree(VOID);
extern VOID OsTaskExitGroup(UINT32 status);
extern VOID OsTaskToExit(LosTaskCB *taskCB, UINT32 status);
extern VOID OsExecDestroyTaskGroup(VOID);
extern UINT32 OsUserTaskOperatePermissionsCheck(LosTaskCB *taskCB);
extern UINT32 OsUserProcessOperatePermissionsCheck(LosTaskCB *taskCB, UINT32 processID);
extern INT32 OsTcbDispatch(LosTaskCB *stcb, siginfo_t *info);
extern VOID OsWriteResourceEvent(UINT32 events);
extern VOID OsWriteResourceEventUnsafe(UINT32 events);
extern UINT32 OsResourceFreeTaskCreate(VOID);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_TASK_PRI_H */
