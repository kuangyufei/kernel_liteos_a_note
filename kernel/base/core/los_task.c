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

#if (LOSCFG_BASE_CORE_TSK_LIMIT <= 0)
#error "task maxnum cannot be zero"
#endif  /* LOSCFG_BASE_CORE_TSK_LIMIT <= 0 */

LITE_OS_SEC_BSS LosTaskCB    *g_taskCBArray;//任务池 128个
LITE_OS_SEC_BSS LOS_DL_LIST  g_losFreeTask;//空闲任务链表
LITE_OS_SEC_BSS LOS_DL_LIST  g_taskRecycleList;//回收任务链表
LITE_OS_SEC_BSS UINT32       g_taskMaxNum;//任务最大个数
LITE_OS_SEC_BSS UINT32       g_taskScheduled; /* one bit for each cores *///任务调度器,每个CPU都有对应位 
LITE_OS_SEC_BSS EVENT_CB_S   g_resourceEvent;//资源的事件
/* spinlock for task module, only available on SMP mode */
LITE_OS_SEC_BSS SPIN_LOCK_INIT(g_taskSpin);

STATIC VOID OsConsoleIDSetHook(UINT32 param1,
                               UINT32 param2) __attribute__((weakref("OsSetConsoleID")));

/* temp task blocks for booting procedure */
LITE_OS_SEC_BSS STATIC LosTaskCB                g_mainTask[LOSCFG_KERNEL_CORE_NUM];//启动引导过程中使用的临时任务

LosTaskCB *OsGetMainTask(VOID)
{
    return (LosTaskCB *)(g_mainTask + ArchCurrCpuid());
}

VOID OsSetMainTask(VOID)
{
    UINT32 i;
    CHAR *name = "osMain";//任务名称
    SchedParam schedParam = { 0 };

    schedParam.policy = LOS_SCHED_RR;
    schedParam.basePrio = OS_PROCESS_PRIORITY_HIGHEST;
    schedParam.priority = OS_TASK_PRIORITY_LOWEST;
	//为每个CPU core 设置mainTask
    for (i = 0; i < LOSCFG_KERNEL_CORE_NUM; i++) {
        g_mainTask[i].taskStatus = OS_TASK_STATUS_UNUSED;
        g_mainTask[i].taskID = LOSCFG_BASE_CORE_TSK_LIMIT;//128
        g_mainTask[i].processCB = OS_KERNEL_PROCESS_GROUP;
#ifdef LOSCFG_KERNEL_SMP_LOCKDEP
        g_mainTask[i].lockDep.lockDepth = 0;
        g_mainTask[i].lockDep.waitLock = NULL;
#endif
        (VOID)strncpy_s(g_mainTask[i].taskName, OS_TCB_NAME_LEN, name, OS_TCB_NAME_LEN - 1);
        LOS_ListInit(&g_mainTask[i].lockList);//初始化任务锁链表,上面挂的是任务已申请到的互斥锁
        (VOID)OsSchedParamInit(&g_mainTask[i], schedParam.policy, &schedParam, NULL);
    }
}
VOID OsSetMainTaskProcess(UINTPTR processCB)
{
    for (UINT32 i = 0; i < LOSCFG_KERNEL_CORE_NUM; i++) {
        g_mainTask[i].processCB = processCB;
#ifdef LOSCFG_PID_CONTAINER
        g_mainTask[i].pidContainer = OS_PID_CONTAINER_FROM_PCB((LosProcessCB *)processCB);
#endif
    }
}
///空闲任务,每个CPU都有自己的空闲任务
LITE_OS_SEC_TEXT WEAK VOID OsIdleTask(VOID)
{
    while (1) {//只有一个死循环
        WFI;//WFI指令:arm core 立即进入low-power standby state，进入休眠模式，等待中断。
    }
}

VOID OsTaskInsertToRecycleList(LosTaskCB *taskCB)
{
    LOS_ListTailInsert(&g_taskRecycleList, &taskCB->pendList);
}

/*!
 * @brief OsTaskJoinPostUnsafe	
 * 	查找task 通过 OS_TCB_FROM_PENDLIST 来完成,相当于由LOS_DL_LIST找到LosTaskCB,
 *	将那些和参数任务绑在一起的task唤醒.
 * @param taskCB	
 * @return	
 *
 * @see
 */
LITE_OS_SEC_TEXT_INIT VOID OsTaskJoinPostUnsafe(LosTaskCB *taskCB)
{
    if (taskCB->taskStatus & OS_TASK_FLAG_PTHREAD_JOIN) {//join任务处理
        if (!LOS_ListEmpty(&taskCB->joinList)) {//注意到了这里 joinList中的节点身上都有阻塞标签
            LosTaskCB *resumedTask = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&(taskCB->joinList)));//通过贴有JOIN标签链表的第一个节点找到Task
            OsTaskWakeClearPendMask(resumedTask);//清除任务的挂起标记
            resumedTask->ops->wake(resumedTask);
        }
    }
    taskCB->taskStatus |= OS_TASK_STATUS_EXIT;//贴上任务退出标签
}
/// 挂起任务,任务进入等待链表,Join代表是支持通过一个任务去唤醒其他的任务
LITE_OS_SEC_TEXT UINT32 OsTaskJoinPendUnsafe(LosTaskCB *taskCB)
{
    if (taskCB->taskStatus & OS_TASK_STATUS_INIT) {
        return LOS_EINVAL;
    }

    if (taskCB->taskStatus & OS_TASK_STATUS_EXIT) {
        return LOS_OK;
    }

    if ((taskCB->taskStatus & OS_TASK_FLAG_PTHREAD_JOIN) && LOS_ListEmpty(&taskCB->joinList)) {
        OsTaskWaitSetPendMask(OS_TASK_WAIT_JOIN, taskCB->taskID, LOS_WAIT_FOREVER);//设置任务的等待标记
        LosTaskCB *runTask = OsCurrTaskGet();
        return runTask->ops->wait(runTask, &taskCB->joinList, LOS_WAIT_FOREVER);
    }

    return LOS_EINVAL;
}
///任务设置分离模式  Deatch和JOIN是一对有你没我的状态
LITE_OS_SEC_TEXT UINT32 OsTaskSetDetachUnsafe(LosTaskCB *taskCB)
{
    if (taskCB->taskStatus & OS_TASK_FLAG_PTHREAD_JOIN) {//join状态时
        if (LOS_ListEmpty(&(taskCB->joinList))) {//joinlist中没有数据了
            LOS_ListDelete(&(taskCB->joinList));//所谓删除就是自己指向自己
            taskCB->taskStatus &= ~OS_TASK_FLAG_PTHREAD_JOIN;//去掉JOIN标签
            return LOS_OK;
        }
        /* This error code has a special purpose and is not allowed to appear again on the interface */
        return LOS_ESRCH;
    }

    return LOS_EINVAL;
}

//初始化任务模块
LITE_OS_SEC_TEXT_INIT UINT32 OsTaskInit(UINTPTR processCB)
{
    UINT32 index;
    UINT32 size;
    UINT32 ret;

    g_taskMaxNum = LOSCFG_BASE_CORE_TSK_LIMIT;//任务池中最多默认128个,可谓铁打的任务池流水的线程
    size = (g_taskMaxNum + 1) * sizeof(LosTaskCB);//计算需分配内存总大小
    /*
     * This memory is resident memory and is used to save the system resources
     * of task control block and will not be freed.
     */
    g_taskCBArray = (LosTaskCB *)LOS_MemAlloc(m_aucSysMem0, size);//任务池常驻内存,不被释放
    if (g_taskCBArray == NULL) {
        ret = LOS_ERRNO_TSK_NO_MEMORY;
        goto EXIT;
    }
    (VOID)memset_s(g_taskCBArray, size, 0, size);

    LOS_ListInit(&g_losFreeTask);//初始化空闲任务链表
    LOS_ListInit(&g_taskRecycleList);//初始化回收任务链表
    for (index = 0; index < g_taskMaxNum; index++) {//任务挨个初始化
        g_taskCBArray[index].taskStatus = OS_TASK_STATUS_UNUSED;//默认未使用,干净.
        g_taskCBArray[index].taskID = index;//任务ID [0 ~ g_taskMaxNum - 1]
        g_taskCBArray[index].processCB = processCB;
        LOS_ListTailInsert(&g_losFreeTask, &g_taskCBArray[index].pendList);//通过pendList节点插入空闲任务列表 
    }//注意:这里挂的是pendList节点,所以取TCB也要通过 OS_TCB_FROM_PENDLIST 取.
    g_taskCBArray[index].taskStatus = OS_TASK_STATUS_UNUSED;
    g_taskCBArray[index].taskID = index;
    g_taskCBArray[index].processCB = processCB;

    ret = OsSchedInit();//调度器初始化

EXIT:
    if (ret != LOS_OK) {
        PRINT_ERR("OsTaskInit error\n");
    }
    return ret;
}
///获取IdletaskId,每个CPU核都对Task进行了内部管理,做到真正的并行处理
UINT32 OsGetIdleTaskId(VOID)
{
    return OsSchedRunqueueIdleGet()->taskID;
}
///创建一个空闲任务
LITE_OS_SEC_TEXT_INIT UINT32 OsIdleTaskCreate(UINTPTR processID)
{
    UINT32 ret;
    TSK_INIT_PARAM_S taskInitParam;
    UINT32 idleTaskID;

    (VOID)memset_s((VOID *)(&taskInitParam), sizeof(TSK_INIT_PARAM_S), 0, sizeof(TSK_INIT_PARAM_S));//任务初始参数清0
    taskInitParam.pfnTaskEntry = (TSK_ENTRY_FUNC)OsIdleTask;//入口函数
    taskInitParam.uwStackSize = LOSCFG_BASE_CORE_TSK_IDLE_STACK_SIZE;//任务栈大小 2K
    taskInitParam.pcName = "Idle";//任务名称 叫pcName有点怪怪的,不能换个撒
    taskInitParam.policy = LOS_SCHED_IDLE;
    taskInitParam.usTaskPrio = OS_TASK_PRIORITY_LOWEST;//默认最低优先级 31
    taskInitParam.processID = processID;
#ifdef LOSCFG_KERNEL_SMP
    taskInitParam.usCpuAffiMask = CPUID_TO_AFFI_MASK(ArchCurrCpuid());//每个idle任务只在单独的cpu上运行
#endif
    ret = LOS_TaskCreateOnly(&idleTaskID, &taskInitParam);
    if (ret != LOS_OK) {
        return ret;
    }
    LosTaskCB *idleTask = OS_TCB_FROM_TID(idleTaskID);
    idleTask->taskStatus |= OS_TASK_FLAG_SYSTEM_TASK; //标记为系统任务,idle任务是给CPU休息用的,当然是个系统任务
    OsSchedRunqueueIdleInit(idleTask);

    return LOS_TaskResume(idleTaskID);
}

/*
 * Description : get id of current running task. | 获取当前CPU正在执行的任务ID
 * Return      : task id
 */
LITE_OS_SEC_TEXT UINT32 LOS_CurTaskIDGet(VOID)
{
    LosTaskCB *runTask = OsCurrTaskGet();

    if (runTask == NULL) {
        return LOS_ERRNO_TSK_ID_INVALID;
    }
    return runTask->taskID;
}
/// 创建指定任务同步信号量
STATIC INLINE UINT32 TaskSyncCreate(LosTaskCB *taskCB)
{
#ifdef LOSCFG_KERNEL_SMP_TASK_SYNC
    UINT32 ret = LOS_SemCreate(0, &taskCB->syncSignal);
    if (ret != LOS_OK) {
        return LOS_ERRNO_TSK_MP_SYNC_RESOURCE;
    }
#else
    (VOID)taskCB;
#endif
    return LOS_OK;
}
/// 销毁指定任务同步信号量
STATIC INLINE VOID OsTaskSyncDestroy(UINT32 syncSignal)
{
#ifdef LOSCFG_KERNEL_SMP_TASK_SYNC
    (VOID)LOS_SemDelete(syncSignal);
#else
    (VOID)syncSignal;
#endif
}

#ifdef LOSCFG_KERNEL_SMP
/*!
 * @brief OsTaskSyncWait	
 * 任务同步等待,通过信号量保持同步
 * @param taskCB	
 * @return	
 *
 * @see
 */
STATIC INLINE UINT32 OsTaskSyncWait(const LosTaskCB *taskCB)
{
#ifdef LOSCFG_KERNEL_SMP_TASK_SYNC
    UINT32 ret = LOS_OK;

    LOS_ASSERT(LOS_SpinHeld(&g_taskSpin));
    LOS_SpinUnlock(&g_taskSpin);
    /*
     * gc soft timer works every OS_MP_GC_PERIOD period, to prevent this timer
     * triggered right at the timeout has reached, we set the timeout as double
     * of the gc peroid.
     */
    if (LOS_SemPend(taskCB->syncSignal, OS_MP_GC_PERIOD * 2) != LOS_OK) {
        ret = LOS_ERRNO_TSK_MP_SYNC_FAILED;
    }

    LOS_SpinLock(&g_taskSpin);

    return ret;
#else
    (VOID)taskCB;
    return LOS_OK;
#endif
}
#endif
/// 同步唤醒
STATIC INLINE VOID OsTaskSyncWake(const LosTaskCB *taskCB)
{
#ifdef LOSCFG_KERNEL_SMP_TASK_SYNC
    (VOID)OsSemPostUnsafe(taskCB->syncSignal, NULL);
#else
    (VOID)taskCB;
#endif
}

STATIC INLINE VOID OsInsertTCBToFreeList(LosTaskCB *taskCB)
{
#ifdef LOSCFG_PID_CONTAINER
    OsFreeVtid(taskCB);
#endif
    UINT32 taskID = taskCB->taskID;
    (VOID)memset_s(taskCB, sizeof(LosTaskCB), 0, sizeof(LosTaskCB));
    taskCB->taskID = taskID;
    taskCB->processCB = (UINTPTR)OsGetDefaultProcessCB();
    taskCB->taskStatus = OS_TASK_STATUS_UNUSED;
    LOS_ListAdd(&g_losFreeTask, &taskCB->pendList);
}

STATIC VOID OsTaskKernelResourcesToFree(UINT32 syncSignal, UINTPTR topOfStack)
{
    OsTaskSyncDestroy(syncSignal);

    (VOID)LOS_MemFree((VOID *)m_aucSysMem1, (VOID *)topOfStack);
}

STATIC VOID OsTaskResourcesToFree(LosTaskCB *taskCB)
{
    UINT32 syncSignal = LOSCFG_BASE_IPC_SEM_LIMIT;
    UINT32 intSave;
    UINTPTR topOfStack;

#ifdef LOSCFG_KERNEL_VM
    if ((taskCB->taskStatus & OS_TASK_FLAG_USER_MODE) && (taskCB->userMapBase != 0)) {
        SCHEDULER_LOCK(intSave);
        UINT32 mapBase = (UINTPTR)taskCB->userMapBase;
        UINT32 mapSize = taskCB->userMapSize;
        taskCB->userMapBase = 0;
        taskCB->userArea = 0;
        SCHEDULER_UNLOCK(intSave);

        LosProcessCB *processCB = OS_PCB_FROM_TCB(taskCB);
        LOS_ASSERT(!(OsProcessVmSpaceGet(processCB) == NULL));
        UINT32 ret = OsUnMMap(OsProcessVmSpaceGet(processCB), (UINTPTR)mapBase, mapSize);
        if ((ret != LOS_OK) && (mapBase != 0) && !OsProcessIsInit(processCB)) {
            PRINT_ERR("process(%u) unmmap user task(%u) stack failed! mapbase: 0x%x size :0x%x, error: %d\n",
                      processCB->processID, taskCB->taskID, mapBase, mapSize, ret);
        }

#ifdef LOSCFG_KERNEL_LITEIPC
        LiteIpcRemoveServiceHandle(taskCB->taskID);
#endif
    }
#endif

    if (taskCB->taskStatus & OS_TASK_STATUS_UNUSED) {
        topOfStack = taskCB->topOfStack;
        taskCB->topOfStack = 0;
#ifdef LOSCFG_KERNEL_SMP_TASK_SYNC
        syncSignal = taskCB->syncSignal;
        taskCB->syncSignal = LOSCFG_BASE_IPC_SEM_LIMIT;
#endif
        OsTaskKernelResourcesToFree(syncSignal, topOfStack);

        SCHEDULER_LOCK(intSave);
#ifdef LOSCFG_KERNEL_VM
        OsClearSigInfoTmpList(&(taskCB->sig));
#endif
        OsInsertTCBToFreeList(taskCB);
        SCHEDULER_UNLOCK(intSave);
    }
    return;
}

LITE_OS_SEC_TEXT VOID OsTaskCBRecycleToFree()
{
    UINT32 intSave;

    SCHEDULER_LOCK(intSave);
    while (!LOS_ListEmpty(&g_taskRecycleList)) {
        LosTaskCB *taskCB = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&g_taskRecycleList));
        LOS_ListDelete(&taskCB->pendList);
        SCHEDULER_UNLOCK(intSave);

        OsTaskResourcesToFree(taskCB);

        SCHEDULER_LOCK(intSave);
    }
    SCHEDULER_UNLOCK(intSave);
}

/*
 * Description : All task entry
 * Input       : taskID     --- The ID of the task to be run
 *///所有任务的入口函数,OsTaskEntry是在new task OsTaskStackInit 时指定的
LITE_OS_SEC_TEXT_INIT VOID OsTaskEntry(UINT32 taskID)
{
    LOS_ASSERT(!OS_TID_CHECK_INVALID(taskID));

    /*
     * task scheduler needs to be protected throughout the whole process
     * from interrupt and other cores. release task spinlock and enable
     * interrupt in sequence at the task entry.
     */
    LOS_SpinUnlock(&g_taskSpin);//释放任务自旋锁
    (VOID)LOS_IntUnLock();//恢复中断

    LosTaskCB *taskCB = OS_TCB_FROM_TID(taskID);
    taskCB->joinRetval = taskCB->taskEntry(taskCB->args[0], taskCB->args[1],//调用任务的入口函数
                                           taskCB->args[2], taskCB->args[3]); /* 2 & 3: just for args array index */
    if (!(taskCB->taskStatus & OS_TASK_FLAG_PTHREAD_JOIN)) {
        taskCB->joinRetval = 0;//结合数为0
    }
	
    OsRunningTaskToExit(taskCB, 0);
}
///任务创建参数检查
STATIC UINT32 TaskCreateParamCheck(const UINT32 *taskID, TSK_INIT_PARAM_S *initParam)
{
    UINT32 poolSize = OS_SYS_MEM_SIZE;

    if (taskID == NULL) {
        return LOS_ERRNO_TSK_ID_INVALID;
    }

    if (initParam == NULL) {
        return LOS_ERRNO_TSK_PTR_NULL;
    }

    if (!OsProcessIsUserMode((LosProcessCB *)initParam->processID)) {
        if (initParam->pcName == NULL) {
            return LOS_ERRNO_TSK_NAME_EMPTY;
        }
    }

    if (initParam->pfnTaskEntry == NULL) {//入口函数不能为空
        return LOS_ERRNO_TSK_ENTRY_NULL;
    }

    if (initParam->usTaskPrio > OS_TASK_PRIORITY_LOWEST) {//优先级必须大于31
        return LOS_ERRNO_TSK_PRIOR_ERROR;
    }

    if (initParam->uwStackSize > poolSize) {//希望申请的栈大小不能大于总池子
        return LOS_ERRNO_TSK_STKSZ_TOO_LARGE;
    }

    if (initParam->uwStackSize == 0) {//任何任务都必须由内核态栈,所以uwStackSize不能为0
        initParam->uwStackSize = LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE;
    }
    initParam->uwStackSize = (UINT32)ALIGN(initParam->uwStackSize, LOSCFG_STACK_POINT_ALIGN_SIZE);

    if (initParam->uwStackSize < LOS_TASK_MIN_STACK_SIZE) {//运行栈空间不能低于最低值
        return LOS_ERRNO_TSK_STKSZ_TOO_SMALL;
    }

    return LOS_OK;
}
///任务栈(内核态)内存分配,由内核态进程空间提供,即 KProcess 的进程空间
STATIC VOID TaskCBDeInit(LosTaskCB *taskCB)
{
    UINT32 intSave;
#ifdef LOSCFG_KERNEL_SMP_TASK_SYNC
    if (taskCB->syncSignal != OS_INVALID_VALUE) {
        OsTaskSyncDestroy(taskCB->syncSignal);
        taskCB->syncSignal = OS_INVALID_VALUE;
    }
#endif

    if (taskCB->topOfStack != (UINTPTR)NULL) {
        (VOID)LOS_MemFree(m_aucSysMem1, (VOID *)taskCB->topOfStack);
        taskCB->topOfStack = (UINTPTR)NULL;
    }

    SCHEDULER_LOCK(intSave);
    LosProcessCB *processCB = OS_PCB_FROM_TCB(taskCB);
    if (processCB != OsGetDefaultProcessCB()) {
        LOS_ListDelete(&taskCB->threadList);
        processCB->threadNumber--;
        processCB->threadCount--;
    }

    OsInsertTCBToFreeList(taskCB);
    SCHEDULER_UNLOCK(intSave);
}

STATIC VOID TaskCBBaseInit(LosTaskCB *taskCB, const TSK_INIT_PARAM_S *initParam)
{
    taskCB->stackPointer = NULL;
    taskCB->args[0]      = initParam->auwArgs[0]; /* 0~3: just for args array index */
    taskCB->args[1]      = initParam->auwArgs[1];
    taskCB->args[2]      = initParam->auwArgs[2];
    taskCB->args[3]      = initParam->auwArgs[3];
    taskCB->topOfStack   = (UINTPTR)NULL;
    taskCB->stackSize    = initParam->uwStackSize;
    taskCB->taskEntry    = initParam->pfnTaskEntry;
    taskCB->signal       = SIGNAL_NONE;
#ifdef LOSCFG_KERNEL_SMP_TASK_SYNC
    taskCB->syncSignal   = OS_INVALID_VALUE;
#endif
#ifdef LOSCFG_KERNEL_SMP
    taskCB->currCpu      = OS_TASK_INVALID_CPUID;
    taskCB->cpuAffiMask  = (initParam->usCpuAffiMask) ?
                            initParam->usCpuAffiMask : LOSCFG_KERNEL_CPU_MASK;
#endif
    taskCB->taskStatus = OS_TASK_STATUS_INIT;
    if (initParam->uwResved & LOS_TASK_ATTR_JOINABLE) {
        taskCB->taskStatus |= OS_TASK_FLAG_PTHREAD_JOIN;
        LOS_ListInit(&taskCB->joinList);
    }

    LOS_ListInit(&taskCB->lockList);//初始化互斥锁链表
    SET_SORTLIST_VALUE(&taskCB->sortList, OS_SORT_LINK_INVALID_TIME);
#ifdef LOSCFG_KERNEL_VM
    taskCB->futex.index = OS_INVALID_VALUE;
#endif
}
///任务初始化
STATIC UINT32 TaskCBInit(LosTaskCB *taskCB, const TSK_INIT_PARAM_S *initParam)
{
    UINT32 ret;
    UINT32 numCount;
    SchedParam schedParam = { 0 };
    UINT16 policy = (initParam->policy == LOS_SCHED_NORMAL) ? LOS_SCHED_RR : initParam->policy;

    TaskCBBaseInit(taskCB, initParam);//初始化任务的基本信息,
    					//taskCB->stackPointer指向内核态栈 sp位置,该位置存着 任务初始上下文

    schedParam.policy = policy;
    ret = OsProcessAddNewTask(initParam->processID, taskCB, &schedParam, &numCount);
    if (ret != LOS_OK) {
        return ret;
    }

    ret = OsSchedParamInit(taskCB, policy, &schedParam, initParam);
    if (ret != LOS_OK) {
        return ret;
    }

    if (initParam->pcName != NULL) {
        ret = (UINT32)OsSetTaskName(taskCB, initParam->pcName, FALSE);
        if (ret == LOS_OK) {
            return LOS_OK;
        }
    }

    if (snprintf_s(taskCB->taskName, OS_TCB_NAME_LEN, OS_TCB_NAME_LEN - 1, "thread%u", numCount) < 0) {
        return LOS_NOK;
    }
    return LOS_OK;
}

STATIC UINT32 TaskStackInit(LosTaskCB *taskCB, const TSK_INIT_PARAM_S *initParam)
{
    VOID *topStack = (VOID *)LOS_MemAllocAlign(m_aucSysMem1, initParam->uwStackSize, LOSCFG_STACK_POINT_ALIGN_SIZE);
    if (topStack == NULL) {
        return LOS_ERRNO_TSK_NO_MEMORY;
    }

    taskCB->topOfStack = (UINTPTR)topStack;
    taskCB->stackPointer = OsTaskStackInit(taskCB->taskID, initParam->uwStackSize, topStack, TRUE);
#ifdef LOSCFG_KERNEL_VM
    if (taskCB->taskStatus & OS_TASK_FLAG_USER_MODE) {
        taskCB->userArea = initParam->userParam.userArea;
        taskCB->userMapBase = initParam->userParam.userMapBase;
        taskCB->userMapSize = initParam->userParam.userMapSize;
        OsUserTaskStackInit(taskCB->stackPointer, (UINTPTR)taskCB->taskEntry, initParam->userParam.userSP);
    }
#endif
    return LOS_OK;
}
///获取一个空闲TCB
STATIC LosTaskCB *GetFreeTaskCB(VOID)
{
    UINT32 intSave;

    SCHEDULER_LOCK(intSave);
    if (LOS_ListEmpty(&g_losFreeTask)) {//全局空闲task为空
        SCHEDULER_UNLOCK(intSave);
        PRINT_ERR("No idle TCB in the system!\n");
        return NULL;
    }

    LosTaskCB *taskCB = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&g_losFreeTask));
    LOS_ListDelete(LOS_DL_LIST_FIRST(&g_losFreeTask));//从g_losFreeTask链表中摘除自己
    SCHEDULER_UNLOCK(intSave);

    return taskCB;
}

/*!
 * @brief LOS_TaskCreateOnly	
 * 创建任务，并使该任务进入suspend状态，不对该任务进行调度。如果需要调度，可以调用LOS_TaskResume使该任务进入ready状态
 * @param initParam	
 * @param taskID	
 * @return	
 *
 * @see
 */
LITE_OS_SEC_TEXT_INIT UINT32 LOS_TaskCreateOnly(UINT32 *taskID, TSK_INIT_PARAM_S *initParam)
{
    UINT32 errRet = TaskCreateParamCheck(taskID, initParam);
    if (errRet != LOS_OK) {
        return errRet;
    }

    LosTaskCB *taskCB = GetFreeTaskCB();
    if (taskCB == NULL) {
        return LOS_ERRNO_TSK_TCB_UNAVAILABLE;
    }

    errRet = TaskCBInit(taskCB, initParam);
    if (errRet != LOS_OK) {
        goto DEINIT_TCB;
    }

    errRet = TaskSyncCreate(taskCB);
    if (errRet != LOS_OK) {
        goto DEINIT_TCB;
    }

    errRet = TaskStackInit(taskCB, initParam);
    if (errRet != LOS_OK) {
        goto DEINIT_TCB;
    }

    if (OsConsoleIDSetHook != NULL) {
        OsConsoleIDSetHook(taskCB->taskID, OsCurrTaskGet()->taskID);
    }

    *taskID = taskCB->taskID;
    OsHookCall(LOS_HOOK_TYPE_TASK_CREATE, taskCB);
    return LOS_OK;

DEINIT_TCB:
    TaskCBDeInit(taskCB);
    return errRet;
}
///创建任务，并使该任务进入ready状态，如果就绪队列中没有更高优先级的任务，则运行该任务
LITE_OS_SEC_TEXT_INIT UINT32 LOS_TaskCreate(UINT32 *taskID, TSK_INIT_PARAM_S *initParam)
{
    UINT32 ret;
    UINT32 intSave;

    if (initParam == NULL) {
        return LOS_ERRNO_TSK_PTR_NULL;
    }

    if (OS_INT_ACTIVE) {
        return LOS_ERRNO_TSK_YIELD_IN_INT;
    }

    if (OsProcessIsUserMode(OsCurrProcessGet())) { //当前进程为用户进程
        initParam->processID = (UINTPTR)OsGetKernelInitProcess();
    } else {
        initParam->processID = (UINTPTR)OsCurrProcessGet();
    }

    ret = LOS_TaskCreateOnly(taskID, initParam);
    if (ret != LOS_OK) {
        return ret;
    }

    LosTaskCB *taskCB = OS_TCB_FROM_TID(*taskID);

    SCHEDULER_LOCK(intSave);
    taskCB->ops->enqueue(OsSchedRunqueue(), taskCB);
    SCHEDULER_UNLOCK(intSave);

    /* in case created task not running on this core,
       schedule or not depends on other schedulers status. */
    LOS_MpSchedule(OS_MP_CPU_ALL);
    if (OS_SCHEDULER_ACTIVE) {
        LOS_Schedule();
    }

    return LOS_OK;
}
///恢复挂起的任务，使该任务进入ready状态
LITE_OS_SEC_TEXT_INIT UINT32 LOS_TaskResume(UINT32 taskID)
{
    UINT32 intSave;
    UINT32 errRet;
    BOOL needSched = FALSE;

    if (OS_TID_CHECK_INVALID(taskID)) {
        return LOS_ERRNO_TSK_ID_INVALID;
    }

    LosTaskCB *taskCB = OS_TCB_FROM_TID(taskID);
    SCHEDULER_LOCK(intSave);

    /* clear pending signal */
    taskCB->signal &= ~SIGNAL_SUSPEND;//清楚挂起信号

    if (taskCB->taskStatus & OS_TASK_STATUS_UNUSED) {
        errRet = LOS_ERRNO_TSK_NOT_CREATED;
        OS_GOTO_ERREND();
    } else if (!(taskCB->taskStatus & OS_TASK_STATUS_SUSPENDED)) {
        errRet = LOS_ERRNO_TSK_NOT_SUSPENDED;
        OS_GOTO_ERREND();
    }

    errRet = taskCB->ops->resume(taskCB, &needSched);
    SCHEDULER_UNLOCK(intSave);

    LOS_MpSchedule(OS_MP_CPU_ALL);
    if (OS_SCHEDULER_ACTIVE && needSched) {
        LOS_Schedule();
    }

    return errRet;

LOS_ERREND:
    SCHEDULER_UNLOCK(intSave);
    return errRet;
}

/*
 * Check if needs to do the suspend operation on the running task.	//检查是否需要对正在运行的任务执行挂起操作。
 * Return TRUE, if needs to do the suspension.						//如果需要暂停，返回TRUE。
 * Rerturn FALSE, if meets following circumstances:					//如果满足以下情况，则返回FALSE：
 * 1. Do the suspension across cores, if SMP is enabled				//1.如果启用了SMP，则跨CPU核执行挂起操作
 * 2. Do the suspension when preemption is disabled					//2.当禁用抢占时则挂起
 * 3. Do the suspension in hard-irq									//3.在硬中断时则挂起
 * then LOS_TaskSuspend will directly return with 'ret' value.		//那么LOS_taskssuspend将直接返回ret值。
 */
LITE_OS_SEC_TEXT_INIT STATIC BOOL OsTaskSuspendCheckOnRun(LosTaskCB *taskCB, UINT32 *ret)
{
    /* init default out return value */
    *ret = LOS_OK;

#ifdef LOSCFG_KERNEL_SMP
    /* ASYNCHRONIZED. No need to do task lock checking */
    if (taskCB->currCpu != ArchCurrCpuid()) {//跨CPU核的情况
        taskCB->signal = SIGNAL_SUSPEND;
        LOS_MpSchedule(taskCB->currCpu);//task所属CPU执行调度
        return FALSE;
    }
#endif

    if (!OsPreemptableInSched()) {//不能抢占时
        /* Suspending the current core's running task */
        *ret = LOS_ERRNO_TSK_SUSPEND_LOCKED;
        return FALSE;
    }

    if (OS_INT_ACTIVE) {//正在硬中断中
        /* suspend running task in interrupt */
        taskCB->signal = SIGNAL_SUSPEND;
        return FALSE;
    }

    return TRUE;
}
///任务暂停,参数可以不是当前任务，也就是说 A任务可以让B任务处于阻塞状态,挂起指定的任务，然后切换任务
LITE_OS_SEC_TEXT STATIC UINT32 OsTaskSuspend(LosTaskCB *taskCB)
{
    UINT32 errRet;
    UINT16 tempStatus = taskCB->taskStatus;
    if (tempStatus & OS_TASK_STATUS_UNUSED) {
        return LOS_ERRNO_TSK_NOT_CREATED;
    }

    if (tempStatus & OS_TASK_STATUS_SUSPENDED) {
        return LOS_ERRNO_TSK_ALREADY_SUSPENDED;
    }

    if ((tempStatus & OS_TASK_STATUS_RUNNING) && //如果参数任务正在运行，注意多Cpu core情况下，贴着正在运行标签的任务并不一定是当前CPU的执行任务，
        !OsTaskSuspendCheckOnRun(taskCB, &errRet)) {//很有可能是别的CPU core在跑的任务
        return errRet;
    }

    return taskCB->ops->suspend(taskCB);
}
///外部接口，对OsTaskSuspend的封装
LITE_OS_SEC_TEXT_INIT UINT32 LOS_TaskSuspend(UINT32 taskID)
{
    UINT32 intSave;
    UINT32 errRet;

    if (OS_TID_CHECK_INVALID(taskID)) {
        return LOS_ERRNO_TSK_ID_INVALID;
    }

    LosTaskCB *taskCB = OS_TCB_FROM_TID(taskID);
    if (taskCB->taskStatus & OS_TASK_FLAG_SYSTEM_TASK) {
        return LOS_ERRNO_TSK_OPERATE_SYSTEM_TASK;
    }

    SCHEDULER_LOCK(intSave);
    errRet = OsTaskSuspend(taskCB);
    SCHEDULER_UNLOCK(intSave);
    return errRet;
}
///设置任务为不使用状态
STATIC INLINE VOID OsTaskStatusUnusedSet(LosTaskCB *taskCB)
{
    taskCB->taskStatus |= OS_TASK_STATUS_UNUSED;
    taskCB->eventMask = 0;

    OS_MEM_CLEAR(taskCB->taskID);
}

STATIC VOID OsTaskReleaseHoldLock(LosTaskCB *taskCB)
{
    LosMux *mux = NULL;
    UINT32 ret;

    while (!LOS_ListEmpty(&taskCB->lockList)) {
        mux = LOS_DL_LIST_ENTRY(LOS_DL_LIST_FIRST(&taskCB->lockList), LosMux, holdList);
        ret = OsMuxUnlockUnsafe(taskCB, mux, NULL);
        if (ret != LOS_OK) {
            LOS_ListDelete(&mux->holdList);
            PRINT_ERR("mux ulock failed! : %u\n", ret);
        }
    }

#ifdef LOSCFG_KERNEL_VM
    if (taskCB->taskStatus & OS_TASK_FLAG_USER_MODE) {
        OsFutexNodeDeleteFromFutexHash(&taskCB->futex, TRUE, NULL, NULL);
    }
#endif

    OsTaskJoinPostUnsafe(taskCB);

    OsTaskSyncWake(taskCB);
}

LITE_OS_SEC_TEXT VOID OsRunningTaskToExit(LosTaskCB *runTask, UINT32 status)
{
    UINT32 intSave;

    if (OsIsProcessThreadGroup(runTask)) {
        OsProcessThreadGroupDestroy();
    }

    OsHookCall(LOS_HOOK_TYPE_TASK_DELETE, runTask);

    SCHEDULER_LOCK(intSave);
    if (OsProcessThreadNumberGet(runTask) == 1) { /* 1: The last task of the process exits */
        SCHEDULER_UNLOCK(intSave);

        OsTaskResourcesToFree(runTask);
        OsProcessResourcesToFree(OS_PCB_FROM_TCB(runTask));

        SCHEDULER_LOCK(intSave);

        OsProcessNaturalExit(OS_PCB_FROM_TCB(runTask), status);
        OsTaskReleaseHoldLock(runTask);
        OsTaskStatusUnusedSet(runTask);
    } else if (runTask->taskStatus & OS_TASK_FLAG_PTHREAD_JOIN) {
        OsTaskReleaseHoldLock(runTask);
    } else {
        SCHEDULER_UNLOCK(intSave);

        OsTaskResourcesToFree(runTask);

        SCHEDULER_LOCK(intSave);
        OsInactiveTaskDelete(runTask);
        OsEventWriteUnsafe(&g_resourceEvent, OS_RESOURCE_EVENT_FREE, FALSE, NULL);
    }

    OsSchedResched();
    SCHEDULER_UNLOCK(intSave);
    return;
}

LITE_OS_SEC_TEXT VOID OsInactiveTaskDelete(LosTaskCB *taskCB)
{
    UINT16 taskStatus = taskCB->taskStatus;

    OsTaskReleaseHoldLock(taskCB);

    taskCB->ops->exit(taskCB);
    if (taskStatus & OS_TASK_STATUS_PENDING) {
        LosMux *mux = (LosMux *)taskCB->taskMux;
        if (LOS_MuxIsValid(mux) == TRUE) {
            OsMuxBitmapRestore(mux, NULL, taskCB);
        }
    }

    OsTaskStatusUnusedSet(taskCB);

    OsDeleteTaskFromProcess(taskCB);

    OsHookCall(LOS_HOOK_TYPE_TASK_DELETE, taskCB);
}

LITE_OS_SEC_TEXT_INIT UINT32 LOS_TaskDelete(UINT32 taskID)
{
    UINT32 intSave;
    UINT32 ret = LOS_OK;

    if (OS_TID_CHECK_INVALID(taskID)) {
        return LOS_ERRNO_TSK_ID_INVALID;
    }

    if (OS_INT_ACTIVE) {
        return LOS_ERRNO_TSK_YIELD_IN_INT;
    }

    LosTaskCB *taskCB = OS_TCB_FROM_TID(taskID);
    if (taskCB == OsCurrTaskGet()) {
        if (!OsPreemptable()) {
            return LOS_ERRNO_TSK_DELETE_LOCKED;
        }
        OsRunningTaskToExit(taskCB, OS_PRO_EXIT_OK);
        return LOS_NOK;
    }

    SCHEDULER_LOCK(intSave);
    if (OsTaskIsNotDelete(taskCB)) {
        if (OsTaskIsUnused(taskCB)) {
            ret = LOS_ERRNO_TSK_NOT_CREATED;
        } else {
            ret = LOS_ERRNO_TSK_OPERATE_SYSTEM_TASK;
        }
        OS_GOTO_ERREND();
    }

#ifdef LOSCFG_KERNEL_SMP
    if (taskCB->taskStatus & OS_TASK_STATUS_RUNNING) {
        taskCB->signal = SIGNAL_KILL;
        LOS_MpSchedule(taskCB->currCpu);
        ret = OsTaskSyncWait(taskCB);
        OS_GOTO_ERREND();
    }
#endif

    OsInactiveTaskDelete(taskCB);
    OsEventWriteUnsafe(&g_resourceEvent, OS_RESOURCE_EVENT_FREE, FALSE, NULL);

LOS_ERREND:
    SCHEDULER_UNLOCK(intSave);
    if (ret == LOS_OK) {
        LOS_Schedule();
    }
    return ret;
}
///任务延时等待，释放CPU，等待时间到期后该任务会重新进入ready状态
LITE_OS_SEC_TEXT UINT32 LOS_TaskDelay(UINT32 tick)
{
    UINT32 intSave;

    if (OS_INT_ACTIVE) {
        PRINT_ERR("In interrupt not allow delay task!\n");
        return LOS_ERRNO_TSK_DELAY_IN_INT;
    }

    LosTaskCB *runTask = OsCurrTaskGet();
    if (runTask->taskStatus & OS_TASK_FLAG_SYSTEM_TASK) {
        OsBackTrace();
        return LOS_ERRNO_TSK_OPERATE_SYSTEM_TASK;
    }

    if (!OsPreemptable()) {
        return LOS_ERRNO_TSK_DELAY_IN_LOCK;
    }
    OsHookCall(LOS_HOOK_TYPE_TASK_DELAY, tick);
    if (tick == 0) {
        return LOS_TaskYield();
    }

    SCHEDULER_LOCK(intSave);
    UINT32 ret = runTask->ops->delay(runTask, OS_SCHED_TICK_TO_CYCLE(tick));
    OsHookCall(LOS_HOOK_TYPE_MOVEDTASKTODELAYEDLIST, runTask);
    SCHEDULER_UNLOCK(intSave);
    return ret;
}
///获取任务的优先级
LITE_OS_SEC_TEXT_MINOR UINT16 LOS_TaskPriGet(UINT32 taskID)
{
    UINT32 intSave;
    SchedParam param = { 0 };

    if (OS_TID_CHECK_INVALID(taskID)) {
        return (UINT16)OS_INVALID;
    }

    LosTaskCB *taskCB = OS_TCB_FROM_TID(taskID);
    SCHEDULER_LOCK(intSave);
    if (taskCB->taskStatus & OS_TASK_STATUS_UNUSED) {//就这么一句话也要来个自旋锁，内核代码自旋锁真是无处不在啊
        SCHEDULER_UNLOCK(intSave);
        return (UINT16)OS_INVALID;
    }

    taskCB->ops->schedParamGet(taskCB, &param);
    SCHEDULER_UNLOCK(intSave);
    return param.priority;
}
///设置指定任务的优先级
LITE_OS_SEC_TEXT_MINOR UINT32 LOS_TaskPriSet(UINT32 taskID, UINT16 taskPrio)
{
    UINT32 intSave;
    SchedParam param = { 0 };

    if (taskPrio > OS_TASK_PRIORITY_LOWEST) {
        return LOS_ERRNO_TSK_PRIOR_ERROR;
    }

    if (OS_TID_CHECK_INVALID(taskID)) {
        return LOS_ERRNO_TSK_ID_INVALID;
    }

    LosTaskCB *taskCB = OS_TCB_FROM_TID(taskID);
    if (taskCB->taskStatus & OS_TASK_FLAG_SYSTEM_TASK) {
        return LOS_ERRNO_TSK_OPERATE_SYSTEM_TASK;
    }

    SCHEDULER_LOCK(intSave);
    if (taskCB->taskStatus & OS_TASK_STATUS_UNUSED) {
        SCHEDULER_UNLOCK(intSave);
        return LOS_ERRNO_TSK_NOT_CREATED;
    }

    taskCB->ops->schedParamGet(taskCB, &param);

    param.priority = taskPrio;

    BOOL needSched = taskCB->ops->schedParamModify(taskCB, &param);
    SCHEDULER_UNLOCK(intSave);

    LOS_MpSchedule(OS_MP_CPU_ALL);
    if (needSched && OS_SCHEDULER_ACTIVE) {
        LOS_Schedule();
    }
    return LOS_OK;
}
///设置当前任务的优先级
LITE_OS_SEC_TEXT_MINOR UINT32 LOS_CurTaskPriSet(UINT16 taskPrio)
{
    return LOS_TaskPriSet(OsCurrTaskGet()->taskID, taskPrio);
}

//当前任务释放CPU，并将其移到具有相同优先级的就绪任务队列的末尾. 读懂这个函数 你就彻底搞懂了 yield
LITE_OS_SEC_TEXT_MINOR UINT32 LOS_TaskYield(VOID)
{
    UINT32 intSave;

    if (OS_INT_ACTIVE) {
        return LOS_ERRNO_TSK_YIELD_IN_INT;
    }

    if (!OsPreemptable()) {
        return LOS_ERRNO_TSK_YIELD_IN_LOCK;
    }

    LosTaskCB *runTask = OsCurrTaskGet();
    if (OS_TID_CHECK_INVALID(runTask->taskID)) {
        return LOS_ERRNO_TSK_ID_INVALID;
    }

    SCHEDULER_LOCK(intSave);
    /* reset timeslice of yielded task */
    runTask->ops->yield(runTask);
    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;
}

LITE_OS_SEC_TEXT_MINOR VOID LOS_TaskLock(VOID)
{
    UINT32 intSave;

    intSave = LOS_IntLock();
    OsSchedLock();
    LOS_IntRestore(intSave);
}

LITE_OS_SEC_TEXT_MINOR VOID LOS_TaskUnlock(VOID)
{
    UINT32 intSave;

    intSave = LOS_IntLock();
    BOOL needSched = OsSchedUnlockResch();
    LOS_IntRestore(intSave);

    if (needSched) {
        LOS_Schedule();
    }
}
//获取任务信息,给shell使用的
LITE_OS_SEC_TEXT_MINOR UINT32 LOS_TaskInfoGet(UINT32 taskID, TSK_INFO_S *taskInfo)
{
    UINT32 intSave;
    SchedParam param = { 0 };

    if (taskInfo == NULL) {
        return LOS_ERRNO_TSK_PTR_NULL;
    }

    if (OS_TID_CHECK_INVALID(taskID)) {
        return LOS_ERRNO_TSK_ID_INVALID;
    }

    LosTaskCB *taskCB = OS_TCB_FROM_TID(taskID);
    SCHEDULER_LOCK(intSave);
    if (taskCB->taskStatus & OS_TASK_STATUS_UNUSED) {
        SCHEDULER_UNLOCK(intSave);
        return LOS_ERRNO_TSK_NOT_CREATED;
    }

    if (!(taskCB->taskStatus & OS_TASK_STATUS_RUNNING) || OS_INT_ACTIVE) {
        taskInfo->uwSP = (UINTPTR)taskCB->stackPointer;
    } else {
        taskInfo->uwSP = ArchSPGet();
    }

    taskCB->ops->schedParamGet(taskCB, &param);
    taskInfo->usTaskStatus = taskCB->taskStatus;
    taskInfo->usTaskPrio = param.priority;
    taskInfo->uwStackSize = taskCB->stackSize;	//内核态栈大小
    taskInfo->uwTopOfStack = taskCB->topOfStack;//内核态栈顶位置
    taskInfo->uwEventMask = taskCB->eventMask;
    taskInfo->taskEvent = taskCB->taskEvent;
    taskInfo->pTaskMux = taskCB->taskMux;
    taskInfo->uwTaskID = taskID;

    if (strncpy_s(taskInfo->acName, LOS_TASK_NAMELEN, taskCB->taskName, LOS_TASK_NAMELEN - 1) != EOK) {
        PRINT_ERR("Task name copy failed!\n");
    }
    taskInfo->acName[LOS_TASK_NAMELEN - 1] = '\0';

    taskInfo->uwBottomOfStack = TRUNCATE(((UINTPTR)taskCB->topOfStack + taskCB->stackSize),//这里可以看出栈底地址是高于栈顶
                                         OS_TASK_STACK_ADDR_ALIGN);
    taskInfo->uwCurrUsed = (UINT32)(taskInfo->uwBottomOfStack - taskInfo->uwSP);//当前任务栈已使用了多少

    taskInfo->bOvf = OsStackWaterLineGet((const UINTPTR *)taskInfo->uwBottomOfStack,//获取栈的使用情况
                                         (const UINTPTR *)taskInfo->uwTopOfStack, &taskInfo->uwPeakUsed);
    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;
}
///CPU亲和性（affinity）将任务绑在指定CPU上,用于多核CPU情况,（该函数仅在SMP模式下支持）
LITE_OS_SEC_TEXT BOOL OsTaskCpuAffiSetUnsafe(UINT32 taskID, UINT16 newCpuAffiMask, UINT16 *oldCpuAffiMask)
{
#ifdef LOSCFG_KERNEL_SMP
    LosTaskCB *taskCB = OS_TCB_FROM_TID(taskID);

    taskCB->cpuAffiMask = newCpuAffiMask;
    *oldCpuAffiMask = CPUID_TO_AFFI_MASK(taskCB->currCpu);
    if (!((*oldCpuAffiMask) & newCpuAffiMask)) {
        taskCB->signal = SIGNAL_AFFI;
        return TRUE;
    }
#else
    (VOID)taskID;
    (VOID)newCpuAffiMask;
    (VOID)oldCpuAffiMask;
#endif /* LOSCFG_KERNEL_SMP */
    return FALSE;
}

LITE_OS_SEC_TEXT_MINOR UINT32 LOS_TaskCpuAffiSet(UINT32 taskID, UINT16 cpuAffiMask)
{
    BOOL needSched = FALSE;
    UINT32 intSave;
    UINT16 currCpuMask;

    if (OS_TID_CHECK_INVALID(taskID)) {//检测taskid是否有效,task由task池分配,鸿蒙默认128个任务 ID范围[0:127]
        return LOS_ERRNO_TSK_ID_INVALID;
    }

    if (!(cpuAffiMask & LOSCFG_KERNEL_CPU_MASK)) {//检测cpu亲和力
        return LOS_ERRNO_TSK_CPU_AFFINITY_MASK_ERR;
    }

    LosTaskCB *taskCB = OS_TCB_FROM_TID(taskID);
    SCHEDULER_LOCK(intSave);
    if (taskCB->taskStatus & OS_TASK_STATUS_UNUSED) {//贴有未使用标签的处理
        SCHEDULER_UNLOCK(intSave);
        return LOS_ERRNO_TSK_NOT_CREATED;
    }
    needSched = OsTaskCpuAffiSetUnsafe(taskID, cpuAffiMask, &currCpuMask);

    SCHEDULER_UNLOCK(intSave);

    if (needSched && OS_SCHEDULER_ACTIVE) {
        LOS_MpSchedule(currCpuMask);//发送信号调度信号给目标CPU
        LOS_Schedule();//申请调度
    }

    return LOS_OK;
}
///查询任务被绑在哪个CPU上
LITE_OS_SEC_TEXT_MINOR UINT16 LOS_TaskCpuAffiGet(UINT32 taskID)
{
#ifdef LOSCFG_KERNEL_SMP
#define INVALID_CPU_AFFI_MASK   0
    UINT16 cpuAffiMask;
    UINT32 intSave;

    if (OS_TID_CHECK_INVALID(taskID)) {
        return INVALID_CPU_AFFI_MASK;
    }

    LosTaskCB *taskCB = OS_TCB_FROM_TID(taskID);
    SCHEDULER_LOCK(intSave);
    if (taskCB->taskStatus & OS_TASK_STATUS_UNUSED) { //任务必须在使用
        SCHEDULER_UNLOCK(intSave);
        return INVALID_CPU_AFFI_MASK;
    }

    cpuAffiMask = taskCB->cpuAffiMask; //获取亲和力掩码
    SCHEDULER_UNLOCK(intSave);

    return cpuAffiMask;
#else
    (VOID)taskID;
    return 1;//单核情况直接返回1 ,0号cpu对应0x01
#endif
}

/*
 * Description : Process pending signals tagged by others cores
 */
 /*!
 	由其他CPU核触发阻塞进程的信号
	函数由汇编代码调用 ..\arch\arm\arm\src\los_dispatch.S
*/
LITE_OS_SEC_TEXT_MINOR VOID OsTaskProcSignal(VOID)
{
    UINT32 ret;
	//私有且不可中断，无需保护。这个任务在其他CPU核看到它时总是在运行，所以它在执行代码的同时也可以继续接收信号
    /*
     * private and uninterruptable, no protection needed.
     * while this task is always running when others cores see it,
     * so it keeps receiving signals while follow code executing.
     */
    LosTaskCB *runTask = OsCurrTaskGet();
    if (runTask->signal == SIGNAL_NONE) {
        return;
    }

    if (runTask->signal & SIGNAL_KILL) {//意思是其他cpu发起了要干掉你的信号
        /*
         * clear the signal, and do the task deletion. if the signaled task has been
         * scheduled out, then this deletion will wait until next run.
         *///如果发出信号的任务已出调度就绪队列，则此删除将等待下次运行
        runTask->signal = SIGNAL_NONE;//清除信号，
        ret = LOS_TaskDelete(runTask->taskID);
        if (ret != LOS_OK) {
            PRINT_ERR("Task proc signal delete task(%u) failed err:0x%x\n", runTask->taskID, ret);
        }
    } else if (runTask->signal & SIGNAL_SUSPEND) {//意思是其他cpu发起了要挂起你的信号
        runTask->signal &= ~SIGNAL_SUSPEND;//任务贴上被其他CPU挂起的标签

        /* suspend killed task may fail, ignore the result */
        (VOID)LOS_TaskSuspend(runTask->taskID);
#ifdef LOSCFG_KERNEL_SMP
    } else if (runTask->signal & SIGNAL_AFFI) {//意思是下次调度其他cpu要媾和你
        runTask->signal &= ~SIGNAL_AFFI;//任务贴上被其他CPU媾和的标签

        /* pri-queue has updated, notify the target cpu */
        LOS_MpSchedule((UINT32)runTask->cpuAffiMask);//发生调度,此任务将移交给媾和CPU运行.
#endif
    }
}

LITE_OS_SEC_TEXT INT32 OsSetTaskName(LosTaskCB *taskCB, const CHAR *name, BOOL setPName)
{
    UINT32 intSave;
    errno_t err;
    const CHAR *namePtr = NULL;
    CHAR nameBuff[OS_TCB_NAME_LEN] = { 0 };

    if ((taskCB == NULL) || (name == NULL)) {
        return EINVAL;
    }

    if (LOS_IsUserAddress((VADDR_T)(UINTPTR)name)) {
        err = LOS_StrncpyFromUser(nameBuff, (const CHAR *)name, OS_TCB_NAME_LEN);
        if (err < 0) {
            return -err;
        }
        namePtr = nameBuff;
    } else {
        namePtr = name;
    }

    SCHEDULER_LOCK(intSave);

    err = strncpy_s(taskCB->taskName, OS_TCB_NAME_LEN, (VOID *)namePtr, OS_TCB_NAME_LEN - 1);
    if (err != EOK) {
        err = EINVAL;
        goto EXIT;
    }

    err = LOS_OK;
    /* if thread is main thread, then set processName as taskName */
    if (OsIsProcessThreadGroup(taskCB) && (setPName == TRUE)) {
        err = (INT32)OsSetProcessName(OS_PCB_FROM_TCB(taskCB), (const CHAR *)taskCB->taskName);
        if (err != LOS_OK) {
            err = EINVAL;
        }
    }

EXIT:
    SCHEDULER_UNLOCK(intSave);
    return err;
}

INT32 OsUserTaskOperatePermissionsCheck(const LosTaskCB *taskCB)
{
    return OsUserProcessOperatePermissionsCheck(taskCB, (UINTPTR)OsCurrProcessGet());
}

INT32 OsUserProcessOperatePermissionsCheck(const LosTaskCB *taskCB, UINTPTR processCB)
{
    if (taskCB == NULL) {
        return LOS_EINVAL;
    }

    if (processCB == (UINTPTR)OsGetDefaultProcessCB()) {
        return LOS_EINVAL;
    }

    if (taskCB->taskStatus & OS_TASK_STATUS_UNUSED) {
        return LOS_EINVAL;
    }

    if (processCB != taskCB->processCB) {
        return LOS_EPERM;
    }

    return LOS_OK;
}
///创建任务之前,检查用户态任务栈的参数,是否地址在用户空间
LITE_OS_SEC_TEXT_INIT STATIC UINT32 OsCreateUserTaskParamCheck(UINT32 processID, TSK_INIT_PARAM_S *param)
{
    UserTaskParam *userParam = NULL;

    if (param == NULL) {
        return OS_INVALID_VALUE;
    }

    userParam = &param->userParam;
    if ((processID == OS_INVALID_VALUE) && !LOS_IsUserAddress(userParam->userArea)) {//堆地址必须在用户空间
        return OS_INVALID_VALUE;
    }

    if (!LOS_IsUserAddress((UINTPTR)param->pfnTaskEntry)) {//入口函数必须在用户空间
        return OS_INVALID_VALUE;
    }
	//堆栈必须在用户空间
    if (userParam->userMapBase && !LOS_IsUserAddressRange(userParam->userMapBase, userParam->userMapSize)) {
        return OS_INVALID_VALUE;
    }
	//检查堆,栈范围
    if (!LOS_IsUserAddress(userParam->userSP)) {
        return OS_INVALID_VALUE;
    }

    return LOS_OK;
}
///创建一个用户态任务
LITE_OS_SEC_TEXT_INIT UINT32 OsCreateUserTask(UINTPTR processID, TSK_INIT_PARAM_S *initParam)
{
    UINT32 taskID;
    UINT32 ret;
    UINT32 intSave;

    ret = OsCreateUserTaskParamCheck(processID, initParam);//检查参数,堆栈,入口地址必须在用户空间
    if (ret != LOS_OK) {
        return ret;
    }
	//这里可看出一个任务有两个栈,内核态栈(内核指定栈大小)和用户态栈(用户指定栈大小)
    initParam->uwStackSize = OS_USER_TASK_SYSCALL_STACK_SIZE;
    initParam->usTaskPrio = OS_TASK_PRIORITY_LOWEST;//设置最低优先级 31级
    initParam->policy = LOS_SCHED_RR;//调度方式为抢占式,注意鸿蒙不仅仅只支持抢占式调度方式
    if (processID == OS_INVALID_VALUE) {//外面没指定进程ID的处理
        SCHEDULER_LOCK(intSave);
        LosProcessCB *processCB = OsCurrProcessGet();
        initParam->processID = (UINTPTR)processCB;
        initParam->consoleID = processCB->consoleID;//任务控制台ID归属
        SCHEDULER_UNLOCK(intSave);
    } else {//进程已经创建
        initParam->processID = processID;//进程ID赋值
        initParam->consoleID = 0;//默认0号控制台
    }

    ret = LOS_TaskCreateOnly(&taskID, initParam);//只创建task实体,不申请调度
    if (ret != LOS_OK) {
        return OS_INVALID_VALUE;
    }

    return taskID;
}
///获取任务的调度方式
LITE_OS_SEC_TEXT INT32 LOS_GetTaskScheduler(INT32 taskID)
{
    UINT32 intSave;
    INT32 policy;
    SchedParam param = { 0 };

    if (OS_TID_CHECK_INVALID(taskID)) {
        return -LOS_EINVAL;
    }

    LosTaskCB *taskCB = OS_TCB_FROM_TID(taskID);
    SCHEDULER_LOCK(intSave);
    if (taskCB->taskStatus & OS_TASK_STATUS_UNUSED) {//状态不能是没有在使用
        policy = -LOS_EINVAL;
        OS_GOTO_ERREND();
    }

    taskCB->ops->schedParamGet(taskCB, &param);
    policy = (INT32)param.policy;

LOS_ERREND:
    SCHEDULER_UNLOCK(intSave);
    return policy;
}

//设置任务的调度信息
LITE_OS_SEC_TEXT INT32 LOS_SetTaskScheduler(INT32 taskID, UINT16 policy, UINT16 priority)
{
    SchedParam param = { 0 };
    UINT32 intSave;

    if (OS_TID_CHECK_INVALID(taskID)) {
        return LOS_ESRCH;
    }

    if (priority > OS_TASK_PRIORITY_LOWEST) {
        return LOS_EINVAL;
    }

    if ((policy != LOS_SCHED_FIFO) && (policy != LOS_SCHED_RR)) {
        return LOS_EINVAL;
    }

    LosTaskCB *taskCB = OS_TCB_FROM_TID(taskID);
    if (taskCB->taskStatus & OS_TASK_FLAG_SYSTEM_TASK) {
        return LOS_EPERM;
    }

    SCHEDULER_LOCK(intSave);
    if (taskCB->taskStatus & OS_TASK_STATUS_UNUSED) {
         SCHEDULER_UNLOCK(intSave);
        return LOS_EINVAL;
    }

    taskCB->ops->schedParamGet(taskCB, &param);
    param.policy = policy;
    param.priority = priority;
    BOOL needSched = taskCB->ops->schedParamModify(taskCB, &param);
    SCHEDULER_UNLOCK(intSave);

    LOS_MpSchedule(OS_MP_CPU_ALL);
    if (needSched && OS_SCHEDULER_ACTIVE) {
        LOS_Schedule();
    }

    return LOS_OK;
}

STATIC UINT32 OsTaskJoinCheck(UINT32 taskID)
{
    if (OS_TID_CHECK_INVALID(taskID)) {
        return LOS_EINVAL;
    }

    if (OS_INT_ACTIVE) {
        return LOS_EINTR;
    }

    if (!OsPreemptable()) {
        return LOS_EINVAL;
    }

    LosTaskCB *taskCB = OS_TCB_FROM_TID(taskID);
    if (taskCB->taskStatus & OS_TASK_FLAG_SYSTEM_TASK) {
        return LOS_EPERM;
    }

    if (taskCB == OsCurrTaskGet()) {
        return LOS_EDEADLK;
    }

    return LOS_OK;
}

UINT32 LOS_TaskJoin(UINT32 taskID, UINTPTR *retval)
{
    UINT32 intSave;
    LosTaskCB *runTask = OsCurrTaskGet();
    UINT32 errRet;

    errRet = OsTaskJoinCheck(taskID);
    if (errRet != LOS_OK) {
        return errRet;
    }

    LosTaskCB *taskCB = OS_TCB_FROM_TID(taskID);
    SCHEDULER_LOCK(intSave);
    if (taskCB->taskStatus & OS_TASK_STATUS_UNUSED) {
        SCHEDULER_UNLOCK(intSave);
        return LOS_EINVAL;
    }

    if (runTask->processCB != taskCB->processCB) {
        SCHEDULER_UNLOCK(intSave);
        return LOS_EPERM;
    }

    errRet = OsTaskJoinPendUnsafe(taskCB);
    SCHEDULER_UNLOCK(intSave);

    if (errRet == LOS_OK) {
        LOS_Schedule();

        if (retval != NULL) {
            *retval = (UINTPTR)taskCB->joinRetval;
        }

        (VOID)LOS_TaskDelete(taskID);
        return LOS_OK;
    }

    return errRet;
}

UINT32 LOS_TaskDetach(UINT32 taskID)
{
    UINT32 intSave;
    LosTaskCB *runTask = OsCurrTaskGet();
    UINT32 errRet;

    if (OS_TID_CHECK_INVALID(taskID)) {
        return LOS_EINVAL;
    }

    if (OS_INT_ACTIVE) {
        return LOS_EINTR;
    }

    LosTaskCB *taskCB = OS_TCB_FROM_TID(taskID);
    SCHEDULER_LOCK(intSave);
    if (taskCB->taskStatus & OS_TASK_STATUS_UNUSED) {
        SCHEDULER_UNLOCK(intSave);
        return LOS_EINVAL;
    }

    if (runTask->processCB != taskCB->processCB) {
        SCHEDULER_UNLOCK(intSave);
        return LOS_EPERM;
    }

    if (taskCB->taskStatus & OS_TASK_STATUS_EXIT) {
        SCHEDULER_UNLOCK(intSave);
        return LOS_TaskJoin(taskID, NULL);
    }

    errRet = OsTaskSetDetachUnsafe(taskCB);
    SCHEDULER_UNLOCK(intSave);
    return errRet;
}

LITE_OS_SEC_TEXT UINT32 LOS_GetSystemTaskMaximum(VOID)
{
    return g_taskMaxNum;
}

LosTaskCB *OsGetDefaultTaskCB(VOID)
{
    return &g_taskCBArray[g_taskMaxNum];
}

LITE_OS_SEC_TEXT VOID OsWriteResourceEvent(UINT32 events)
{
    (VOID)LOS_EventWrite(&g_resourceEvent, events);
}

LITE_OS_SEC_TEXT VOID OsWriteResourceEventUnsafe(UINT32 events)
{
    (VOID)OsEventWriteUnsafe(&g_resourceEvent, events, FALSE, NULL);
}
///资源回收任务
STATIC VOID OsResourceRecoveryTask(VOID)
{
    UINT32 ret;

    while (1) {//死循环,回收资源不存在退出情况,只要系统在运行资源就需要回收
        ret = LOS_EventRead(&g_resourceEvent, OS_RESOURCE_EVENT_MASK,
                            LOS_WAITMODE_OR | LOS_WAITMODE_CLR, LOS_WAIT_FOREVER);//读取资源事件
        if (ret & (OS_RESOURCE_EVENT_FREE | OS_RESOURCE_EVENT_OOM)) {//收到资源释放或内存异常情况
            OsTaskCBRecycleToFree();
            OsProcessCBRecycleToFree();//回收进程到空闲进程池
        }

#ifdef LOSCFG_ENABLE_OOM_LOOP_TASK //内存溢出监测任务开关
        if (ret & OS_RESOURCE_EVENT_OOM) {//触发了这个事件
            (VOID)OomCheckProcess();//检查进程的内存溢出情况
        }
#endif
    }
}
///创建一个回收资源的任务
LITE_OS_SEC_TEXT UINT32 OsResourceFreeTaskCreate(VOID)
{
    UINT32 ret;
    UINT32 taskID;
    TSK_INIT_PARAM_S taskInitParam;

    ret = LOS_EventInit((PEVENT_CB_S)&g_resourceEvent);//初始化资源事件
    if (ret != LOS_OK) {
        return LOS_NOK;
    }

    (VOID)memset_s((VOID *)(&taskInitParam), sizeof(TSK_INIT_PARAM_S), 0, sizeof(TSK_INIT_PARAM_S));
    taskInitParam.pfnTaskEntry = (TSK_ENTRY_FUNC)OsResourceRecoveryTask;//入口函数
    taskInitParam.uwStackSize = OS_TASK_RESOURCE_STATIC_SIZE;
    taskInitParam.pcName = "ResourcesTask";
    taskInitParam.usTaskPrio = OS_TASK_RESOURCE_FREE_PRIORITY;// 5 ,优先级很高
    ret = LOS_TaskCreate(&taskID, &taskInitParam);
    if (ret == LOS_OK) {
        OS_TCB_FROM_TID(taskID)->taskStatus |= OS_TASK_FLAG_NO_DELETE;
    }
    return ret;
}

LOS_MODULE_INIT(OsResourceFreeTaskCreate, LOS_INIT_LEVEL_KMOD_TASK);//资源回收任务初始化
