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

#define OS_CHECK_TASK_BLOCK (OS_TASK_STATUS_DELAY |    \
                             OS_TASK_STATUS_PENDING |  \
                             OS_TASK_STATUS_SUSPENDED)

/* temp task blocks for booting procedure */
LITE_OS_SEC_BSS STATIC LosTaskCB                g_mainTask[LOSCFG_KERNEL_CORE_NUM];//启动引导过程中使用的临时任务

LosTaskCB *OsGetMainTask()
{
    return (LosTaskCB *)(g_mainTask + ArchCurrCpuid());
}

VOID OsSetMainTask()
{
    UINT32 i;
    CHAR *name = "osMain";//任务名称

	//为每个CPU core 设置mainTask
    for (i = 0; i < LOSCFG_KERNEL_CORE_NUM; i++) {
        g_mainTask[i].taskStatus = OS_TASK_STATUS_UNUSED;
        g_mainTask[i].taskID = LOSCFG_BASE_CORE_TSK_LIMIT;//128
        g_mainTask[i].priority = OS_TASK_PRIORITY_LOWEST;//31
#ifdef LOSCFG_KERNEL_SMP_LOCKDEP
        g_mainTask[i].lockDep.lockDepth = 0;
        g_mainTask[i].lockDep.waitLock = NULL;
#endif
        (VOID)strncpy_s(g_mainTask[i].taskName, OS_TCB_NAME_LEN, name, OS_TCB_NAME_LEN - 1);
        LOS_ListInit(&g_mainTask[i].lockList);//初始化任务锁链表,上面挂的是任务已申请到的互斥锁
    }
}
///空闲任务,每个CPU都有自己的空闲任务
LITE_OS_SEC_TEXT WEAK VOID OsIdleTask(VOID)
{
    while (1) {//只有一个死循环
        WFI;
    }
}

//插入一个TCB到空闲链表
STATIC INLINE VOID OsInsertTCBToFreeList(LosTaskCB *taskCB)
{
    UINT32 taskID = taskCB->taskID;
    (VOID)memset_s(taskCB, sizeof(LosTaskCB), 0, sizeof(LosTaskCB));
    taskCB->taskID = taskID;
    taskCB->taskStatus = OS_TASK_STATUS_UNUSED;
    taskCB->processID = OS_INVALID_VALUE;
    LOS_ListAdd(&g_losFreeTask, &taskCB->pendList);//内核挂在g_losFreeTask上的任务都是由pendList完成
}//查找task 就通过 OS_TCB_FROM_PENDLIST 来完成,相当于由LOS_DL_LIST找到LosTaskCB
//把那些和参数任务绑在一起的task唤醒.
LITE_OS_SEC_TEXT_INIT VOID OsTaskJoinPostUnsafe(LosTaskCB *taskCB)
{
    LosTaskCB *resumedTask = NULL;

    if (taskCB->taskStatus & OS_TASK_FLAG_PTHREAD_JOIN) {//join任务处理
        if (!LOS_ListEmpty(&taskCB->joinList)) {//注意到了这里 joinList中的节点身上都有阻塞标签
            resumedTask = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&(taskCB->joinList)));//通过贴有JOIN标签链表的第一个节点找到Task
            OsTaskWakeClearPendMask(resumedTask);
            OsSchedTaskWake(resumedTask);
        }
    }
    taskCB->taskStatus |= OS_TASK_STATUS_EXIT;//贴上任务退出标签
}

LITE_OS_SEC_TEXT UINT32 OsTaskJoinPendUnsafe(LosTaskCB *taskCB)
{
    LosProcessCB *processCB = OS_PCB_FROM_PID(taskCB->processID);
    if (!(processCB->processStatus & OS_PROCESS_STATUS_RUNNING)) {
        return LOS_EPERM;
    }

    if (taskCB->taskStatus & OS_TASK_STATUS_INIT) {
        return LOS_EINVAL;
    }

    if (taskCB->taskStatus & OS_TASK_STATUS_EXIT) {
        return LOS_OK;
    }

    if ((taskCB->taskStatus & OS_TASK_FLAG_PTHREAD_JOIN) && LOS_ListEmpty(&taskCB->joinList)) {
        OsTaskWaitSetPendMask(OS_TASK_WAIT_JOIN, taskCB->taskID, LOS_WAIT_FOREVER);
        return OsSchedTaskWait(&taskCB->joinList, LOS_WAIT_FOREVER, TRUE);
    }

    return LOS_EINVAL;
}
///任务设置分离模式  Deatch和JOIN是一对有你没我的状态
LITE_OS_SEC_TEXT UINT32 OsTaskSetDetachUnsafe(LosTaskCB *taskCB)
{
    LosProcessCB *processCB = OS_PCB_FROM_PID(taskCB->processID);//获取进程实体
    if (!(processCB->processStatus & OS_PROCESS_STATUS_RUNNING)) {//进程必须是运行状态
        return LOS_EPERM;
    }

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
LITE_OS_SEC_TEXT_INIT UINT32 OsTaskInit(VOID)
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
        LOS_ListTailInsert(&g_losFreeTask, &g_taskCBArray[index].pendList);//通过pendList节点插入空闲任务列表 
    }//注意:这里挂的是pendList节点,所以取TCB也要通过 OS_TCB_FROM_PENDLIST 取.

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
    Percpu *perCpu = OsPercpuGet();//获取当前Cpu信息
    return perCpu->idleTaskID;//返回当前CPU 空闲任务ID
}
///创建一个空闲任务
LITE_OS_SEC_TEXT_INIT UINT32 OsIdleTaskCreate(VOID)
{
    UINT32 ret;
    TSK_INIT_PARAM_S taskInitParam;
    Percpu *perCpu = OsPercpuGet();//获取当前运行CPU信息
    UINT32 *idleTaskID = &perCpu->idleTaskID;//每个CPU都有一个空闲任务

    (VOID)memset_s((VOID *)(&taskInitParam), sizeof(TSK_INIT_PARAM_S), 0, sizeof(TSK_INIT_PARAM_S));//任务初始参数清0
    taskInitParam.pfnTaskEntry = (TSK_ENTRY_FUNC)OsIdleTask;//入口函数
    taskInitParam.uwStackSize = LOSCFG_BASE_CORE_TSK_IDLE_STACK_SIZE;//任务栈大小 2K
    taskInitParam.pcName = "Idle";//任务名称 叫pcName有点怪怪的,不能换个撒
    taskInitParam.usTaskPrio = OS_TASK_PRIORITY_LOWEST;//默认最低优先级 31
    taskInitParam.processID = OsGetIdleProcessID();
#ifdef LOSCFG_KERNEL_SMP
    taskInitParam.usCpuAffiMask = CPUID_TO_AFFI_MASK(ArchCurrCpuid());//每个idle任务只在单独的cpu上运行
#endif
    ret = LOS_TaskCreateOnly(idleTaskID, &taskInitParam);
    LosTaskCB *idleTask = OS_TCB_FROM_TID(*idleTaskID);
    idleTask->taskStatus |= OS_TASK_FLAG_SYSTEM_TASK;
    OsSchedSetIdleTaskSchedParam(idleTask);

    return ret;
}

/*
 * Description : get id of current running task.
 * Return      : task id
 */
LITE_OS_SEC_TEXT UINT32 LOS_CurTaskIDGet(VOID)//获取当前任务的ID
{
    LosTaskCB *runTask = OsCurrTaskGet();

    if (runTask == NULL) {
        return LOS_ERRNO_TSK_ID_INVALID;
    }
    return runTask->taskID;
}

STATIC INLINE UINT32 OsTaskSyncCreate(LosTaskCB *taskCB)
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

STATIC INLINE VOID OsTaskSyncDestroy(UINT32 syncSignal)
{
#ifdef LOSCFG_KERNEL_SMP_TASK_SYNC
    (VOID)LOS_SemDelete(syncSignal);
#else
    (VOID)syncSignal;
#endif
}

#ifdef LOSCFG_KERNEL_SMP
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

STATIC INLINE VOID OsTaskSyncWake(const LosTaskCB *taskCB)
{
#ifdef LOSCFG_KERNEL_SMP_TASK_SYNC
    (VOID)OsSemPostUnsafe(taskCB->syncSignal, NULL);
#else
    (VOID)taskCB;
#endif
}

STATIC VOID OsTaskReleaseHoldLock(LosProcessCB *processCB, LosTaskCB *taskCB)
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
    if (processCB->processMode == OS_USER_MODE) {
        OsFutexNodeDeleteFromFutexHash(&taskCB->futex, TRUE, NULL, NULL);
    }
#endif

    OsTaskJoinPostUnsafe(taskCB);

    OsTaskSyncWake(taskCB);
}
///一个任务的退出过程
LITE_OS_SEC_TEXT VOID OsTaskToExit(LosTaskCB *taskCB, UINT32 status)
{
    UINT32 intSave;

    LosProcessCB *runProcess = OS_PCB_FROM_PID(taskCB->processID);
    LosTaskCB *mainTask = OS_TCB_FROM_TID(runProcess->threadGroupID);
    if (mainTask == taskCB) {//如果参数任务就是主任务
        OsTaskExitGroup(status);//task退出线程组
    }

    SCHEDULER_LOCK(intSave);
    if (runProcess->threadNumber == 1) { /* 1: The last task of the process exits *///进程的最后一个任务退出
        SCHEDULER_UNLOCK(intSave);
        (VOID)OsProcessExit(taskCB, status);//调用进程退出流程
        return;
    }

    if ((taskCB->taskStatus & OS_TASK_FLAG_EXIT_KILL) || !(taskCB->taskStatus & OS_TASK_FLAG_PTHREAD_JOIN)) {
        UINT32 ret = OsTaskDeleteUnsafe(taskCB, status, intSave);
        LOS_Panic("Task delete failed! ERROR : 0x%x\n", ret);
        return;
    }

    OsTaskReleaseHoldLock(runProcess, taskCB);

    OsSchedResched();//申请调度
    SCHEDULER_UNLOCK(intSave);
    return;
}

/*
 * Description : All task entry
 * Input       : taskID     --- The ID of the task to be run
 *///所有任务的入口函数,OsTaskEntry是在new task OsTaskStackInit 时指定的
LITE_OS_SEC_TEXT_INIT VOID OsTaskEntry(UINT32 taskID)
{
    LosTaskCB *taskCB = NULL;

    LOS_ASSERT(!OS_TID_CHECK_INVALID(taskID));

    /*
     * task scheduler needs to be protected throughout the whole process
     * from interrupt and other cores. release task spinlock and enable
     * interrupt in sequence at the task entry.
     */
    LOS_SpinUnlock(&g_taskSpin);//释放任务自旋锁
    (VOID)LOS_IntUnLock();//恢复中断

    taskCB = OS_TCB_FROM_TID(taskID);
    taskCB->joinRetval = taskCB->taskEntry(taskCB->args[0], taskCB->args[1],//调用任务的入口函数
                                           taskCB->args[2], taskCB->args[3]); /* 2 & 3: just for args array index */
    if (!(taskCB->taskStatus & OS_TASK_FLAG_PTHREAD_JOIN)) {
        taskCB->joinRetval = 0;//结合数为0
    }
	
    OsTaskToExit(taskCB, 0);//到这里任务跑完了要退出了
}
///任务创建参数检查
LITE_OS_SEC_TEXT_INIT STATIC UINT32 OsTaskCreateParamCheck(const UINT32 *taskID,
    TSK_INIT_PARAM_S *initParam, VOID **pool)
{
    LosProcessCB *process = NULL;
    UINT32 poolSize = OS_SYS_MEM_SIZE;
    *pool = (VOID *)m_aucSysMem1;//默认使用

    if (taskID == NULL) {
        return LOS_ERRNO_TSK_ID_INVALID;
    }

    if (initParam == NULL) {
        return LOS_ERRNO_TSK_PTR_NULL;
    }

    process = OS_PCB_FROM_PID(initParam->processID);
    if (process->processMode > OS_USER_MODE) {
        return LOS_ERRNO_TSK_ID_INVALID;
    }

    if (!OsProcessIsUserMode(process)) {
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
LITE_OS_SEC_TEXT_INIT STATIC VOID OsTaskStackAlloc(VOID **topStack, UINT32 stackSize, VOID *pool)
{
    *topStack = (VOID *)LOS_MemAllocAlign(pool, stackSize, LOSCFG_STACK_POINT_ALIGN_SIZE);
}

//释放任务内核资源
STATIC VOID OsTaskKernelResourcesToFree(UINT32 syncSignal, UINTPTR topOfStack)
{
    VOID *poolTmp = (VOID *)m_aucSysMem1;

    OsTaskSyncDestroy(syncSignal);

    (VOID)LOS_MemFree(poolTmp, (VOID *)topOfStack);
}
///从回收链表中回收任务到空闲链表
LITE_OS_SEC_TEXT VOID OsTaskCBRecycleToFree()
{
    LosTaskCB *taskCB = NULL;
    UINT32 intSave;

    SCHEDULER_LOCK(intSave);
    while (!LOS_ListEmpty(&g_taskRecycleList)) {//不空就一个一个回收任务
        taskCB = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&g_taskRecycleList));//取出第一个待回收任务
        LOS_ListDelete(&taskCB->pendList);//从回收链表上将自己摘除
        SCHEDULER_UNLOCK(intSave);

        OsTaskResourcesToFree(taskCB);//释放任务资源

        SCHEDULER_LOCK(intSave);
    }
    SCHEDULER_UNLOCK(intSave);
}

LITE_OS_SEC_TEXT VOID OsTaskResourcesToFree(LosTaskCB *taskCB)
{
    UINT32 syncSignal = LOSCFG_BASE_IPC_SEM_LIMIT;
    UINT32 intSave;
    UINTPTR topOfStack;

#ifdef LOSCFG_KERNEL_VM
    LosProcessCB *processCB = OS_PCB_FROM_PID(taskCB->processID);
    if (OsProcessIsUserMode(processCB) && (taskCB->userMapBase != 0)) {
        SCHEDULER_LOCK(intSave);
        UINT32 mapBase = (UINTPTR)taskCB->userMapBase;
        UINT32 mapSize = taskCB->userMapSize;
        taskCB->userMapBase = 0;
        taskCB->userArea = 0;
        SCHEDULER_UNLOCK(intSave);

        LOS_ASSERT(!(processCB->vmSpace == NULL));
        UINT32 ret = OsUnMMap(processCB->vmSpace, (UINTPTR)mapBase, mapSize);
        if ((ret != LOS_OK) && (mapBase != 0) && !(processCB->processStatus & OS_PROCESS_STATUS_INIT)) {
            PRINT_ERR("process(%u) ummap user task(%u) stack failed! mapbase: 0x%x size :0x%x, error: %d\n",
                      processCB->processID, taskCB->taskID, mapBase, mapSize, ret);
        }

#ifdef LOSCFG_KERNEL_LITEIPC
        LiteIpcRemoveServiceHandle(taskCB);
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
        OsClearSigInfoTmpList(&(taskCB->sig));
        OsInsertTCBToFreeList(taskCB);
        SCHEDULER_UNLOCK(intSave);
    }
    return;
}
///任务基本信息的初始化
LITE_OS_SEC_TEXT_INIT STATIC VOID OsTaskCBInitBase(LosTaskCB *taskCB,
                                                   const VOID *stackPtr,
                                                   const VOID *topStack,
                                                   const TSK_INIT_PARAM_S *initParam)
{
    taskCB->stackPointer = (VOID *)stackPtr;//内核态SP位置
    taskCB->args[0]      = initParam->auwArgs[0]; /* 0~3: just for args array index */
    taskCB->args[1]      = initParam->auwArgs[1];
    taskCB->args[2]      = initParam->auwArgs[2];
    taskCB->args[3]      = initParam->auwArgs[3];
    taskCB->topOfStack   = (UINTPTR)topStack;	//内核态栈顶
    taskCB->stackSize    = initParam->uwStackSize;//
    taskCB->priority     = initParam->usTaskPrio;
    taskCB->taskEntry    = initParam->pfnTaskEntry;
    taskCB->signal       = SIGNAL_NONE;

#ifdef LOSCFG_KERNEL_SMP
    taskCB->currCpu      = OS_TASK_INVALID_CPUID;
    taskCB->cpuAffiMask  = (initParam->usCpuAffiMask) ?
                            initParam->usCpuAffiMask : LOSCFG_KERNEL_CPU_MASK;
#endif
#ifdef LOSCFG_KERNEL_LITEIPC
    LOS_ListInit(&(taskCB->msgListHead));//初始化 liteipc的消息链表 
#endif
    taskCB->policy = (initParam->policy == LOS_SCHED_FIFO) ? LOS_SCHED_FIFO : LOS_SCHED_RR;//调度模式
    taskCB->taskStatus = OS_TASK_STATUS_INIT;
    if (initParam->uwResved & LOS_TASK_ATTR_JOINABLE) {
        taskCB->taskStatus |= OS_TASK_FLAG_PTHREAD_JOIN;
        LOS_ListInit(&taskCB->joinList);
    }

    taskCB->futex.index = OS_INVALID_VALUE;
    LOS_ListInit(&taskCB->lockList);//初始化互斥锁链表
    SET_SORTLIST_VALUE(&taskCB->sortList, OS_SORT_LINK_INVALID_TIME);
}
///任务初始化
STATIC UINT32 OsTaskCBInit(LosTaskCB *taskCB, const TSK_INIT_PARAM_S *initParam,
                           const VOID *stackPtr, const VOID *topStack)
{
    UINT32 intSave;
    UINT32 ret;
    UINT32 numCount;
    UINT16 mode;
    LosProcessCB *processCB = NULL;

    OsTaskCBInitBase(taskCB, stackPtr, topStack, initParam);//初始化任务的基本信息,
    					//taskCB->stackPointer指向内核态栈 sp位置,该位置存着 任务初始上下文

    SCHEDULER_LOCK(intSave);
    processCB = OS_PCB_FROM_PID(initParam->processID);//通过ID获取PCB ,单核进程数最多64个
    taskCB->processID = processCB->processID;//进程-线程的父子关系绑定
    mode = processCB->processMode;//模式方式同步process
    LOS_ListTailInsert(&(processCB->threadSiblingList), &(taskCB->threadList));//挂入进程的线程链表
    if (mode == OS_USER_MODE) {//任务支持用户态时,将改写 taskCB->stackPointer = initParam->userParam.userSP
        taskCB->userArea = initParam->userParam.userArea;
        taskCB->userMapBase = initParam->userParam.userMapBase;
        taskCB->userMapSize = initParam->userParam.userMapSize;
        OsUserTaskStackInit(taskCB->stackPointer, (UINTPTR)taskCB->taskEntry, initParam->userParam.userSP);//初始化用户态任务栈
        //这里要注意,任务的上下文是始终保存在内核栈空间,而用户态时运行在用户态栈空间.(context->SP = userSP 指向了用户态栈空间)
    }

    if (!processCB->threadNumber) {//进程线程数量为0时，
        processCB->threadGroupID = taskCB->taskID;//任务为线程组 组长
    }
    processCB->threadNumber++;//这里说明 线程和TASK是一个意思 threadNumber代表活动线程数,thread消亡的时候会 threadNumber--

    numCount = processCB->threadCount;//代表总线程数，包括销毁的，只要存在过的都算，这个值也就是在这里用下，
    processCB->threadCount++;//线程总数++,注意这个数会一直累加的,哪怕thread最后退出了,这个统计这个进程曾经存在过的线程数量
    SCHEDULER_UNLOCK(intSave);

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
///获取一个空闲TCB
LITE_OS_SEC_TEXT LosTaskCB *OsGetFreeTaskCB(VOID)
{
    UINT32 intSave;
    LosTaskCB *taskCB = NULL;

    SCHEDULER_LOCK(intSave);
    if (LOS_ListEmpty(&g_losFreeTask)) {//全局空闲task为空
        SCHEDULER_UNLOCK(intSave);
        PRINT_ERR("No idle TCB in the system!\n");
#ifdef LOSCFG_DEBUG_VERSION
	(VOID)OsShellCmdTskInfoGet(OS_ALL_TASK_MASK, NULL, OS_PROCESS_INFO_ALL);
#endif
        return NULL;
    }

    taskCB = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&g_losFreeTask));//拿到第一节点并通过pendlist拿到完整的TCB
    LOS_ListDelete(LOS_DL_LIST_FIRST(&g_losFreeTask));//从g_losFreeTask链表中摘除自己
    SCHEDULER_UNLOCK(intSave);

    return taskCB;
}
///创建任务，并使该任务进入suspend状态，不对该任务进行调度。如果需要调度，可以调用LOS_TaskResume使该任务进入ready状态
LITE_OS_SEC_TEXT_INIT UINT32 LOS_TaskCreateOnly(UINT32 *taskID, TSK_INIT_PARAM_S *initParam)
{
    UINT32 intSave, errRet;
    VOID *topStack = NULL;
    VOID *stackPtr = NULL;
    LosTaskCB *taskCB = NULL;
    VOID *pool = NULL;

    errRet = OsTaskCreateParamCheck(taskID, initParam, &pool);//参数检查
    if (errRet != LOS_OK) {
        return errRet;
    }

    taskCB = OsGetFreeTaskCB();//从g_losFreeTask中获取,还记得吗任务池中最多默认128个
    if (taskCB == NULL) {
        errRet = LOS_ERRNO_TSK_TCB_UNAVAILABLE;
        goto LOS_ERREND;
    }

    errRet = OsTaskSyncCreate(taskCB);//SMP cpu多核间负载均衡相关
    if (errRet != LOS_OK) {
        goto LOS_ERREND_REWIND_TCB;
    }
	//OsTaskStackAlloc 只在LOS_TaskCreateOnly中被调用,此处是分配任务在内核态栈空间 
    OsTaskStackAlloc(&topStack, initParam->uwStackSize, pool);//为任务分配内核栈空间,注意此内存来自系统内核空间
    if (topStack == NULL) {
        errRet = LOS_ERRNO_TSK_NO_MEMORY;
        goto LOS_ERREND_REWIND_SYNC;
    }

    stackPtr = OsTaskStackInit(taskCB->taskID, initParam->uwStackSize, topStack, TRUE);//初始化内核态任务栈,返回栈SP位置
    errRet = OsTaskCBInit(taskCB, initParam, stackPtr, topStack);//初始化TCB,包括绑定进程等操作
    if (errRet != LOS_OK) {
        goto LOS_ERREND_TCB_INIT;
    }
    if (OsConsoleIDSetHook != NULL) {//每个任务都可以有属于自己的控制台
        OsConsoleIDSetHook(taskCB->taskID, OsCurrTaskGet()->taskID);//设置控制台ID
    }

    *taskID = taskCB->taskID;
    OsHookCall(LOS_HOOK_TYPE_TASK_CREATE, taskCB);
    return LOS_OK;

LOS_ERREND_TCB_INIT:
    (VOID)LOS_MemFree(pool, topStack);
LOS_ERREND_REWIND_SYNC:
#ifdef LOSCFG_KERNEL_SMP_TASK_SYNC
    OsTaskSyncDestroy(taskCB->syncSignal);
#endif
LOS_ERREND_REWIND_TCB:
    SCHEDULER_LOCK(intSave);
    OsInsertTCBToFreeList(taskCB);//归还freetask
    SCHEDULER_UNLOCK(intSave);
LOS_ERREND:
    return errRet;
}
///创建任务，并使该任务进入ready状态，如果就绪队列中没有更高优先级的任务，则运行该任务
LITE_OS_SEC_TEXT_INIT UINT32 LOS_TaskCreate(UINT32 *taskID, TSK_INIT_PARAM_S *initParam)
{
    UINT32 ret;
    UINT32 intSave;
    LosTaskCB *taskCB = NULL;

    if (initParam == NULL) {
        return LOS_ERRNO_TSK_PTR_NULL;
    }

    if (OS_INT_ACTIVE) {
        return LOS_ERRNO_TSK_YIELD_IN_INT;
    }

    if (OsProcessIsUserMode(OsCurrProcessGet())) {
        initParam->processID = OsGetKernelInitProcessID();
    } else {
        initParam->processID = OsCurrProcessGet()->processID;
    }

    ret = LOS_TaskCreateOnly(taskID, initParam);
    if (ret != LOS_OK) {
        return ret;
    }
    taskCB = OS_TCB_FROM_TID(*taskID);

    SCHEDULER_LOCK(intSave);
    OsSchedTaskEnQueue(taskCB);
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
    LosTaskCB *taskCB = NULL;
    BOOL needSched = FALSE;

    if (OS_TID_CHECK_INVALID(taskID)) {
        return LOS_ERRNO_TSK_ID_INVALID;
    }

    taskCB = OS_TCB_FROM_TID(taskID);//拿到任务实体
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

    taskCB->taskStatus &= ~OS_TASK_STATUS_SUSPENDED;
    if (!(taskCB->taskStatus & OS_CHECK_TASK_BLOCK)) {
        OsSchedTaskEnQueue(taskCB);
        if (OS_SCHEDULER_ACTIVE) {
            needSched = TRUE;
        }
    }
    SCHEDULER_UNLOCK(intSave);

    LOS_MpSchedule(OS_MP_CPU_ALL);
    if (needSched) {
        LOS_Schedule();
    }

    return LOS_OK;

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
    UINT16 tempStatus;

    tempStatus = taskCB->taskStatus;
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

    if (tempStatus & OS_TASK_STATUS_READY) {
        OsSchedTaskDeQueue(taskCB);
    }

    taskCB->taskStatus |= OS_TASK_STATUS_SUSPENDED;
    OsHookCall(LOS_HOOK_TYPE_MOVEDTASKTOSUSPENDEDLIST, taskCB);
    if (taskCB == OsCurrTaskGet()) {
        OsSchedResched();
    }

    return LOS_OK;
}
///外部接口，对OsTaskSuspend的封装
LITE_OS_SEC_TEXT_INIT UINT32 LOS_TaskSuspend(UINT32 taskID)
{
    UINT32 intSave;
    LosTaskCB *taskCB = NULL;
    UINT32 errRet;

    if (OS_TID_CHECK_INVALID(taskID)) {
        return LOS_ERRNO_TSK_ID_INVALID;
    }

    taskCB = OS_TCB_FROM_TID(taskID);
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

//删除一个正在运行的任务
LITE_OS_SEC_TEXT VOID OsRunTaskToDelete(LosTaskCB *runTask)
{
    LosProcessCB *processCB = OS_PCB_FROM_PID(runTask->processID);//拿到task所属进程
    OsTaskReleaseHoldLock(processCB, runTask);//task还锁
    OsTaskStatusUnusedSet(runTask);//task重置为未使用状态，等待回收

    LOS_ListDelete(&runTask->threadList);//从进程的线程链表中将自己摘除
    processCB->threadNumber--;//进程的活动task --，注意进程还有一个记录总task的变量 processCB->threadCount
    LOS_ListTailInsert(&g_taskRecycleList, &runTask->pendList);//将task插入回收链表，等待回收资源再利用
    OsEventWriteUnsafe(&g_resourceEvent, OS_RESOURCE_EVENT_FREE, FALSE, NULL);//发送释放资源的事件,事件由 OsResourceRecoveryTask 消费

    OsSchedResched();//申请调度
    return;
}
/**
 * @brief 获取参数位图中最高位为1的索引位 例如: 00110110 返回 5
 * @verbatim
    CLZ 用于计算操作数最高端0的个数，这条指令主要用于以下两个场合
    　　1.计算操作数规范化（使其最高位为1）时需要左移的位数
    　　2.确定一个优先级掩码中最高优先级
 * @endverbatim
 * @param bitmap 
 * @return UINT16 
 */
/*
 * Check if needs to do the delete operation on the running task.
 * Return TRUE, if needs to do the deletion.
 * Rerturn FALSE, if meets following circumstances:
 * 1. Do the deletion across cores, if SMP is enabled
 * 2. Do the deletion when preemption is disabled
 * 3. Do the deletion in hard-irq
 * then LOS_TaskDelete will directly return with 'ret' value.
 */

/**
 * @brief 
 * @verbatim
    检查是否需要对正在运行的任务执行删除操作,如果需要删除，则返回TRUE。
    如果满足以下情况，则返回FALSE：
    1.如果启用了SMP，则跨CPU执行删除
    2.禁用抢占时执行删除
    3.在硬irq中删除
    然后LOS_TaskDelete将直接返回ret值
 * @endverbatim
 * @param taskCB 
 * @param ret 
 * @return STATIC 
 */
STATIC BOOL OsRunTaskToDeleteCheckOnRun(LosTaskCB *taskCB, UINT32 *ret)
{
    /* init default out return value */
    *ret = LOS_OK;

#ifdef LOSCFG_KERNEL_SMP
    /* ASYNCHRONIZED. No need to do task lock checking *///异步操作,不需要进行任务锁检查
    if (taskCB->currCpu != ArchCurrCpuid()) {//任务运行在其他CPU,跨核心执行删除
        /*
         * the task is running on another cpu.
         * mask the target task with "kill" signal, and trigger mp schedule
         * which might not be essential but the deletion could more in time.
         */
        taskCB->signal = SIGNAL_KILL;	//贴上干掉标记
        LOS_MpSchedule(taskCB->currCpu);//通知任务所属CPU发生调度
        *ret = OsTaskSyncWait(taskCB);	//同步等待可怜的任务被干掉
        return FALSE;
    }
#endif

    if (!OsPreemptableInSched()) {//如果任务正在运行且调度程序已锁定，则无法删除它
        /* If the task is running and scheduler is locked then you can not delete it */
        *ret = LOS_ERRNO_TSK_DELETE_LOCKED;
        return FALSE;
    }

    if (OS_INT_ACTIVE) {//硬中断进行中...会屏蔽掉所有信号,当然包括kill了
        /*
         * delete running task in interrupt.
         * mask "kill" signal and later deletion will be handled.
         */
        taskCB->signal = SIGNAL_KILL;//硬中断后将处理删除。
        return FALSE;
    }

    return TRUE;
}
///删除不活动的任务 !OS_TASK_STATUS_RUNNING 
STATIC VOID OsTaskDeleteInactive(LosProcessCB *processCB, LosTaskCB *taskCB)
{
    LosMux *mux = (LosMux *)taskCB->taskMux; //任务
    UINT16 taskStatus = taskCB->taskStatus;

    LOS_ASSERT(!(taskStatus & OS_TASK_STATUS_RUNNING));

    OsTaskReleaseHoldLock(processCB, taskCB);

    OsSchedTaskExit(taskCB);
    if (taskStatus & OS_TASK_STATUS_PENDING) {
        if (LOS_MuxIsValid(mux) == TRUE) {
            OsMuxBitmapRestore(mux, taskCB, (LosTaskCB *)mux->owner);
        }
    }

    OsTaskStatusUnusedSet(taskCB);

    LOS_ListDelete(&taskCB->threadList);
    processCB->threadNumber--;
    LOS_ListTailInsert(&g_taskRecycleList, &taskCB->pendList);
    return;
}
///以不安全的方式删除参数任务
LITE_OS_SEC_TEXT UINT32 OsTaskDeleteUnsafe(LosTaskCB *taskCB, UINT32 status, UINT32 intSave)
{
    LosProcessCB *processCB = OS_PCB_FROM_PID(taskCB->processID);//获取进程实体
    UINT32 mode = processCB->processMode;
    UINT32 errRet = LOS_OK;

    if (taskCB->taskStatus & OS_TASK_FLAG_SYSTEM_TASK) {//系统任务是不能被删除的, 请您说出3个系统任务.
        errRet = LOS_ERRNO_TSK_OPERATE_SYSTEM_TASK;
        goto EXIT;
    }

    if ((taskCB->taskStatus & OS_TASK_STATUS_RUNNING) && !OsRunTaskToDeleteCheckOnRun(taskCB, &errRet)) {//正在运行且检测正在运行不能删除的情况
        goto EXIT;
    }

    if (!(taskCB->taskStatus & OS_TASK_STATUS_RUNNING)) {//任务不在活动
        OsTaskDeleteInactive(processCB, taskCB);//删除未活动的任务
        SCHEDULER_UNLOCK(intSave);//释放自旋锁,这就是不安全的标记,函数内部并没有拿自旋锁
        OsWriteResourceEvent(OS_RESOURCE_EVENT_FREE);//写一个资源释放事件,资源回收任务会收到事件,并回收任务的资源.
        return errRet;//消费OS_RESOURCE_EVENT_FREE事件可见于 OsResourceRecoveryTask 的处理
    }
    OsHookCall(LOS_HOOK_TYPE_TASK_DELETE, taskCB);
    if (mode == OS_USER_MODE) { //用户态模式
        SCHEDULER_UNLOCK(intSave);//先释放锁
        OsTaskResourcesToFree(taskCB);//释放任务资源
        SCHEDULER_LOCK(intSave);
    }

#ifdef LOSCFG_KERNEL_SMP
    LOS_ASSERT(OsPercpuGet()->taskLockCnt == 1);
#else
    LOS_ASSERT(OsPercpuGet()->taskLockCnt == 0);
#endif
    OsRunTaskToDelete(taskCB);//删除一个正在运行的任务

EXIT:
    SCHEDULER_UNLOCK(intSave);
    return errRet;
}
///删除指定的任务,回归任务池
LITE_OS_SEC_TEXT_INIT UINT32 LOS_TaskDelete(UINT32 taskID)
{
    UINT32 intSave;
    UINT32 ret;
    LosTaskCB *taskCB = NULL;
    LosProcessCB *processCB = NULL;

    if (OS_TID_CHECK_INVALID(taskID)) {
        return LOS_ERRNO_TSK_ID_INVALID;
    }

    if (OS_INT_ACTIVE) {
        return LOS_ERRNO_TSK_YIELD_IN_INT;
    }

    taskCB = OS_TCB_FROM_TID(taskID);
    SCHEDULER_LOCK(intSave);
    if (taskCB->taskStatus & OS_TASK_STATUS_UNUSED) {
        ret = LOS_ERRNO_TSK_NOT_CREATED;
        OS_GOTO_ERREND();
    }

    if (taskCB->taskStatus & (OS_TASK_FLAG_SYSTEM_TASK | OS_TASK_FLAG_NO_DELETE)) {
        SCHEDULER_UNLOCK(intSave);
        OsBackTrace();
        return LOS_ERRNO_TSK_OPERATE_SYSTEM_TASK;
    }
    processCB = OS_PCB_FROM_PID(taskCB->processID);
    if (processCB->threadNumber == 1) {//此任务为进程的最后一个任务的处理
        if (processCB == OsCurrProcessGet()) {//是否为当前任务
            SCHEDULER_UNLOCK(intSave);
            OsProcessExit(taskCB, OS_PRO_EXIT_OK);//进程退出
            return LOS_OK;
        }

        ret = LOS_ERRNO_TSK_ID_INVALID;
        OS_GOTO_ERREND();
    }

    return OsTaskDeleteUnsafe(taskCB, OS_PRO_EXIT_OK, intSave);//任务以非安全模式删除

LOS_ERREND:
    SCHEDULER_UNLOCK(intSave);
    return ret;
}
///任务延时等待，释放CPU，等待时间到期后该任务会重新进入ready状态
LITE_OS_SEC_TEXT UINT32 LOS_TaskDelay(UINT32 tick)
{
    UINT32 intSave;
    LosTaskCB *runTask = NULL;

    if (OS_INT_ACTIVE) {
        PRINT_ERR("In interrupt not allow delay task!\n");
        return LOS_ERRNO_TSK_DELAY_IN_INT;
    }

    runTask = OsCurrTaskGet();
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
    OsSchedDelay(runTask, tick);
    OsHookCall(LOS_HOOK_TYPE_MOVEDTASKTODELAYEDLIST, runTask);
    SCHEDULER_UNLOCK(intSave);

    return LOS_OK;
}
///获取任务的优先级
LITE_OS_SEC_TEXT_MINOR UINT16 LOS_TaskPriGet(UINT32 taskID)
{
    UINT32 intSave;
    LosTaskCB *taskCB = NULL;
    UINT16 priority;

    if (OS_TID_CHECK_INVALID(taskID)) {
        return (UINT16)OS_INVALID;
    }

    taskCB = OS_TCB_FROM_TID(taskID);
    SCHEDULER_LOCK(intSave);
    if (taskCB->taskStatus & OS_TASK_STATUS_UNUSED) {//就这么一句话也要来个自旋锁，内核代码自旋锁真是无处不在啊
        SCHEDULER_UNLOCK(intSave);
        return (UINT16)OS_INVALID;
    }

    priority = taskCB->priority;
    SCHEDULER_UNLOCK(intSave);
    return priority;
}
///设置指定任务的优先级
LITE_OS_SEC_TEXT_MINOR UINT32 LOS_TaskPriSet(UINT32 taskID, UINT16 taskPrio)
{
    UINT32 intSave;
    LosTaskCB *taskCB = NULL;

    if (taskPrio > OS_TASK_PRIORITY_LOWEST) {
        return LOS_ERRNO_TSK_PRIOR_ERROR;
    }

    if (OS_TID_CHECK_INVALID(taskID)) {
        return LOS_ERRNO_TSK_ID_INVALID;
    }

    taskCB = OS_TCB_FROM_TID(taskID);
    if (taskCB->taskStatus & OS_TASK_FLAG_SYSTEM_TASK) {
        return LOS_ERRNO_TSK_OPERATE_SYSTEM_TASK;
    }

    SCHEDULER_LOCK(intSave);
    if (taskCB->taskStatus & OS_TASK_STATUS_UNUSED) {
        SCHEDULER_UNLOCK(intSave);
        return LOS_ERRNO_TSK_NOT_CREATED;
    }

    BOOL isReady = OsSchedModifyTaskSchedParam(taskCB, taskCB->policy, taskPrio);
    SCHEDULER_UNLOCK(intSave);

    LOS_MpSchedule(OS_MP_CPU_ALL);
    if (isReady && OS_SCHEDULER_ACTIVE) {
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
    /* reset timeslice of yeilded task */
    OsSchedYield();
    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;
}

LITE_OS_SEC_TEXT_MINOR VOID LOS_TaskLock(VOID)
{
    UINT32 intSave;

    intSave = LOS_IntLock();
    OsCpuSchedLock(OsPercpuGet());
    LOS_IntRestore(intSave);
}

LITE_OS_SEC_TEXT_MINOR VOID LOS_TaskUnlock(VOID)
{
    OsCpuSchedUnlock(OsPercpuGet(), LOS_IntLock());
}

//获取任务信息,给shell使用的
LITE_OS_SEC_TEXT_MINOR UINT32 LOS_TaskInfoGet(UINT32 taskID, TSK_INFO_S *taskInfo)
{
    UINT32 intSave;
    LosTaskCB *taskCB = NULL;

    if (taskInfo == NULL) {
        return LOS_ERRNO_TSK_PTR_NULL;
    }

    if (OS_TID_CHECK_INVALID(taskID)) {
        return LOS_ERRNO_TSK_ID_INVALID;
    }

    taskCB = OS_TCB_FROM_TID(taskID);
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

    taskInfo->usTaskStatus = taskCB->taskStatus;
    taskInfo->usTaskPrio = taskCB->priority;
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
    LosTaskCB *taskCB = NULL;
    BOOL needSched = FALSE;
    UINT32 intSave;
    UINT16 currCpuMask;

    if (OS_TID_CHECK_INVALID(taskID)) {//检测taskid是否有效,task由task池分配,鸿蒙默认128个任务 ID范围[0:127]
        return LOS_ERRNO_TSK_ID_INVALID;
    }

    if (!(cpuAffiMask & LOSCFG_KERNEL_CPU_MASK)) {//检测cpu亲和力
        return LOS_ERRNO_TSK_CPU_AFFINITY_MASK_ERR;
    }

    taskCB = OS_TCB_FROM_TID(taskID);//获取任务实体
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
    LosTaskCB *taskCB = NULL;
    UINT16 cpuAffiMask;
    UINT32 intSave;

    if (OS_TID_CHECK_INVALID(taskID)) {
        return INVALID_CPU_AFFI_MASK;
    }

    taskCB = OS_TCB_FROM_TID(taskID);//获取任务实体
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
 /******************************************************
 	由其他CPU核触发阻塞进程的信号
	函数由汇编代码调用 ..\arch\arm\arm\src\los_dispatch.S
 ******************************************************/
LITE_OS_SEC_TEXT_MINOR VOID OsTaskProcSignal(VOID)
{
    UINT32 intSave, ret;
	//私有且不可中断，无需保护。这个任务在其他CPU核看到它时总是在运行，所以它在执行代码的同时也可以继续接收信号
    /*
     * private and uninterruptable, no protection needed.
     * while this task is always running when others cores see it,
     * so it keeps recieving signals while follow code excuting.
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
        SCHEDULER_LOCK(intSave);
        runTask->signal = SIGNAL_NONE;//清除信号，
        ret = OsTaskDeleteUnsafe(runTask, OS_PRO_EXIT_OK, intSave);//任务的自杀行动,这可是正在运行的任务.
        if (ret) {
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
    LosProcessCB *processCB = NULL;
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
    processCB = OS_PCB_FROM_PID(taskCB->processID);
    /* if thread is main thread, then set processName as taskName */
    if ((taskCB->taskID == processCB->threadGroupID) && (setPName == TRUE)) {
        err = (INT32)OsSetProcessName(processCB, (const CHAR *)taskCB->taskName);
        if (err != LOS_OK) {
            err = EINVAL;
        }
    }

EXIT:
    SCHEDULER_UNLOCK(intSave);
    return err;
}

STATIC VOID OsExitGroupActiveTaskKilled(LosProcessCB *processCB, LosTaskCB *taskCB)
{
    INT32 ret;

    taskCB->taskStatus |= OS_TASK_FLAG_EXIT_KILL;
#ifdef LOSCFG_KERNEL_SMP
    /* The other core that the thread is running on and is currently running in a non-system call */
    if (!taskCB->sig.sigIntLock && (taskCB->taskStatus & OS_TASK_STATUS_RUNNING)) {
        taskCB->signal = SIGNAL_KILL;
        LOS_MpSchedule(taskCB->currCpu);
    } else
#endif
#ifdef LOSCFG_KERNEL_VM
    {
        ret = OsTaskKillUnsafe(taskCB->taskID, SIGKILL);
        if (ret != LOS_OK) {
            PRINT_ERR("pid %u exit, Exit task group %u kill %u failed! ERROR: %d\n",
                      taskCB->processID, OsCurrTaskGet()->taskID, taskCB->taskID, ret);
        }
    }
#endif

    if (!(taskCB->taskStatus & OS_TASK_FLAG_PTHREAD_JOIN)) {
        taskCB->taskStatus |= OS_TASK_FLAG_PTHREAD_JOIN;
        LOS_ListInit(&taskCB->joinList);
    }

    ret = OsTaskJoinPendUnsafe(taskCB);
    if (ret != LOS_OK) {
        PRINT_ERR("pid %u exit, Exit task group %u to wait others task %u(0x%x) exit failed! ERROR: %d\n",
                  taskCB->processID, OsCurrTaskGet()->taskID, taskCB->taskID, taskCB->taskStatus, ret);
    }
}
///1.当前进程中的任务集体退群, 2.当前进程贴上退出标签
LITE_OS_SEC_TEXT VOID OsTaskExitGroup(UINT32 status)
{
    UINT32 intSave;

    LosProcessCB *processCB = OsCurrProcessGet();
    LosTaskCB *currTask = OsCurrTaskGet();
    SCHEDULER_LOCK(intSave);//调度自旋锁,这块锁的代码有点多,这块容易出问题!出问题也不好复现,希望鸿蒙有充分测试这块的功能. @note_thinking
    if ((processCB->processStatus & OS_PROCESS_FLAG_EXIT) || !OsProcessIsUserMode(processCB)) {
        SCHEDULER_UNLOCK(intSave);
        return;
    }

    processCB->processStatus |= OS_PROCESS_FLAG_EXIT;//贴上进程要退出的标签
    processCB->threadGroupID = currTask->taskID;

    LOS_DL_LIST *list = &processCB->threadSiblingList;
    LOS_DL_LIST *head = list;
    do {
        LosTaskCB *taskCB = LOS_DL_LIST_ENTRY(list->pstNext, LosTaskCB, threadList);
        if ((taskCB->taskStatus & (OS_TASK_STATUS_INIT | OS_TASK_STATUS_EXIT) ||
            ((taskCB->taskStatus & OS_TASK_STATUS_READY) && !taskCB->sig.sigIntLock)) &&
            !(taskCB->taskStatus & OS_TASK_STATUS_RUNNING)) {
            OsTaskDeleteInactive(processCB, taskCB);
        } else {
            if (taskCB != currTask) {
                OsExitGroupActiveTaskKilled(processCB, taskCB);
            } else {
                /* Skip the current task */
                list = list->pstNext;
            }
        }
    } while (head != list->pstNext);

    SCHEDULER_UNLOCK(intSave);

    LOS_ASSERT(processCB->threadNumber == 1);//这一趟下来,进程只有一个正在活动的任务
    return;
}
///任务退群并销毁,进入任务的回收链表之后再进入空闲链表,等着再次被分配使用.
LITE_OS_SEC_TEXT VOID OsExecDestroyTaskGroup(VOID)
{
    OsTaskExitGroup(OS_PRO_EXIT_OK);//任务退出
    OsTaskCBRecycleToFree();
}

UINT32 OsUserTaskOperatePermissionsCheck(LosTaskCB *taskCB)
{
    return OsUserProcessOperatePermissionsCheck(taskCB, OsCurrProcessGet()->processID);
}

UINT32 OsUserProcessOperatePermissionsCheck(LosTaskCB *taskCB, UINT32 processID)
{
    if (taskCB == NULL) {
        return LOS_EINVAL;
    }

    if (processID == OS_INVALID_VALUE) {
        return OS_INVALID_VALUE;
    }

    if (taskCB->taskStatus & OS_TASK_STATUS_UNUSED) {
        return LOS_EINVAL;
    }

    if (processID != taskCB->processID) {
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
LITE_OS_SEC_TEXT_INIT UINT32 OsCreateUserTask(UINT32 processID, TSK_INIT_PARAM_S *initParam)
{
    LosProcessCB *processCB = NULL;
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
        processCB = OsCurrProcessGet();//拿当前运行的进程
        initParam->processID = processCB->processID;//进程ID赋值
        initParam->consoleID = processCB->consoleID;//任务控制台ID归属
        SCHEDULER_UNLOCK(intSave);
    } else {//进程已经创建
        processCB = OS_PCB_FROM_PID(processID);//通过ID拿到进程PCB
        if (!(processCB->processStatus & (OS_PROCESS_STATUS_INIT | OS_PROCESS_STATUS_RUNNING))) {//进程未初始化和未正在运行时
            return OS_INVALID_VALUE;//@note_why 为什么这两种情况下会创建任务失败
        }
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
    LosTaskCB *taskCB = NULL;
    INT32 policy;

    if (OS_TID_CHECK_INVALID(taskID)) {
        return -LOS_EINVAL;
    }

    taskCB = OS_TCB_FROM_TID(taskID);//通过任务ID获得任务TCB
    SCHEDULER_LOCK(intSave);
    if (taskCB->taskStatus & OS_TASK_STATUS_UNUSED) {//状态不能是没有在使用
        policy = -LOS_EINVAL;
        OS_GOTO_ERREND();
    }

    policy = taskCB->policy;//任务的调度方式

LOS_ERREND:
    SCHEDULER_UNLOCK(intSave);
    return policy;
}

//设置任务的调度信息
LITE_OS_SEC_TEXT INT32 LOS_SetTaskScheduler(INT32 taskID, UINT16 policy, UINT16 priority)
{
    UINT32 intSave;
    BOOL needSched = FALSE;

    if (OS_TID_CHECK_INVALID(taskID)) {
        return LOS_ESRCH;
    }

    if (priority > OS_TASK_PRIORITY_LOWEST) {
        return LOS_EINVAL;
    }

    if ((policy != LOS_SCHED_FIFO) && (policy != LOS_SCHED_RR)) {
        return LOS_EINVAL;
    }

    SCHEDULER_LOCK(intSave);
    needSched = OsSchedModifyTaskSchedParam(OS_TCB_FROM_TID(taskID), policy, priority);
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

    if (taskID == OsCurrTaskGet()->taskID) {
        return LOS_EDEADLK;
    }
    return LOS_OK;
}

UINT32 LOS_TaskJoin(UINT32 taskID, UINTPTR *retval)
{
    UINT32 intSave;
    LosTaskCB *runTask = OsCurrTaskGet();
    LosTaskCB *taskCB = NULL;
    UINT32 errRet;

    errRet = OsTaskJoinCheck(taskID);
    if (errRet != LOS_OK) {
        return errRet;
    }

    taskCB = OS_TCB_FROM_TID(taskID);
    SCHEDULER_LOCK(intSave);
    if (taskCB->taskStatus & OS_TASK_STATUS_UNUSED) {
        SCHEDULER_UNLOCK(intSave);
        return LOS_EINVAL;
    }

    if (runTask->processID != taskCB->processID) {
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
    LosTaskCB *taskCB = NULL;
    UINT32 errRet;

    if (OS_TID_CHECK_INVALID(taskID)) {
        return LOS_EINVAL;
    }

    if (OS_INT_ACTIVE) {
        return LOS_EINTR;
    }

    taskCB = OS_TCB_FROM_TID(taskID);
    SCHEDULER_LOCK(intSave);
    if (taskCB->taskStatus & OS_TASK_STATUS_UNUSED) {
        SCHEDULER_UNLOCK(intSave);
        return LOS_EINVAL;
    }

    if (runTask->processID != taskCB->processID) {
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
