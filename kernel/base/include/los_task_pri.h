/*
 * Copyright (c) 2013-2019, Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020, Huawei Device Co., Ltd. All rights reserved.
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
#include "los_sched_pri.h"
#include "los_sortlink_pri.h"
#include "los_spinlock.h"
#if (LOSCFG_KERNEL_SCHED_STATISTICS == YES)
#include "los_stat_pri.h"
#endif
#include "los_stackinfo_pri.h"
#include "los_futex_pri.h"
#include "los_signal.h"

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
#define SIGNAL_NONE                 0U
#define SIGNAL_KILL                 (1U << 0)
#define SIGNAL_SUSPEND              (1U << 1)
#define SIGNAL_AFFI                 (1U << 2)

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
#define OS_TASK_PRIORITY_HIGHEST    0

/**
 * @ingroup los_task
 * Define a usable task priority.
 *
 * Lowest task priority.
 */
#define OS_TASK_PRIORITY_LOWEST     31

/**
 * @ingroup los_task
 * Flag that indicates the task or task control block status.
 *
 * The task is init.
 */
#define OS_TASK_STATUS_INIT         0x0001U

/**
 * @ingroup los_task
 * Flag that indicates the task or task control block status.
 *
 * The task is ready.
 */
#define OS_TASK_STATUS_READY        0x0002U

/**
 * @ingroup los_task
 * Flag that indicates the task or task control block status.
 *
 * The task is running.
 */
#define OS_TASK_STATUS_RUNNING      0x0004U

/**
 * @ingroup los_task
 * Flag that indicates the task or task control block status.
 *
 * The task is suspended.
 */
#define OS_TASK_STATUS_SUSPEND      0x0008U

/**
 * @ingroup los_task
 * Flag that indicates the task or task control block status.
 *
 * The task is blocked.
 */
#define OS_TASK_STATUS_PEND         0x0010U

/**
 * @ingroup los_task
 * Flag that indicates the task or task control block status.
 *
 * The task is delayed.
 */
#define OS_TASK_STATUS_DELAY        0x0020U

/**
 * @ingroup los_task
 * Flag that indicates the task or task control block status.
 *
 * The time for waiting for an event to occur expires.
 */
#define OS_TASK_STATUS_TIMEOUT      0x0040U

/**
 * @ingroup los_task
 * Flag that indicates the task or task control block status.
 *
 * The task is pend for a period of time.
 */
#define OS_TASK_STATUS_PEND_TIME    0x0080U

/**
 * @ingroup los_task
 * Flag that indicates the task or task control block status.
 *
 * The task is exit.
 */
#define OS_TASK_STATUS_EXIT         0x0100U

/**
 * @ingroup los_task
 * Flag that indicates the task or task control block status.
 *
 * The task control block is unused.
 */
#define OS_TASK_STATUS_UNUSED       0x0200U

/**
 * @ingroup los_task
 * Flag that indicates the task or task control block status.
 *
 * The task is joinable.
 */
#define OS_TASK_FLAG_PTHREAD_JOIN   0x0400U //主task和子task连在一块不分离
//一个可结合的线程能够被其他线程收回其资源和杀死。在被其他线程回收之前，它的存储器资源（例如栈）是不释放的。
/**
 * @ingroup los_task
 * Flag that indicates the task or task control block status.
 *
 * The task is status detached.
 */
#define OS_TASK_FLAG_DETACHED       0x0800U //任务分离 主task与子task分离，子task结束后，资源自动回收
//一个分离的线程是不能被其他线程回收或杀死的，它的存储器资源在它终止时由系统自动释放。
/**
 * @ingroup los_task
 * Flag that indicates the task property.
 *
 * The task is idle task, Belong to idle process.
 */
#define OS_TASK_FLAG_IDLEFLAG       0x1000U

/**
 * @ingroup los_task
 * Flag that indicates the task property.
 *
 * The task is system-level task, like idle, swtmr and etc.
 */
#define OS_TASK_FLAG_SYSTEM_TASK    0x2000U //系统任务

/**
 * @ingroup los_task
 * Flag that indicates the task property.
 *
 * Specifies the process creation task.
 */
#define OS_TASK_FLAG_SPECIFIES_PROCESS 0x4000U

/**
 * @ingroup los_task
 * Boundary on which the stack size is aligned.
 *
 */
#define OS_TASK_STACK_SIZE_ALIGN    16U

/**
 * @ingroup los_task
 * Boundary on which the stack address is aligned.
 *
 */
#define OS_TASK_STACK_ADDR_ALIGN    8U

/**
 * @ingroup los_task
 * Number of usable task priorities.
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
*/
#define OS_TCB_FROM_PENDLIST(ptr) LOS_DL_LIST_ENTRY(ptr, LosTaskCB, pendList)
//通过pendList取出TCB,用于挂入链表节点时使用 pendList的情况 
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
#define OS_TCB_FROM_TID(taskID) (((LosTaskCB *)g_taskCBArray) + (taskID))
//通过Tid找到TCB
#ifndef LOSCFG_STACK_POINT_ALIGN_SIZE
#define LOSCFG_STACK_POINT_ALIGN_SIZE                       (sizeof(UINTPTR) * 2)
#endif

#define OS_TASK_RESOURCE_STATCI_SIZE    0x1000	//4K
#define OS_TASK_RESOURCE_FREE_PRIORITY  5		//回收资源任务的优先级
#define OS_RESOURCE_EVENT_MASK          0xFF	//资源事件的掩码
#define OS_RESOURCE_EVENT_OOM           0x02	//资源异常事件 暂时的理解是:OutOfMemory @note_thinking
#define OS_RESOURCE_EVENT_FREE          0x04	//资源释放事件
#define OS_TCB_NAME_LEN 32

typedef struct {
    VOID            *stackPointer;      /**< Task stack pointer */	//非用户模式下的栈指针
    UINT16          taskStatus;         /**< Task status */			//各种状态标签，可以拥有多种标签，按位标识
    UINT16          priority;           /**< Task priority */		//任务优先级[0:31],默认是31级
    UINT16          policy;				//任务的调度方式(三种 .. LOS_SCHED_RR )
    UINT16          timeSlice;          /**< Remaining time slice *///剩余时间片
    UINT32          stackSize;          /**< Task stack size */		//非用户模式下栈大小
    UINTPTR         topOfStack;         /**< Task stack top */		//非用户模式下的栈顶
    UINT32          taskID;             /**< Task ID */				//任务ID，任务池本质是一个大数组，ID就是数组的索引，默认 < 128
    TSK_ENTRY_FUNC  taskEntry;          /**< Task entrance function */	//任务执行入口函数
    VOID            *joinRetval;        /**< pthread adaption */
    VOID            *taskSem;           /**< Task-held semaphore */	//task在等哪个信号量
    VOID            *taskMux;           /**< Task-held mutex */		//task在等哪把锁
    VOID            *taskEvent;         /**< Task-held event */		//task在等哪个事件
    UINTPTR         args[4];            /**< Parameter, of which the maximum number is 4 */	//入口函数的参数 例如 main (int argc,char *argv[])
    CHAR            taskName[OS_TCB_NAME_LEN]; /**< Task name */	//任务的名称
    LOS_DL_LIST     pendList;           /**< Task pend node */		//如果任务阻塞时就通过它挂到各种阻塞情况的链表上,比如OsTaskWait时
    LOS_DL_LIST     threadList;         /**< thread list */			//挂到所属进程的线程链表上
    SortLinkList    sortList;           /**< Task sortlink node */	//挂到cpu core 的任务执行链表上
    UINT32          eventMask;          /**< Event mask */	//事件屏蔽
    UINT32          eventMode;          /**< Event mode */	//事件模式
    UINT32          priBitMap;          /**< BitMap for recording the change of task priority,	//任务在执行过程中优先级会经常变化，这个变量用来记录所有曾经变化
                                             the priority can not be greater than 31 */			//过的优先级，例如 ..01001011 曾经有过 0,1,3,6 优先级
    INT32           errorNo;            /**< Error Num */
    UINT32          signal;             /**< Task signal */ //任务信号 注意这个信号和 下面的sig是完全不一样的两个东东。
    sig_cb          sig;				//信号控制块，和上面的signal是两个东西，独立使用。鸿蒙这样放在一块会误导开发者！
#if (LOSCFG_KERNEL_SMP == YES)
    UINT16          currCpu;            /**< CPU core number of this task is running on */	//正在运行此任务的CPU内核号
    UINT16          lastCpu;            /**< CPU core number of this task is running on last time */ //上次运行此任务的CPU内核号
    UINT16          cpuAffiMask;        /**< CPU affinity mask, support up to 16 cores */	//CPU亲和力掩码，最多支持16核，亲和力很重要，多核情况下尽量一个任务在一个CPU核上运行，提高效率
    UINT32          timerCpu;           /**< CPU core number of this task is delayed or pended */	//此任务的CPU内核号被延迟或挂起
#if (LOSCFG_KERNEL_SMP_TASK_SYNC == YES)
    UINT32          syncSignal;         /**< Synchronization for signal handling */	//信号同步处理
#endif
#if (LOSCFG_KERNEL_SMP_LOCKDEP == YES)
    LockDep         lockDep;
#endif
#if (LOSCFG_KERNEL_SCHED_STATISTICS == YES)
    SchedStat       schedStat;          /**< Schedule statistics */	//调度统计
#endif
#endif
    UINTPTR         userArea;			//使用区域,由运行时划定,根据运行态不同而不同
    UINTPTR         userMapBase;		//用户模式下的栈底位置
    UINT32          userMapSize;        /**< user thread stack size ,real size : userMapSize + USER_STACK_MIN_SIZE */
    UINT32          processID;          /**< Which belong process *///所属进程ID
    FutexNode       futex;				//实现快锁功能
    LOS_DL_LIST     joinList;           /**< join list */ //联结链表,允许任务之间相互释放彼此
    LOS_DL_LIST     lockList;           /**< Hold the lock list */	//拿到了哪些锁链表
    UINT32          waitID;             /**< Wait for the PID or GID of the child process */	//等待孩子的PID或GID进程
    UINT16          waitFlag;           /**< The type of child process that is waiting, belonging to a group or parent,
                                             a specific child process, or any child process */
#if (LOSCFG_KERNEL_LITEIPC == YES)
    UINT32          ipcStatus;			//IPC状态
    LOS_DL_LIST     msgListHead;		//消息队列头结点
    BOOL            accessMap[LOSCFG_BASE_CORE_TSK_LIMIT];//访问图,指的是task之间是否能访问的标识,LOSCFG_BASE_CORE_TSK_LIMIT 为任务池总数
#endif
} LosTaskCB;
//LosTask结构体是给外部用的
typedef struct {
    LosTaskCB *runTask;
    LosTaskCB *newTask;
} LosTask;

struct ProcessSignalInfo {//进程信号信息
    siginfo_t *sigInfo;       /**< Signal to be dispatched */		//要发送的信号 例如 9 代表 kill process信号
    LosTaskCB *defaultTcb;    /**< Default TCB */					//默认task,指的是信号的发送方
    LosTaskCB *unblockedTcb;  /**< The signal unblock on this TCB*/	//解除阻塞这个task的信号
    LosTaskCB *awakenedTcb;   /**< This TCB was awakened */			//指定task要被唤醒的信号
    LosTaskCB *receivedTcb;   /**< This TCB received the signal */	//指定task接收信号
};

typedef int (*ForEachTaskCB)(LosTaskCB *tcb, void *arg);//函数指针

/**
 * @ingroup los_task
 * Maximum number of tasks.
 *
 */
extern UINT32 g_taskMaxNum;//任务最大数量 默认128个


/**
 * @ingroup los_task
 * Starting address of a task.
 *
 */
extern LosTaskCB *g_taskCBArray;//外部变量 任务池 默认128个

/**
 * @ingroup los_task
 * Time slice structure.
 */
typedef struct {//时间片结构体，任务轮询
    LosTaskCB *task; /**< Current running task */	//当前运行着的任务
    UINT16 time;     /**< Expiration time point */	//过期时间点
    UINT16 timeout;  /**< Expiration duration */	//有效期
} OsTaskRobin;
//获取当前CPU  core运行的任务
STATIC INLINE LosTaskCB *OsCurrTaskGet(VOID)
{
    return (LosTaskCB *)ArchCurrTaskGet();
}
//设置当前CPUcore运行任务
STATIC INLINE VOID OsCurrTaskSet(LosTaskCB *task)
{
    ArchCurrTaskSet(task);
}
//用户模式下设置当前Cpucore运行任务，参数为线程
STATIC INLINE VOID OsCurrUserTaskSet(UINTPTR thread)
{
    ArchCurrUserTaskSet(thread);
}
//通过任务ID获取任务实体，task由任务池分配，本质是个数组，彼此都挨在一块
STATIC INLINE LosTaskCB *OsGetTaskCB(UINT32 taskID)
{
    return OS_TCB_FROM_TID(taskID);
}
//任务是否在使用
STATIC INLINE BOOL OsTaskIsUnused(const LosTaskCB *taskCB)
{
    if (taskCB->taskStatus & OS_TASK_STATUS_UNUSED) {//在freelist中的任务都是 OS_TASK_STATUS_UNUSED 状态
        return TRUE;
    }

    return FALSE;
}
//任务是否在运行
STATIC INLINE BOOL OsTaskIsRunning(const LosTaskCB *taskCB)
{
    if (taskCB->taskStatus & OS_TASK_STATUS_RUNNING) {//一个CPU core 只能有一个 OS_TASK_STATUS_RUNNING task
        return TRUE;
    }

    return FALSE;
}
//任务是否不再活动
STATIC INLINE BOOL OsTaskIsInactive(const LosTaskCB *taskCB)
{
    if (taskCB->taskStatus & (OS_TASK_STATUS_UNUSED | OS_TASK_STATUS_INIT | OS_TASK_STATUS_EXIT)) {//三个标签有一个代表不在活动
        return TRUE;
    }

    return FALSE;
}

#define OS_TID_CHECK_INVALID(taskID) ((UINT32)(taskID) >= g_taskMaxNum)//是否有无效的任务 > 128

/* get task info */
#define OS_ALL_TASK_MASK  0xFFFFFFFF
//进程就绪队列大小
#define OS_PROCESS_PRI_QUEUE_SIZE(processCB) OsPriQueueProcessSize(g_priQueueList, (processCB)->priority)
//默认是从尾入进程的任务就绪队列
#define OS_TASK_PRI_QUEUE_ENQUEUE(processCB, taskCB) \
    OsPriQueueEnqueue((processCB)->threadPriQueueList, &((processCB)->threadScheduleMap), \
                      &((taskCB)->pendList), (taskCB)->priority)
//从头部入进程的任务就绪队列
#define OS_TASK_PRI_QUEUE_ENQUEUE_HEAD(processCB, taskCB) \
    OsPriQueueEnqueueHead((processCB)->threadPriQueueList, &((processCB)->threadScheduleMap), \
                      &((taskCB)->pendList), (taskCB)->priority)
//从进程的任务就绪队列的头部摘除
#define OS_TASK_PRI_QUEUE_DEQUEUE(processCB, taskCB) \
    OsPriQueueDequeue((processCB)->threadPriQueueList, &((processCB)->threadScheduleMap), &((taskCB)->pendList))

//入和出 任务调度就绪队列
#define OS_TASK_SCHED_QUEUE_ENQUEUE(taskCB, status) OsTaskSchedQueueEnqueue(taskCB, status)
#define OS_TASK_SCHED_QUEUE_DEQUEUE(taskCB, status) OsTaskSchedQueueDequeue(taskCB, status)
//入和出 进程的就绪队列 ，还提供了从头部入队列的方法
#define OS_PROCESS_PRI_QUEUE_ENQUEUE(processCB) \	
    OsPriQueueEnqueue(g_priQueueList, &g_priQueueBitmap, &((processCB)->pendList), (processCB)->priority)
#define OS_PROCESS_PRI_QUEUE_ENQUEUE_HEAD(processCB) \
    OsPriQueueEnqueueHead(g_priQueueList, &g_priQueueBitmap, &((processCB)->pendList), (processCB)->priority)
#define OS_PROCESS_PRI_QUEUE_DEQUEUE(processCB) OsPriQueueProcessDequeue(&((processCB)->pendList))
//任务就绪队列的大小 和 获取进程中一个优先级最高的任务
#define OS_TASK_PRI_QUEUE_SIZE(processCB, taskCB) OsPriQueueSize((processCB)->threadPriQueueList, (taskCB)->priority)
#define OS_TASK_GET_NEW(processCB) LOS_DL_LIST_ENTRY(OsPriQueueTop((processCB)->threadPriQueueList,     \
                                                                    &((processCB)->threadScheduleMap)), \
                                                     LosTaskCB, pendList)
//获取一个优先级最高的进程
#define OS_PROCESS_GET_NEW() \
        LOS_DL_LIST_ENTRY(OsPriQueueTop(g_priQueueList, &g_priQueueBitmap), LosProcessCB, pendList)

/**
 * @ingroup  los_task
 * @brief Modify the priority of task.
 *
 * @par Description:
 * This API is used to modify the priority of task.
 *
 * @attention
 * <ul>
 * <li>The taskCB should be a correct pointer to task control block structure.</li>
 * <li>the priority should be in [0, OS_TASK_PRIORITY_LOWEST].</li>
 * </ul>
 *
 * @param  taskCB [IN] Type #LosTaskCB * pointer to task control block structure.
 * @param  priority  [IN] Type #UINT16 the priority of task.
 *
 * @retval  None.
 * @par Dependency:
 * <ul><li>los_task_pri.h: the header file that contains the API declaration.</li></ul>
 * @see
 */
extern VOID OsTaskPriModify(LosTaskCB *taskCB, UINT16 priority);

/**
 * @ingroup  los_task
 * @brief pend running task to pendlist
 *
 * @par Description:
 * This API is used to pend task to pendlist and add to sorted delay list.
 *
 * @attention
 * <ul>
 * <li>The list should be a vaild pointer to pendlist.</li>
 * </ul>
 *
 * @param  list       [IN] Type #LOS_DL_LIST * pointer to list which running task will be pended.
 * @param  timeout    [IN] Type #UINT32  Expiry time. The value range is [0,LOS_WAIT_FOREVER].
 * @param  needSched  [IN] Type #bool  need sched
 *
 * @retval  LOS_OK       wait success
 * @retval  LOS_NOK      pend out
 * @par Dependency:
 * <ul><li>los_task_pri.h: the header file that contains the API declaration.</li></ul>
 * @see OsTaskWake
 */
extern UINT32 OsTaskWait(LOS_DL_LIST *list, UINT32 timeout, BOOL needSched);

/**
 * @ingroup  los_task
 * @brief delete task from pendlist.
 *
 * @par Description:
 * This API is used to delete task from pendlist and also add to the priqueue.
 *
 * @attention
 * <ul>
 * <li>The resumedTask should be the task which will be add to priqueue.</li>
 * </ul>
 *
 * @param  resumedTask [IN] Type #LosTaskCB * pointer to the task which will be add to priqueue.
 *
 * @retval  None.
 * @par Dependency:
 * <ul><li>los_task_pri.h: the header file that contains the API declaration.</li></ul>
 * @see OsTaskWait
 */
extern VOID OsTaskWake(LosTaskCB *resumedTask);

extern UINT32 OsTaskSetDeatchUnsafe(LosTaskCB *taskCB);
extern VOID OsTaskJoinPostUnsafe(LosTaskCB *taskCB);
extern UINT32 OsTaskJoinPendUnsafe(LosTaskCB *taskCB);
extern VOID OsTaskSchedule(LosTaskCB *, LosTaskCB *);
extern VOID OsStartToRun(LosTaskCB *);
extern VOID OsTaskScan(VOID);
extern VOID OsIdleTask(VOID);
extern UINT32 OsIdleTaskCreate(VOID);
extern UINT32 OsTaskInit(VOID);
extern UINT32 OsShellCmdDumpTask(INT32 argc, const CHAR **argv);
extern UINT32 OsShellCmdTskInfoGet(UINT32 taskID, VOID *seqfile, UINT16 flag);
extern VOID* OsGetMainTask(VOID);
extern VOID OsSetMainTask(VOID);
extern LosTaskCB* OsGetTopTask(VOID);
extern UINT32 OsGetIdleTaskId(VOID);
extern VOID OsTaskEntry(UINT32 taskID);
extern SortLinkAttribute *OsTaskSortLinkGet(VOID);
extern UINT32 OsTaskSwitchCheck(LosTaskCB *oldTask, LosTaskCB *newTask);
extern UINT32 OsTaskProcSignal(VOID);
extern VOID OsSchedStatistics(LosTaskCB *runTask, LosTaskCB *newTask);
extern UINT32 OsTaskDeleteUnsafe(LosTaskCB *taskCB, UINT32 status, UINT32 intSave);
extern VOID OsTaskResourcesToFree(LosTaskCB *taskCB);
extern VOID OsRunTaskToDelete(LosTaskCB *taskCB);
extern UINT32 OsTaskSyncWait(const LosTaskCB *taskCB);
extern INT32 OsCreateUserTask(UINT32 processID, TSK_INIT_PARAM_S *initParam);
extern INT32 OsTaskSchedulerSetUnsafe(LosTaskCB *taskCB, UINT16 policy, UINT16 priority,
                                      BOOL policyFlag, UINT32 intSave);
extern INT32 OsSetCurrTaskName(const CHAR *name);
extern VOID OsTaskCBRecyleToFree(VOID);
extern VOID OsTaskExitGroup(UINT32 status);
extern VOID OsTaskToExit(LosTaskCB *taskCB, UINT32 status);
extern VOID OsExecDestroyTaskGroup(VOID);
extern VOID OsProcessSuspendAllTask(VOID);
extern UINT32 OsUserTaskOperatePermissionsCheck(LosTaskCB *taskCB);
extern VOID OsWriteResourceEvent(UINT32 events);
extern UINT32 OsCreateResourceFreeTask(VOID);
#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_TASK_PRI_H */
