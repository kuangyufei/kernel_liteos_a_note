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
#include "los_sched_pri.h"

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
 * Flag that indicates the task or task control block status.
 *
 * The task is user mode task.
 */
#define OS_TASK_FLAG_USER_MODE      0x0800U

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

/// 通过任务ID获取任务实体，task由任务池分配，本质是个数组，彼此都挨在一块
STATIC INLINE LosTaskCB *OsGetTaskCB(UINT32 taskID)
{
    return OS_TCB_FROM_TID(taskID);
}
/// 任务是否在使用
STATIC INLINE BOOL OsTaskIsUnused(const LosTaskCB *taskCB)
{
    return ((taskCB->taskStatus & OS_TASK_STATUS_UNUSED) != 0);
}

STATIC INLINE BOOL OsTaskIsKilled(const LosTaskCB *taskCB)
{
    return ((taskCB->taskStatus & OS_TASK_FLAG_EXIT_KILL) != 0);
}

STATIC INLINE BOOL OsTaskIsUserMode(const LosTaskCB *taskCB)
{
    return ((taskCB->taskStatus & OS_TASK_FLAG_USER_MODE) != 0);
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
extern VOID OsTaskProcSignal(VOID);
extern UINT32 OsCreateUserTask(UINT32 processID, TSK_INIT_PARAM_S *initParam);
extern INT32 OsSetTaskName(LosTaskCB *taskCB, const CHAR *name, BOOL setPName);
extern VOID OsTaskCBRecycleToFree(VOID);
extern VOID OsRunningTaskToExit(LosTaskCB *runTask, UINT32 status);
extern UINT32 OsUserTaskOperatePermissionsCheck(LosTaskCB *taskCB);
extern UINT32 OsUserProcessOperatePermissionsCheck(LosTaskCB *taskCB, UINT32 processID);
extern INT32 OsTcbDispatch(LosTaskCB *stcb, siginfo_t *info);
extern VOID OsWriteResourceEvent(UINT32 events);
extern VOID OsWriteResourceEventUnsafe(UINT32 events);
extern UINT32 OsResourceFreeTaskCreate(VOID);
extern VOID OsTaskInsertToRecycleList(LosTaskCB *taskCB);
extern VOID OsInactiveTaskDelete(LosTaskCB *taskCB);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_TASK_PRI_H */
