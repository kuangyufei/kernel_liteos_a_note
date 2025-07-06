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
/**
 * @file los_task.c
 * @brief 
 * @verbatim
    基本概念
        从系统角度看，任务是竞争系统资源的最小运行单元。任务可以使用或等待CPU、
            使用内存空间等系统资源，并独立于其它任务运行。
        任务模块可以给用户提供多个任务，实现任务间的切换，帮助用户管理业务程序流程。具有如下特性：
        支持多任务。
            一个任务表示一个线程。
            抢占式调度机制，高优先级的任务可打断低优先级任务，低优先级任务必须在高优先级任务阻塞或结束后才能得到调度。
            相同优先级任务支持时间片轮转调度方式。
            共有32个优先级[0-31]，最高优先级为0，最低优先级为31。 
            
    任务状态通常分为以下四种：
        就绪（Ready）：该任务在就绪队列中，只等待CPU。
        运行（Running）：该任务正在执行。
        阻塞（Blocked）：该任务不在就绪队列中。包含任务被挂起（suspend状态）、任务被延时（delay状态）、
            任务正在等待信号量、读写队列或者等待事件等。
        退出态（Dead）：该任务运行结束，等待系统回收资源。

    任务状态迁移说明
        就绪态→运行态
            任务创建后进入就绪态，发生任务切换时，就绪队列中最高优先级的任务被执行，
            从而进入运行态，但此刻该任务依旧在就绪队列中。
        运行态→阻塞态
            正在运行的任务发生阻塞（挂起、延时、读信号量等）时，该任务会从就绪队列中删除，
            任务状态由运行态变成阻塞态，然后发生任务切换，运行就绪队列中最高优先级任务。
        阻塞态→就绪态（阻塞态→运行态）
            阻塞的任务被恢复后（任务恢复、延时时间超时、读信号量超时或读到信号量等），此时被
            恢复的任务会被加入就绪队列，从而由阻塞态变成就绪态；此时如果被恢复任务的优先级高于
            正在运行任务的优先级，则会发生任务切换，该任务由就绪态变成运行态。
        就绪态→阻塞态
            任务也有可能在就绪态时被阻塞（挂起），此时任务状态由就绪态变为阻塞态，该任务
            从就绪队列中删除，不会参与任务调度，直到该任务被恢复。
        运行态→就绪态
            有更高优先级任务创建或者恢复后，会发生任务调度，此刻就绪队列中最高优先级任务
            变为运行态，那么原先运行的任务由运行态变为就绪态，依然在就绪队列中。
        运行态→退出态
            运行中的任务运行结束，任务状态由运行态变为退出态。退出态包含任务运行结束的正常退出状态
            以及Invalid状态。例如，任务运行结束但是没有自删除，对外呈现的就是Invalid状态，即退出态。
        阻塞态→退出态
            阻塞的任务调用删除接口，任务状态由阻塞态变为退出态。
            
    主要术语
        任务ID
            任务ID，在任务创建时通过参数返回给用户，是任务的重要标识。系统中的ID号是唯一的。用户可以
            通过任务ID对指定任务进行任务挂起、任务恢复、查询任务名等操作。

        任务优先级
            优先级表示任务执行的优先顺序。任务的优先级决定了在发生任务切换时即将要执行的任务，
            就绪队列中最高优先级的任务将得到执行。

        任务入口函数
            新任务得到调度后将执行的函数。该函数由用户实现，在任务创建时，通过任务创建结构体设置。

        任务栈
            每个任务都拥有一个独立的栈空间，我们称为任务栈。栈空间里保存的信息包含局部变量、寄存器、函数参数、函数返回地址等。

        任务上下文
            任务在运行过程中使用的一些资源，如寄存器等，称为任务上下文。当这个任务挂起时，其他任务继续执行，
            可能会修改寄存器等资源中的值。如果任务切换时没有保存任务上下文，可能会导致任务恢复后出现未知错误。
            因此，Huawei LiteOS在任务切换时会将切出任务的任务上下文信息，保存在自身的任务栈中，以便任务恢复后，
            从栈空间中恢复挂起时的上下文信息，从而继续执行挂起时被打断的代码。
        任务控制块TCB
            每个任务都含有一个任务控制块(TCB)。TCB包含了任务上下文栈指针（stack pointer）、任务状态、
            任务优先级、任务ID、任务名、任务栈大小等信息。TCB可以反映出每个任务运行情况。
        任务切换
            任务切换包含获取就绪队列中最高优先级任务、切出任务上下文保存、切入任务上下文恢复等动作。

    运作机制
        用户创建任务时，系统会初始化任务栈，预置上下文。此外，系统还会将“任务入口函数”
        地址放在相应位置。这样在任务第一次启动进入运行态时，将会执行“任务入口函数”。
 * @endverbatim
 * @param pathname 
 * @return int 
 */
#include "los_task_pri.h"
#include "los_base_pri.h"
#include "los_event_pri.h"
#include "los_exc.h"
#include "los_hw_pri.h"
#include "los_init.h"
#include "los_memstat_pri.h"
#include "los_mp.h"
#include "los_mux_pri.h"
#include "los_sched_pri.h"
#include "los_sem_pri.h"
#include "los_spinlock.h"
#include "los_strncpy_from_user.h"
#include "los_percpu_pri.h"
#include "los_process_pri.h"
#include "los_vm_map.h"
#include "los_vm_syscall.h"
#include "los_signal.h"
#include "los_hook.h"

#ifdef LOSCFG_KERNEL_CPUP
#include "los_cpup_pri.h"
#endif
#ifdef LOSCFG_BASE_CORE_SWTMR_ENABLE
#include "los_swtmr_pri.h"
#endif
#ifdef LOSCFG_KERNEL_LITEIPC
#include "hm_liteipc.h"
#endif
#ifdef LOSCFG_ENABLE_OOM_LOOP_TASK
#include "los_oom.h"
#endif
#ifdef LOSCFG_KERNEL_CONTAINER
#include "los_container_pri.h"
#endif

#if (LOSCFG_BASE_CORE_TSK_LIMIT <= 0)
#error "task maxnum cannot be zero"
#endif  /* LOSCFG_BASE_CORE_TSK_LIMIT <= 0 */

/* 任务控制块数组，存储系统中所有任务的控制信息 */
LITE_OS_SEC_BSS LosTaskCB    *g_taskCBArray;
/* 空闲任务链表，管理系统中未使用的任务控制块 */
LITE_OS_SEC_BSS LOS_DL_LIST  g_losFreeTask;
/* 任务回收链表，用于延迟释放已退出任务的资源 */
LITE_OS_SEC_BSS LOS_DL_LIST  g_taskRecycleList;
/* 系统支持的最大任务数量 */
LITE_OS_SEC_BSS UINT32       g_taskMaxNum;
/* 任务调度标志，每一位代表一个CPU核心的调度状态 */
LITE_OS_SEC_BSS UINT32       g_taskScheduled; /* one bit for each cores */
/* 资源事件控制块，用于任务资源相关的事件同步 */
LITE_OS_SEC_BSS EVENT_CB_S   g_resourceEvent;
/* 任务模块自旋锁，仅在SMP模式下可用 */
LITE_OS_SEC_BSS SPIN_LOCK_INIT(g_taskSpin);

/* 控制台ID设置钩子函数，弱引用OsSetConsoleID */
STATIC VOID OsConsoleIDSetHook(UINT32 param1,
                               UINT32 param2) __attribute__((weakref("OsSetConsoleID")));

/* 启动过程中的临时任务块 */
LITE_OS_SEC_BSS STATIC LosTaskCB                g_mainTask[LOSCFG_KERNEL_CORE_NUM];

/**
 * @brief 获取当前CPU核心的主任务控制块
 *
 * @details 该函数根据当前CPU核心ID，从主任务数组中获取对应的主任务控制块
 *
 * @return LosTaskCB* - 当前CPU核心的主任务控制块指针
 */
LosTaskCB *OsGetMainTask(VOID)
{
    return (LosTaskCB *)(g_mainTask + ArchCurrCpuid());
}

/**
 * @brief 初始化主任务控制块
 *
 * @details 该函数初始化系统中的主任务控制块数组，设置任务名称、调度参数等基本信息
 *          主任务是系统启动过程中的临时任务，每个CPU核心对应一个主任务
 */
VOID OsSetMainTask(VOID)
{
    UINT32 i;
    CHAR *name = "osMain";
    SchedParam schedParam = { 0 };

    schedParam.policy = LOS_SCHED_RR;  // 设置调度策略为时间片轮转
    schedParam.basePrio = OS_PROCESS_PRIORITY_HIGHEST;  // 设置基础优先级为最高
    schedParam.priority = OS_TASK_PRIORITY_LOWEST;  // 设置任务优先级为最低

    for (i = 0; i < LOSCFG_KERNEL_CORE_NUM; i++) {
        g_mainTask[i].taskStatus = OS_TASK_STATUS_UNUSED;  // 初始化为未使用状态
        g_mainTask[i].taskID = LOSCFG_BASE_CORE_TSK_LIMIT;  // 设置任务ID
        g_mainTask[i].processCB = OS_KERNEL_PROCESS_GROUP;  // 归属内核进程组
#ifdef LOSCFG_KERNEL_SMP_LOCKDEP
        g_mainTask[i].lockDep.lockDepth = 0;  // 锁深度初始化为0
        g_mainTask[i].lockDep.waitLock = NULL;  // 等待锁初始化为空
#endif
        (VOID)strncpy_s(g_mainTask[i].taskName, OS_TCB_NAME_LEN, name, OS_TCB_NAME_LEN - 1);  // 复制任务名称
        LOS_ListInit(&g_mainTask[i].lockList);  // 初始化锁链表
        (VOID)OsSchedParamInit(&g_mainTask[i], schedParam.policy, &schedParam, NULL);  // 初始化调度参数
    }
}

/**
 * @brief 设置主任务的进程控制块
 *
 * @details 该函数为所有CPU核心的主任务设置进程控制块，用于将主任务关联到指定进程
 *
 * @param processCB - 进程控制块指针
 */
VOID OsSetMainTaskProcess(UINTPTR processCB)
{
    for (UINT32 i = 0; i < LOSCFG_KERNEL_CORE_NUM; i++) {
        g_mainTask[i].processCB = processCB;  // 设置进程控制块
#ifdef LOSCFG_PID_CONTAINER
        g_mainTask[i].pidContainer = OS_PID_CONTAINER_FROM_PCB((LosProcessCB *)processCB);  // 设置PID容器
#endif
    }
}

/**
 * @brief 空闲任务入口函数
 *
 * @details 空闲任务是系统中优先级最低的任务，当没有其他任务可运行时执行
 *          该函数实现了一个无限循环，执行WFI指令使CPU进入低功耗状态
 */
LITE_OS_SEC_TEXT WEAK VOID OsIdleTask(VOID)
{
    while (1) {
        WFI;  // 执行等待中断指令，使CPU进入低功耗状态
    }
}

/**
 * @brief 将任务控制块插入回收链表
 *
 * @details 该函数将已退出的任务控制块添加到任务回收链表尾部，等待后续资源释放
 *
 * @param taskCB - 需要回收的任务控制块指针
 */
VOID OsTaskInsertToRecycleList(LosTaskCB *taskCB)
{
    LOS_ListTailInsert(&g_taskRecycleList, &taskCB->pendList);  // 将任务控制块插入回收链表尾部
}

/**
 * @brief 处理任务join操作的后续唤醒
 *
 * @details 当任务退出且设置了可join属性时，该函数唤醒等待在该任务join链表上的任务
 *          并将任务状态标记为已退出
 *
 * @param taskCB - 已退出的任务控制块指针
 */
LITE_OS_SEC_TEXT_INIT VOID OsTaskJoinPostUnsafe(LosTaskCB *taskCB)
{
    if (taskCB->taskStatus & OS_TASK_FLAG_PTHREAD_JOIN) {  // 检查任务是否设置了可join属性
        if (!LOS_ListEmpty(&taskCB->joinList)) {  // 检查是否有任务等待join
            LosTaskCB *resumedTask = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&(taskCB->joinList)));  // 获取第一个等待任务
            OsTaskWakeClearPendMask(resumedTask);  // 清除唤醒任务的等待掩码
            resumedTask->ops->wake(resumedTask);  // 唤醒等待任务
        }
    }
    taskCB->taskStatus |= OS_TASK_STATUS_EXIT;  // 设置任务状态为已退出
}

/**
 * @brief 任务join操作的等待处理
 *
 * @details 该函数处理任务的join等待逻辑，如果目标任务已退出则直接返回，否则将当前任务加入等待链表
 *
 * @param taskCB - 目标任务控制块指针
 * @return UINT32 - 操作结果，LOS_OK表示成功，其他值表示失败
 */
LITE_OS_SEC_TEXT UINT32 OsTaskJoinPendUnsafe(LosTaskCB *taskCB)
{
    if (taskCB->taskStatus & OS_TASK_STATUS_INIT) {  // 检查目标任务是否未初始化
        return LOS_EINVAL;  // 返回参数无效错误
    }

    if (taskCB->taskStatus & OS_TASK_STATUS_EXIT) {  // 检查目标任务是否已退出
        return LOS_OK;  // 直接返回成功
    }

    if ((taskCB->taskStatus & OS_TASK_FLAG_PTHREAD_JOIN) && LOS_ListEmpty(&taskCB->joinList)) {  // 检查任务是否可join且等待链表为空
        OsTaskWaitSetPendMask(OS_TASK_WAIT_JOIN, taskCB->taskID, LOS_WAIT_FOREVER);  // 设置等待掩码
        LosTaskCB *runTask = OsCurrTaskGet();  // 获取当前运行任务
        return runTask->ops->wait(runTask, &taskCB->joinList, LOS_WAIT_FOREVER);  // 将当前任务加入等待链表
    }

    return LOS_EINVAL;  // 返回参数无效错误
}

/**
 * @brief 设置任务为分离状态
 *
 * @details 该函数将可join的任务设置为分离状态，使其退出后自动释放资源，无需其他任务join
 *
 * @param taskCB - 任务控制块指针
 * @return UINT32 - 操作结果，LOS_OK表示成功，其他值表示失败
 */
LITE_OS_SEC_TEXT UINT32 OsTaskSetDetachUnsafe(LosTaskCB *taskCB)
{
    if (taskCB->taskStatus & OS_TASK_FLAG_PTHREAD_JOIN) {  // 检查任务是否可join
        if (LOS_ListEmpty(&(taskCB->joinList))) {  // 检查是否有任务等待join
            LOS_ListDelete(&(taskCB->joinList));  // 删除join链表
            taskCB->taskStatus &= ~OS_TASK_FLAG_PTHREAD_JOIN;  // 清除可join标志
            return LOS_OK;  // 返回成功
        }
        /* 此错误码有特殊用途，不允许在接口上再次出现 */
        return LOS_ESRCH;  // 返回任务不存在错误
    }

    return LOS_EINVAL;  // 返回参数无效错误
}

/**
 * @brief 任务模块初始化
 *
 * @details 该函数初始化任务控制块数组、空闲任务链表和任务回收链表，并分配任务控制块内存
 *          内存为常驻内存，用于保存任务控制块的系统资源，不会被释放
 *
 * @param processCB - 进程控制块指针
 * @return UINT32 - 操作结果，LOS_OK表示成功，其他值表示失败
 */
LITE_OS_SEC_TEXT_INIT UINT32 OsTaskInit(UINTPTR processCB)
{
    UINT32 index;
    UINT32 size;
    UINT32 ret;

    g_taskMaxNum = LOSCFG_BASE_CORE_TSK_LIMIT;  // 设置最大任务数量
    size = (g_taskMaxNum + 1) * sizeof(LosTaskCB);  // 计算任务控制块数组大小
    /*
     * 此内存为常驻内存，用于保存任务控制块的系统资源，不会被释放
     */
    g_taskCBArray = (LosTaskCB *)LOS_MemAlloc(m_aucSysMem0, size);  // 分配任务控制块数组内存
    if (g_taskCBArray == NULL) {  // 检查内存分配是否成功
        ret = LOS_ERRNO_TSK_NO_MEMORY;  // 设置内存不足错误
        goto EXIT;  // 跳转到出口
    }
    (VOID)memset_s(g_taskCBArray, size, 0, size);  // 初始化任务控制块数组内存

    LOS_ListInit(&g_losFreeTask);  // 初始化空闲任务链表
    LOS_ListInit(&g_taskRecycleList);  // 初始化任务回收链表
    for (index = 0; index < g_taskMaxNum; index++) {  // 遍历任务控制块数组
        g_taskCBArray[index].taskStatus = OS_TASK_STATUS_UNUSED;  // 设置任务状态为未使用
        g_taskCBArray[index].taskID = index;  // 设置任务ID
        g_taskCBArray[index].processCB = processCB;  // 设置进程控制块
        LOS_ListTailInsert(&g_losFreeTask, &g_taskCBArray[index].pendList);  // 将任务控制块插入空闲链表
    }

    g_taskCBArray[index].taskStatus = OS_TASK_STATUS_UNUSED;  // 设置最后一个任务控制块状态
    g_taskCBArray[index].taskID = index;  // 设置最后一个任务控制块ID
    g_taskCBArray[index].processCB = processCB;  // 设置最后一个任务控制块的进程控制块

    ret = OsSchedInit();  // 初始化调度器

EXIT:
    if (ret != LOS_OK) {  // 检查初始化是否失败
        PRINT_ERR("OsTaskInit error\n");  // 打印错误信息
    }
    return ret;  // 返回结果
}

/**
 * @brief 获取空闲任务ID
 *
 * @details 该函数从调度队列中获取空闲任务的ID
 *
 * @return UINT32 - 空闲任务ID
 */
UINT32 OsGetIdleTaskId(VOID)
{
    return OsSchedRunqueueIdleGet()->taskID;  // 返回空闲任务ID
}

/**
 * @brief 创建空闲任务
 *
 * @details 该函数创建系统空闲任务，设置空闲任务的入口函数、堆栈大小、优先级等参数
 *          并将空闲任务加入调度队列
 *
 * @param processID - 进程ID
 * @return UINT32 - 操作结果，LOS_OK表示成功，其他值表示失败
 */
LITE_OS_SEC_TEXT_INIT UINT32 OsIdleTaskCreate(UINTPTR processID)
{
    UINT32 ret;
    TSK_INIT_PARAM_S taskInitParam;
    UINT32 idleTaskID;

    (VOID)memset_s((VOID *)(&taskInitParam), sizeof(TSK_INIT_PARAM_S), 0, sizeof(TSK_INIT_PARAM_S));  // 初始化任务参数
    taskInitParam.pfnTaskEntry = (TSK_ENTRY_FUNC)OsIdleTask;  // 设置任务入口函数为空闲任务
    taskInitParam.uwStackSize = LOSCFG_BASE_CORE_TSK_IDLE_STACK_SIZE;  // 设置空闲任务堆栈大小
    taskInitParam.pcName = "Idle";  // 设置任务名称
    taskInitParam.policy = LOS_SCHED_IDLE;  // 设置调度策略为空闲调度
    taskInitParam.usTaskPrio = OS_TASK_PRIORITY_LOWEST;  // 设置优先级为最低
    taskInitParam.processID = processID;  // 设置进程ID
#ifdef LOSCFG_KERNEL_SMP
    taskInitParam.usCpuAffiMask = CPUID_TO_AFFI_MASK(ArchCurrCpuid());  // 设置CPU亲和性掩码
#endif
    ret = LOS_TaskCreateOnly(&idleTaskID, &taskInitParam);  // 创建任务但不启动
    if (ret != LOS_OK) {  // 检查任务创建是否成功
        return ret;  // 返回错误
    }
    LosTaskCB *idleTask = OS_TCB_FROM_TID(idleTaskID);  // 获取空闲任务控制块
    idleTask->taskStatus |= OS_TASK_FLAG_SYSTEM_TASK;  // 设置为系统任务
    OsSchedRunqueueIdleInit(idleTask);  // 初始化空闲任务调度队列

    return LOS_TaskResume(idleTaskID);  // 恢复空闲任务运行
}

/*
 * Description : 获取当前运行任务的ID
 * Return      : 任务ID
 */
LITE_OS_SEC_TEXT UINT32 LOS_CurTaskIDGet(VOID)
{
    LosTaskCB *runTask = OsCurrTaskGet();  // 获取当前运行任务控制块

    if (runTask == NULL) {  // 检查任务控制块是否为空
        return LOS_ERRNO_TSK_ID_INVALID;  // 返回无效任务ID
    }
    return runTask->taskID;  // 返回当前任务ID
}

/**
 * @brief 创建任务同步信号量
 *
 * @details 在SMP模式下，该函数创建用于任务同步的信号量
 *
 * @param taskCB - 任务控制块指针
 * @return UINT32 - 操作结果，LOS_OK表示成功，其他值表示失败
 */
STATIC INLINE UINT32 TaskSyncCreate(LosTaskCB *taskCB)
{
#ifdef LOSCFG_KERNEL_SMP_TASK_SYNC
    UINT32 ret = LOS_SemCreate(0, &taskCB->syncSignal);  // 创建初始值为0的信号量
    if (ret != LOS_OK) {  // 检查信号量创建是否成功
        return LOS_ERRNO_TSK_MP_SYNC_RESOURCE;  // 返回同步资源错误
    }
#else
    (VOID)taskCB;  // 未使用参数，避免编译警告
#endif
    return LOS_OK;  // 返回成功
}

/**
 * @brief 销毁任务同步信号量
 *
 * @details 在SMP模式下，该函数销毁用于任务同步的信号量
 *
 * @param syncSignal - 信号量ID
 */
STATIC INLINE VOID OsTaskSyncDestroy(UINT32 syncSignal)
{
#ifdef LOSCFG_KERNEL_SMP_TASK_SYNC
    (VOID)LOS_SemDelete(syncSignal);  // 删除信号量
#else
    (VOID)syncSignal;  // 未使用参数，避免编译警告
#endif
}

#ifdef LOSCFG_KERNEL_SMP
/**
 * @brief 等待任务同步信号量
 *
 * @details 在SMP模式下，该函数等待任务同步信号量，用于任务间同步
 *          超时时间设置为GC周期的两倍，防止GC定时器在超时时刻触发
 *
 * @param taskCB - 任务控制块指针
 * @return UINT32 - 操作结果，LOS_OK表示成功，其他值表示失败
 */
STATIC INLINE UINT32 OsTaskSyncWait(const LosTaskCB *taskCB)
{
#ifdef LOSCFG_KERNEL_SMP_TASK_SYNC
    UINT32 ret = LOS_OK;

    LOS_ASSERT(LOS_SpinHeld(&g_taskSpin));  // 断言自旋锁已持有
    LOS_SpinUnlock(&g_taskSpin);  // 释放自旋锁
    /*
     * GC软定时器每隔OS_MP_GC_PERIOD周期运行，为防止该定时器在超时时刻触发
     * 将超时时间设置为GC周期的两倍
     */
    if (LOS_SemPend(taskCB->syncSignal, OS_MP_GC_PERIOD * 2) != LOS_OK) { /* 2: 等待200毫秒 */
        ret = LOS_ERRNO_TSK_MP_SYNC_FAILED;  // 设置同步失败错误
    }

    LOS_SpinLock(&g_taskSpin);  // 获取自旋锁

    return ret;  // 返回结果
#else
    (VOID)taskCB;  // 未使用参数，避免编译警告
    return LOS_OK;  // 返回成功
#endif
}
#endif

/**
 * @brief 唤醒任务同步信号量
 *
 * @details 在SMP模式下，该函数发送任务同步信号量，唤醒等待的任务
 *
 * @param taskCB - 任务控制块指针
 */
STATIC INLINE VOID OsTaskSyncWake(const LosTaskCB *taskCB)
{
#ifdef LOSCFG_KERNEL_SMP_TASK_SYNC
    (VOID)OsSemPostUnsafe(taskCB->syncSignal, NULL);  // 发送信号量
#else
    (VOID)taskCB;  // 未使用参数，避免编译警告
#endif
}

/**
 * @brief 将任务控制块插入空闲链表
 *
 * @details 该函数重置任务控制块信息，并将其插入空闲任务链表，使其可被重新分配
 *
 * @param taskCB - 任务控制块指针
 */
STATIC INLINE VOID OsInsertTCBToFreeList(LosTaskCB *taskCB)
{
#ifdef LOSCFG_PID_CONTAINER
    OsFreeVtid(taskCB);  // 释放虚拟线程ID
#endif
    UINT32 taskID = taskCB->taskID;  // 保存任务ID
    (VOID)memset_s(taskCB, sizeof(LosTaskCB), 0, sizeof(LosTaskCB));  // 重置任务控制块内存
    taskCB->taskID = taskID;  // 恢复任务ID
    taskCB->processCB = (UINTPTR)OsGetDefaultProcessCB();  // 设置默认进程控制块
    taskCB->taskStatus = OS_TASK_STATUS_UNUSED;  // 设置任务状态为未使用
    LOS_ListAdd(&g_losFreeTask, &taskCB->pendList);  // 将任务控制块添加到空闲链表
}

/**
 * @brief 释放任务内核资源
 *
 * @details 该函数释放任务的同步信号量和堆栈内存等内核资源
 *
 * @param syncSignal - 同步信号量ID
 * @param topOfStack - 堆栈顶部地址
 */
STATIC VOID OsTaskKernelResourcesToFree(UINT32 syncSignal, UINTPTR topOfStack)
{
    OsTaskSyncDestroy(syncSignal);  // 销毁同步信号量

    (VOID)LOS_MemFree((VOID *)m_aucSysMem1, (VOID *)topOfStack);  // 释放堆栈内存
}

/**
 * @brief 释放任务资源
 *
 * @details 该函数释放任务的用户空间映射、同步信号量和堆栈等资源，并将任务控制块插入空闲链表
 *          对于用户模式任务，还会解除内存映射并清理相关服务句柄
 *
 * @param taskCB - 任务控制块指针
 */
STATIC VOID OsTaskResourcesToFree(LosTaskCB *taskCB)
{
    UINT32 syncSignal = LOSCFG_BASE_IPC_SEM_LIMIT;  // 初始化同步信号量ID
    UINT32 intSave;  // 中断状态保存变量
    UINTPTR topOfStack;  // 堆栈顶部地址

#ifdef LOSCFG_KERNEL_VM
    if ((taskCB->taskStatus & OS_TASK_FLAG_USER_MODE) && (taskCB->userMapBase != 0)) {  // 检查是否为用户模式任务且有内存映射
        SCHEDULER_LOCK(intSave);  // 关闭调度器
        UINT32 mapBase = (UINTPTR)taskCB->userMapBase;  // 保存映射基地址
        UINT32 mapSize = taskCB->userMapSize;  // 保存映射大小
        taskCB->userMapBase = 0;  // 重置映射基地址
        taskCB->userArea = 0;  // 重置用户区域
        SCHEDULER_UNLOCK(intSave);  // 开启调度器

        LosProcessCB *processCB = OS_PCB_FROM_TCB(taskCB);  // 获取进程控制块
        LOS_ASSERT(!(OsProcessVmSpaceGet(processCB) == NULL));  // 断言虚拟内存空间不为空
        UINT32 ret = OsUnMMap(OsProcessVmSpaceGet(processCB), (UINTPTR)mapBase, mapSize);  // 解除内存映射
        if ((ret != LOS_OK) && (mapBase != 0) && !OsProcessIsInit(processCB)) {  // 检查解除映射是否失败
            PRINT_ERR("process(%u) unmmap user task(%u) stack failed! mapbase: 0x%x size :0x%x, error: %d\n",
                      processCB->processID, taskCB->taskID, mapBase, mapSize, ret);  // 打印错误信息
        }

#ifdef LOSCFG_KERNEL_LITEIPC
        LiteIpcRemoveServiceHandle(taskCB->taskID);  // 移除LiteIPC服务句柄
#endif
    }
#endif

    if (taskCB->taskStatus & OS_TASK_STATUS_UNUSED) {  // 检查任务是否为未使用状态
        topOfStack = taskCB->topOfStack;  // 保存堆栈顶部地址
        taskCB->topOfStack = 0;  // 重置堆栈顶部地址
#ifdef LOSCFG_KERNEL_SMP_TASK_SYNC
        syncSignal = taskCB->syncSignal;  // 保存同步信号量ID
        taskCB->syncSignal = LOSCFG_BASE_IPC_SEM_LIMIT;  // 重置同步信号量ID
#endif
        OsTaskKernelResourcesToFree(syncSignal, topOfStack);  // 释放内核资源

        SCHEDULER_LOCK(intSave);  // 关闭调度器
#ifdef LOSCFG_KERNEL_VM
        OsClearSigInfoTmpList(&(taskCB->sig));  // 清除信号信息临时链表
#endif
        OsInsertTCBToFreeList(taskCB);  // 将任务控制块插入空闲链表
        SCHEDULER_UNLOCK(intSave);  // 开启调度器
    }
    return;  // 返回
}

/**
 * @brief 将回收链表中的任务控制块释放到空闲链表
 *
 * @details 该函数遍历任务回收链表，释放每个任务控制块的资源，并将其插入空闲链表
 *          用于延迟释放已退出任务的资源，避免在中断上下文中进行内存操作
 */
LITE_OS_SEC_TEXT VOID OsTaskCBRecycleToFree(void)
{
    UINT32 intSave;  // 中断状态保存变量

    SCHEDULER_LOCK(intSave);  // 关闭调度器
    while (!LOS_ListEmpty(&g_taskRecycleList)) {  // 遍历回收链表
        LosTaskCB *taskCB = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&g_taskRecycleList));  // 获取第一个任务控制块
        LOS_ListDelete(&taskCB->pendList);  // 从回收链表中删除
        SCHEDULER_UNLOCK(intSave);  // 开启调度器

        OsTaskResourcesToFree(taskCB);  // 释放任务资源

        SCHEDULER_LOCK(intSave);  // 关闭调度器
    }
    SCHEDULER_UNLOCK(intSave);  // 开启调度器
}

/*
 * Description : 所有任务入口函数
 * Input       : taskID     --- 要运行的任务ID
 */
LITE_OS_SEC_TEXT_INIT VOID OsTaskEntry(UINT32 taskID)
{
    LOS_ASSERT(!OS_TID_CHECK_INVALID(taskID));  // 断言任务ID有效

    /*
     * 任务调度器需要在整个过程中受到保护，防止中断和其他核心的干扰
     * 在任务入口处按顺序释放任务自旋锁并启用中断
     */
    LOS_SpinUnlock(&g_taskSpin);  // 释放任务自旋锁
    (VOID)LOS_IntUnLock();  // 启用中断

    LosTaskCB *taskCB = OS_TCB_FROM_TID(taskID);  // 获取任务控制块
    taskCB->joinRetval = taskCB->taskEntry(taskCB->args[0], taskCB->args[1],
                                           taskCB->args[2], taskCB->args[3]); /* 2和3: 仅用于参数数组索引 */
    if (!(taskCB->taskStatus & OS_TASK_FLAG_PTHREAD_JOIN)) {  // 检查任务是否不可join
        taskCB->joinRetval = 0;  // 重置join返回值
    }

    OsRunningTaskToExit(taskCB, 0);  // 处理任务退出
}

/**
 * @brief 任务创建参数检查
 *
 * @details 该函数检查任务创建参数的有效性，包括任务ID指针、初始化参数、任务入口、优先级和堆栈大小等
 *
 * @param taskID - 任务ID指针
 * @param initParam - 任务初始化参数
 * @return UINT32 - 操作结果，LOS_OK表示参数有效，其他值表示参数无效
 */
STATIC UINT32 TaskCreateParamCheck(const UINT32 *taskID, TSK_INIT_PARAM_S *initParam)
{
    UINT32 poolSize = OS_SYS_MEM_SIZE;  // 获取系统内存池大小

    if (taskID == NULL) {  // 检查任务ID指针是否为空
        return LOS_ERRNO_TSK_ID_INVALID;  // 返回无效任务ID错误
    }

    if (initParam == NULL) {  // 检查初始化参数是否为空
        return LOS_ERRNO_TSK_PTR_NULL;  // 返回空指针错误
    }

    if (!OsProcessIsUserMode((LosProcessCB *)initParam->processID)) {  // 检查是否为内核模式进程
        if (initParam->pcName == NULL) {  // 检查任务名称是否为空
            return LOS_ERRNO_TSK_NAME_EMPTY;  // 返回任务名称为空错误
        }
    }

    if (initParam->pfnTaskEntry == NULL) {  // 检查任务入口函数是否为空
        return LOS_ERRNO_TSK_ENTRY_NULL;  // 返回入口函数为空错误
    }

    if (initParam->usTaskPrio > OS_TASK_PRIORITY_LOWEST) {  // 检查优先级是否超出范围
        return LOS_ERRNO_TSK_PRIOR_ERROR;  // 返回优先级错误
    }

    if (initParam->uwStackSize > poolSize) {  // 检查堆栈大小是否超过内存池大小
        return LOS_ERRNO_TSK_STKSZ_TOO_LARGE;  // 返回堆栈过大错误
    }

    if (initParam->uwStackSize == 0) {  // 检查堆栈大小是否为0
        initParam->uwStackSize = LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE;  // 设置默认堆栈大小
    }
    initParam->uwStackSize = (UINT32)ALIGN(initParam->uwStackSize, LOSCFG_STACK_POINT_ALIGN_SIZE);  // 堆栈大小按要求对齐

    if (initParam->uwStackSize < LOS_TASK_MIN_STACK_SIZE) {  // 检查堆栈大小是否小于最小值
        return LOS_ERRNO_TSK_STKSZ_TOO_SMALL;  // 返回堆栈过小错误
    }

    return LOS_OK;  // 返回参数有效
}

/**
 * @brief 任务控制块反初始化
 *
 * @details 该函数释放任务控制块相关资源，包括同步信号量和堆栈内存，并将任务控制块插回空闲链表
 *
 * @param taskCB - 任务控制块指针
 */
STATIC VOID TaskCBDeInit(LosTaskCB *taskCB)
{
    UINT32 intSave;  // 中断状态保存变量
#ifdef LOSCFG_KERNEL_SMP_TASK_SYNC
    if (taskCB->syncSignal != OS_INVALID_VALUE) {  // 检查同步信号量是否有效
        OsTaskSyncDestroy(taskCB->syncSignal);  // 销毁同步信号量
        taskCB->syncSignal = OS_INVALID_VALUE;  // 重置同步信号量ID
    }
#endif

    if (taskCB->topOfStack != (UINTPTR)NULL) {  // 检查堆栈地址是否有效
        (VOID)LOS_MemFree(m_aucSysMem1, (VOID *)taskCB->topOfStack);  // 释放堆栈内存
        taskCB->topOfStack = (UINTPTR)NULL;  // 重置堆栈顶部地址
    }

    SCHEDULER_LOCK(intSave);  // 关闭调度器
    LosProcessCB *processCB = OS_PCB_FROM_TCB(taskCB);  // 获取进程控制块
    if (processCB != OsGetDefaultProcessCB()) {  // 检查是否为默认进程
        LOS_ListDelete(&taskCB->threadList);  // 从线程链表中删除
        processCB->threadNumber--;  // 减少线程数量
        processCB->threadCount--;  // 减少线程计数
    }

    OsInsertTCBToFreeList(taskCB);  // 将任务控制块插入空闲链表
    SCHEDULER_UNLOCK(intSave);  // 开启调度器
}

/**
 * @brief 任务控制块基础初始化
 *
 * @details 该函数初始化任务控制块的基础字段，包括参数、堆栈信息、入口函数和状态等
 *
 * @param taskCB - 任务控制块指针
 * @param initParam - 任务初始化参数
 */
STATIC VOID TaskCBBaseInit(LosTaskCB *taskCB, const TSK_INIT_PARAM_S *initParam)
{
    taskCB->stackPointer = NULL;  // 初始化堆栈指针
    taskCB->args[0]      = initParam->auwArgs[0]; /* 0~3: 仅用于参数数组索引 */
    taskCB->args[1]      = initParam->auwArgs[1];
    taskCB->args[2]      = initParam->auwArgs[2];
    taskCB->args[3]      = initParam->auwArgs[3];
    taskCB->topOfStack   = (UINTPTR)NULL;  // 初始化堆栈顶部地址
    taskCB->stackSize    = initParam->uwStackSize;  // 设置堆栈大小
    taskCB->taskEntry    = initParam->pfnTaskEntry;  // 设置任务入口函数
    taskCB->signal       = SIGNAL_NONE;  // 初始化信号
#ifdef LOSCFG_KERNEL_SMP_TASK_SYNC
    taskCB->syncSignal   = OS_INVALID_VALUE;  // 初始化同步信号量ID
#endif
#ifdef LOSCFG_KERNEL_SMP
    taskCB->currCpu      = OS_TASK_INVALID_CPUID;  // 初始化当前CPU ID
    taskCB->cpuAffiMask  = (initParam->usCpuAffiMask) ?
                            initParam->usCpuAffiMask : LOSCFG_KERNEL_CPU_MASK;  // 设置CPU亲和性掩码
#endif
    taskCB->taskStatus = OS_TASK_STATUS_INIT;  // 设置任务状态为初始化
    if (initParam->uwResved & LOS_TASK_ATTR_JOINABLE) {  // 检查是否设置可join属性
        taskCB->taskStatus |= OS_TASK_FLAG_PTHREAD_JOIN;  // 设置可join标志
        LOS_ListInit(&taskCB->joinList);  // 初始化join链表
    }

    LOS_ListInit(&taskCB->lockList);  // 初始化锁链表
    SET_SORTLIST_VALUE(&taskCB->sortList, OS_SORT_LINK_INVALID_TIME);  // 设置排序链表时间
#ifdef LOSCFG_KERNEL_VM
    taskCB->futex.index = OS_INVALID_VALUE;  // 初始化futex索引
#endif
}

/**
 * @brief 任务控制块初始化
 *
 * @details 该函数完成任务控制块的完整初始化，包括基础信息、调度参数和任务名称等
 *          如果未指定任务名称，则自动生成以"thread"为前缀的名称
 *
 * @param taskCB - 任务控制块指针
 * @param initParam - 任务初始化参数
 * @return UINT32 - 操作结果，LOS_OK表示成功，其他值表示失败
 */
STATIC UINT32 TaskCBInit(LosTaskCB *taskCB, const TSK_INIT_PARAM_S *initParam)
{
    UINT32 ret;
    UINT32 numCount;
    SchedParam schedParam = { 0 };
    LosSchedParam initSchedParam = {0};
    UINT16 policy = (initParam->policy == LOS_SCHED_NORMAL) ? LOS_SCHED_RR : initParam->policy;  // 确定调度策略

    TaskCBBaseInit(taskCB, initParam);  // 基础初始化

    schedParam.policy = policy;  // 设置调度策略
    ret = OsProcessAddNewTask(initParam->processID, taskCB, &schedParam, &numCount);  // 将任务添加到进程
    if (ret != LOS_OK) {  // 检查添加是否成功
        return ret;  // 返回错误
    }

    if (policy == LOS_SCHED_DEADLINE) {  // 检查是否为截止期限调度策略
        initSchedParam.runTimeUs = initParam->runTimeUs;  // 设置运行时间
        initSchedParam.deadlineUs = initParam->deadlineUs;  // 设置截止期限
        initSchedParam.periodUs = initParam->periodUs;  // 设置周期
    } else {
        initSchedParam.priority = initParam->usTaskPrio;  // 设置优先级
    }
    ret = OsSchedParamInit(taskCB, policy, &schedParam, &initSchedParam);  // 初始化调度参数
    if (ret != LOS_OK) {  // 检查初始化是否成功
        return ret;  // 返回错误
    }

    if (initParam->pcName != NULL) {  // 检查是否指定任务名称
        ret = (UINT32)OsSetTaskName(taskCB, initParam->pcName, FALSE);  // 设置任务名称
        if (ret == LOS_OK) {  // 检查设置是否成功
            return LOS_OK;  // 返回成功
        }
    }

    if (snprintf_s(taskCB->taskName, OS_TCB_NAME_LEN, OS_TCB_NAME_LEN - 1, "thread%u", numCount) < 0) {  // 生成默认任务名称
        return LOS_NOK;  // 返回失败
    }
    return LOS_OK;  // 返回成功
}

/**
 * @brief 任务堆栈初始化
 *
 * @details 该函数为任务分配并初始化堆栈内存，设置堆栈指针
 *          对于用户模式任务，还会初始化用户空间堆栈信息
 *
 * @param taskCB - 任务控制块指针
 * @param initParam - 任务初始化参数
 * @return UINT32 - 操作结果，LOS_OK表示成功，其他值表示失败
 */
STATIC UINT32 TaskStackInit(LosTaskCB *taskCB, const TSK_INIT_PARAM_S *initParam)
{
    VOID *topStack = (VOID *)LOS_MemAllocAlign(m_aucSysMem1, initParam->uwStackSize, LOSCFG_STACK_POINT_ALIGN_SIZE);  // 分配对齐的堆栈内存
    if (topStack == NULL) {  // 检查内存分配是否成功
        return LOS_ERRNO_TSK_NO_MEMORY;  // 返回内存不足错误
    }

    taskCB->topOfStack = (UINTPTR)topStack;  // 设置堆栈顶部地址
    taskCB->stackPointer = OsTaskStackInit(taskCB->taskID, initParam->uwStackSize, topStack, TRUE);  // 初始化堆栈指针
#ifdef LOSCFG_KERNEL_VM
    if (taskCB->taskStatus & OS_TASK_FLAG_USER_MODE) {  // 检查是否为用户模式任务
        taskCB->userArea = initParam->userParam.userArea;  // 设置用户区域
        taskCB->userMapBase = initParam->userParam.userMapBase;  // 设置用户映射基地址
        taskCB->userMapSize = initParam->userParam.userMapSize;  // 设置用户映射大小
        OsUserTaskStackInit(taskCB->stackPointer, (UINTPTR)taskCB->taskEntry, initParam->userParam.userSP);  // 初始化用户任务堆栈
    }
#endif
    return LOS_OK;  // 返回成功
}

/**
 * @brief 获取空闲任务控制块
 *
 * @details 该函数从空闲任务链表中获取一个未使用的任务控制块
 *
 * @return LosTaskCB* - 空闲任务控制块指针，NULL表示无可用任务控制块
 */
STATIC LosTaskCB *GetFreeTaskCB(VOID)
{
    UINT32 intSave;  // 中断状态保存变量

    SCHEDULER_LOCK(intSave);  // 关闭调度器
    if (LOS_ListEmpty(&g_losFreeTask)) {  // 检查空闲链表是否为空
        SCHEDULER_UNLOCK(intSave);  // 开启调度器
        PRINT_ERR("No idle TCB in the system!\n");  // 打印错误信息
        return NULL;  // 返回空指针
    }

    LosTaskCB *taskCB = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&g_losFreeTask));  // 获取第一个空闲任务控制块
    LOS_ListDelete(LOS_DL_LIST_FIRST(&g_losFreeTask));  // 从空闲链表中删除
    SCHEDULER_UNLOCK(intSave);  // 开启调度器

    return taskCB;  // 返回任务控制块指针
}

/**
 * @brief 创建任务但不启动
 *
 * @details 该函数创建任务控制块并初始化，但不将任务加入调度队列
 *          需调用LOS_TaskResume函数启动任务
 *
 * @param taskID - 输出参数，返回创建的任务ID
 * @param initParam - 任务初始化参数
 * @return UINT32 - 操作结果，LOS_OK表示成功，其他值表示失败
 */
LITE_OS_SEC_TEXT_INIT UINT32 LOS_TaskCreateOnly(UINT32 *taskID, TSK_INIT_PARAM_S *initParam)
{
    UINT32 errRet = TaskCreateParamCheck(taskID, initParam);  // 检查任务创建参数
    if (errRet != LOS_OK) {  // 检查参数是否有效
        return errRet;  // 返回错误
    }

    LosTaskCB *taskCB = GetFreeTaskCB();  // 获取空闲任务控制块
    if (taskCB == NULL) {  // 检查是否获取到任务控制块
        return LOS_ERRNO_TSK_TCB_UNAVAILABLE;  // 返回任务控制块不可用错误
    }

    errRet = TaskCBInit(taskCB, initParam);  // 初始化任务控制块
    if (errRet != LOS_OK) {  // 检查初始化是否成功
        goto DEINIT_TCB;  // 跳转到反初始化
    }

    errRet = TaskSyncCreate(taskCB);  // 创建同步信号量
    if (errRet != LOS_OK) {  // 检查同步信号量创建是否成功
        goto DEINIT_TCB;  // 跳转到反初始化
    }

    errRet = TaskStackInit(taskCB, initParam);  // 初始化任务堆栈
    if (errRet != LOS_OK) {  // 检查堆栈初始化是否成功
        goto DEINIT_TCB;  // 跳转到反初始化
    }

    if (OsConsoleIDSetHook != NULL) {  // 检查控制台ID设置钩子是否存在
        OsConsoleIDSetHook(taskCB->taskID, OsCurrTaskGet()->taskID);  // 调用钩子函数
    }

    *taskID = taskCB->taskID;  // 设置任务ID
    OsHookCall(LOS_HOOK_TYPE_TASK_CREATE, taskCB);  // 调用任务创建钩子
    return LOS_OK;  // 返回成功

DEINIT_TCB:
    TaskCBDeInit(taskCB);  // 反初始化任务控制块
    return errRet;  // 返回错误
}

/**
 * @brief 创建并启动任务
 *
 * @details 该函数创建任务控制块、初始化资源，并将任务加入调度队列使其可被调度执行
 *          对于用户模式进程，任务将归属内核初始化进程
 *
 * @param taskID - 输出参数，返回创建的任务ID
 * @param initParam - 任务初始化参数
 * @return UINT32 - 操作结果，LOS_OK表示成功，其他值表示失败
 */
LITE_OS_SEC_TEXT_INIT UINT32 LOS_TaskCreate(UINT32 *taskID, TSK_INIT_PARAM_S *initParam)
{
    UINT32 ret;
    UINT32 intSave;  // 中断状态保存变量

    if (initParam == NULL) {  // 检查初始化参数是否为空
        return LOS_ERRNO_TSK_PTR_NULL;  // 返回空指针错误
    }

    if (OS_INT_ACTIVE) {  // 检查是否在中断上下文中
        return LOS_ERRNO_TSK_YIELD_IN_INT;  // 返回中断中不允许错误
    }

    if (OsProcessIsUserMode(OsCurrProcessGet())) {  // 检查当前进程是否为用户模式
        initParam->processID = (UINTPTR)OsGetKernelInitProcess();  // 设置进程ID为内核初始化进程
    } else {
        initParam->processID = (UINTPTR)OsCurrProcessGet();  // 设置进程ID为当前进程
    }

    ret = LOS_TaskCreateOnly(taskID, initParam);  // 创建任务但不启动
    if (ret != LOS_OK) {  // 检查任务创建是否成功
        return ret;  // 返回错误
    }

    LosTaskCB *taskCB = OS_TCB_FROM_TID(*taskID);  // 获取任务控制块

    SCHEDULER_LOCK(intSave);  // 关闭调度器
    taskCB->ops->enqueue(OsSchedRunqueue(), taskCB);  // 将任务加入调度队列
    SCHEDULER_UNLOCK(intSave);  // 开启调度器

    /* 若创建的任务不在当前核心运行，是否调度取决于其他调度器状态 */
    LOS_MpSchedule(OS_MP_CPU_ALL);  // 多处理器调度
    if (OS_SCHEDULER_ACTIVE) {  // 检查调度器是否激活
        LOS_Schedule();  // 执行调度
    }

    return LOS_OK;  // 返回成功
}

/**
 * @brief 恢复任务运行
 *
 * @details 该函数恢复被挂起的任务，将其加入调度队列使其可被调度执行
 *          如果需要，将触发调度器进行任务切换
 *
 * @param taskID - 任务ID
 * @return UINT32 - 操作结果，LOS_OK表示成功，其他值表示失败
 */
LITE_OS_SEC_TEXT_INIT UINT32 LOS_TaskResume(UINT32 taskID)
{
    UINT32 intSave;  // 中断状态保存变量
    UINT32 errRet;
    BOOL needSched = FALSE;  // 是否需要调度标志

    if (OS_TID_CHECK_INVALID(taskID)) {  // 检查任务ID是否有效
        return LOS_ERRNO_TSK_ID_INVALID;  // 返回无效任务ID错误
    }

    LosTaskCB *taskCB = OS_TCB_FROM_TID(taskID);  // 获取任务控制块
    SCHEDULER_LOCK(intSave);  // 关闭调度器

    /* 清除挂起信号 */
    taskCB->signal &= ~SIGNAL_SUSPEND;  // 清除挂起信号

    if (taskCB->taskStatus & OS_TASK_STATUS_UNUSED) {  // 检查任务是否未创建
        errRet = LOS_ERRNO_TSK_NOT_CREATED;  // 设置任务未创建错误
        OS_GOTO_ERREND();  // 跳转到错误处理
    } else if (!(taskCB->taskStatus & OS_TASK_STATUS_SUSPENDED)) {  // 检查任务是否未挂起
        errRet = LOS_ERRNO_TSK_NOT_SUSPENDED;  // 设置任务未挂起错误
        OS_GOTO_ERREND();  // 跳转到错误处理
    }

    errRet = taskCB->ops->resume(taskCB, &needSched);  // 恢复任务
    SCHEDULER_UNLOCK(intSave);  // 开启调度器

    LOS_MpSchedule(OS_MP_CPU_ALL);  // 多处理器调度
    if (OS_SCHEDULER_ACTIVE && needSched) {  // 检查调度器是否激活且需要调度
        LOS_Schedule();  // 执行调度
    }

    return errRet;  // 返回结果

LOS_ERREND:
    SCHEDULER_UNLOCK(intSave);  // 开启调度器
    return errRet;  // 返回错误
}

/*
 * 检查是否需要对运行中的任务执行挂起操作
 * 如果需要执行挂起，返回TRUE
 * 如果满足以下情况，返回FALSE，LOS_TaskSuspend将直接返回'ret'值：
 * 1. 启用SMP时跨核心挂起
 * 2. 禁止抢占时挂起
 * 3. 在硬中断中挂起
 */
LITE_OS_SEC_TEXT_INIT STATIC BOOL OsTaskSuspendCheckOnRun(LosTaskCB *taskCB, UINT32 *ret)
{
    /* 初始化输出返回值 */
    *ret = LOS_OK;

#ifdef LOSCFG_KERNEL_SMP
    /* 异步操作，无需进行任务锁检查 */
    if (taskCB->currCpu != ArchCurrCpuid()) {  // 检查任务是否在当前核心运行
        taskCB->signal = SIGNAL_SUSPEND;  // 设置挂起信号
        LOS_MpSchedule(taskCB->currCpu);  // 多处理器调度
        return FALSE;  // 返回不需要挂起
    }
#endif

    if (!OsPreemptableInSched()) {  // 检查是否允许抢占
        /* 挂起当前核心的运行任务 */
        *ret = LOS_ERRNO_TSK_SUSPEND_LOCKED;  // 设置挂起锁定错误
        return FALSE;  // 返回不需要挂起
    }

    if (OS_INT_ACTIVE) {  // 检查是否在中断上下文中
        /* 在中断中挂起运行任务 */
        taskCB->signal = SIGNAL_SUSPEND;  // 设置挂起信号
        return FALSE;  // 返回不需要挂起
    }

    return TRUE;  // 返回需要挂起
}

/**
 * @brief 挂起指定任务
 * @details 检查任务状态并执行挂起操作，处理不同状态下的任务挂起逻辑
 * @param taskCB 任务控制块指针
 * @return 操作结果
 * @retval LOS_OK 挂起成功
 * @retval LOS_ERRNO_TSK_NOT_CREATED 任务未创建
 * @retval LOS_ERRNO_TSK_ALREADY_SUSPENDED 任务已挂起
 */
LITE_OS_SEC_TEXT STATIC UINT32 OsTaskSuspend(LosTaskCB *taskCB)
{
    UINT32 errRet;
    UINT16 tempStatus = taskCB->taskStatus;  // 保存任务状态
    if (tempStatus & OS_TASK_STATUS_UNUSED) {  // 检查任务是否未使用
        return LOS_ERRNO_TSK_NOT_CREATED;  // 返回任务未创建错误
    }

    if (tempStatus & OS_TASK_STATUS_SUSPENDED) {
        return LOS_ERRNO_TSK_ALREADY_SUSPENDED;  // 任务已挂起，返回错误
    }

    if ((tempStatus & OS_TASK_STATUS_RUNNING) &&
        !OsTaskSuspendCheckOnRun(taskCB, &errRet)) {  // 检查运行中任务是否可挂起
        return errRet;  // 返回检查结果
    }

    return taskCB->ops->suspend(taskCB);  // 调用具体架构的挂起实现
}

/**
 * @brief 挂起指定ID的任务（外部API）
 * @details 提供给用户的任务挂起接口，包含参数检查和调度器锁保护
 * @param taskID 任务ID
 * @return 操作结果
 * @retval LOS_OK 挂起成功
 * @retval LOS_ERRNO_TSK_ID_INVALID 任务ID无效
 * @retval LOS_ERRNO_TSK_OPERATE_SYSTEM_TASK 不允许操作系统任务
 */
LITE_OS_SEC_TEXT_INIT UINT32 LOS_TaskSuspend(UINT32 taskID)
{
    UINT32 intSave;
    UINT32 errRet;

    if (OS_TID_CHECK_INVALID(taskID)) {  // 检查任务ID是否有效
        return LOS_ERRNO_TSK_ID_INVALID;  // 返回无效ID错误
    }

    LosTaskCB *taskCB = OS_TCB_FROM_TID(taskID);  // 通过任务ID获取任务控制块
    if (taskCB->taskStatus & OS_TASK_FLAG_SYSTEM_TASK) {  // 检查是否为系统任务
        return LOS_ERRNO_TSK_OPERATE_SYSTEM_TASK;  // 系统任务不允许挂起，返回错误
    }

    SCHEDULER_LOCK(intSave);  // 调度器加锁，保护任务操作
    errRet = OsTaskSuspend(taskCB);  // 调用内部挂起函数
    SCHEDULER_UNLOCK(intSave);  // 调度器解锁
    return errRet;  // 返回操作结果
}

/**
 * @brief 设置任务状态为未使用
 * @details 将任务控制块状态标记为未使用，并清除相关信息
 * @param taskCB 任务控制块指针
 */
STATIC INLINE VOID OsTaskStatusUnusedSet(LosTaskCB *taskCB)
{
    taskCB->taskStatus |= OS_TASK_STATUS_UNUSED;  // 设置未使用状态位
    taskCB->eventMask = 0;  // 清除事件掩码

    OS_MEM_CLEAR(taskCB->taskID);  // 清除任务ID
}

/**
 * @brief 释放任务持有的锁资源
 * @details 遍历并释放任务持有的所有互斥锁，清理相关同步资源
 * @param taskCB 任务控制块指针
 */
STATIC VOID OsTaskReleaseHoldLock(LosTaskCB *taskCB)
{
    LosMux *mux = NULL;
    UINT32 ret;

    // 循环释放任务持有的所有互斥锁
    while (!LOS_ListEmpty(&taskCB->lockList)) {
        mux = LOS_DL_LIST_ENTRY(LOS_DL_LIST_FIRST(&taskCB->lockList), LosMux, holdList);
        ret = OsMuxUnlockUnsafe(taskCB, mux, NULL);  // 不安全方式解锁（已在临界区）
        if (ret != LOS_OK) {  // 解锁失败处理
            LOS_ListDelete(&mux->holdList);  // 从持有列表中删除
            PRINT_ERR("mux ulock failed! : %u\n", ret);  // 打印错误信息
        }
    }

#ifdef LOSCFG_KERNEL_VM  // 虚拟内存配置下的特殊处理
    if (taskCB->taskStatus & OS_TASK_FLAG_USER_MODE) {  // 检查是否为用户模式任务
        OsFutexNodeDeleteFromFutexHash(&taskCB->futex, TRUE, NULL, NULL);  // 删除futex节点
    }
#endif

    OsTaskJoinPostUnsafe(taskCB);  // 处理任务连接相关清理

    OsTaskSyncWake(taskCB);  // 唤醒等待该任务的同步对象
}

/**
 * @brief 运行中任务退出处理
 * @details 处理正在运行的任务退出逻辑，包括资源释放和进程清理
 * @param runTask 当前运行的任务控制块
 * @param status 退出状态码
 */
LITE_OS_SEC_TEXT VOID OsRunningTaskToExit(LosTaskCB *runTask, UINT32 status)
{
    UINT32 intSave;

    if (OsIsProcessThreadGroup(runTask)) {  // 检查是否为进程的主线程
        OsProcessThreadGroupDestroy();  // 销毁进程线程组
    }

    OsHookCall(LOS_HOOK_TYPE_TASK_DELETE, runTask);  // 调用任务删除钩子函数

    SCHEDULER_LOCK(intSave);  // 调度器加锁
    // 判断是否为进程的最后一个任务
    if (OsProcessThreadNumberGet(runTask) == 1) { /* 1: The last task of the process exits */
        SCHEDULER_UNLOCK(intSave);  // 临时解锁以执行资源释放

        OsTaskResourcesToFree(runTask);  // 释放任务资源
        OsProcessResourcesToFree(OS_PCB_FROM_TCB(runTask));  // 释放进程资源

        SCHEDULER_LOCK(intSave);  // 重新加锁

        OsProcessNaturalExit(OS_PCB_FROM_TCB(runTask), status);  // 进程正常退出
        OsTaskReleaseHoldLock(runTask);  // 释放任务持有的锁
        OsTaskStatusUnusedSet(runTask);  // 设置任务为未使用状态
    } else if (runTask->taskStatus & OS_TASK_FLAG_PTHREAD_JOIN) {  // 检查是否有线程等待连接
        OsTaskReleaseHoldLock(runTask);  // 释放持有的锁
    } else {
        SCHEDULER_UNLOCK(intSave);  // 临时解锁以执行资源释放

        OsTaskResourcesToFree(runTask);  // 释放任务资源

        SCHEDULER_LOCK(intSave);  // 重新加锁
        OsInactiveTaskDelete(runTask);  // 删除非活动任务
        // 写入资源释放事件
        OsEventWriteUnsafe(&g_resourceEvent, OS_RESOURCE_EVENT_FREE, FALSE, NULL);
    }

    OsSchedResched();  // 触发调度
    SCHEDULER_UNLOCK(intSave);  // 调度器解锁
    return;
}

/**
 * @brief 非活动任务删除
 * @details 处理非运行状态任务的删除逻辑，包括资源清理和状态重置
 * @param taskCB 任务控制块指针
 */
LITE_OS_SEC_TEXT VOID OsInactiveTaskDelete(LosTaskCB *taskCB)
{
    UINT16 taskStatus = taskCB->taskStatus;  // 保存当前任务状态

    OsTaskReleaseHoldLock(taskCB);  // 释放任务持有的锁

    taskCB->ops->exit(taskCB);  // 调用具体架构的任务退出实现
    if (taskStatus & OS_TASK_STATUS_PENDING) {  // 检查任务是否处于等待状态
        LosMux *mux = (LosMux *)taskCB->taskMux;  // 获取任务等待的互斥锁
        if (LOS_MuxIsValid(mux) == TRUE) {  // 检查互斥锁是否有效
            OsMuxBitmapRestore(mux, NULL, taskCB);  // 恢复互斥锁位图
        }
    }

    OsTaskStatusUnusedSet(taskCB);  // 设置任务为未使用状态

    OsDeleteTaskFromProcess(taskCB);  // 从进程中删除任务

    OsHookCall(LOS_HOOK_TYPE_TASK_DELETE, taskCB);  // 调用任务删除钩子函数
}

/**
 * @brief 删除指定ID的任务（外部API）
 * @details 提供给用户的任务删除接口，处理不同状态任务的删除逻辑
 * @param taskID 任务ID
 * @return 操作结果
 * @retval LOS_OK 删除成功
 * @retval LOS_ERRNO_TSK_ID_INVALID 任务ID无效
 * @retval LOS_ERRNO_TSK_YIELD_IN_INT 中断中不允许删除
 * @retval LOS_ERRNO_TSK_DELETE_LOCKED 任务处于锁定状态，不允许删除
 */
LITE_OS_SEC_TEXT_INIT UINT32 LOS_TaskDelete(UINT32 taskID)
{
    UINT32 intSave;
    UINT32 ret = LOS_OK;

    if (OS_TID_CHECK_INVALID(taskID)) {  // 检查任务ID是否有效
        return LOS_ERRNO_TSK_ID_INVALID;  // 返回无效ID错误
    }

    if (OS_INT_ACTIVE) {  // 检查是否在中断上下文中
        return LOS_ERRNO_TSK_YIELD_IN_INT;  // 中断中不允许删除任务
    }

    LosTaskCB *taskCB = OS_TCB_FROM_TID(taskID);  // 通过任务ID获取任务控制块
    if (taskCB == OsCurrTaskGet()) {  // 检查是否为当前运行任务
        if (!OsPreemptable()) {  // 检查是否可抢占
            return LOS_ERRNO_TSK_DELETE_LOCKED;  // 不可抢占状态下不允许删除自身
        }

        OsRunningTaskToExit(taskCB, OS_PRO_EXIT_OK);  // 处理当前任务退出
        return LOS_NOK;  // 任务已退出，返回特殊值
    }

    SCHEDULER_LOCK(intSave);  // 调度器加锁
    if (OsTaskIsNotDelete(taskCB)) {  // 检查任务是否不可删除
        if (OsTaskIsUnused(taskCB)) {  // 检查任务是否未使用
            ret = LOS_ERRNO_TSK_NOT_CREATED;  // 返回任务未创建错误
        } else {
            ret = LOS_ERRNO_TSK_OPERATE_SYSTEM_TASK;  // 返回系统任务操作错误
        }
        OS_GOTO_ERREND();  // 跳转到错误处理
    }

#ifdef LOSCFG_KERNEL_SMP  // SMP多核配置下的处理
    if (taskCB->taskStatus & OS_TASK_STATUS_RUNNING) {  // 检查任务是否在其他核运行
        taskCB->signal = SIGNAL_KILL;  // 设置终止信号
        LOS_MpSchedule(taskCB->currCpu);  // 调度到任务所在CPU处理
        ret = OsTaskSyncWait(taskCB);  // 等待同步完成
        OS_GOTO_ERREND();  // 跳转到错误处理
    }
#endif

    OsInactiveTaskDelete(taskCB);  // 删除非活动任务
    // 写入资源释放事件
    OsEventWriteUnsafe(&g_resourceEvent, OS_RESOURCE_EVENT_FREE, FALSE, NULL);

LOS_ERREND:  // 错误处理标签
    SCHEDULER_UNLOCK(intSave);  // 调度器解锁
    if (ret == LOS_OK) {  // 如果操作成功
        LOS_Schedule();  // 触发调度
    }
    return ret;  // 返回操作结果
}

/**
 * @brief 任务延迟（外部API）
 * @details 使当前任务延迟指定的时钟滴答数，进入阻塞状态
 * @param tick 延迟的时钟滴答数，0表示立即让出CPU
 * @return 操作结果
 * @retval LOS_OK 延迟成功
 * @retval LOS_ERRNO_TSK_DELAY_IN_INT 中断中不允许延迟
 * @retval LOS_ERRNO_TSK_OPERATE_SYSTEM_TASK 不允许延迟系统任务
 * @retval LOS_ERRNO_TSK_DELAY_IN_LOCK 锁定状态下不允许延迟
 */
LITE_OS_SEC_TEXT UINT32 LOS_TaskDelay(UINT32 tick)
{
    UINT32 intSave;

    if (OS_INT_ACTIVE) {  // 检查是否在中断上下文中
        PRINT_ERR("In interrupt not allow delay task!\n");  // 打印错误信息
        return LOS_ERRNO_TSK_DELAY_IN_INT;  // 返回中断中延迟错误
    }

    LosTaskCB *runTask = OsCurrTaskGet();  // 获取当前运行任务
    if (runTask->taskStatus & OS_TASK_FLAG_SYSTEM_TASK) {  // 检查是否为系统任务
        OsBackTrace();  // 打印调用栈
        return LOS_ERRNO_TSK_OPERATE_SYSTEM_TASK;  // 返回系统任务操作错误
    }

    if (!OsPreemptable()) {  // 检查是否可抢占
        return LOS_ERRNO_TSK_DELAY_IN_LOCK;  // 返回锁定状态延迟错误
    }
    OsHookCall(LOS_HOOK_TYPE_TASK_DELAY, tick);  // 调用任务延迟钩子函数
    if (tick == 0) {  // 延迟为0时，直接让出CPU
        return LOS_TaskYield();  // 调用任务让出函数
    }

    SCHEDULER_LOCK(intSave);  // 调度器加锁
    // 调用具体架构的延迟实现，转换滴答数为周期数
    UINT32 ret = runTask->ops->delay(runTask, OS_SCHED_TICK_TO_CYCLE(tick));
    // 调用任务移至延迟列表钩子函数
    OsHookCall(LOS_HOOK_TYPE_MOVEDTASKTODELAYEDLIST, runTask);
    SCHEDULER_UNLOCK(intSave);  // 调度器解锁
    return ret;  // 返回操作结果
}

/**
 * @brief 获取任务优先级（外部API）
 * @details 获取指定任务ID的当前优先级
 * @param taskID 任务ID
 * @return 任务优先级，OS_INVALID表示失败
 */
LITE_OS_SEC_TEXT_MINOR UINT16 LOS_TaskPriGet(UINT32 taskID)
{
    UINT32 intSave;
    SchedParam param = { 0 };  // 调度参数结构体

    if (OS_TID_CHECK_INVALID(taskID)) {  // 检查任务ID是否有效
        return (UINT16)OS_INVALID;  // 返回无效值
    }

    LosTaskCB *taskCB = OS_TCB_FROM_TID(taskID);  // 通过任务ID获取任务控制块
    SCHEDULER_LOCK(intSave);  // 调度器加锁
    if (taskCB->taskStatus & OS_TASK_STATUS_UNUSED) {  // 检查任务是否未使用
        SCHEDULER_UNLOCK(intSave);  // 调度器解锁
        return (UINT16)OS_INVALID;  // 返回无效值
    }

    taskCB->ops->schedParamGet(taskCB, &param);  // 获取调度参数
    SCHEDULER_UNLOCK(intSave);  // 调度器解锁
    return param.priority;  // 返回优先级
}
/**
 * @brief 设置指定任务的优先级
 * @details 该函数用于修改指定任务的调度优先级，若优先级变更导致需要重新调度，则触发调度器
 * @param taskID 任务ID
 * @param taskPrio 新的任务优先级，范围需在0~OS_TASK_PRIORITY_LOWEST之间
 * @return 操作结果
 * @retval LOS_OK 成功
 * @retval LOS_ERRNO_TSK_PRIOR_ERROR 优先级无效
 * @retval LOS_ERRNO_TSK_ID_INVALID 任务ID无效
 * @retval LOS_ERRNO_TSK_OPERATE_SYSTEM_TASK 不允许操作系统任务
 * @retval LOS_ERRNO_TSK_NOT_CREATED 任务未创建
 */
LITE_OS_SEC_TEXT_MINOR UINT32 LOS_TaskPriSet(UINT32 taskID, UINT16 taskPrio)
{
    UINT32 intSave;                     // 用于保存中断状态
    SchedParam param = { 0 };           // 调度参数结构体

    if (taskPrio > OS_TASK_PRIORITY_LOWEST) {  // 检查优先级是否超出允许范围
        return LOS_ERRNO_TSK_PRIOR_ERROR;     // 返回优先级错误
    }

    if (OS_TID_CHECK_INVALID(taskID)) {       // 检查任务ID是否有效
        return LOS_ERRNO_TSK_ID_INVALID;      // 返回任务ID无效错误
    }

    LosTaskCB *taskCB = OS_TCB_FROM_TID(taskID);  // 通过任务ID获取任务控制块指针
    if (taskCB->taskStatus & OS_TASK_FLAG_SYSTEM_TASK) {  // 检查是否为系统任务
        return LOS_ERRNO_TSK_OPERATE_SYSTEM_TASK;         // 系统任务不允许修改优先级
    }

    SCHEDULER_LOCK(intSave);            // 锁定调度器，防止调度中断
    if (taskCB->taskStatus & OS_TASK_STATUS_UNUSED) {  // 检查任务是否已创建
        SCHEDULER_UNLOCK(intSave);                    // 解锁调度器
        return LOS_ERRNO_TSK_NOT_CREATED;             // 返回任务未创建错误
    }

    taskCB->ops->schedParamGet(taskCB, &param);  // 获取当前调度参数

    param.priority = taskPrio;                   // 设置新的优先级

    BOOL needSched = taskCB->ops->schedParamModify(taskCB, &param);  // 修改调度参数，获取是否需要调度
    SCHEDULER_UNLOCK(intSave);                   // 解锁调度器

    LOS_MpSchedule(OS_MP_CPU_ALL);               // 多CPU核间调度通知
    if (needSched && OS_SCHEDULER_ACTIVE) {      // 如果需要调度且调度器激活
        LOS_Schedule();                          // 触发任务调度
    }
    return LOS_OK;                               // 返回成功
}

/**
 * @brief 设置当前任务的优先级
 * @details 便捷函数，直接使用当前任务ID调用LOS_TaskPriSet
 * @param taskPrio 新的任务优先级
 * @return 操作结果，同LOS_TaskPriSet
 */
LITE_OS_SEC_TEXT_MINOR UINT32 LOS_CurTaskPriSet(UINT16 taskPrio)
{
    return LOS_TaskPriSet(OsCurrTaskGet()->taskID, taskPrio);  // 获取当前任务ID并设置优先级
}

/**
 * @brief 任务主动让出CPU
 * @details 当前任务主动放弃CPU使用权，重置时间片并触发调度
 * @return 操作结果
 * @retval LOS_OK 成功
 * @retval LOS_ERRNO_TSK_YIELD_IN_INT 中断上下文中不允许调用
 * @retval LOS_ERRNO_TSK_YIELD_IN_LOCK 调度器锁定时不允许调用
 * @retval LOS_ERRNO_TSK_ID_INVALID 当前任务ID无效
 */
LITE_OS_SEC_TEXT_MINOR UINT32 LOS_TaskYield(VOID)
{
    UINT32 intSave;                     // 用于保存中断状态

    if (OS_INT_ACTIVE) {                // 检查是否在中断上下文中
        return LOS_ERRNO_TSK_YIELD_IN_INT;  // 中断中不允许让出CPU
    }

    if (!OsPreemptable()) {             // 检查是否可抢占
        return LOS_ERRNO_TSK_YIELD_IN_LOCK;  // 不可抢占时不允许让出CPU
    }

    LosTaskCB *runTask = OsCurrTaskGet();  // 获取当前运行任务控制块
    if (OS_TID_CHECK_INVALID(runTask->taskID)) {  // 检查任务ID是否有效
        return LOS_ERRNO_TSK_ID_INVALID;          // 返回任务ID无效错误
    }

    SCHEDULER_LOCK(intSave);            // 锁定调度器
    /* reset timeslice of yielded task */
    runTask->ops->yield(runTask);       // 调用调度操作接口，重置时间片
    SCHEDULER_UNLOCK(intSave);          // 解锁调度器
    return LOS_OK;                      // 返回成功
}

/**
 * @brief 锁定任务调度器
 * @details 禁止任务调度，通常用于保护临界区资源
 */
LITE_OS_SEC_TEXT_MINOR VOID LOS_TaskLock(VOID)
{
    UINT32 intSave;                     // 用于保存中断状态

    intSave = LOS_IntLock();            // 关闭中断
    OsSchedLock();                      // 锁定调度器
    LOS_IntRestore(intSave);            // 恢复中断
}

/**
 * @brief 解锁任务调度器
 * @details 允许任务调度，如果需要则触发调度
 */
LITE_OS_SEC_TEXT_MINOR VOID LOS_TaskUnlock(VOID)
{
    UINT32 intSave;                     // 用于保存中断状态

    intSave = LOS_IntLock();            // 关闭中断
    BOOL needSched = OsSchedUnlockResch();  // 解锁调度器并检查是否需要调度
    LOS_IntRestore(intSave);            // 恢复中断

    if (needSched) {                    // 如果需要调度
        LOS_Schedule();                 // 触发任务调度
    }
}

/**
 * @brief 获取指定任务的详细信息
 * @details 收集任务的状态、优先级、堆栈使用等信息并填充到任务信息结构体
 * @param taskID 任务ID
 * @param taskInfo 任务信息结构体指针，用于存储获取到的任务信息
 * @return 操作结果
 * @retval LOS_OK 成功
 * @retval LOS_ERRNO_TSK_PTR_NULL 结构体指针为空
 * @retval LOS_ERRNO_TSK_ID_INVALID 任务ID无效
 * @retval LOS_ERRNO_TSK_NOT_CREATED 任务未创建
 */
LITE_OS_SEC_TEXT_MINOR UINT32 LOS_TaskInfoGet(UINT32 taskID, TSK_INFO_S *taskInfo)
{
    UINT32 intSave;                     // 用于保存中断状态
    SchedParam param = { 0 };           // 调度参数结构体

    if (taskInfo == NULL) {             // 检查结构体指针是否为空
        return LOS_ERRNO_TSK_PTR_NULL;  // 返回指针为空错误
    }

    if (OS_TID_CHECK_INVALID(taskID)) { // 检查任务ID是否有效
        return LOS_ERRNO_TSK_ID_INVALID;  // 返回任务ID无效错误
    }

    LosTaskCB *taskCB = OS_TCB_FROM_TID(taskID);  // 通过任务ID获取任务控制块
    SCHEDULER_LOCK(intSave);            // 锁定调度器
    if (taskCB->taskStatus & OS_TASK_STATUS_UNUSED) {  // 检查任务是否已创建
        SCHEDULER_UNLOCK(intSave);                    // 解锁调度器
        return LOS_ERRNO_TSK_NOT_CREATED;             // 返回任务未创建错误
    }

    if (!(taskCB->taskStatus & OS_TASK_STATUS_RUNNING) || OS_INT_ACTIVE) {  // 非运行状态或中断中
        taskInfo->uwSP = (UINTPTR)taskCB->stackPointer;  // 使用任务控制块中的堆栈指针
    } else {                                                              // 运行状态
        taskInfo->uwSP = ArchSPGet();                    // 获取当前堆栈指针
    }

    taskCB->ops->schedParamGet(taskCB, &param);  // 获取调度参数
    taskInfo->usTaskStatus = taskCB->taskStatus; // 任务状态
    taskInfo->usTaskPrio = param.priority;       // 任务优先级
    taskInfo->uwStackSize = taskCB->stackSize;   // 堆栈大小
    taskInfo->uwTopOfStack = taskCB->topOfStack; // 堆栈顶部地址
    taskInfo->uwEventMask = taskCB->eventMask;   // 事件掩码
    taskInfo->taskEvent = taskCB->taskEvent;     // 任务事件
    taskInfo->pTaskMux = taskCB->taskMux;        // 任务互斥量
    taskInfo->uwTaskID = taskID;                 // 任务ID

    if (strncpy_s(taskInfo->acName, LOS_TASK_NAMELEN, taskCB->taskName, LOS_TASK_NAMELEN - 1) != EOK) {  // 复制任务名称
        PRINT_ERR("Task name copy failed!\n");  // 复制失败打印错误
    }
    taskInfo->acName[LOS_TASK_NAMELEN - 1] = '\0';  // 确保字符串结束符

    taskInfo->uwBottomOfStack = TRUNCATE(((UINTPTR)taskCB->topOfStack + taskCB->stackSize),  // 计算堆栈底部地址
                                         OS_TASK_STACK_ADDR_ALIGN);
    taskInfo->uwCurrUsed = (UINT32)(taskInfo->uwBottomOfStack - taskInfo->uwSP);  // 当前堆栈使用量

    taskInfo->bOvf = OsStackWaterLineGet((const UINTPTR *)taskInfo->uwBottomOfStack,  // 获取堆栈水位线
                                         (const UINTPTR *)taskInfo->uwTopOfStack, &taskInfo->uwPeakUsed);
    SCHEDULER_UNLOCK(intSave);          // 解锁调度器
    return LOS_OK;                      // 返回成功
}

/**
 * @brief 不安全地设置任务CPU亲和性掩码
 * @details 内部函数，不进行参数检查，直接修改任务控制块中的CPU亲和性掩码
 * @param taskID 任务ID
 * @param newCpuAffiMask 新的CPU亲和性掩码
 * @param oldCpuAffiMask 用于返回旧的CPU亲和性掩码
 * @return 是否需要调度
 * @retval TRUE 需要调度
 * @retval FALSE 不需要调度
 */
LITE_OS_SEC_TEXT BOOL OsTaskCpuAffiSetUnsafe(UINT32 taskID, UINT16 newCpuAffiMask, UINT16 *oldCpuAffiMask)
{
#ifdef LOSCFG_KERNEL_SMP  // SMP多核配置下才支持CPU亲和性
    LosTaskCB *taskCB = OS_TCB_FROM_TID(taskID);  // 获取任务控制块

    taskCB->cpuAffiMask = newCpuAffiMask;         // 设置新的CPU亲和性掩码
    *oldCpuAffiMask = CPUID_TO_AFFI_MASK(taskCB->currCpu);  // 计算旧的CPU亲和性掩码
    if (!((*oldCpuAffiMask) & newCpuAffiMask)) {  // 检查当前CPU是否在新的亲和性掩码中
        taskCB->signal = SIGNAL_AFFI;             // 设置CPU亲和性变更信号
        return TRUE;                              // 需要调度
    }
#else  // 非SMP配置下不支持CPU亲和性
    (VOID)taskID;                                // 未使用参数，避免编译警告
    (VOID)newCpuAffiMask;                        // 未使用参数，避免编译警告
    (VOID)oldCpuAffiMask;                        // 未使用参数，避免编译警告
#endif /* LOSCFG_KERNEL_SMP */
    return FALSE;                               // 不需要调度
}

/**
 * @brief 设置任务CPU亲和性
 * @details 指定任务可以在哪些CPU核心上运行，仅在SMP配置下有效
 * @param taskID 任务ID
 * @param cpuAffiMask CPU亲和性掩码，每一位代表一个CPU核心
 * @return 操作结果
 * @retval LOS_OK 成功
 * @retval LOS_ERRNO_TSK_ID_INVALID 任务ID无效
 * @retval LOS_ERRNO_TSK_CPU_AFFINITY_MASK_ERR 亲和性掩码无效
 * @retval LOS_ERRNO_TSK_NOT_CREATED 任务未创建
 */
LITE_OS_SEC_TEXT_MINOR UINT32 LOS_TaskCpuAffiSet(UINT32 taskID, UINT16 cpuAffiMask)
{
    BOOL needSched = FALSE;             // 是否需要调度的标志
    UINT32 intSave;                     // 用于保存中断状态
    UINT16 currCpuMask;                 // 当前CPU掩码

    if (OS_TID_CHECK_INVALID(taskID)) { // 检查任务ID是否有效
        return LOS_ERRNO_TSK_ID_INVALID;  // 返回任务ID无效错误
    }

    if (!(cpuAffiMask & LOSCFG_KERNEL_CPU_MASK)) {  // 检查亲和性掩码是否有效
        return LOS_ERRNO_TSK_CPU_AFFINITY_MASK_ERR;  // 返回掩码无效错误
    }

    LosTaskCB *taskCB = OS_TCB_FROM_TID(taskID);  // 获取任务控制块
    SCHEDULER_LOCK(intSave);            // 锁定调度器
    if (taskCB->taskStatus & OS_TASK_STATUS_UNUSED) {  // 检查任务是否已创建
        SCHEDULER_UNLOCK(intSave);                    // 解锁调度器
        return LOS_ERRNO_TSK_NOT_CREATED;             // 返回任务未创建错误
    }
    needSched = OsTaskCpuAffiSetUnsafe(taskID, cpuAffiMask, &currCpuMask);  // 不安全地设置CPU亲和性

    SCHEDULER_UNLOCK(intSave);          // 解锁调度器
    if (needSched && OS_SCHEDULER_ACTIVE) {  // 如果需要调度且调度器激活
        LOS_MpSchedule(currCpuMask);         // 多CPU核间调度通知
        LOS_Schedule();                      // 触发任务调度
    }

    return LOS_OK;                      // 返回成功
}

/**
 * @brief 获取任务CPU亲和性掩码
 * @details 获取指定任务的CPU亲和性配置，仅在SMP配置下有效
 * @param taskID 任务ID
 * @return CPU亲和性掩码，非SMP配置下返回1，无效时返回0
 */
LITE_OS_SEC_TEXT_MINOR UINT16 LOS_TaskCpuAffiGet(UINT32 taskID)
{
#ifdef LOSCFG_KERNEL_SMP  // SMP多核配置下才支持CPU亲和性
#define INVALID_CPU_AFFI_MASK   0       // 无效的CPU亲和性掩码值
    UINT16 cpuAffiMask;                 // CPU亲和性掩码
    UINT32 intSave;                     // 用于保存中断状态

    if (OS_TID_CHECK_INVALID(taskID)) { // 检查任务ID是否有效
        return INVALID_CPU_AFFI_MASK;   // 返回无效掩码
    }

    LosTaskCB *taskCB = OS_TCB_FROM_TID(taskID);  // 获取任务控制块
    SCHEDULER_LOCK(intSave);            // 锁定调度器
    if (taskCB->taskStatus & OS_TASK_STATUS_UNUSED) {  // 检查任务是否已创建
        SCHEDULER_UNLOCK(intSave);                    // 解锁调度器
        return INVALID_CPU_AFFI_MASK;                 // 返回无效掩码
    }

    cpuAffiMask = taskCB->cpuAffiMask;  // 获取CPU亲和性掩码
    SCHEDULER_UNLOCK(intSave);          // 解锁调度器

    return cpuAffiMask;                 // 返回亲和性掩码
#else  // 非SMP配置下不支持CPU亲和性
    (VOID)taskID;                       // 未使用参数，避免编译警告
    return 1;                           // 返回默认值1
#endif
}

/*
 * Description : Process pending signals tagged by others cores
 */
LITE_OS_SEC_TEXT_MINOR VOID OsTaskProcSignal(VOID)
{
    UINT32 ret;                         // 操作结果

    /*
     * private and uninterruptable, no protection needed.
     * while this task is always running when others cores see it,
     * so it keeps receiving signals while follow code executing.
     */
    LosTaskCB *runTask = OsCurrTaskGet();  // 获取当前运行任务控制块
    if (runTask->signal == SIGNAL_NONE) {  // 如果没有信号需要处理
        return;                            // 直接返回
    }

    if (runTask->signal & SIGNAL_KILL) {  // 处理终止信号
        /*
         * clear the signal, and do the task deletion. if the signaled task has been
         * scheduled out, then this deletion will wait until next run.
         */
        runTask->signal = SIGNAL_NONE;  // 清除信号
        ret = LOS_TaskDelete(runTask->taskID);  // 删除任务
        if (ret != LOS_OK) {  // 检查删除结果
            PRINT_ERR("Task proc signal delete task(%u) failed err:0x%x\n", runTask->taskID, ret);  // 打印错误信息
        }
    } else if (runTask->signal & SIGNAL_SUSPEND) {  // 处理挂起信号
        runTask->signal &= ~SIGNAL_SUSPEND;  // 清除挂起信号

        /* suspend killed task may fail, ignore the result */
        (VOID)LOS_TaskSuspend(runTask->taskID);  // 挂起任务，忽略返回值
#ifdef LOSCFG_KERNEL_SMP
    } else if (runTask->signal & SIGNAL_AFFI) {  // 处理CPU亲和性变更信号
        runTask->signal &= ~SIGNAL_AFFI;  // 清除亲和性信号

        /* priority queue has updated, notify the target cpu */
        LOS_MpSchedule((UINT32)runTask->cpuAffiMask);  // 通知目标CPU进行调度
#endif
    }
}

LITE_OS_SEC_TEXT INT32 OsSetTaskName(LosTaskCB *taskCB, const CHAR *name, BOOL setPName)
{
    UINT32 intSave;  // 中断状态保存变量
    errno_t err;  // 错误码
    const CHAR *namePtr = NULL;  // 任务名称指针
    CHAR nameBuff[OS_TCB_NAME_LEN] = { 0 };  // 任务名称缓冲区

    if ((taskCB == NULL) || (name == NULL)) {  // 检查参数有效性
        return EINVAL;  // 返回无效参数错误
    }

    if (LOS_IsUserAddress((VADDR_T)(UINTPTR)name)) {  // 检查是否为用户空间地址
        err = LOS_StrncpyFromUser(nameBuff, (const CHAR *)name, OS_TCB_NAME_LEN);  // 从用户空间复制名称
        if (err < 0) {  // 检查复制结果
            return -err;  // 返回错误码
        }
        namePtr = nameBuff;  // 设置名称指针为缓冲区
    } else {
        namePtr = name;  // 设置名称指针为原始地址
    }

    SCHEDULER_LOCK(intSave);  // 锁定调度器

    err = strncpy_s(taskCB->taskName, OS_TCB_NAME_LEN, (VOID *)namePtr, OS_TCB_NAME_LEN - 1);  // 复制任务名称
    if (err != EOK) {  // 检查复制结果
        err = EINVAL;  // 设置错误码
        goto EXIT;  // 跳转到退出标签
    }

    err = LOS_OK;  // 设置成功状态
    /* if thread is main thread, then set processName as taskName */
    if (OsIsProcessThreadGroup(taskCB) && (setPName == TRUE)) {  // 检查是否为主线程且需要设置进程名
        err = (INT32)OsSetProcessName(OS_PCB_FROM_TCB(taskCB), (const CHAR *)taskCB->taskName);  // 设置进程名称
        if (err != LOS_OK) {  // 检查设置结果
            err = EINVAL;  // 设置错误码
        }
    }

EXIT:
    SCHEDULER_UNLOCK(intSave);  // 解锁调度器
    return err;  // 返回结果
}

INT32 OsUserTaskOperatePermissionsCheck(const LosTaskCB *taskCB)
{
    return OsUserProcessOperatePermissionsCheck(taskCB, (UINTPTR)OsCurrProcessGet());  // 调用进程权限检查函数
}

INT32 OsUserProcessOperatePermissionsCheck(const LosTaskCB *taskCB, UINTPTR processCB)
{
    if (taskCB == NULL) {  // 检查任务控制块有效性
        return LOS_EINVAL;  // 返回无效参数错误
    }

    if (processCB == (UINTPTR)OsGetDefaultProcessCB()) {  // 检查是否为默认进程
        return LOS_EINVAL;  // 返回无效参数错误
    }

    if (taskCB->taskStatus & OS_TASK_STATUS_UNUSED) {  // 检查任务是否未使用
        return LOS_EINVAL;  // 返回无效参数错误
    }

    if (processCB != taskCB->processCB) {  // 检查进程是否匹配
        return LOS_EPERM;  // 返回权限不足错误
    }

    return LOS_OK;  // 返回成功
}

LITE_OS_SEC_TEXT_INIT STATIC UINT32 OsCreateUserTaskParamCheck(UINT32 processID, TSK_INIT_PARAM_S *param)
{
    UserTaskParam *userParam = NULL;  // 用户任务参数指针

    if (param == NULL) {  // 检查参数有效性
        return OS_INVALID_VALUE;  // 返回无效值
    }

    userParam = &param->userParam;  // 获取用户任务参数
    if ((processID == OS_INVALID_VALUE) && !LOS_IsUserAddress(userParam->userArea)) {  // 检查用户区域地址
        return OS_INVALID_VALUE;  // 返回无效值
    }

    if (!LOS_IsUserAddress((UINTPTR)param->pfnTaskEntry)) {  // 检查任务入口地址
        return OS_INVALID_VALUE;  // 返回无效值
    }

    if (userParam->userMapBase && !LOS_IsUserAddressRange(userParam->userMapBase, userParam->userMapSize)) {  // 检查用户映射区域
        return OS_INVALID_VALUE;  // 返回无效值
    }

    if (!LOS_IsUserAddress(userParam->userSP)) {  // 检查用户栈指针
        return OS_INVALID_VALUE;  // 返回无效值
    }

    return LOS_OK;  // 返回成功
}

LITE_OS_SEC_TEXT_INIT UINT32 OsCreateUserTask(UINTPTR processID, TSK_INIT_PARAM_S *initParam)
{
    UINT32 taskID;  // 任务ID
    UINT32 ret;  // 操作结果
    UINT32 intSave;  // 中断状态保存变量
    INT32 policy;  // 调度策略
    SchedParam param;  // 调度参数

    ret = OsCreateUserTaskParamCheck(processID, initParam);  // 检查用户任务参数
    if (ret != LOS_OK) {  // 检查参数检查结果
        return ret;  // 返回错误
    }

    initParam->uwStackSize = OS_USER_TASK_SYSCALL_STACK_SIZE;  // 设置栈大小
    initParam->usTaskPrio = OS_TASK_PRIORITY_LOWEST;  // 设置优先级为最低
    if (processID == OS_INVALID_VALUE) {  // 检查进程ID是否无效
        SCHEDULER_LOCK(intSave);  // 锁定调度器
        LosProcessCB *processCB = OsCurrProcessGet();  // 获取当前进程控制块
        initParam->processID = (UINTPTR)processCB;  // 设置进程ID
        initParam->consoleID = processCB->consoleID;  // 设置控制台ID
        SCHEDULER_UNLOCK(intSave);  // 解锁调度器
        ret = LOS_GetProcessScheduler(processCB->processID, &policy, NULL);  // 获取进程调度策略
        if (ret != LOS_OK) {  // 检查获取结果
            return OS_INVALID_VALUE;  // 返回无效值
        }
        initParam->policy = policy;  // 设置调度策略
        if (policy == LOS_SCHED_DEADLINE) {  // 检查是否为截止时间调度
            OsSchedProcessDefaultSchedParamGet((UINT16)policy, &param);  // 获取默认调度参数
            initParam->runTimeUs = param.runTimeUs;  // 设置运行时间
            initParam->deadlineUs = param.deadlineUs;  // 设置截止时间
            initParam->periodUs = param.periodUs;  // 设置周期
        }
    } else {
        initParam->policy = LOS_SCHED_RR;  // 设置调度策略为时间片轮转
        initParam->processID = processID;  // 设置进程ID
        initParam->consoleID = 0;  // 设置控制台ID为0
    }

    ret = LOS_TaskCreateOnly(&taskID, initParam);  // 创建任务（仅创建不调度）
    if (ret != LOS_OK) {  // 检查创建结果
        return OS_INVALID_VALUE;  // 返回无效值
    }

    return taskID;  // 返回任务ID
}

LITE_OS_SEC_TEXT INT32 LOS_GetTaskScheduler(INT32 taskID)
{
    UINT32 intSave;  // 中断状态保存变量
    INT32 policy;  // 调度策略
    SchedParam param = { 0 };  // 调度参数

    if (OS_TID_CHECK_INVALID(taskID)) {  // 检查任务ID有效性
        return -LOS_EINVAL;  // 返回无效参数错误
    }

    LosTaskCB *taskCB = OS_TCB_FROM_TID(taskID);  // 获取任务控制块
    SCHEDULER_LOCK(intSave);  // 锁定调度器
    if (taskCB->taskStatus & OS_TASK_STATUS_UNUSED) {  // 检查任务是否未使用
        policy = -LOS_EINVAL;  // 设置策略为错误值
        OS_GOTO_ERREND();  // 跳转到错误处理
    }

    taskCB->ops->schedParamGet(taskCB, &param);  // 获取调度参数
    policy = (INT32)param.policy;  // 获取调度策略

LOS_ERREND:
    SCHEDULER_UNLOCK(intSave);  // 解锁调度器
    return policy;  // 返回调度策略
}

LITE_OS_SEC_TEXT INT32 LOS_SetTaskScheduler(INT32 taskID, UINT16 policy, UINT16 priority)
{
    SchedParam param = { 0 };  // 调度参数
    UINT32 intSave;  // 中断状态保存变量

    if (OS_TID_CHECK_INVALID(taskID)) {  // 检查任务ID有效性
        return LOS_ESRCH;  // 返回任务不存在错误
    }

    if (priority > OS_TASK_PRIORITY_LOWEST) {  // 检查优先级范围
        return LOS_EINVAL;  // 返回无效参数错误
    }

    if ((policy != LOS_SCHED_FIFO) && (policy != LOS_SCHED_RR)) {  // 检查调度策略有效性
        return LOS_EINVAL;  // 返回无效参数错误
    }

    LosTaskCB *taskCB = OS_TCB_FROM_TID(taskID);  // 获取任务控制块
    if (taskCB->taskStatus & OS_TASK_FLAG_SYSTEM_TASK) {  // 检查是否为系统任务
        return LOS_EPERM;  // 返回权限不足错误
    }

    SCHEDULER_LOCK(intSave);  // 锁定调度器
    if (taskCB->taskStatus & OS_TASK_STATUS_UNUSED) {  // 检查任务是否未使用
        SCHEDULER_UNLOCK(intSave);  // 解锁调度器
        return LOS_EINVAL;  // 返回无效参数错误
    }

    taskCB->ops->schedParamGet(taskCB, &param);  // 获取当前调度参数
    param.policy = policy;  // 设置新调度策略
    param.priority = priority;  // 设置新优先级
    BOOL needSched = taskCB->ops->schedParamModify(taskCB, &param);  // 修改调度参数
    SCHEDULER_UNLOCK(intSave);  // 解锁调度器

    LOS_MpSchedule(OS_MP_CPU_ALL);  // 通知所有CPU进行调度
    if (needSched && OS_SCHEDULER_ACTIVE) {  // 检查是否需要调度且调度器激活
        LOS_Schedule();  // 触发调度
    }

    return LOS_OK;  // 返回成功
}

STATIC UINT32 OsTaskJoinCheck(UINT32 taskID)
{
    if (OS_TID_CHECK_INVALID(taskID)) {  // 检查任务ID有效性
        return LOS_EINVAL;  // 返回无效参数错误
    }

    if (OS_INT_ACTIVE) {  // 检查是否在中断上下文
        return LOS_EINTR;  // 返回中断错误
    }

    if (!OsPreemptable()) {  // 检查是否可抢占
        return LOS_EINVAL;  // 返回无效参数错误
    }

    LosTaskCB *taskCB = OS_TCB_FROM_TID(taskID);  // 获取任务控制块
    if (taskCB->taskStatus & OS_TASK_FLAG_SYSTEM_TASK) {  // 检查是否为系统任务
        return LOS_EPERM;  // 返回权限不足错误
    }

    if (taskCB == OsCurrTaskGet()) {  // 检查是否为当前任务
        return LOS_EDEADLK;  // 返回死锁错误
    }

    return LOS_OK;  // 返回成功
}

UINT32 LOS_TaskJoin(UINT32 taskID, UINTPTR *retval)
{
    UINT32 intSave;  // 中断状态保存变量
    LosTaskCB *runTask = OsCurrTaskGet();  // 获取当前运行任务
    UINT32 errRet;  // 错误返回值

    errRet = OsTaskJoinCheck(taskID);  // 检查任务连接条件
    if (errRet != LOS_OK) {  // 检查检查结果
        return errRet;  // 返回错误
    }

    LosTaskCB *taskCB = OS_TCB_FROM_TID(taskID);  // 获取目标任务控制块
    SCHEDULER_LOCK(intSave);  // 锁定调度器
    if (taskCB->taskStatus & OS_TASK_STATUS_UNUSED) {  // 检查任务是否未使用
        SCHEDULER_UNLOCK(intSave);  // 解锁调度器
        return LOS_EINVAL;  // 返回无效参数错误
    }

    if (runTask->processCB != taskCB->processCB) {  // 检查进程是否相同
        SCHEDULER_UNLOCK(intSave);  // 解锁调度器
        return LOS_EPERM;  // 返回权限不足错误
    }

    errRet = OsTaskJoinPendUnsafe(taskCB);  // 不安全地等待任务结束
    SCHEDULER_UNLOCK(intSave);  // 解锁调度器

    if (errRet == LOS_OK) {  // 检查等待结果
        LOS_Schedule();  // 触发调度

        if (retval != NULL) {  // 检查返回值指针是否有效
            *retval = (UINTPTR)taskCB->joinRetval;  // 设置返回值
        }

        (VOID)LOS_TaskDelete(taskID);  // 删除已结束的任务
        return LOS_OK;  // 返回成功
    }

    return errRet;  // 返回错误
}

UINT32 LOS_TaskDetach(UINT32 taskID)
{
    UINT32 intSave;  // 中断状态保存变量
    LosTaskCB *runTask = OsCurrTaskGet();  // 获取当前运行任务
    UINT32 errRet;  // 错误返回值

    if (OS_TID_CHECK_INVALID(taskID)) {  // 检查任务ID有效性
        return LOS_EINVAL;  // 返回无效参数错误
    }

    if (OS_INT_ACTIVE) {  // 检查是否在中断上下文
        return LOS_EINTR;  // 返回中断错误
    }

    LosTaskCB *taskCB = OS_TCB_FROM_TID(taskID);  // 获取目标任务控制块
    SCHEDULER_LOCK(intSave);  // 锁定调度器
    if (taskCB->taskStatus & OS_TASK_STATUS_UNUSED) {  // 检查任务是否未使用
        SCHEDULER_UNLOCK(intSave);  // 解锁调度器
        return LOS_EINVAL;  // 返回无效参数错误
    }

    if (runTask->processCB != taskCB->processCB) {  // 检查进程是否相同
        SCHEDULER_UNLOCK(intSave);  // 解锁调度器
        return LOS_EPERM;  // 返回权限不足错误
    }

    if (taskCB->taskStatus & OS_TASK_STATUS_EXIT) {  // 检查任务是否已退出
        SCHEDULER_UNLOCK(intSave);  // 解锁调度器
        return LOS_TaskJoin(taskID, NULL);  // 调用任务连接
    }

    errRet = OsTaskSetDetachUnsafe(taskCB);  // 不安全地设置任务分离
    SCHEDULER_UNLOCK(intSave);  // 解锁调度器
    return errRet;  // 返回结果
}

LITE_OS_SEC_TEXT UINT32 LOS_GetSystemTaskMaximum(VOID)
{
    return g_taskMaxNum;  // 返回系统最大任务数
}

LosTaskCB *OsGetDefaultTaskCB(VOID)
{
    return &g_taskCBArray[g_taskMaxNum];  // 返回默认任务控制块
}

LITE_OS_SEC_TEXT VOID OsWriteResourceEvent(UINT32 events)
{
    (VOID)LOS_EventWrite(&g_resourceEvent, events);  // 写入资源事件
}

LITE_OS_SEC_TEXT VOID OsWriteResourceEventUnsafe(UINT32 events)
{
    (VOID)OsEventWriteUnsafe(&g_resourceEvent, events, FALSE, NULL);  // 不安全地写入资源事件
}

STATIC VOID OsResourceRecoveryTask(VOID)
{
    UINT32 ret;  // 操作结果

    while (1) {  // 无限循环
        ret = LOS_EventRead(&g_resourceEvent, OS_RESOURCE_EVENT_MASK,  // 读取资源事件
                            LOS_WAITMODE_OR | LOS_WAITMODE_CLR, LOS_WAIT_FOREVER);
        if (ret & (OS_RESOURCE_EVENT_FREE | OS_RESOURCE_EVENT_OOM)) {  // 检查资源释放或OOM事件
            OsTaskCBRecycleToFree();  // 回收任务控制块

            OsProcessCBRecycleToFree();  // 回收进程控制块
        }

#ifdef LOSCFG_ENABLE_OOM_LOOP_TASK
        if (ret & OS_RESOURCE_EVENT_OOM) {  // 检查OOM事件
            (VOID)OomCheckProcess();  // 执行OOM检查
        }
#endif
    }
}

LITE_OS_SEC_TEXT UINT32 OsResourceFreeTaskCreate(VOID)
{
    UINT32 ret;  // 操作结果
    UINT32 taskID;  // 任务ID
    TSK_INIT_PARAM_S taskInitParam;  // 任务初始化参数

    ret = LOS_EventInit((PEVENT_CB_S)&g_resourceEvent);  // 初始化资源事件
    if (ret != LOS_OK) {  // 检查初始化结果
        return LOS_NOK;  // 返回失败
    }

    (VOID)memset_s((VOID *)(&taskInitParam), sizeof(TSK_INIT_PARAM_S), 0, sizeof(TSK_INIT_PARAM_S));  // 清空任务参数
    taskInitParam.pfnTaskEntry = (TSK_ENTRY_FUNC)OsResourceRecoveryTask;  // 设置任务入口函数
    taskInitParam.uwStackSize = OS_TASK_RESOURCE_STATIC_SIZE;  // 设置栈大小
    taskInitParam.pcName = "ResourcesTask";  // 设置任务名称
    taskInitParam.usTaskPrio = OS_TASK_RESOURCE_FREE_PRIORITY;  // 设置任务优先级
    ret = LOS_TaskCreate(&taskID, &taskInitParam);  // 创建任务
    if (ret == LOS_OK) {  // 检查创建结果
        OS_TCB_FROM_TID(taskID)->taskStatus |= OS_TASK_FLAG_NO_DELETE;  // 设置任务不可删除标志
    }
    return ret;  // 返回结果
}

LOS_MODULE_INIT(OsResourceFreeTaskCreate, LOS_INIT_LEVEL_KMOD_TASK);

