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
 * @brief 任务信号类型定义
 * @par 信号类型说明：
 * - 信号采用位掩码方式定义，支持多信号组合
 * - 未使用的信号位应保持为0
 */
#define SIGNAL_NONE                 0U                  /**< 无信号 */
#define SIGNAL_KILL                 (1U << 0)           /**< 终止任务信号 (位0) */
#define SIGNAL_SUSPEND              (1U << 1)           /**< 挂起任务信号 (位1) */
#define SIGNAL_AFFI                 (1U << 2)           /**< CPU亲和性变更信号 (位2) */

/** @name 调度器锁操作宏 */
/** @{ */
extern SPIN_LOCK_S g_taskSpin;                          /**< 任务调度器自旋锁 */

/**
 * @brief 检查调度器锁是否被持有
 * @return 1-已持有，0-未持有
 */
#define SCHEDULER_HELD()            LOS_SpinHeld(&g_taskSpin)

/**
 * @brief 获取调度器锁并保存中断状态
 * @param state [OUT] 用于保存中断状态的变量
 * @note 必须与SCHEDULER_UNLOCK配对使用
 */
#define SCHEDULER_LOCK(state)       LOS_SpinLockSave(&g_taskSpin, &(state))

/**
 * @brief 释放调度器锁并恢复中断状态
 * @param state [IN] 之前保存的中断状态
 * @note 必须与SCHEDULER_LOCK配对使用
 */
#define SCHEDULER_UNLOCK(state)     LOS_SpinUnlockRestore(&g_taskSpin, state)
/** @} */

/** @name 任务ID与CPUID定义 */
/** @{ */
#define OS_TASK_INVALID_CPUID       0xFFFF              /**< 无效CPU ID (十进制65535) */

/**
 * @ingroup los_task
 * @brief 无效任务ID
 * @note 用于表示任务创建或查找失败
 */
#define OS_TASK_ERRORID             0xFFFFFFFF          /**< 无效任务ID (十进制4294967295) */
/** @} */

/** @name 任务状态与属性标志位 */
/** @{ */
/**
 * @ingroup los_task
 * @brief 任务控制块未使用标志
 * @note 当TCB状态为该值时，表示此TCB可分配给新任务
 */
#define OS_TASK_STATUS_UNUSED       0x0400U             /**< TCB未使用 (十进制1024) */

/**
 * @ingroup los_task
 * @brief 任务可连接标志
 * @note 设置此标志的任务在退出后会保留TCB，直到其他任务调用join获取其退出状态
 */
#define OS_TASK_FLAG_PTHREAD_JOIN   0x0800U             /**< 可连接任务标志 (十进制2048) */

#define OS_TASK_FLAG_USER_MODE      0x1000U             /**< 用户模式任务标志 (十进制4096) */
#define OS_TASK_FLAG_SYSTEM_TASK    0x2000U             /**< 系统任务标志 (十进制8192)，如idle、swtmr任务 */
#define OS_TASK_FLAG_NO_DELETE      0x4000U             /**< 禁止删除标志 (十进制16384)，如resourceTask任务 */
#define OS_TASK_FLAG_EXIT_KILL      0x8000U             /**< 进程退出时终止标志 (十进制32768) */
#define OS_TASK_FLAG_SPECIFIES_PROCESS 0x0U             /**< 进程创建任务标志 */
/** @} */

/** @name 任务栈对齐定义 */
/** @{ */
#define OS_TASK_STACK_SIZE_ALIGN    16U                 /**< 栈大小对齐边界 (16字节) */
#define OS_TASK_STACK_ADDR_ALIGN    8U                  /**< 栈地址对齐边界 (8字节) */
/** @} */

/**
 * @ingroup los_task
 * @brief 可用任务优先级数量
 * @note 计算公式：最低优先级 - 最高优先级 + 1
 */
#define OS_TSK_PRINUM               (OS_TASK_PRIORITY_LOWEST - OS_TASK_PRIORITY_HIGHEST + 1)

/**
* @ingroup  los_task
* @brief 获取任务索引
* @par 描述：将任务ID直接作为任务索引使用
* @param  taskID [IN] 任务ID
* @return 任务索引值，等于输入的taskID
* @par 依赖：
* <ul><li>los_task_pri.h: 包含此API声明的头文件</li></ul>
*/
#define OS_TSK_GET_INDEX(taskID) (taskID)

/**
* @ingroup  los_task
* @brief 从等待链表项获取任务控制块指针
* @par 描述：通过等待链表节点反向获取对应的任务控制块
* @param  ptr [IN] 等待链表节点指针
* @return 任务控制块指针
* @see LosTaskCB, LOS_DL_LIST_ENTRY
*/
#define OS_TCB_FROM_PENDLIST(ptr) LOS_DL_LIST_ENTRY(ptr, LosTaskCB, pendList)

/**
* @ingroup  los_task
* @brief 从真实任务ID获取任务控制块指针
* @par 描述：通过全局任务控制块数组计算指定任务ID对应的TCB地址
* @param  TaskID [IN] 真实任务ID (系统内唯一)
* @return 任务控制块指针
* @see g_taskCBArray, LosTaskCB
*/
#define OS_TCB_FROM_RTID(taskID) (((LosTaskCB *)g_taskCBArray) + (taskID))

#ifdef LOSCFG_PID_CONTAINER
/**
* @brief 支持PID容器时，从虚拟任务ID获取TCB
* @note 通过OsGetTCBFromVtid函数进行虚拟ID到真实ID的转换
*/
#define OS_TCB_FROM_TID(taskID) OsGetTCBFromVtid(taskID)
#else
/**
* @brief 不支持PID容器时，直接使用真实ID获取TCB
* @note 虚拟ID与真实ID相同，直接调用OS_TCB_FROM_RTID
*/
#define OS_TCB_FROM_TID(taskID) OS_TCB_FROM_RTID(taskID)
#endif

/**
 * @brief 栈指针对齐大小配置
 * @note 默认为指针大小的2倍，确保栈指针按架构要求对齐
 */
#ifndef LOSCFG_STACK_POINT_ALIGN_SIZE
#define LOSCFG_STACK_POINT_ALIGN_SIZE                       (sizeof(UINTPTR) * 2)
#endif

/** @name 任务资源管理常量 */
/** @{ */
#define OS_TASK_RESOURCE_STATIC_SIZE    0x1000             /**< 静态资源大小 (4096字节) */
#define OS_TASK_RESOURCE_FREE_PRIORITY  5                  /**< 资源释放任务优先级 */
#define OS_RESOURCE_EVENT_MASK          0xFF               /**< 资源事件掩码 (8位) */
#define OS_RESOURCE_EVENT_OOM           0x02               /**< 内存不足事件 (位1) */
#define OS_RESOURCE_EVENT_FREE          0x04               /**< 资源释放事件 (位2) */
/** @} */

/**
 * @ingroup los_task
 * @brief 任务调度信息结构体
 * @note 用于保存调度过程中的任务切换信息
 */
typedef struct {
    LosTaskCB *runTask;       /**< 当前运行任务控制块指针 */
    LosTaskCB *newTask;       /**< 即将运行的新任务控制块指针 */
} LosTask;

/**
 * @ingroup los_task
 * @brief 进程信号信息结构体
 * @note 用于在进程内分发信号时保存相关任务信息
 */
struct ProcessSignalInfo {
    siginfo_t *sigInfo;       /**< 待分发的信号信息 */
    LosTaskCB *defaultTcb;    /**< 默认任务控制块 (通常为进程主线程) */
    LosTaskCB *unblockedTcb;  /**< 被信号解除阻塞的任务控制块 */
    LosTaskCB *awakenedTcb;   /**< 被信号唤醒的任务控制块 */
    LosTaskCB *receivedTcb;   /**< 最终接收信号的任务控制块 */
};
/**
 * @ingroup los_task
 * @brief 任务遍历回调函数类型定义
 *
 * @param tcb 任务控制块指针，指向当前遍历到的任务
 * @param arg 用户传入的参数，用于回调函数内部处理
 * @return int 回调函数返回值，通常0表示继续遍历，非0表示停止遍历
 */
typedef int (*ForEachTaskCB)(LosTaskCB *tcb, void *arg);

/**
 * @ingroup los_task
 * @brief 最大任务数量
 *
 * 系统支持的最大任务数，任务ID不能超过此值
 */
extern UINT32 g_taskMaxNum;

/**
 * @ingroup los_task
 * @brief 任务控制块数组起始地址
 *
 * 指向系统中所有任务控制块组成的数组的首地址
 */
extern LosTaskCB *g_taskCBArray;

/**
 * @ingroup los_task
 * @brief 时间片轮转结构体
 *
 * 用于记录和管理任务的时间片信息，包括当前运行任务、过期时间点和持续时长
 */
typedef struct {
    LosTaskCB *task; /**< 当前运行的任务控制块指针 */
    UINT16 time;     /**< 时间片过期时间点，单位：系统时钟滴答数 */
    UINT16 timeout;  /**< 时间片持续时长，单位：系统时钟滴答数 */
} OsTaskRobin;

/**
 * @ingroup los_task
 * @brief 根据任务ID获取任务控制块指针
 *
 * @param taskID 任务ID
 * @return LosTaskCB* 任务控制块指针，若任务ID无效则返回NULL
 * @note 内部使用宏OS_TCB_FROM_TID实现，需确保taskID有效
 */
STATIC INLINE LosTaskCB *OsGetTaskCB(UINT32 taskID)
{
    return OS_TCB_FROM_TID(taskID);
}

/**
 * @ingroup los_task
 * @brief 检查任务是否未使用
 *
 * @param taskCB 任务控制块指针
 * @return BOOL 若任务状态为未使用则返回TRUE，否则返回FALSE
 * @note 任务未使用状态由OS_TASK_STATUS_UNUSED标志位表示
 */
STATIC INLINE BOOL OsTaskIsUnused(const LosTaskCB *taskCB)
{
    return ((taskCB->taskStatus & OS_TASK_STATUS_UNUSED) != 0);
}

/**
 * @ingroup los_task
 * @brief 检查任务是否已被杀死
 *
 * @param taskCB 任务控制块指针
 * @return BOOL 若任务被标记为杀死状态则返回TRUE，否则返回FALSE
 * @note 任务杀死状态由OS_TASK_FLAG_EXIT_KILL标志位表示
 */
STATIC INLINE BOOL OsTaskIsKilled(const LosTaskCB *taskCB)
{
    return((taskCB->taskStatus & OS_TASK_FLAG_EXIT_KILL) != 0);
}

/**
 * @ingroup los_task
 * @brief 检查任务是否不可删除
 *
 * @param taskCB 任务控制块指针
 * @return BOOL 若任务不可删除则返回TRUE，否则返回FALSE
 * @note 满足以下任一条件的任务不可删除：
 *       1. 任务状态为未使用(OS_TASK_STATUS_UNUSED)
 *       2. 任务为系统任务(OS_TASK_FLAG_SYSTEM_TASK)
 *       3. 任务被标记为不可删除(OS_TASK_FLAG_NO_DELETE)
 */
STATIC INLINE BOOL OsTaskIsNotDelete(const LosTaskCB *taskCB)
{
    return ((taskCB->taskStatus & (OS_TASK_STATUS_UNUSED | OS_TASK_FLAG_SYSTEM_TASK | OS_TASK_FLAG_NO_DELETE)) != 0);
}

/**
 * @ingroup los_task
 * @brief 检查任务是否运行在用户模式
 *
 * @param taskCB 任务控制块指针
 * @return BOOL 若任务运行在用户模式则返回TRUE，否则返回FALSE
 * @note 用户模式由OS_TASK_FLAG_USER_MODE标志位表示
 */
STATIC INLINE BOOL OsTaskIsUserMode(const LosTaskCB *taskCB)
{
    return ((taskCB->taskStatus & OS_TASK_FLAG_USER_MODE) != 0);
}

/**
 * @ingroup los_task
 * @brief 检查任务ID是否无效
 *
 * @param taskID 任务ID
 * @return BOOL 若任务ID无效则返回TRUE，否则返回FALSE
 * @note 任务ID有效范围为[0, g_taskMaxNum-1]，大于等于g_taskMaxNum的ID视为无效
 */
#define OS_TID_CHECK_INVALID(taskID) ((UINT32)(taskID) >= g_taskMaxNum)

/* 获取任务信息相关宏定义 */
#define OS_ALL_TASK_MASK  0xFFFFFFFF /**< 所有任务掩码，表示匹配系统中所有任务 */

/**
 * @ingroup los_task
 * @brief 任务等待类型掩码定义
 *
 * 用于标识任务正在等待的资源类型，不同的掩码值对应不同的等待对象
 */
#define OS_TASK_WAIT_ANYPROCESS (1 << 0U)  /**< 等待任意进程，值为1(0x0001) */
#define OS_TASK_WAIT_PROCESS    (1 << 1U)  /**< 等待特定进程，值为2(0x0002) */
#define OS_TASK_WAIT_GID        (1 << 2U)  /**< 等待组ID，值为4(0x0004) */
#define OS_TASK_WAIT_SEM        (OS_TASK_WAIT_GID + 1)        /**< 等待信号量，值为5(0x0005) */
#define OS_TASK_WAIT_QUEUE      (OS_TASK_WAIT_SEM + 1)        /**< 等待消息队列，值为6(0x0006) */
#define OS_TASK_WAIT_JOIN       (OS_TASK_WAIT_QUEUE + 1)       /**< 等待任务连接，值为7(0x0007) */
#define OS_TASK_WAIT_SIGNAL     (OS_TASK_WAIT_JOIN + 1)       /**< 等待信号，值为8(0x0008) */
#define OS_TASK_WAIT_LITEIPC    (OS_TASK_WAIT_SIGNAL + 1)     /**< 等待轻量级IPC，值为9(0x0009) */
#define OS_TASK_WAIT_MUTEX      (OS_TASK_WAIT_LITEIPC + 1)    /**< 等待互斥锁，值为10(0x000A) */
#define OS_TASK_WAIT_FUTEX      (OS_TASK_WAIT_MUTEX + 1)      /**< 等待Futex，值为11(0x000B) */
#define OS_TASK_WAIT_EVENT      (OS_TASK_WAIT_FUTEX + 1)      /**< 等待事件，值为12(0x000C) */
#define OS_TASK_WAIT_COMPLETE   (OS_TASK_WAIT_EVENT + 1)      /**< 等待完成量，值为13(0x000D) */

/**
 * @ingroup los_task
 * @brief 设置任务等待状态的挂起掩码
 *
 * @param mask 等待类型掩码，取值为OS_TASK_WAIT_xxx系列宏
 * @param lockID 等待的锁ID，标识具体等待的同步对象
 * @param timeout 超时时间，单位：系统时钟滴答数
 * @note 该函数会修改当前运行任务的waitID和waitFlag字段
 */
STATIC INLINE VOID OsTaskWaitSetPendMask(UINT16 mask, UINTPTR lockID, UINT32 timeout)
{
    LosTaskCB *runTask = OsCurrTaskGet();
    runTask->waitID = lockID;
    runTask->waitFlag = mask;
    (VOID)timeout; /**< 未使用超时参数，此处显式转换为VOID避免编译警告 */
}

/**
 * @ingroup los_task
 * @brief 清除任务唤醒时的挂起掩码
 *
 * @param resumeTask 被唤醒的任务控制块指针
 * @note 该函数会将任务的waitID和waitFlag字段清零，表示任务不再等待任何对象
 */
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
extern UINT32 OsIdleTaskCreate(UINTPTR processID);
extern UINT32 OsTaskInit(UINTPTR processCB);
extern UINT32 OsShellCmdDumpTask(INT32 argc, const CHAR **argv);
extern UINT32 OsShellCmdTskInfoGet(UINT32 taskID, VOID *seqfile, UINT16 flag);
extern LosTaskCB *OsGetMainTask(VOID);
extern VOID OsSetMainTask(VOID);
extern UINT32 OsGetIdleTaskId(VOID);
extern VOID OsTaskEntry(UINT32 taskID);
extern VOID OsTaskProcSignal(VOID);
extern UINT32 OsCreateUserTask(UINTPTR processID, TSK_INIT_PARAM_S *initParam);
extern INT32 OsSetTaskName(LosTaskCB *taskCB, const CHAR *name, BOOL setPName);
extern VOID OsTaskCBRecycleToFree(VOID);
extern VOID OsRunningTaskToExit(LosTaskCB *runTask, UINT32 status);
extern INT32 OsUserTaskOperatePermissionsCheck(const LosTaskCB *taskCB);
extern INT32 OsUserProcessOperatePermissionsCheck(const LosTaskCB *taskCB, UINTPTR processCB);
extern INT32 OsTcbDispatch(LosTaskCB *stcb, siginfo_t *info);
extern VOID OsWriteResourceEvent(UINT32 events);
extern VOID OsWriteResourceEventUnsafe(UINT32 events);
extern UINT32 OsResourceFreeTaskCreate(VOID);
extern VOID OsTaskInsertToRecycleList(LosTaskCB *taskCB);
extern VOID OsInactiveTaskDelete(LosTaskCB *taskCB);
extern VOID OsSetMainTaskProcess(UINTPTR processCB);
extern LosTaskCB *OsGetDefaultTaskCB(VOID);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_TASK_PRI_H */
