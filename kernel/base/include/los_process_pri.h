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

#ifndef _LOS_PROCESS_PRI_H
#define _LOS_PROCESS_PRI_H

#include "los_sortlink_pri.h"
#include "los_priqueue_pri.h"
#include "los_task_pri.h"
#include "los_sem_pri.h"
#include "los_process.h"
#include "los_vm_map.h"
#if (LOSCFG_KERNEL_LITEIPC == YES)
#include "hm_liteipc.h"
#endif
#ifdef LOSCFG_SECURITY_CAPABILITY
#include "capability_type.h"
#endif
#ifdef LOSCFG_SECURITY_VID
#include "vid_type.h"
#endif

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#define OS_PCB_NAME_LEN OS_TCB_NAME_LEN

#ifdef LOSCFG_SECURITY_CAPABILITY
#define OS_GROUPS_NUMBER_MAX 256

typedef struct {
    UINT32  userID;
    UINT32  effUserID;
    UINT32  gid;
    UINT32  effGid;
    UINT32  groupNumber;
    UINT32  groups[1];
} User;
#endif

typedef struct {
    UINT32      groupID;         /**< Process group ID is the PID of the process that created the group */
    LOS_DL_LIST processList;     /**< List of processes under this process group */
    LOS_DL_LIST exitProcessList; /**< List of closed processes (zombie processes) under this group */
    LOS_DL_LIST groupList;       /**< Process group list */
} ProcessGroup;

typedef struct ProcessCB {
    CHAR                 processName[OS_PCB_NAME_LEN]; /**< Process name */
    UINT32               processID;                    /**< process ID = leader thread ID */
    UINT16               processStatus;                /**< [15:4] process Status; [3:0] The number of threads currently
                                                            running in the process *///这里设计很巧妙.用一个16表示了两层逻辑 数量和状态,点赞!
    UINT16               priority;                     /**< process priority */
    UINT16               policy;                       /**< process policy */
    UINT16               timeSlice;                    /**< Remaining time slice */
    UINT16               consoleID;                    /**< The console id of task belongs  */
    UINT16               processMode;                  /**< Kernel Mode:0; User Mode:1; */
    UINT32               parentProcessID;              /**< Parent process ID */
    UINT32               exitCode;                     /**< process exit status */
    LOS_DL_LIST          pendList;                     /**< Block list to which the process belongs */
    LOS_DL_LIST          childrenList;                 /**< my children process list */
    LOS_DL_LIST          exitChildList;                /**< my exit children process list */	//那些要退出孩子进程链表，白发人送黑发人。
    LOS_DL_LIST          siblingList;                  /**< linkage in my parent's children list */
    ProcessGroup         *group;                       /**< Process group to which a process belongs */
    LOS_DL_LIST          subordinateGroupList;         /**< linkage in my group list */
    UINT32               threadGroupID;                /**< Which thread group , is the main thread ID of the process */
    UINT32               threadScheduleMap;            /**< The scheduling bitmap table for the thread group of the
                                                            process */
    LOS_DL_LIST          threadSiblingList;            /**< List of threads under this process */
    LOS_DL_LIST          threadPriQueueList[OS_PRIORITY_QUEUE_NUM]; /**< The process's thread group schedules the
                                                                         priority hash table */	//进程的线程组调度优先级哈希表
    volatile UINT32      threadNumber; /**< Number of threads alive under this process */	//此进程下的活动线程数
    UINT32               threadCount;  /**< Total number of threads created under this process */	//在此进程下创建的线程总数
    LOS_DL_LIST          waitList;     /**< The process holds the waitLits to support wait/waitpid *///进程持有等待链表以支持wait/waitpid
#if (LOSCFG_KERNEL_SMP == YES)
    UINT32               timerCpu;     /**< CPU core number of this task is delayed or pended */
#endif
    UINTPTR              sigHandler;   /**< signal handler */
    sigset_t             sigShare;     /**< signal share bit */
#if (LOSCFG_KERNEL_LITEIPC == YES)
    ProcIpcInfo         ipcInfo;       /**< memory pool for lite ipc */ 
#endif
    LosVmSpace          *vmSpace;       /**< VMM space for processes */ //进程空间
#ifdef LOSCFG_FS_VFS
    struct files_struct *files;        /**< Files held by the process */ //进程所持有的所有文件，注者称之为 文件管理器
#endif
    timer_t             timerID;       /**< iTimer */

#ifdef LOSCFG_SECURITY_CAPABILITY
    User                *user;
    UINT32              capability;
#endif
#ifdef LOSCFG_SECURITY_VID
    TimerIdMap          timerIdMap;
#endif
#ifdef LOSCFG_DRIVERS_TZDRIVER
    struct file         *execFile;     /**< Exec bin of the process */
#endif
    mode_t umask;
} LosProcessCB;

#define CLONE_VM       0x00000100
#define CLONE_FS       0x00000200
#define CLONE_FILES    0x00000400
#define CLONE_SIGHAND  0x00000800
#define CLONE_PTRACE   0x00002000
#define CLONE_VFORK    0x00004000
#define CLONE_PARENT   0x00008000
#define CLONE_THREAD   0x00010000

#define OS_PCB_FROM_PID(processID) (((LosProcessCB *)g_processCBArray) + (processID))//通过数组找到LosProcessCB
#define OS_PCB_FROM_SIBLIST(ptr)   LOS_DL_LIST_ENTRY((ptr), LosProcessCB, siblingList)//通过siblingList节点找到 LosProcessCB
#define OS_PCB_FROM_PENDLIST(ptr)  LOS_DL_LIST_ENTRY((ptr), LosProcessCB, pendList) //通过pendlist节点找到 LosProcessCB

/**
 * @ingroup los_process
 * Flag that indicates the process or process control block status.
 *
 * The process is created but does not participate in scheduling.
 */
#define OS_PROCESS_STATUS_INIT           0x0010U	//进程初始状态

/**
 * @ingroup los_process
 * Flag that indicates the process or process control block status.
 *
 * The process is ready.
 */
#define OS_PROCESS_STATUS_READY          0x0020U	//进程就绪状态

/**
 * @ingroup los_process
 * Flag that indicates the process or process control block status.
 *
 * The process is running.
 */
#define OS_PROCESS_STATUS_RUNNING        0x0040U	//进程运行状态

/**
 * @ingroup los_process
 * Flag that indicates the process or process control block status.
 *
 * The process is pend
 */
#define OS_PROCESS_STATUS_PEND           0x0080U	//进程阻塞状态

/**
 * @ingroup los_process
 * Flag that indicates the process or process control block status.
 *
 * The process is run out but the resources occupied by the process are not recovered.
 */
#define OS_PROCESS_STATUS_ZOMBIES        0x100U		//进程僵死状态

/**
 * @ingroup los_process
 * Flag that indicates the process or process control block status.
 *
 * The number of task currently running under the process, it only works with multiple cores.
 */
#define OS_PROCESS_RUNTASK_COUNT_MASK    0x000FU	//进程处于运行状态的数量掩码
//进程当前运行的任务数，它只适用于多个内核,这里注意 一个进程的多个任务是可以同时给多个内核运行的.
/**
 * @ingroup los_process
 * Flag that indicates the process or process control block status.
 *
 * The process status mask.
 */
#define OS_PROCESS_STATUS_MASK           0xFFF0U	//进程状态掩码

/**
 * @ingroup los_process
 * Flag that indicates the process or process control block status.
 *
 * The process status equal this is process control block unused,
 * coexisting with OS_PROCESS_STATUS_ZOMBIES means that the control block is not recovered.
 */
#define OS_PROCESS_FLAG_UNUSED            0x0200U	//进程未使用标签,一般用于进程的初始状态 freelist里面都是这种标签

/**
 * @ingroup los_process
 * Flag that indicates the process or process control block status.
 *
 * The process has been call exit, it only works with multiple cores.
 */
#define OS_PROCESS_FLAG_EXIT              0x0400U	//进程退出标签,退出的进程进入回收链表等待回收资源

/**
 * @ingroup los_process
 * Flag that indicates the process or process control block status.
 *
 * The process is the leader of the process group.
 */
#define OS_PROCESS_FLAG_GROUP_LEADER      0x0800U	//进程当了进程组领导标签

/**
 * @ingroup los_process
 * Flag that indicates the process or process control block status.
 *
 * The process has performed the exec operation.
 */
#define OS_PROCESS_FLAG_ALREADY_EXEC      0x1000U //进程已经开始工作的标签

/**
 * @ingroup los_process
 * Flag that indicates the process or process control block status.
 *
 * The process is dying or already dying.
 */
#define OS_PROCESS_STATUS_INACTIVE       (OS_PROCESS_FLAG_EXIT | OS_PROCESS_STATUS_ZOMBIES) 
//进程不活跃状态定义: 身上贴有退出便签且状态为僵死的进程
/**
 * @ingroup los_process
 * Used to check if the process control block is unused.
 */
STATIC INLINE BOOL OsProcessIsUnused(const LosProcessCB *processCB)//查下进程是否没在使用?
{
    return ((processCB->processStatus & OS_PROCESS_FLAG_UNUSED) != 0);
}

/**
 * @ingroup los_process
 * Used to check if the process is inactive.
 */
STATIC INLINE BOOL OsProcessIsInactive(const LosProcessCB *processCB)//查下进程是否不活跃?
{
    return ((processCB->processStatus & (OS_PROCESS_FLAG_UNUSED | OS_PROCESS_STATUS_INACTIVE)) != 0);
}//进程不活跃函数定义:身上贴有不使用且不活跃标签的进程

/**
 * @ingroup los_process
 * Used to check if the process is dead.
 */
STATIC INLINE BOOL OsProcessIsDead(const LosProcessCB *processCB)//查下进程是否死啦死啦滴?
{
    return ((processCB->processStatus & (OS_PROCESS_FLAG_UNUSED | OS_PROCESS_STATUS_ZOMBIES)) != 0);
}//进程死啦死啦的定义: 身上贴有不使用且状态为僵死的进程

/**
 * @ingroup los_process
 * Hold the time slice process
 */
#define OS_PROCESS_SCHED_RR_INTERVAL     LOSCFG_BASE_CORE_TIMESLICE_TIMEOUT //抢占式调度方式采用的时间片数量

/**
 * @ingroup los_process
 * The highest priority of a kernel mode process.
 */
#define OS_PROCESS_PRIORITY_HIGHEST      0	//进程最高优先级

/**
 * @ingroup los_process
 * The lowest priority of a kernel mode process
 */
#define OS_PROCESS_PRIORITY_LOWEST       31 //进程最低优先级

/**
 * @ingroup los_process
 * The highest priority of a user mode process.
 */
#define OS_USER_PROCESS_PRIORITY_HIGHEST 10 //内核模式和用户模式的优先级分割线 10-31 用户级, 0-9内核级

/**
 * @ingroup los_process
 * The lowest priority of a user mode process
 */
#define OS_USER_PROCESS_PRIORITY_LOWEST  OS_PROCESS_PRIORITY_LOWEST //用户进程的最低优先级

/**
 * @ingroup los_process
 * User state root process default priority
 */
#define OS_PROCESS_USERINIT_PRIORITY     28	//用户进程默认的优先级,28级好低啊

#define OS_GET_PROCESS_STATUS(status) ((UINT16)((UINT16)(status) & OS_PROCESS_STATUS_MASK))
#define OS_PROCESS_GET_RUNTASK_COUNT(status) ((UINT16)(((UINT16)(status)) & OS_PROCESS_RUNTASK_COUNT_MASK))
#define OS_PROCESS_RUNTASK_COUNT_ADD(status) ((UINT16)(((UINT16)(status)) & OS_PROCESS_STATUS_MASK) | \
        ((OS_PROCESS_GET_RUNTASK_COUNT(status) + 1) & OS_PROCESS_RUNTASK_COUNT_MASK))
#define OS_PROCESS_RUNTASK_COUNT_DEC(status) ((UINT16)(((UINT16)(status)) & OS_PROCESS_STATUS_MASK) | \
        ((OS_PROCESS_GET_RUNTASK_COUNT(status) - 1) & OS_PROCESS_RUNTASK_COUNT_MASK))

#define OS_TASK_DEFAULT_STACK_SIZE      0x2000	//task默认栈大小 8K
#define OS_USER_TASK_SYSCALL_SATCK_SIZE 0x3000	//用户通过系统调用的栈大小 12K ,这时是运行在内核模式下
#define OS_USER_TASK_STACK_SIZE         0x100000	//用户任务运行在用户空间的栈大小 1M 

#define OS_KERNEL_MODE 0x0U	//内核模式
#define OS_USER_MODE   0x1U	//用户模式
STATIC INLINE BOOL OsProcessIsUserMode(const LosProcessCB *processCB)//是用户进程吗?
{
    return (processCB->processMode == OS_USER_MODE);
}

#define LOS_SCHED_NORMAL  0U	//正常调度
#define LOS_SCHED_FIFO    1U 	//先进先出，按顺序
#define LOS_SCHED_RR      2U 	//抢占式调度

#define LOS_PRIO_PROCESS  0U 	//进程标识
#define LOS_PRIO_PGRP     1U	//进程组标识	
#define LOS_PRIO_USER     2U	//用户标识

#define OS_KERNEL_PROCESS_GROUP         2U	//内核进程组
#define OS_USER_PRIVILEGE_PROCESS_GROUP 1U 	//用户特权进程组

/*
 * Process exit code
 * 31    15           8           7        0
 * |     | exit code  | core dump | signal |
 */
#define OS_PRO_EXIT_OK 0
//以下函数都是进程的退出方式
STATIC INLINE VOID OsProcessExitCodeCoreDumpSet(LosProcessCB *processCB)
{
    processCB->exitCode |= 0x80U;// 0b10000000 对应退出码为看
}

STATIC INLINE VOID OsProcessExitCodeSignalSet(LosProcessCB *processCB, UINT32 signal)
{
    processCB->exitCode |= signal & 0x7FU;//0b01111111
}

STATIC INLINE VOID OsProcessExitCodeSignalClear(LosProcessCB *processCB)
{
    processCB->exitCode &= (~0x7FU);//低7位全部清0
}

STATIC INLINE BOOL OsProcessExitCodeSignalIsSet(LosProcessCB *processCB)
{
    return (processCB->exitCode) & 0x7FU;//低7位全部置1
}

STATIC INLINE VOID OsProcessExitCodeSet(LosProcessCB *processCB, UINT32 code)//由外界提供退出码
{
    processCB->exitCode |= ((code & 0x000000FFU) << 8U) & 0x0000FF00U; /* 8: Move 8 bits to the left, exitCode */
}

extern LosProcessCB *g_processCBArray;//进程池 OsProcessInit
extern LosProcessCB *g_runProcess[LOSCFG_KERNEL_CORE_NUM];//运行进程，并行(Parallel)进程
extern UINT32 g_processMaxNum;//进程最大数量

#define OS_PID_CHECK_INVALID(pid) (((UINT32)(pid)) >= g_processMaxNum)

STATIC INLINE BOOL OsProcessIDUserCheckInvalid(UINT32 pid)
{
    return ((pid >= g_processMaxNum) || (pid == 0));
}
//获取当前进程PCB
STATIC INLINE LosProcessCB *OsCurrProcessGet(VOID)
{
    UINT32 intSave;
    LosProcessCB *runProcess = NULL;

    intSave = LOS_IntLock();//不响应硬件中断
    runProcess = g_runProcess[ArchCurrCpuid()];//定义当前进程:当前运行进程数组里找索引位当前运行的CPU核ID的
    LOS_IntRestore(intSave);//恢复硬件中断
    return runProcess;
}
//设置当前进程,加入g_runProcess中
STATIC INLINE VOID OsCurrProcessSet(const LosProcessCB *process)
{
    g_runProcess[ArchCurrCpuid()] = (LosProcessCB *)process;
}

STATIC INLINE UINT32 OsCpuProcessIDGetUnsafe(UINT16 cpuID)
{
    LosProcessCB *runProcess = g_runProcess[cpuID];
    return runProcess->processID;
}

STATIC INLINE UINT32 OsCpuProcessIDGet(UINT16 cpuID)
{
    UINT32 pid;
    UINT32 intSave;

    SCHEDULER_LOCK(intSave);
    pid = OsCpuProcessIDGetUnsafe(cpuID);
    SCHEDULER_UNLOCK(intSave);

    return pid;
}

#ifdef LOSCFG_SECURITY_CAPABILITY
STATIC INLINE User *OsCurrUserGet(VOID)
{
    User *user = NULL;
    UINT32 intSave;

    intSave = LOS_IntLock();
    user = OsCurrProcessGet()->user;
    LOS_IntRestore(intSave);
    return user;
}
#endif

/*
 * return immediately if no child has exited.
 */
#define LOS_WAIT_WNOHANG   (1 << 0U)

/*
 * return if a child has stopped (but not traced via ptrace(2)).
 * Status for traced children which have stopped is provided even
 * if this option is not specified.
 */
#define LOS_WAIT_WUNTRACED (1 << 1U)

/*
 * return if a stopped child has been resumed by delivery of SIGCONT.
 * (For Linux-only options, see below.)
 */
#define LOS_WAIT_WCONTINUED (1 << 3U)

/*
 * Indicates that you are already in a wait state
 */
#define OS_PROCESS_WAIT     (1 << 15U)

/*
 * Wait for any child process to finish
 */
#define OS_PROCESS_WAIT_ANY (1 << 0U)

/*
 * Wait for the child process specified by the pid to finish
 */
#define OS_PROCESS_WAIT_PRO (1 << 1U)

/*
 * Waits for any child process in the specified process group to finish.
 */
#define OS_PROCESS_WAIT_GID (1 << 2U)

#define OS_PROCESS_INFO_ALL 1
#define OS_PROCESS_DEFAULT_UMASK 0022

extern UINTPTR __user_init_entry;
extern UINTPTR __user_init_bss;
extern UINTPTR __user_init_end;
extern UINTPTR __user_init_load_addr;
extern UINT32 OsKernelInitProcess(VOID);
extern VOID OsProcessCBRecyleToFree(VOID);
extern VOID OsProcessResourcesToFree(LosProcessCB *processCB);
extern VOID OsProcessExit(LosTaskCB *runTask, INT32 status);
extern UINT32 OsUserInitProcess(VOID);
extern VOID OsTaskSchedQueueDequeue(LosTaskCB *taskCB, UINT16 status);
extern VOID OsTaskSchedQueueEnqueue(LosTaskCB *taskCB, UINT16 status);
extern INT32 OsClone(UINT32 flags, UINTPTR sp, UINT32 size);
extern VOID OsWaitSignalToWakeProcess(LosProcessCB *processCB);
extern UINT32 OsExecRecycleAndInit(LosProcessCB *processCB, const CHAR *name,
                                   LosVmSpace *oldAspace, UINTPTR oldFiles);
extern UINT32 OsExecStart(const TSK_ENTRY_FUNC entry, UINTPTR sp, UINTPTR mapBase, UINT32 mapSize);
extern INT32 OsSetProcessScheduler(INT32 which, INT32 pid, UINT16 prio, UINT16 policy, BOOL policyFlag);
extern INT32 OsGetProcessPriority(INT32 which, INT32 pid);
extern VOID *OsUserStackAlloc(UINT32 processID, UINT32 *size);
extern UINT32 OsGetUserInitProcessID(VOID);
extern UINT32 OsGetIdleProcessID(VOID);
extern INT32 OsSetProcessGroupID(UINT32 pid, UINT32 gid);
extern INT32 OsSetCurrProcessGroupID(UINT32 gid);
extern UINT32 OsGetKernelInitProcessID(VOID);
extern VOID OsSetSigHandler(UINTPTR addr);
extern UINTPTR OsGetSigHandler(VOID);
#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif
