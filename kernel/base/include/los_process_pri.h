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

#ifndef _LOS_PROCESS_PRI_H
#define _LOS_PROCESS_PRI_H

#include "los_task_pri.h"
#include "sched.h"
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
#ifdef LOSCFG_KERNEL_CONTAINER
#include "los_container_pri.h"
#endif
#ifdef LOSCFG_KERNEL_PLIMITS
#include "los_plimits.h"
#endif

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */
/**
 * @ingroup los_process
 * @brief 进程控制块名称长度
 *
 * 进程名称的最大长度，与任务控制块名称长度保持一致
 */
#define OS_PCB_NAME_LEN OS_TCB_NAME_LEN

#ifdef LOSCFG_SECURITY_CAPABILITY
/**
 * @ingroup los_process
 * @brief 最大用户组数量
 *
 * 系统支持的最大用户组数量，值为256
 */
#define OS_GROUPS_NUMBER_MAX 256

/**
 * @ingroup los_process
 * @brief 用户信息结构体
 *
 * 用于存储用户及用户组相关信息，仅在启用LOSCFG_SECURITY_CAPABILITY时生效
 */
typedef struct {
    UINT32  userID;       /**< 用户ID */
    UINT32  effUserID;    /**< 有效用户ID */
    UINT32  gid;          /**< 用户组ID */
    UINT32  effGid;       /**< 有效用户组ID */
    UINT32  groupNumber;  /**< 用户组数量 */
    UINT32  groups[1];    /**< 用户组列表，柔性数组，实际大小由groupNumber决定 */
} User;
#endif

/**
 * @ingroup los_process
 * @brief 进程组结构体
 *
 * 用于管理一组相关进程，包含进程组领导者、进程列表及退出进程列表等信息
 */
typedef struct ProcessGroup {
    UINTPTR      pgroupLeader;    /**< 进程组领导者PID，创建该组的进程ID */
    LOS_DL_LIST  processList;     /**< 该进程组下的进程链表 */
    LOS_DL_LIST  exitProcessList; /**< 该组下已退出的进程(僵尸进程)链表 */
    LOS_DL_LIST  groupList;       /**< 进程组链表，用于连接系统中的所有进程组 */
} ProcessGroup;

/**
 * @ingroup los_process
 * @brief 进程控制块结构体
 *
 * 存储进程的所有属性和状态信息，是操作系统管理进程的核心数据结构
 */
typedef struct ProcessCB {
    CHAR                 processName[OS_PCB_NAME_LEN]; /**< 进程名称，长度由OS_PCB_NAME_LEN定义 */
    UINT32               processID;                    /**< 进程ID，系统中唯一标识一个进程 */
    UINT16               processStatus;                /**< 进程状态，[15:4]位表示进程状态，[3:0]位表示进程中当前运行的线程数 */
    UINT16               consoleID;                    /**< 进程所属的控制台ID */
    UINT16               processMode;                  /**< 进程运行模式：0表示内核模式，1表示用户模式 */
    struct ProcessCB     *parentProcess;               /**< 父进程控制块指针 */
    UINT32               exitCode;                     /**< 进程退出状态码 */
    LOS_DL_LIST          pendList;                     /**< 进程所属的阻塞链表 */
    LOS_DL_LIST          childrenList;                 /**< 子进程链表，包含所有直接子进程 */
    LOS_DL_LIST          exitChildList;                /**< 已退出的子进程链表，用于wait/waitpid系统调用 */
    LOS_DL_LIST          siblingList;                  /**< 兄弟进程链表节点，用于连接到父进程的childrenList */
    ProcessGroup         *pgroup;                      /**< 进程所属的进程组指针 */
    LOS_DL_LIST          subordinateGroupList;         /**< 进程组链表节点，用于连接到进程组的groupList */
    LosTaskCB            *threadGroup;                 /**< 线程组领头线程控制块指针 */
    LOS_DL_LIST          threadSiblingList;            /**< 进程下的线程链表，包含进程所有线程 */
    volatile UINT32      threadNumber; /**< 进程中当前活跃的线程数量，volatile确保多线程安全访问 */
    UINT32               threadCount;  /**< 进程创建的线程总数（包括已退出的线程） */
    LOS_DL_LIST          waitList;     /**< 进程等待链表，用于支持wait/waitpid系统调用 */
#ifdef LOSCFG_KERNEL_SMP
    UINT32               timerCpu;     /**< 进程延迟或阻塞时所在的CPU核心号，仅SMP配置下有效 */
#endif
    UINTPTR              sigHandler;   /**< 信号处理函数指针 */
    sigset_t             sigShare;     /**< 信号共享位图，记录进程接收到的信号 */
#ifdef LOSCFG_KERNEL_LITEIPC
    ProcIpcInfo          *ipcInfo;      /**< 轻量级IPC内存池信息指针，仅LOSCFG_KERNEL_LITEIPC配置下有效 */
#endif
#ifdef LOSCFG_KERNEL_VM
    LosVmSpace           *vmSpace;     /**< 进程虚拟内存空间指针，仅启用虚拟内存时有效 */
#endif
#ifdef LOSCFG_FS_VFS
    struct files_struct  *files;       /**< 进程打开的文件列表，仅VFS配置下有效 */
#endif
    timer_t              timerID;      /**< 进程间隔定时器ID */

#ifdef LOSCFG_SECURITY_CAPABILITY
    User                *user;         /**< 用户信息指针，仅启用安全能力时有效 */
    UINT32              capability;    /**< 进程能力集，仅启用安全能力时有效 */
#endif
#ifdef LOSCFG_SECURITY_VID
    TimerIdMap           timerIdMap;   /**< 定时器ID映射表，仅启用VID安全机制时有效 */
#endif
#ifdef LOSCFG_DRIVERS_TZDRIVER
    struct Vnode        *execVnode;   /**< 进程可执行文件的Vnode指针，仅可信驱动配置下有效 */
#endif
    mode_t               umask;        /**< 文件创建掩码，控制新创建文件的默认权限 */
#ifdef LOSCFG_KERNEL_CPUP
    OsCpupBase           *processCpup; /**< 进程CPU使用率统计信息，仅启用CPU性能统计时有效 */
#endif
    struct rlimit        *resourceLimit; /**< 进程资源限制结构体指针 */
#ifdef LOSCFG_KERNEL_CONTAINER
    Container            *container;   /**< 进程所属容器指针，仅启用容器功能时有效 */
#ifdef LOSCFG_USER_CONTAINER
    struct Credentials   *credentials; /**< 进程凭证信息，仅用户态容器配置下有效 */
#endif
#endif
#ifdef LOSCFG_PROC_PROCESS_DIR
    struct ProcDirEntry *procDir;     /**< 进程在/proc文件系统中的目录项，仅启用proc文件系统时有效 */
#endif
#ifdef LOSCFG_KERNEL_PLIMITS
    ProcLimiterSet *plimits;          /**< 进程资源限制集合，仅启用进程限制功能时有效 */
    LOS_DL_LIST    plimitsList;       /**< 进程限制链表节点，用于连接到限制器的进程列表 */
    PLimitsData    limitStat;         /**< 进程限制统计数据 */
#endif
} LosProcessCB;

extern LosProcessCB *g_processCBArray;
extern UINT32 g_processMaxNum;
/**
 * @ingroup los_process
 * @brief 通过实际进程ID获取进程控制块指针
 *
 * @param processID 实际进程ID(Real PID)
 * @return LosProcessCB* 进程控制块指针
 * @note 直接通过数组索引计算PCB地址，适用于未启用容器化PID的场景
 */
#define OS_PCB_FROM_RPID(processID)     (((LosProcessCB *)g_processCBArray) + (processID))

#ifdef LOSCFG_PID_CONTAINER
/**
 * @ingroup los_process
 * @brief 通过虚拟进程ID获取进程控制块指针(容器化场景)
 *
 * @param processID 虚拟进程ID(Virtual PID)
 * @return LosProcessCB* 进程控制块指针
 * @note 容器化环境下通过OsGetPCBFromVpid函数进行VPID到RPID的转换
 */
#define OS_PCB_FROM_PID(processID)      OsGetPCBFromVpid(processID)
#else
/**
 * @ingroup los_process
 * @brief 通过进程ID获取进程控制块指针(非容器化场景)
 *
 * @param processID 进程ID(PID)
 * @return LosProcessCB* 进程控制块指针
 * @note 非容器化环境下直接调用OS_PCB_FROM_RPID宏
 */
#define OS_PCB_FROM_PID(processID)      OS_PCB_FROM_RPID(processID)
#endif

/**
 * @ingroup los_process
 * @brief 通过任务控制块获取所属进程控制块
 *
 * @param taskCB 任务控制块指针
 * @return LosProcessCB* 进程控制块指针
 * @note 从任务控制块的processCB成员直接获取
 */
#define OS_PCB_FROM_TCB(taskCB)         ((LosProcessCB *)((taskCB)->processCB))

/**
 * @ingroup los_process
 * @brief 通过任务ID获取所属进程控制块
 *
 * @param taskID 任务ID
 * @return LosProcessCB* 进程控制块指针
 * @note 先通过任务ID获取任务控制块，再获取其所属进程控制块
 */
#define OS_PCB_FROM_TID(taskID)         ((LosProcessCB *)(OS_TCB_FROM_TID(taskID)->processCB))

/**
 * @ingroup los_process
 * @brief 获取进程组的领导者进程控制块
 *
 * @param pgroup 进程组结构体指针
 * @return LosProcessCB* 进程组领导者的进程控制块指针
 * @note 进程组领导者是创建该进程组的进程
 */
#define OS_GET_PGROUP_LEADER(pgroup)    ((LosProcessCB *)((pgroup)->pgroupLeader))

/**
 * @ingroup los_process
 * @brief 通过兄弟进程链表节点获取进程控制块
 *
 * @param ptr 链表节点指针
 * @return LosProcessCB* 进程控制块指针
 * @note 使用LOS_DL_LIST_ENTRY宏从siblingList节点反向解析出PCB
 */
#define OS_PCB_FROM_SIBLIST(ptr)        LOS_DL_LIST_ENTRY((ptr), LosProcessCB, siblingList)

/**
 * @ingroup los_process
 * @brief 通过阻塞链表节点获取进程控制块
 *
 * @param ptr 链表节点指针
 * @return LosProcessCB* 进程控制块指针
 * @note 使用LOS_DL_LIST_ENTRY宏从pendList节点反向解析出PCB
 */
#define OS_PCB_FROM_PENDLIST(ptr)       LOS_DL_LIST_ENTRY((ptr), LosProcessCB, pendList)

/**
 * @ingroup los_process
 * @brief 进程状态标志：初始状态
 *
 * 进程已创建但尚未参与调度，值为0x0001U
 */
#define OS_PROCESS_STATUS_INIT           OS_TASK_STATUS_INIT

/**
 * @ingroup los_process
 * @brief 进程状态标志：就绪状态
 *
 * 进程已准备好运行，等待CPU调度，值为0x0002U
 */
#define OS_PROCESS_STATUS_READY          OS_TASK_STATUS_READY

/**
 * @ingroup los_process
 * @brief 进程状态标志：运行状态
 *
 * 进程正在CPU上运行，值为0x0004U
 */
#define OS_PROCESS_STATUS_RUNNING        OS_TASK_STATUS_RUNNING

/**
 * @ingroup los_process
 * @brief 进程状态标志：阻塞状态
 *
 * 进程因等待资源而阻塞，包含三种子状态：
 * - OS_TASK_STATUS_PENDING(0x0008U)：等待事件
 * - OS_TASK_STATUS_DELAY(0x0010U)：延时等待
 * - OS_TASK_STATUS_SUSPENDED(0x0020U)：被挂起
 * 组合值为0x0038U(56)
 */
#define OS_PROCESS_STATUS_PENDING       (OS_TASK_STATUS_PENDING | OS_TASK_STATUS_DELAY | OS_TASK_STATUS_SUSPENDED)

/**
 * @ingroup los_process
 * @brief 进程状态标志：僵尸状态
 *
 * 进程已退出但资源未回收，值为0x0100U(256)
 * @note 处于此状态的进程需要父进程调用wait/waitpid回收
 */
#define OS_PROCESS_STATUS_ZOMBIES        0x0100U

/**
 * @ingroup los_process
 * @brief 进程标志位：未使用
 *
 * 表示进程控制块未被使用，值为0x0200U(512)
 * @note 若与OS_PROCESS_STATUS_ZOMBIES同时置位，表示控制块未回收
 */
#define OS_PROCESS_FLAG_UNUSED            0x0200U

/**
 * @ingroup los_process
 * @brief 进程标志位：已退出
 *
 * 表示进程已调用exit退出，仅在多核系统中有效，值为0x0400U(1024)
 */
#define OS_PROCESS_FLAG_EXIT              0x0400U

/**
 * @ingroup los_process
 * @brief 进程标志位：进程组领导者
 *
 * 表示该进程是进程组的领导者，值为0x0800U(2048)
 */
#define OS_PROCESS_FLAG_GROUP_LEADER      0x0800U

/**
 * @ingroup los_process
 * @brief 进程标志位：已执行exec
 *
 * 表示进程已执行过exec系列系统调用，值为0x1000U(4096)
 */
#define OS_PROCESS_FLAG_ALREADY_EXEC      0x1000U

/**
 * @ingroup los_process
 * @brief 进程状态标志：非活动状态
 *
 * 表示进程正在退出或已退出，由以下状态组合而成：
 * - OS_PROCESS_FLAG_EXIT(0x0400U)：已调用exit
 * - OS_PROCESS_STATUS_ZOMBIES(0x0100U)：僵尸状态
 * 组合值为0x0500U(1280)
 */
#define OS_PROCESS_STATUS_INACTIVE       (OS_PROCESS_FLAG_EXIT | OS_PROCESS_STATUS_ZOMBIES)

/**
 * @ingroup los_process
 * @brief 检查进程控制块是否未使用
 *
 * @param processCB 进程控制块指针
 * @return BOOL 未使用返回TRUE(非0)，已使用返回FALSE(0)
 * @note 通过检查OS_PROCESS_FLAG_UNUSED标志位实现
 */
STATIC INLINE BOOL OsProcessIsUnused(const LosProcessCB *processCB)
{
    return ((processCB->processStatus & OS_PROCESS_FLAG_UNUSED) != 0);
}

/**
 * @ingroup los_process
 * @brief 检查进程是否处于非活动状态
 *
 * @param processCB 进程控制块指针
 * @return BOOL 非活动返回TRUE(非0)，活动返回FALSE(0)
 * @note 非活动状态包括：未使用(OS_PROCESS_FLAG_UNUSED)、已退出(OS_PROCESS_FLAG_EXIT)或僵尸状态(OS_PROCESS_STATUS_ZOMBIES)
 */
STATIC INLINE BOOL OsProcessIsInactive(const LosProcessCB *processCB)
{
    return ((processCB->processStatus & (OS_PROCESS_FLAG_UNUSED | OS_PROCESS_STATUS_INACTIVE)) != 0);
}

/**
 * @ingroup los_process
 * @brief 检查进程是否处于僵尸状态
 *
 * @param processCB 进程控制块指针
 * @return BOOL 僵尸状态返回TRUE(非0)，否则返回FALSE(0)
 * @note 通过检查OS_PROCESS_STATUS_ZOMBIES标志位实现
 */
STATIC INLINE BOOL OsProcessIsDead(const LosProcessCB *processCB)
{
    return ((processCB->processStatus & OS_PROCESS_STATUS_ZOMBIES) != 0);
}

/**
 * @ingroup los_process
 * @brief 检查进程是否处于初始状态
 *
 * @param processCB 进程控制块指针
 * @return BOOL 初始状态返回TRUE(非0)，否则返回FALSE(0)
 * @note 通过检查OS_PROCESS_STATUS_INIT标志位实现
 */
STATIC INLINE BOOL OsProcessIsInit(const LosProcessCB *processCB)
{
    return ((processCB->processStatus & OS_PROCESS_STATUS_INIT) != 0);
}

/**
 * @ingroup los_process
 * @brief 检查进程是否为进程组领导者
 *
 * @param processCB 进程控制块指针
 * @return BOOL 是领导者返回TRUE(非0)，否则返回FALSE(0)
 * @note 通过检查OS_PROCESS_FLAG_GROUP_LEADER标志位实现
 */
STATIC INLINE BOOL OsProcessIsPGroupLeader(const LosProcessCB *processCB)
{
    return ((processCB->processStatus & OS_PROCESS_FLAG_GROUP_LEADER) != 0);
}
/**
 * @ingroup los_process
 * @brief 内核态进程的最高优先级
 * @note 优先级数值越小表示优先级越高，0为系统可配置的最高优先级
 */
#define OS_PROCESS_PRIORITY_HIGHEST      0

/**
 * @ingroup los_process
 * @brief 内核态进程的最低优先级
 * @note 31表示内核态进程可使用的最低优先级，与用户态进程最低优先级共享此值
 */
#define OS_PROCESS_PRIORITY_LOWEST       31

/**
 * @ingroup los_process
 * @brief 用户态进程的最高优先级
 * @note 10为用户态进程的最高优先级，高于此值(数值更小)的优先级保留给内核态进程使用
 */
#define OS_USER_PROCESS_PRIORITY_HIGHEST 10

/**
 * @ingroup los_process
 * @brief 用户态进程的最低优先级
 * @note 与内核态进程最低优先级相同，均为31
 */
#define OS_USER_PROCESS_PRIORITY_LOWEST  OS_PROCESS_PRIORITY_LOWEST

/**
 * @ingroup los_process
 * @brief 用户态根进程的默认优先级
 * @note 28为用户空间初始化进程的默认优先级，属于用户态进程的中低优先级范围
 */
#define OS_PROCESS_USERINIT_PRIORITY     28

/**
 * @ingroup los_process
 * @brief 内核空闲进程ID
 * @note 0号进程为系统保留的空闲进程ID，不允许用户态进程使用
 */
#define OS_KERNEL_IDLE_PROCESS_ID       0U

/**
 * @ingroup los_process
 * @brief 用户态根进程ID
 * @note 1号进程为用户空间的根进程，所有用户态进程均由此进程派生
 */
#define OS_USER_ROOT_PROCESS_ID         1U

/**
 * @ingroup los_process
 * @brief 内核态根进程ID
 * @note 2号进程为内核空间的根进程，负责系统核心服务的初始化
 */
#define OS_KERNEL_ROOT_PROCESS_ID       2U

/**
 * @ingroup los_process
 * @brief 默认任务栈大小
 * @note 十六进制0x2000等于十进制8192字节，为内核任务的默认栈空间大小
 */
#define OS_TASK_DEFAULT_STACK_SIZE      0x2000

/**
 * @ingroup los_process
 * @brief 用户态任务系统调用栈大小
 * @note 十六进制0x3000等于十进制12288字节，专门用于用户态任务执行系统调用时的栈空间
 */
#define OS_USER_TASK_SYSCALL_STACK_SIZE 0x3000

/**
 * @ingroup los_process
 * @brief 用户态任务栈大小
 * @note 十六进制0x100000等于十进制1048576字节(1MB)，为用户态任务的默认栈空间大小
 */
#define OS_USER_TASK_STACK_SIZE         0x100000

/**
 * @ingroup los_process
 * @brief 内核模式标志
 * @note 0x0U表示进程运行在内核模式，拥有最高系统权限
 */
#define OS_KERNEL_MODE 0x0U

/**
 * @ingroup los_process
 * @brief 用户模式标志
 * @note 0x1U表示进程运行在用户模式，受到内存保护和权限限制
 */
#define OS_USER_MODE   0x1U

/**
 * @ingroup los_process
 * @brief 检查进程是否为用户模式
 * @param processCB 进程控制块指针
 * @return BOOL - 若为用户模式返回TRUE，否则返回FALSE
 * @note 进程模式通过LosProcessCB结构体的processMode字段判断
 */
STATIC INLINE BOOL OsProcessIsUserMode(const LosProcessCB *processCB)
{
    return (processCB->processMode == OS_USER_MODE);
}

/**
 * @ingroup los_process
 * @brief 进程优先级类型
 * @note 用于标识优先级操作的对象类型为进程
 */
#define LOS_PRIO_PROCESS  0U

/**
 * @ingroup los_process
 * @brief 进程组优先级类型
 * @note 用于标识优先级操作的对象类型为进程组
 */
#define LOS_PRIO_PGRP     1U

/**
 * @ingroup los_process
 * @brief 用户优先级类型
 * @note 用于标识优先级操作的对象类型为用户
 */
#define LOS_PRIO_USER     2U

/**
 * @ingroup los_process
 * @brief 用户特权进程组
 * @note 通过OsGetUserInitProcess()获取用户初始化进程作为用户特权进程组的代表
 */
#define OS_USER_PRIVILEGE_PROCESS_GROUP ((UINTPTR)OsGetUserInitProcess())

/**
 * @ingroup los_process
 * @brief 内核进程组
 * @note 通过OsGetKernelInitProcess()获取内核初始化进程作为内核进程组的代表
 */
#define OS_KERNEL_PROCESS_GROUP         ((UINTPTR)OsGetKernelInitProcess())

/*
 * 进程退出码格式说明
 * 31    15           8           7        0
 * |     | exit code  | core dump | signal |
 * 位31-16: 保留位
 * 位15-8:  退出码(0-255)
 * 位7:     核心转储标志(1表示产生core dump)
 * 位0-6:   终止信号(0-127)
 */
#define OS_PRO_EXIT_OK 0 ///< 进程正常退出码

/**
 * @ingroup los_process
 * @brief 设置进程退出码的核心转储标志位
 * @param processCB 进程控制块指针
 * @note 将exitCode的第7位置1，表示进程退出时需要生成核心转储文件
 */
STATIC INLINE VOID OsProcessExitCodeCoreDumpSet(LosProcessCB *processCB)
{
    processCB->exitCode |= 0x80U; ///< 0x80U(128)对应二进制10000000，设置第7位
}

/**
 * @ingroup los_process
 * @brief 设置进程退出码的信号值
 * @param processCB 进程控制块指针
 * @param signal 终止信号值(0-127)
 * @note 将signal值写入exitCode的低7位，信号值范围限制在0-127
 */
STATIC INLINE VOID OsProcessExitCodeSignalSet(LosProcessCB *processCB, UINT32 signal)
{
    processCB->exitCode |= signal & 0x7FU; ///< 0x7FU(127)对应二进制01111111，确保信号值不超过7位
}

/**
 * @ingroup los_process
 * @brief 清除进程退出码的信号值
 * @param processCB 进程控制块指针
 * @note 将exitCode的低7位清零，清除之前设置的信号值
 */
STATIC INLINE VOID OsProcessExitCodeSignalClear(LosProcessCB *processCB)
{
    processCB->exitCode &= (~0x7FU); ///< ~0x7FU对应二进制11111111111111111111111110000000，清除低7位
}

/**
 * @ingroup los_process
 * @brief 检查进程退出码是否设置了信号值
 * @param processCB 进程控制块指针
 * @return BOOL - 若设置了信号值返回TRUE，否则返回FALSE
 * @note 检查exitCode的低7位是否非零
 */
STATIC INLINE BOOL OsProcessExitCodeSignalIsSet(LosProcessCB *processCB)
{
    return (processCB->exitCode) & 0x7FU; ///< 检查低7位是否有信号值
}

/**
 * @ingroup los_process
 * @brief 设置进程退出码
 * @param processCB 进程控制块指针
 * @param code 退出码值(0-255)
 * @note 将code值写入exitCode的8-15位，退出码范围限制在0-255
 */
STATIC INLINE VOID OsProcessExitCodeSet(LosProcessCB *processCB, UINT32 code)
{
    processCB->exitCode |= ((code & 0x000000FFU) << 8U) & 0x0000FF00U; /* 8: 左移8位，将退出码存入15-8位 */
}

/**
 * @ingroup los_process
 * @brief 检查进程ID是否无效
 * @param pid 进程ID
 * @return BOOL - 若PID无效返回TRUE，否则返回FALSE
 * @note 当PID大于等于系统最大进程数时判定为无效
 */
#define OS_PID_CHECK_INVALID(pid) (((UINT32)(pid)) >= g_processMaxNum)

/**
 * @ingroup los_process
 * @brief 检查用户进程ID是否无效
 * @param pid 进程ID
 * @return BOOL - 若用户PID无效返回TRUE，否则返回FALSE
 * @note 用户进程ID不能为0(内核空闲进程)且不能大于等于系统最大进程数
 */
STATIC INLINE BOOL OsProcessIDUserCheckInvalid(UINT32 pid)
{
    return ((pid >= g_processMaxNum) || (pid == 0)); ///< 0为内核保留PID，用户进程无效
}

/**
 * @ingroup los_process
 * @brief 获取当前运行的进程控制块
 * @return LosProcessCB* - 当前进程控制块指针
 * @note 关闭中断防止获取过程中进程切换，通过当前任务控制块转换得到进程控制块
 */
STATIC INLINE LosProcessCB *OsCurrProcessGet(VOID)
{
    UINT32 intSave;

    intSave = LOS_IntLock(); ///< 关闭中断，保护进程控制块获取过程
    LosProcessCB *runProcess = OS_PCB_FROM_TCB(OsCurrTaskGet()); ///< 从当前任务控制块获取进程控制块
    LOS_IntRestore(intSave); ///< 恢复中断
    return runProcess;
}

#ifdef LOSCFG_SECURITY_CAPABILITY ///< 若启用安全能力配置
/**
 * @ingroup los_process
 * @brief 获取当前进程的用户结构体
 * @return User* - 当前用户结构体指针
 * @note 关闭中断防止获取过程中用户信息变更
 */
STATIC INLINE User *OsCurrUserGet(VOID)
{
    User *user = NULL;
    UINT32 intSave;

    intSave = LOS_IntLock(); ///< 关闭中断，保护用户信息获取过程
    user = OsCurrProcessGet()->user; ///< 从当前进程控制块获取用户指针
    LOS_IntRestore(intSave); ///< 恢复中断
    return user;
}

/**
 * @ingroup los_process
 * @brief 获取任务所属进程的用户ID
 * @param taskCB 任务控制块指针
 * @return UINT32 - 用户ID，若进程无用户信息返回OS_INVALID
 * @note 关闭中断防止获取过程中进程信息变更
 */
STATIC INLINE UINT32 OsProcessUserIDGet(const LosTaskCB *taskCB)
{
    UINT32 intSave = LOS_IntLock(); ///< 关闭中断，保护用户ID获取过程
    UINT32 uid = OS_INVALID; ///< 初始化为无效用户ID

    LosProcessCB *process = OS_PCB_FROM_TCB(taskCB); ///< 从任务控制块获取进程控制块
    if (process->user != NULL) { ///< 若进程关联了用户信息
        uid = process->user->userID; ///< 获取用户ID
    }
    LOS_IntRestore(intSave); ///< 恢复中断
    return uid;
}
#endif ///< LOSCFG_SECURITY_CAPABILITY
/**
 * @ingroup los_process
 * @brief 检查任务是否为进程的线程组组长
 * @param taskCB 任务控制块指针
 * @return BOOL - 若是线程组组长返回TRUE，否则返回FALSE
 * @note 线程组组长是进程的首个任务，负责进程资源管理
 */
STATIC INLINE BOOL OsIsProcessThreadGroup(const LosTaskCB *taskCB)
{
    return (OS_PCB_FROM_TCB(taskCB)->threadGroup == taskCB);
}

/**
 * @ingroup los_process
 * @brief 获取进程的线程数量
 * @param taskCB 任务控制块指针
 * @return UINT32 - 进程的线程总数
 * @note 通过任务控制块找到所属进程控制块，返回threadNumber成员值
 */
STATIC INLINE UINT32 OsProcessThreadNumberGet(const LosTaskCB *taskCB)
{
    return OS_PCB_FROM_TCB(taskCB)->threadNumber;
}

#ifdef LOSCFG_KERNEL_VM ///< 若启用内核虚拟内存配置
/**
 * @ingroup los_process
 * @brief 获取进程的虚拟内存空间
 * @param processCB 进程控制块指针
 * @return LosVmSpace* - 虚拟内存空间指针
 * @note 仅在LOSCFG_KERNEL_VM配置开启时可用
 */
STATIC INLINE LosVmSpace *OsProcessVmSpaceGet(const LosProcessCB *processCB)
{
    return processCB->vmSpace; ///< 返回进程控制块中的vmSpace成员
}
#endif ///< LOSCFG_KERNEL_VM

#ifdef LOSCFG_DRIVERS_TZDRIVER ///< 若启用TZ驱动配置
/**
 * @ingroup los_process
 * @brief 获取进程的可执行文件vnode
 * @param processCB 进程控制块指针
 * @return struct Vnode* - 可执行文件vnode指针
 * @note 仅在LOSCFG_DRIVERS_TZDRIVER配置开启时可用，用于安全区域文件访问
 */
STATIC INLINE struct Vnode *OsProcessExecVnodeGet(const LosProcessCB *processCB)
{
    return processCB->execVnode; ///< 返回进程控制块中的execVnode成员
}
#endif ///< LOSCFG_DRIVERS_TZDRIVER

/**
 * @ingroup los_process
 * @brief 获取进程ID
 * @param processCB 进程控制块指针
 * @return UINT32 - 进程ID
 * @note 若启用PID容器(LOSCFG_PID_CONTAINER)，返回当前容器内的虚拟PID；否则返回实际PID
 */
STATIC INLINE UINT32 OsGetPid(const LosProcessCB *processCB)
{
#ifdef LOSCFG_PID_CONTAINER ///< 若启用PID容器
    if (OS_PROCESS_CONTAINER_CHECK(processCB, OsCurrProcessGet())) { ///< 检查进程是否在当前容器内
        return OsGetVpidFromCurrContainer(processCB); ///< 返回容器内虚拟PID
    }
#endif
    return processCB->processID; ///< 返回实际进程ID
}

/**
 * @ingroup los_process
 * @brief 获取进程的根容器PID
 * @param processCB 进程控制块指针
 * @return UINT32 - 根容器中的PID
 * @note 若启用PID容器，返回根容器中的虚拟PID；否则返回实际PID
 */
STATIC INLINE UINT32 OsGetRootPid(const LosProcessCB *processCB)
{
#ifdef LOSCFG_PID_CONTAINER ///< 若启用PID容器
    return OsGetVpidFromRootContainer(processCB); ///< 返回根容器虚拟PID
#else
    return processCB->processID; ///< 返回实际进程ID
#endif
}

/*
 * 等待标志位定义
 * LOS_WAIT_WNOHANG:  0x00000001 (1) - 若没有子进程退出则立即返回
 * LOS_WAIT_WUNTRACED: 0x00000002 (2) - 返回已停止但未被跟踪的子进程状态
 * LOS_WAIT_WEXITED:   0x00000004 (4) - 等待已退出的子进程
 * LOS_WAIT_WCONTINUED:0x00000008 (8) - 返回因SIGCONT信号恢复的子进程状态
 * LOS_WAIT_WNOWAIT:   0x01000000 (16777216) - 保持子进程可等待状态，允许后续等待
 */

/**
 * @ingroup los_process
 * @brief 非阻塞等待标志
 * @note 位0置1，表示若没有子进程退出则立即返回，不阻塞
 */
#define LOS_WAIT_WNOHANG   (1 << 0U) ///< 0x00000001，非阻塞等待

/**
 * @ingroup los_process
 * @brief 等待已停止子进程标志
 * @note 位1置1，表示返回已停止但未被ptrace跟踪的子进程状态
 */
#define LOS_WAIT_WUNTRACED (1 << 1U) ///< 0x00000002，等待未跟踪的停止子进程
#define LOS_WAIT_WSTOPPED (1 << 1U)  ///< 0x00000002，LOS_WAIT_WUNTRACED的别名

/**
 * @ingroup los_process
 * @brief 等待已退出子进程标志
 * @note 位2置1，表示仅等待已退出的子进程
 */
#define LOS_WAIT_WEXITED (1 << 2U) ///< 0x00000004，等待已退出子进程

/**
 * @ingroup los_process
 * @brief 等待已恢复子进程标志
 * @note 位3置1，表示返回因SIGCONT信号恢复运行的子进程状态
 */
#define LOS_WAIT_WCONTINUED (1 << 3U) ///< 0x00000008，等待已恢复的子进程

/**
 * @ingroup los_process
 * @brief 保持等待状态标志
 * @note 位24置1，表示子进程状态保留，允许后续wait调用再次获取
 */
#define LOS_WAIT_WNOWAIT (1 << 24U) ///< 0x01000000，保持子进程可等待状态

/**
 * @ingroup los_process
 * @brief 进程等待中标志
 * @note 位15置1，表示进程当前处于等待状态
 */
#define OS_PROCESS_WAIT     (1 << 15U) ///< 0x00008000，进程等待中状态标记

/**
 * @ingroup los_process
 * @brief 等待任意子进程
 * @note 等待进程的任意子进程结束，与OS_TASK_WAIT_ANYPROCESS等价
 */
#define OS_PROCESS_WAIT_ANY OS_TASK_WAIT_ANYPROCESS

/**
 * @ingroup los_process
 * @brief 等待指定PID的子进程
 * @note 等待特定PID的子进程结束，与OS_TASK_WAIT_PROCESS等价
 */
#define OS_PROCESS_WAIT_PRO OS_TASK_WAIT_PROCESS

/**
 * @ingroup los_process
 * @brief 等待指定进程组的子进程
 * @note 等待特定进程组内的任意子进程结束，与OS_TASK_WAIT_GID等价
 */
#define OS_PROCESS_WAIT_GID OS_TASK_WAIT_GID

/**
 * @ingroup los_process
 * @brief 获取所有进程信息标志
 * @note 用于进程信息查询接口，表示需要返回全部进程信息
 */
#define OS_PROCESS_INFO_ALL 1 ///< 查询所有进程信息的标志值

/**
 * @ingroup los_process
 * @brief 默认文件创建权限掩码
 * @note 八进制0022表示默认屏蔽组写和其他写权限，对应权限位为111111011010
 */
#define OS_PROCESS_DEFAULT_UMASK 0022 ///< 默认umask值，八进制0022

/**
 * @ingroup los_process
 * @brief 用户初始化入口地址
 * @note 链接器定义的符号，表示用户态初始化代码的入口点
 */
extern UINTPTR __user_init_entry;

/**
 * @ingroup los_process
 * @brief 用户初始化BSS段起始地址
 * @note 链接器定义的符号，表示用户态初始化数据BSS段的起始位置
 */
extern UINTPTR __user_init_bss;

/**
 * @ingroup los_process
 * @brief 用户初始化结束地址
 * @note 链接器定义的符号，表示用户态初始化代码和数据的结束位置
 */
extern UINTPTR __user_init_end;

/**
 * @ingroup los_process
 * @brief 用户初始化加载地址
 * @note 链接器定义的符号，表示用户态初始化镜像的加载地址
 */
extern UINTPTR __user_init_load_addr;

extern UINT32 OsProcessInit(VOID);
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
extern INT32 OsSetProcessScheduler(INT32 which, INT32 pid, UINT16 policy, const LosSchedParam *param);
extern INT32 OsGetProcessPriority(INT32 which, INT32 pid);
extern LosProcessCB *OsGetUserInitProcess(VOID);
extern LosProcessCB *OsGetIdleProcess(VOID);
extern INT32 OsSetProcessGroupID(UINT32 pid, UINT32 gid);
extern INT32 OsSetCurrProcessGroupID(UINT32 gid);
extern LosProcessCB *OsGetKernelInitProcess(VOID);
extern VOID OsSetSigHandler(UINTPTR addr);
extern UINTPTR OsGetSigHandler(VOID);
extern VOID OsWaitWakeTask(LosTaskCB *taskCB, UINT32 wakePID);
extern INT32 OsSendSignalToProcessGroup(INT32 pid, siginfo_t *info, INT32 permission);
extern INT32 OsSendSignalToAllProcess(siginfo_t *info, INT32 permission);
extern UINT32 OsProcessAddNewTask(UINTPTR processID, LosTaskCB *taskCB, SchedParam *param, UINT32 *numCount);
extern VOID OsDeleteTaskFromProcess(LosTaskCB *taskCB);
extern VOID OsProcessThreadGroupDestroy(VOID);
extern UINT32 OsGetProcessGroupCB(UINT32 pid, UINTPTR *ppgroupLeader);
extern LosProcessCB *OsGetDefaultProcessCB(VOID);
extern ProcessGroup *OsCreateProcessGroup(LosProcessCB *processCB);
INT32 OsSchedulerParamCheck(UINT16 policy, BOOL isThread, const LosSchedParam *param);
#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif
