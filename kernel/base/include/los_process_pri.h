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

#ifndef _LOS_PROCESS_PRI_H
#define _LOS_PROCESS_PRI_H

#include "los_task_pri.h"
#include "los_sem_pri.h"
#include "los_process.h"
#include "los_vm_map.h"
#ifdef LOSCFG_KERNEL_LITEIPC
#include "hm_liteipc.h"
#endif
#ifdef LOSCFG_SECURITY_CAPABILITY
#include "capability_type.h"
#endif
#ifdef LOSCFG_SECURITY_VID
#include "vid_type.h"
#endif
#include "sys/resource.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#define OS_PCB_NAME_LEN OS_TCB_NAME_LEN

#ifdef LOSCFG_SECURITY_CAPABILITY
#define OS_GROUPS_NUMBER_MAX 256

/*! 用户描述体*/
typedef struct {
    UINT32  userID;	///<用户ID [0,60000],0为root用户
    UINT32  effUserID;
    UINT32  gid;	///<用户组ID [0,60000],0为root用户组
    UINT32  effGid;
    UINT32  groupNumber;///< 用户组数量 
    UINT32  groups[1];	//所属用户组列表,一个用户可属多个用户组
} User;
#endif
/*! 进程组结构体*/
typedef struct {
    UINT32      groupID;         /**< Process group ID is the PID of the process that created the group | 进程组ID是创建进程组的那个进程的ID*/
    LOS_DL_LIST processList;     /**< List of processes under this process group | 属于该进程组的进程链表*/
    LOS_DL_LIST exitProcessList; /**< List of closed processes (zombie processes) under this group | 进程组的僵死进程链表*/
    LOS_DL_LIST groupList;       /**< Process group list | 进程组链表,上面挂的都是进程组*/
} ProcessGroup;

/**
 *  进程控制块.
 */
typedef struct ProcessCB {
    CHAR                 processName[OS_PCB_NAME_LEN]; /**< Process name | 进程名称 */
    UINT32               processID;                    /**< Process ID = leader thread ID | 进程ID,由进程池分配,范围[0,64] */
    UINT16               processStatus;                /**< [15:4] Process Status; [3:0] The number of threads currently
                                                            running in the process | 这里设计很巧妙.用一个变量表示了两层逻辑 数量和状态,点赞! @note_good 从这里也可以看出一个进程可以有多个正在运行的任务*/
    UINT16               consoleID;                    /**< The console id of task belongs | 任务的控制台id归属 */
    UINT16               processMode;                  /**< Kernel Mode:0; User Mode:1; | 模式指定为内核还是用户进程 */
    UINT32               parentProcessID;              /**< Parent process ID | 父进程ID*/
    UINT32               exitCode;                     /**< Process exit status | 进程退出状态码*/
    LOS_DL_LIST          pendList;                     /**< Block list to which the process belongs | 进程所在的阻塞列表,进程因阻塞挂入相应的链表.*/
    LOS_DL_LIST          childrenList;                 /**< Children process list | 孩子进程都挂到这里,形成双循环链表*/
    LOS_DL_LIST          exitChildList;                /**< Exit children process list | 要退出的孩子进程链表，白发人要送黑发人.*/
    LOS_DL_LIST          siblingList;                  /**< Linkage in parent's children list | 兄弟进程链表, 56个民族是一家,来自同一个父进程.*/
    ProcessGroup         *group;                       /**< Process group to which a process belongs | 所属进程组*/
    LOS_DL_LIST          subordinateGroupList;         /**< Linkage in group list | 进程组员链表*/
    UINT32               threadGroupID;                /**< Which thread group , is the main thread ID of the process */
    LOS_DL_LIST          threadSiblingList;            /**< List of threads under this process | 进程的线程(任务)列表 */
    volatile UINT32      threadNumber; /**< Number of threads alive under this process | 此进程下的活动线程数*/
    UINT32               threadCount;  /**< Total number of threads created under this process | 在此进程下创建的线程总数*/	//
    LOS_DL_LIST          waitList;     /**< The process holds the waitLits to support wait/waitpid | 父进程通过进程等待的方式，回收子进程资源，获取子进程退出信息*/
#ifdef LOSCFG_KERNEL_SMP
    UINT32               timerCpu;     /**< CPU core number of this task is delayed or pended | 统计各线程被延期或阻塞的时间*/
#endif
    UINTPTR              sigHandler;   /**< Signal handler | 信号处理函数,处理如 SIGSYS 等信号*/
    sigset_t             sigShare;     /**< Signal share bit | 信号共享位 sigset_t是个64位的变量,对应64种信号*/
#ifdef LOSCFG_KERNEL_LITEIPC
    ProcIpcInfo         *ipcInfo;       /**< Memory pool for lite ipc | 用于进程间通讯的虚拟设备文件系统,设备装载点为 /dev/lite_ipc*/
#endif
#ifdef LOSCFG_KERNEL_VM
    LosVmSpace          *vmSpace;       /**< VMM space for processes | 虚拟空间,描述进程虚拟内存的数据结构，linux称为内存描述符 */
#endif
#ifdef LOSCFG_FS_VFS
    struct files_struct *files;        /**< Files held by the process | 进程所持有的所有文件，注者称之为进程的文件管理器*/
#endif	//每个进程都有属于自己的文件管理器,记录对文件的操作. 注意:一个文件可以被多个进程操作
    timer_t             timerID;       /**< iTimer */

#ifdef LOSCFG_SECURITY_CAPABILITY	//安全能力
    User                *user;		///< 进程的拥有者
    UINT32              capability;	///< 安全能力范围 对应 CAP_SETGID
#endif
#ifdef LOSCFG_SECURITY_VID	//虚拟ID映射功能
    TimerIdMap          timerIdMap;
#endif
#ifdef LOSCFG_DRIVERS_TZDRIVER
    struct Vnode        *execVnode;     /**< Exec bin of the process | 进程的可执行文件 */
#endif
    mode_t               umask; ///< umask(user file-creatiopn mode mask)为用户文件创建掩码，是创建文件或文件夹时默认权限的基础。
#ifdef LOSCFG_KERNEL_CPUP
    OsCpupBase           *processCpup; /**< Process cpu usage | 进程占用CPU情况统计*/
#endif
    struct rlimit        *resourceLimit; ///< 每个进程在运行时系统不会无限制的允许单个进程不断的消耗资源，因此都会设置资源限制。
} LosProcessCB;

#define CLONE_VM       0x00000100	///< 子进程与父进程运行于相同的内存空间
#define CLONE_FS       0x00000200	///< 子进程与父进程共享相同的文件系统，包括root、当前目录、umask
#define CLONE_FILES    0x00000400	///< 子进程与父进程共享相同的文件描述符（file descriptor）表
#define CLONE_SIGHAND  0x00000800	///< 子进程与父进程共享相同的信号处理（signal handler）表
#define CLONE_PTRACE   0x00002000	///< 若父进程被trace，子进程也被trace
#define CLONE_VFORK    0x00004000	///< 父进程被挂起，直至子进程释放虚拟内存资源
#define CLONE_PARENT   0x00008000	///< 创建的子进程的父进程是调用者的父进程，新进程与创建它的进程成了“兄弟”而不是“父子”
#define CLONE_THREAD   0x00010000	///< Linux 2.4中增加以支持POSIX线程标准，子进程与父进程共享相同的线程群
//CLONE_NEWNS 在新的namespace启动子进程，namespace描述了进程的文件hierarchy
//CLONE_PID 子进程在创建时PID与父进程一致
#define OS_PCB_FROM_PID(processID) (((LosProcessCB *)g_processCBArray) + (processID))///< 通过数组找到LosProcessCB
#define OS_PCB_FROM_SIBLIST(ptr)   LOS_DL_LIST_ENTRY((ptr), LosProcessCB, siblingList)///< 通过siblingList节点找到 LosProcessCB
#define OS_PCB_FROM_PENDLIST(ptr)  LOS_DL_LIST_ENTRY((ptr), LosProcessCB, pendList) ///< 通过pendlist节点找到 LosProcessCB

/**
 * @ingroup los_process
 * Flag that indicates the process or process control block status.
 *
 * The process is created but does not participate in scheduling.
 */
#define OS_PROCESS_STATUS_INIT           OS_TASK_STATUS_INIT

/**
 * @ingroup los_process
 * Flag that indicates the process or process control block status.
 *
 * The process is ready.
 */
#define OS_PROCESS_STATUS_READY          OS_TASK_STATUS_READY

/**
 * @ingroup los_process
 * Flag that indicates the process or process control block status.
 *
 * The process is running.
 */
#define OS_PROCESS_STATUS_RUNNING        OS_TASK_STATUS_RUNNING

/**
 * @ingroup los_process
 * Flag that indicates the process or process control block status.
 *
 * The process is pending
 */
#define OS_PROCESS_STATUS_PENDING       (OS_TASK_STATUS_PENDING | OS_TASK_STATUS_DELAY | OS_TASK_STATUS_SUSPENDED)

/**
 * @ingroup los_process
 * Flag that indicates the process or process control block status.
 *
 * The process is run out but the resources occupied by the process are not recovered.
 */
#define OS_PROCESS_STATUS_ZOMBIES        0x0100U		///< 进程状态: 僵死

/**
 * @ingroup los_process
 * Flag that indicates the process or process control block status.
 *
 * The process status equal this is process control block unused,
 * coexisting with OS_PROCESS_STATUS_ZOMBIES means that the control block is not recovered.
 */
#define OS_PROCESS_FLAG_UNUSED            0x0200U	///< 进程未使用标签,一般用于进程的初始状态 freelist里面都是这种标签

/**
 * @ingroup los_process
 * Flag that indicates the process or process control block status.
 *
 * The process has been call exit, it only works with multiple cores.
 */
#define OS_PROCESS_FLAG_EXIT              0x0400U	///< 进程退出标签,退出的进程进入回收链表等待回收资源

/**
 * @ingroup los_process
 * Flag that indicates the process or process control block status.
 *
 * The process is the leader of the process group.
 */
#define OS_PROCESS_FLAG_GROUP_LEADER      0x0800U	///< 进程当了进程组领导标签

/**
 * @ingroup los_process
 * Flag that indicates the process or process control block status.
 *
 * The process has performed the exec operation.
 */
#define OS_PROCESS_FLAG_ALREADY_EXEC      0x1000U ///< 进程已执行exec操作 load elf时使用

/**
 * @ingroup los_process
 * Flag that indicates the process or process control block status.
 *
 * The process is dying or already dying.
 */ /// 进程不活跃状态定义: 身上贴有退出便签且状态为僵死的进程
#define OS_PROCESS_STATUS_INACTIVE       (OS_PROCESS_FLAG_EXIT | OS_PROCESS_STATUS_ZOMBIES) 

/**
 * @ingroup los_process
 * Used to check if the process control block is unused.
 */
STATIC INLINE BOOL OsProcessIsUnused(const LosProcessCB *processCB)//查下进程是否还在使用?
{
    return ((processCB->processStatus & OS_PROCESS_FLAG_UNUSED) != 0);
}

/**
 * @ingroup los_process
 * Used to check if the process is inactive.
 */ /// 进程不活跃函数定义:身上贴有不使用且不活跃标签的进程
STATIC INLINE BOOL OsProcessIsInactive(const LosProcessCB *processCB)//查下进程是否不活跃?
{
    return ((processCB->processStatus & (OS_PROCESS_FLAG_UNUSED | OS_PROCESS_STATUS_INACTIVE)) != 0);
}

/**
 * @ingroup los_process
 * Used to check if the process is dead.
 */ /// 进程死啦死啦的定义: 身上贴有不使用且状态为僵死的进程
STATIC INLINE BOOL OsProcessIsDead(const LosProcessCB *processCB)//查下进程是否死啦死啦滴?
{
    return ((processCB->processStatus & (OS_PROCESS_FLAG_UNUSED | OS_PROCESS_STATUS_ZOMBIES)) != 0);
}

STATIC INLINE BOOL OsProcessIsInit(const LosProcessCB *processCB)
{
    return (processCB->processStatus & OS_PROCESS_STATUS_INIT);
}

/**
 * @ingroup los_process
 * The highest priority of a kernel mode process.
 */
#define OS_PROCESS_PRIORITY_HIGHEST      0	///< 进程最高优先级

/**
 * @ingroup los_process
 * The lowest priority of a kernel mode process
 */
#define OS_PROCESS_PRIORITY_LOWEST       31 ///< 进程最低优先级

/**
 * @ingroup los_process
 * The highest priority of a user mode process.
 */
#define OS_USER_PROCESS_PRIORITY_HIGHEST 10 ///< 内核模式和用户模式的优先级分割线 10-31 用户级, 0-9内核级

/**
 * @ingroup los_process
 * The lowest priority of a user mode process
 */
#define OS_USER_PROCESS_PRIORITY_LOWEST  OS_PROCESS_PRIORITY_LOWEST ///< 用户进程的最低优先级

/**
 * @ingroup los_process
 * User state root process default priority
 */
#define OS_PROCESS_USERINIT_PRIORITY     28	///< 用户进程默认的优先级,28级好低啊

#define OS_TASK_DEFAULT_STACK_SIZE      0x2000	///< task默认栈大小 8K
#define OS_USER_TASK_SYSCALL_STACK_SIZE 0x3000	///< 用户通过系统调用的栈大小 12K ,这时是运行在内核模式下
#define OS_USER_TASK_STACK_SIZE         0x100000	///< 用户任务运行在用户空间的栈大小 1M 

#define OS_KERNEL_MODE 0x0U	///< 内核态
#define OS_USER_MODE   0x1U	///< 用户态
/*! 用户态进程*/
STATIC INLINE BOOL OsProcessIsUserMode(const LosProcessCB *processCB)
{
    return (processCB->processMode == OS_USER_MODE);
}

#define LOS_SCHED_NORMAL  0U	///< 正常调度
#define LOS_SCHED_FIFO    1U 	///< 先进先出，按顺序
#define LOS_SCHED_RR      2U 	///< 抢占式调度,鸿蒙默认调度方式
#define LOS_SCHED_IDLE    3U	///< 空闲不调度

#define LOS_PRIO_PROCESS  0U 	///< 进程标识
#define LOS_PRIO_PGRP     1U	///< 进程组标识	
#define LOS_PRIO_USER     2U	///< 用户标识

#define OS_USER_PRIVILEGE_PROCESS_GROUP 1U	///< 用户态进程组ID
#define OS_KERNEL_PROCESS_GROUP         2U	///< 内核态进程组ID

/*
 * Process exit code
 * 31    15           8           7        0
 * |     | exit code  | core dump | signal |
 */
#define OS_PRO_EXIT_OK 0 ///< 进程正常退出
/// 置进程退出码第七位为1
STATIC INLINE VOID OsProcessExitCodeCoreDumpSet(LosProcessCB *processCB)
{
    processCB->exitCode |= 0x80U;   //  0b10000000 
}
/// 设置进程退出信号(0 ~ 7)
STATIC INLINE VOID OsProcessExitCodeSignalSet(LosProcessCB *processCB, UINT32 signal)
{
    processCB->exitCode |= signal & 0x7FU;// 0b01111111
}
/// 清除进程退出信号(0 ~ 7)
STATIC INLINE VOID OsProcessExitCodeSignalClear(LosProcessCB *processCB)
{
    processCB->exitCode &= (~0x7FU);// 低7位全部清0
}
/// 进程退出码是否被设置过,默认是 0 ,如果 & 0x7FU 还是 0 ,说明没有被设置过.
STATIC INLINE BOOL OsProcessExitCodeSignalIsSet(LosProcessCB *processCB)
{
    return (processCB->exitCode) & 0x7FU;
}
/// 设置进程退出号(8 ~ 15)
STATIC INLINE VOID OsProcessExitCodeSet(LosProcessCB *processCB, UINT32 code)
{
    processCB->exitCode |= ((code & 0x000000FFU) << 8U) & 0x0000FF00U; /* 8: Move 8 bits to the left, exitCode */
}

extern LosProcessCB *g_processCBArray;///< 进程池 OsProcessInit
extern UINT32 g_processMaxNum;///< 进程最大数量

#define OS_PID_CHECK_INVALID(pid) (((UINT32)(pid)) >= g_processMaxNum)
/*! 内联函数 进程ID是否有效 */
STATIC INLINE BOOL OsProcessIDUserCheckInvalid(UINT32 pid)
{
    return ((pid >= g_processMaxNum) || (pid == 0));
}
/*! 获取当前进程PCB */
STATIC INLINE LosProcessCB *OsCurrProcessGet(VOID)
{
    UINT32 intSave;

    intSave = LOS_IntLock();
    LosProcessCB *runProcess = OS_PCB_FROM_PID(OsCurrTaskGet()->processID);
    LOS_IntRestore(intSave);
    return runProcess;
}

#ifdef LOSCFG_SECURITY_CAPABILITY
/*! 获取当前进程的所属用户 */
STATIC INLINE User *OsCurrUserGet(VOID)
{
    User *user = NULL;
    UINT32 intSave;

    intSave = LOS_IntLock();
    user = OsCurrProcessGet()->user;
    LOS_IntRestore(intSave);
    return user;
}

STATIC INLINE UINT32 OsProcessUserIDGet(const LosTaskCB *taskCB)
{
    UINT32 intSave = LOS_IntLock();
    UINT32 uid = OS_INVALID;

    LosProcessCB *process = OS_PCB_FROM_PID(taskCB->processID);
    if (process->user != NULL) {
        uid = process->user->userID;
    }
    LOS_IntRestore(intSave);
    return uid;
}
#endif

STATIC INLINE UINT32 OsProcessThreadGroupIDGet(const LosTaskCB *taskCB)
{
    return OS_PCB_FROM_PID(taskCB->processID)->threadGroupID;
}

STATIC INLINE UINT32 OsProcessThreadNumberGet(const LosTaskCB *taskCB)
{
    return OS_PCB_FROM_PID(taskCB->processID)->threadNumber;
}

#ifdef LOSCFG_KERNEL_VM
STATIC INLINE LosVmSpace *OsProcessVmSpaceGet(const LosProcessCB *processCB)
{
    return processCB->vmSpace;
}
#endif

#ifdef LOSCFG_DRIVERS_TZDRIVER
STATIC INLINE struct Vnode *OsProcessExecVnodeGet(const LosProcessCB *processCB)
{
    return processCB->execVnode;
}
#endif
/*
 * return immediately if no child has exited.
 */
#define LOS_WAIT_WNOHANG   (1 << 0U) ///< 如果没有孩子进程退出，则立即返回,而不是阻塞在这个函数上等待；如果结束了，则返回该子进程的进程号。

/*
 * return if a child has stopped (but not traced via ptrace(2)).
 * Status for traced children which have stopped is provided even
 * if this option is not specified.
 */
#define LOS_WAIT_WUNTRACED (1 << 1U) ///< 如果子进程进入暂停情况则马上返回，不予以理会结束状态。untraced
#define LOS_WAIT_WSTOPPED (1 << 1U)

/*
 * Wait for exited processes
 */
#define LOS_WAIT_WEXITED (1 << 2U)

/*
 * return if a stopped child has been resumed by delivery of SIGCONT.
 * (For Linux-only options, see below.)
 */
#define LOS_WAIT_WCONTINUED (1 << 3U) ///< 可获取子进程恢复执行的状态，也就是可获取continued状态 continued

/*
 * Leave the child in a waitable state;
 * a later wait call can be used to again retrieve the child status information.
 */
#define LOS_WAIT_WNOWAIT (1 << 24U)

/*
 * Indicates that you are already in a wait state
 */
#define OS_PROCESS_WAIT     (1 << 15U) ///< 表示已经处于等待状态

/*
 * Wait for any child process to finish
 */
#define OS_PROCESS_WAIT_ANY OS_TASK_WAIT_ANYPROCESS ///< 等待任意子进程完成

/*
 * Wait for the child process specified by the pid to finish
 */
#define OS_PROCESS_WAIT_PRO OS_TASK_WAIT_PROCESS ///< 等待pid指定的子进程完成

/*
 * Waits for any child process in the specified process group to finish.
 */
#define OS_PROCESS_WAIT_GID OS_TASK_WAIT_GID ///< 等待指定进程组中的任意子进程完成

#define OS_PROCESS_INFO_ALL 1
#define OS_PROCESS_DEFAULT_UMASK 0022 ///< 系统默认的用户掩码(umask),大多数的Linux系统的默认掩码为022。
//用户掩码的作用是用户在创建文件时从文件的默认权限中去除掩码中的权限。所以文件创建之后的权限实际为:创建文件的权限为：0666-0022=0644。创建文件夹的权限为：0777-0022=0755
extern UINTPTR __user_init_entry;	///<  第一个用户态进程(init)的入口地址 查看 LITE_USER_SEC_ENTRY
extern UINTPTR __user_init_bss;		///<  查看 LITE_USER_SEC_BSS ,赋值由liteos.ld完成
extern UINTPTR __user_init_end;		///<  init 进程的用户空间初始化结束地址
extern UINTPTR __user_init_load_addr;///< init进程的加载地址
extern UINT32 OsSystemProcessCreate(VOID);
extern VOID OsProcessNaturalExit(LosProcessCB *processCB, UINT32 status);
extern VOID OsProcessCBRecycleToFree(VOID);
extern VOID OsProcessResourcesToFree(LosProcessCB *processCB);
extern UINT32 OsUserInitProcess(VOID);
extern INT32 OsClone(UINT32 flags, UINTPTR sp, UINT32 size);
extern VOID OsExecProcessVmSpaceRestore(LosVmSpace *oldSpace);
extern LosVmSpace *OsExecProcessVmSpaceReplace(LosVmSpace *newSpace, UINTPTR stackBase, INT32 randomDevFD);
extern UINT32 OsExecRecycleAndInit(LosProcessCB *processCB, const CHAR *name, LosVmSpace *oldAspace, UINTPTR oldFiles);
extern UINT32 OsExecStart(const TSK_ENTRY_FUNC entry, UINTPTR sp, UINTPTR mapBase, UINT32 mapSize);
extern UINT32 OsSetProcessName(LosProcessCB *processCB, const CHAR *name);
extern INT32 OsSetProcessScheduler(INT32 which, INT32 pid, UINT16 prio, UINT16 policy);
extern INT32 OsGetProcessPriority(INT32 which, INT32 pid);
extern UINT32 OsGetUserInitProcessID(VOID);
extern UINT32 OsGetIdleProcessID(VOID);
extern INT32 OsSetProcessGroupID(UINT32 pid, UINT32 gid);
extern INT32 OsSetCurrProcessGroupID(UINT32 gid);
extern UINT32 OsGetKernelInitProcessID(VOID);
extern VOID OsSetSigHandler(UINTPTR addr);
extern UINTPTR OsGetSigHandler(VOID);
extern VOID OsWaitWakeTask(LosTaskCB *taskCB, UINT32 wakePID);
extern INT32 OsSendSignalToProcessGroup(INT32 pid, siginfo_t *info, INT32 permission);
extern INT32 OsSendSignalToAllProcess(siginfo_t *info, INT32 permission);
extern UINT32 OsProcessAddNewTask(UINT32 pid, LosTaskCB *taskCB);
extern VOID OsDeleteTaskFromProcess(LosTaskCB *taskCB);
extern VOID OsProcessThreadGroupDestroy(VOID);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif
