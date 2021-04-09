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

#include "los_task_pri.h"
#include "los_base_pri.h"
#include "los_priqueue_pri.h"
#include "los_sem_pri.h"
#include "los_event_pri.h"
#include "los_mux_pri.h"
#include "los_hw_pri.h"
#include "los_exc.h"
#include "los_memstat_pri.h"
#include "los_mp.h"
#include "los_spinlock.h"
#include "los_percpu_pri.h"
#include "los_process_pri.h"
#if (LOSCFG_KERNEL_TRACE == YES)
#include "los_trace.h"
#endif

#ifdef LOSCFG_KERNEL_TICKLESS
#include "los_tickless_pri.h"
#endif
#ifdef LOSCFG_KERNEL_CPUP
#include "los_cpup_pri.h"
#endif
#if (LOSCFG_BASE_CORE_SWTMR == YES)
#include "los_swtmr_pri.h"
#endif
#ifdef LOSCFG_EXC_INTERACTION
#include "los_exc_interaction_pri.h"
#endif
#if (LOSCFG_KERNEL_LITEIPC == YES)
#include "hm_liteipc.h"
#endif
#include "user_copy.h"
#include "los_vm_syscall.h"
#ifdef LOSCFG_ENABLE_OOM_LOOP_TASK
#include "los_oom.h"
#endif
#include "los_vm_map.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/******************************************************************************
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
******************************************************************************/

#if (LOSCFG_BASE_CORE_TSK_LIMIT <= 0)
#error "task maxnum cannot be zero"
#endif  /* LOSCFG_BASE_CORE_TSK_LIMIT <= 0 */

LITE_OS_SEC_BSS LosTaskCB    *g_taskCBArray;//任务池 128个
LITE_OS_SEC_BSS LOS_DL_LIST  g_losFreeTask;//空闲任务链表
LITE_OS_SEC_BSS LOS_DL_LIST  g_taskRecyleList;//回收任务链表
LITE_OS_SEC_BSS UINT32       g_taskMaxNum;//任务最大个数
LITE_OS_SEC_BSS UINT32       g_taskScheduled; /* one bit for each cores *///一位代表一个CPU core 的调度
LITE_OS_SEC_BSS EVENT_CB_S   g_resourceEvent;//关于资源的事件
/* spinlock for task module, only available on SMP mode */
LITE_OS_SEC_BSS SPIN_LOCK_INIT(g_taskSpin);

STATIC VOID OsConsoleIDSetHook(UINT32 param1,
                               UINT32 param2) __attribute__((weakref("OsSetConsoleID")));
STATIC VOID OsExcStackCheckHook(VOID) __attribute__((weakref("OsExcStackCheck")));

#define OS_CHECK_TASK_BLOCK (OS_TASK_STATUS_DELAY |    \
                             OS_TASK_STATUS_PEND |     \
                             OS_TASK_STATUS_SUSPEND)

/* temp task blocks for booting procedure */
LITE_OS_SEC_BSS STATIC LosTaskCB                g_mainTask[LOSCFG_KERNEL_CORE_NUM];//启动引导过程中使用的临时任务

VOID* OsGetMainTask()//获取当前任务
{
    return (g_mainTask + ArchCurrCpuid());
}

VOID OsSetMainTask()
{
    UINT32 i;
    CHAR *name = "osMain";
    errno_t ret;
//为每个CPU core 设置mainTask
    for (i = 0; i < LOSCFG_KERNEL_CORE_NUM; i++) {
        g_mainTask[i].taskStatus = OS_TASK_STATUS_UNUSED;
        g_mainTask[i].taskID = LOSCFG_BASE_CORE_TSK_LIMIT;//128
        g_mainTask[i].priority = OS_TASK_PRIORITY_LOWEST;//31
#if (LOSCFG_KERNEL_SMP_LOCKDEP == YES)
        g_mainTask[i].lockDep.lockDepth = 0;
        g_mainTask[i].lockDep.waitLock = NULL;
#endif
        ret = memcpy_s(g_mainTask[i].taskName, OS_TCB_NAME_LEN, name, strlen(name));//设置主任务名称"osMain" 
        if (ret != EOK) {
            g_mainTask[i].taskName[0] = '\0';
        }
        LOS_ListInit(&g_mainTask[i].lockList);//初始化 每个CPU core 持有的锁链表
    }
}
//空闲任务 注意 #define WEAK       __attribute__((weak)) 是用于防止crash的
LITE_OS_SEC_TEXT WEAK VOID OsIdleTask(VOID)
{
    while (1) {//只有一个死循环
#ifdef LOSCFG_KERNEL_TICKLESS //低功耗模式开关, idle task 中关闭tick
        if (OsTickIrqFlagGet()) {
            OsTickIrqFlagSet(0);
            OsTicklessStart();
        }
#endif
        Wfi();//WFI指令:arm core 立即进入low-power standby state，等待中断，进入休眠模式。
    }
}

/*
 * Description : Change task priority.
 * Input       : taskCB    --- task control block
 *               priority  --- priority
 */
 //修改任务的优先级
LITE_OS_SEC_TEXT_MINOR VOID OsTaskPriModify(LosTaskCB *taskCB, UINT16 priority)
{
    LosProcessCB *processCB = NULL;

    LOS_ASSERT(LOS_SpinHeld(&g_taskSpin));

    if (taskCB->taskStatus & OS_TASK_STATUS_READY) {//只有就绪队列
        processCB = OS_PCB_FROM_PID(taskCB->processID);
        OS_TASK_PRI_QUEUE_DEQUEUE(processCB, taskCB);//先出队列再入队列
        taskCB->priority = priority;				//修改优先级
        OS_TASK_PRI_QUEUE_ENQUEUE(processCB, taskCB);//再入队列,从尾部插入
    } else {
        taskCB->priority = priority;
    }
}
//把任务加到CPU等待链表中
LITE_OS_SEC_TEXT STATIC INLINE VOID OsAdd2TimerList(LosTaskCB *taskCB, UINT32 timeOut)
{
    SET_SORTLIST_VALUE(&taskCB->sortList, timeOut);//设置idxRollNum的值为timeOut
    OsAdd2SortLink(&OsPercpuGet()->taskSortLink, &taskCB->sortList);//将任务挂到定时器排序链表上
#if (LOSCFG_KERNEL_SMP == YES)//注意:这里的排序不是传统意义上12345的排序,而是根据timeOut的值来决定放到CPU core哪个taskSortLink[0:7]链表上
    taskCB->timerCpu = ArchCurrCpuid();
#endif
}
//删除定时器链表
LITE_OS_SEC_TEXT STATIC INLINE VOID OsTimerListDelete(LosTaskCB *taskCB)
{
#if (LOSCFG_KERNEL_SMP == YES)
    SortLinkAttribute *sortLinkHeader = &g_percpu[taskCB->timerCpu].taskSortLink;
#else
    SortLinkAttribute *sortLinkHeader = &g_percpu[0].taskSortLink;
#endif
    OsDeleteSortLink(sortLinkHeader, &taskCB->sortList);//把task从taskSortLink链表上摘出去
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

    if (taskCB->taskStatus & OS_TASK_FLAG_PTHREAD_JOIN) {//任务贴有
        if (!LOS_ListEmpty(&taskCB->joinList)) {//注意到了这里 joinList中的节点身上都有阻塞标签
            resumedTask = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&(taskCB->joinList)));//通过贴有JOIN标签链表的第一个节点找到Task
            OsTaskWake(resumedTask);//唤醒任务
        }
        taskCB->taskStatus &= ~OS_TASK_FLAG_PTHREAD_JOIN;//去掉JOIN标签
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

    if ((taskCB->taskStatus & OS_TASK_FLAG_PTHREAD_JOIN) && LOS_ListEmpty(&taskCB->joinList)) {
        return OsTaskWait(&taskCB->joinList, LOS_WAIT_FOREVER, TRUE);
    } else if (taskCB->taskStatus & OS_TASK_STATUS_EXIT) {
        return LOS_OK;
    }

    return LOS_EINVAL;
}
//任务设置分离模式  Deatch和JOIN是一对有你没我的状态
LITE_OS_SEC_TEXT UINT32 OsTaskSetDeatchUnsafe(LosTaskCB *taskCB)
{
    LosProcessCB *processCB = OS_PCB_FROM_PID(taskCB->processID);//获取进程实体
    if (!(processCB->processStatus & OS_PROCESS_STATUS_RUNNING)) {//进程必须是运行状态
        return LOS_EPERM;
    }

    if (taskCB->taskStatus & OS_TASK_FLAG_PTHREAD_JOIN) {//join状态时
        if (LOS_ListEmpty(&(taskCB->joinList))) {//joinlist中没有数据了
            LOS_ListDelete(&(taskCB->joinList));//所谓删除就是自己指向自己
            taskCB->taskStatus &= ~OS_TASK_FLAG_PTHREAD_JOIN;//去掉JOIN标签
            taskCB->taskStatus |= OS_TASK_FLAG_DETACHED;//贴上分离标签,自己独立存在,不和其他任务媾和,不能被别的任务回收和干掉,只能由系统回收
            return LOS_OK;
        }
        /* This error code has a special purpose and is not allowed to appear again on the interface */
        return LOS_ESRCH;
    }

    return LOS_EINVAL;
}
//任务扫描处理,注意这个函数只有 时钟中断处理函数OsTickHandler调用,非常稳定.所以游标每次进来都+1对应一个tick
LITE_OS_SEC_TEXT VOID OsTaskScan(VOID)
{
    SortLinkList *sortList = NULL;
    LosTaskCB *taskCB = NULL;
    BOOL needSchedule = FALSE;
    UINT16 tempStatus;
    LOS_DL_LIST *listObject = NULL;
    SortLinkAttribute *taskSortLink = NULL;

    taskSortLink = &OsPercpuGet()->taskSortLink;//获取任务的排序链表
    taskSortLink->cursor = (taskSortLink->cursor + 1) & OS_TSK_SORTLINK_MASK;
    listObject = taskSortLink->sortLink + taskSortLink->cursor;//只处理这个游标上的链表,因为系统对超时任务都已经规链表了.
	//当任务因超时而挂起时，任务块处于超时排序链接上,（每个cpu）和ipc（互斥锁、扫描电镜等）的块同时被唤醒
    /*不管是超时还是相应的ipc，它都在等待。现在使用synchronize sortlink precedure，因此整个任务扫描需要保护，防止另一个核心同时删除sortlink。
     * When task is pended with timeout, the task block is on the timeout sortlink
     * (per cpu) and ipc(mutex,sem and etc.)'s block at the same time, it can be waken
     * up by either timeout or corresponding ipc it's waiting.
     *
     * Now synchronize sortlink preocedure is used, therefore the whole task scan needs
     * to be protected, preventing another core from doing sortlink deletion at same time.
     */
    LOS_SpinLock(&g_taskSpin);

    if (LOS_ListEmpty(listObject)) {
        LOS_SpinUnlock(&g_taskSpin);
        return;
    }
    sortList = LOS_DL_LIST_ENTRY(listObject->pstNext, SortLinkList, sortLinkNode);//拿本次Tick对应链表的SortLinkList的第一个节点sortLinkNode
    ROLLNUM_DEC(sortList->idxRollNum);//滚动数--

    while (ROLLNUM(sortList->idxRollNum) == 0) {//找到时间到了节点,注意这些节点都是由定时器产生的,
        LOS_ListDelete(&sortList->sortLinkNode);
        taskCB = LOS_DL_LIST_ENTRY(sortList, LosTaskCB, sortList);//拿任务,这里的任务都是超时任务
        taskCB->taskStatus &= ~OS_TASK_STATUS_PEND_TIME;
        tempStatus = taskCB->taskStatus;
        if (tempStatus & OS_TASK_STATUS_PEND) {
            taskCB->taskStatus &= ~OS_TASK_STATUS_PEND;
#if (LOSCFG_KERNEL_LITEIPC == YES)
            taskCB->ipcStatus &= ~IPC_THREAD_STATUS_PEND;
#endif
            taskCB->taskStatus |= OS_TASK_STATUS_TIMEOUT;
            LOS_ListDelete(&taskCB->pendList);
            taskCB->taskSem = NULL;
            taskCB->taskMux = NULL;
        } else {
            taskCB->taskStatus &= ~OS_TASK_STATUS_DELAY;
        }

        if (!(tempStatus & OS_TASK_STATUS_SUSPEND)) {
            OS_TASK_SCHED_QUEUE_ENQUEUE(taskCB, OS_PROCESS_STATUS_PEND);
            needSchedule = TRUE;
        }

        if (LOS_ListEmpty(listObject)) {
            break;
        }

        sortList = LOS_DL_LIST_ENTRY(listObject->pstNext, SortLinkList, sortLinkNode);
    }

    LOS_SpinUnlock(&g_taskSpin);

    if (needSchedule != FALSE) {//需要调度
        LOS_MpSchedule(OS_MP_CPU_ALL);//核间通讯,给所有CPU发送调度信号
        LOS_Schedule();//开始调度
    }
}
//初始化任务
LITE_OS_SEC_TEXT_INIT UINT32 OsTaskInit(VOID)
{
    UINT32 index;
    UINT32 ret;
    UINT32 size;

    g_taskMaxNum = LOSCFG_BASE_CORE_TSK_LIMIT;//任务池中最多默认128个,可谓铁打的任务池流水的线程
    size = (g_taskMaxNum + 1) * sizeof(LosTaskCB);//计算需分配内存总大小
    /*
     * This memory is resident memory and is used to save the system resources
     * of task control block and will not be freed.
     */
    g_taskCBArray = (LosTaskCB *)LOS_MemAlloc(m_aucSysMem0, size);//任务池 常驻内存,不被释放
    if (g_taskCBArray == NULL) {
        return LOS_ERRNO_TSK_NO_MEMORY;
    }
    (VOID)memset_s(g_taskCBArray, size, 0, size);

    LOS_ListInit(&g_losFreeTask);//空闲任务链表
    LOS_ListInit(&g_taskRecyleList);//需回收任务链表
    for (index = 0; index < g_taskMaxNum; index++) {
        g_taskCBArray[index].taskStatus = OS_TASK_STATUS_UNUSED;
        g_taskCBArray[index].taskID = index;//任务ID最大默认127
        LOS_ListTailInsert(&g_losFreeTask, &g_taskCBArray[index].pendList);//都插入空闲任务列表 
    }//注意:这里挂的是pendList节点,所以取TCB要通过 OS_TCB_FROM_PENDLIST 取.

    ret = OsPriQueueInit();//创建32个任务优先级队列，即32个双向循环链表
    if (ret != LOS_OK) {
        return LOS_ERRNO_TSK_NO_MEMORY;
    }

    /* init sortlink for each core */
    for (index = 0; index < LOSCFG_KERNEL_CORE_NUM; index++) {
        ret = OsSortLinkInit(&g_percpu[index].taskSortLink);//每个CPU内核都有一个执行任务链表
        if (ret != LOS_OK) {
            return LOS_ERRNO_TSK_NO_MEMORY;
        }
    }
    return LOS_OK;
}
//获取IdletaskId,每个CPU核都对Task进行了内部管理,做到真正的并行处理
UINT32 OsGetIdleTaskId(VOID)
{
    Percpu *perCpu = OsPercpuGet();//获取当前Cpu信息
    return perCpu->idleTaskID;//返回当前CPU 空闲任务ID
}
//创建一个空闲任务
LITE_OS_SEC_TEXT_INIT UINT32 OsIdleTaskCreate(VOID)
{
    UINT32 ret;
    TSK_INIT_PARAM_S taskInitParam;
    Percpu *perCpu = OsPercpuGet();//获取CPU信息
    UINT32 *idleTaskID = &perCpu->idleTaskID;//指定CPU的空闲任务

    (VOID)memset_s((VOID *)(&taskInitParam), sizeof(TSK_INIT_PARAM_S), 0, sizeof(TSK_INIT_PARAM_S));//任务初始参数清0
    taskInitParam.pfnTaskEntry = (TSK_ENTRY_FUNC)OsIdleTask;//入口函数指定idle
    taskInitParam.uwStackSize = LOSCFG_BASE_CORE_TSK_IDLE_STACK_SIZE;//任务栈大小
    taskInitParam.pcName = "Idle";//任务名称 叫pcName有点怪怪的,不能换个撒
    taskInitParam.usTaskPrio = OS_TASK_PRIORITY_LOWEST;//默认最低优先级 31
    taskInitParam.uwResved = OS_TASK_FLAG_IDLEFLAG;//默认idle flag
#if (LOSCFG_KERNEL_SMP == YES)//CPU多核情况
    taskInitParam.usCpuAffiMask = CPUID_TO_AFFI_MASK(ArchCurrCpuid());//意思是归于哪个CPU核调度,
#endif
    ret = LOS_TaskCreate(idleTaskID, &taskInitParam);//创建task并申请调度,
    OS_TCB_FROM_TID(*idleTaskID)->taskStatus |= OS_TASK_FLAG_SYSTEM_TASK;//设置task状态为系统任务,系统任务运行在内核态.
	//这里说下系统任务有哪些?比如: idle,swtmr(软时钟),资源回收等等 
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

#if (LOSCFG_BASE_CORE_TSK_MONITOR == YES)
LITE_OS_SEC_TEXT STATIC VOID OsTaskStackCheck(LosTaskCB *oldTask, LosTaskCB *newTask)
{
    if (!OS_STACK_MAGIC_CHECK(oldTask->topOfStack)) {//magic检查无效情况
        LOS_Panic("CURRENT task ID: %s:%d stack overflow!\n", oldTask->taskName, oldTask->taskID);
    }

    if (((UINTPTR)(newTask->stackPointer) <= newTask->topOfStack) ||
        ((UINTPTR)(newTask->stackPointer) > (newTask->topOfStack + newTask->stackSize))) {
        LOS_Panic("HIGHEST task ID: %s:%u SP error! StackPointer: %p TopOfStack: %p\n",
                  newTask->taskName, newTask->taskID, newTask->stackPointer, newTask->topOfStack);
    }

    if (OsExcStackCheckHook != NULL) {
        OsExcStackCheckHook();
    }
}

#endif
//任务切换检查
LITE_OS_SEC_TEXT_MINOR UINT32 OsTaskSwitchCheck(LosTaskCB *oldTask, LosTaskCB *newTask)
{
#if (LOSCFG_BASE_CORE_TSK_MONITOR == YES)//这里宏指任务栈有没有启动监控
    OsTaskStackCheck(oldTask, newTask);//任务栈监控检查
#endif /* LOSCFG_BASE_CORE_TSK_MONITOR == YES */

#if (LOSCFG_KERNEL_TRACE == YES)
    LOS_Trace(LOS_TRACE_SWITCH, newTask->taskID, oldTask->taskID);//打印新老任务
#endif

    return LOS_OK;
}
//一个任务的退出过程
LITE_OS_SEC_TEXT VOID OsTaskToExit(LosTaskCB *taskCB, UINT32 status)
{
    UINT32 intSave;
    LosProcessCB *runProcess = NULL;
    LosTaskCB *mainTask = NULL;

    SCHEDULER_LOCK(intSave);//禁止调度
    runProcess = OS_PCB_FROM_PID(taskCB->processID);//通过任务ID拿到进程实体
    mainTask = OS_TCB_FROM_TID(runProcess->threadGroupID);//通过线程组ID拿到主任务实体,threadGroupID就是等于mainTask的taskId,这个是在线程组创建的时候指定的.
    SCHEDULER_UNLOCK(intSave);//恢复调度
    if (mainTask == taskCB) {//如果参数任务就是主任务
        OsTaskExitGroup(status);//task退出线程组
    }

    SCHEDULER_LOCK(intSave);
    if (runProcess->threadNumber == 1) { /* 1: The last task of the process exits *///进程的最后一个任务退出
        SCHEDULER_UNLOCK(intSave);
        (VOID)OsProcessExit(taskCB, status);//调用进程退出流程
        return;
    }

    if (taskCB->taskStatus & OS_TASK_FLAG_DETACHED) {//任务状态是否有分离标签
        (VOID)OsTaskDeleteUnsafe(taskCB, status, intSave);//任务要over了,释放占用的资源
    }

    OsTaskJoinPostUnsafe(taskCB);//退出前唤醒跟自己绑在一块的任务
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
    if (taskCB->taskStatus & OS_TASK_FLAG_DETACHED) {//task有分离标签时
        taskCB->joinRetval = 0;//结合数为0
    }
	
    OsTaskToExit(taskCB, 0);//到这里任务跑完了要退出了
}
//任务创建参数检查
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

#ifdef LOSCFG_EXC_INTERACTION 
    if (!OsExcInteractionTaskCheck(initParam)) {
        *pool = m_aucSysMem0;
        poolSize = OS_EXC_INTERACTMEM_SIZE;
    }
#endif
    if (initParam->uwStackSize > poolSize) {//希望申请的栈大小不能大于总池子
        return LOS_ERRNO_TSK_STKSZ_TOO_LARGE;
    }

    if (initParam->uwStackSize == 0) {//运行栈空间不能为0
        initParam->uwStackSize = LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE;
    }
    initParam->uwStackSize = (UINT32)ALIGN(initParam->uwStackSize, LOSCFG_STACK_POINT_ALIGN_SIZE);

    if (initParam->uwStackSize < LOS_TASK_MIN_STACK_SIZE) {//运行栈空间不能低于最低值
        return LOS_ERRNO_TSK_STKSZ_TOO_SMALL;
    }

    return LOS_OK;
}

LITE_OS_SEC_TEXT_INIT STATIC VOID OsTaskStackAlloc(VOID **topStack, UINT32 stackSize, VOID *pool)
{
    *topStack = (VOID *)LOS_MemAllocAlign(pool, stackSize, LOSCFG_STACK_POINT_ALIGN_SIZE);
}
//创建任务同步信号
STATIC INLINE UINT32 OsTaskSyncCreate(LosTaskCB *taskCB)
{
#if (LOSCFG_KERNEL_SMP_TASK_SYNC == YES)//CPU多核情况下任务同步宏是否开启
    UINT32 ret = LOS_SemCreate(0, &taskCB->syncSignal);//创建一个syncSignal信号量
    if (ret != LOS_OK) {
        return LOS_ERRNO_TSK_MP_SYNC_RESOURCE;
    }
#else
    (VOID)taskCB;
#endif
    return LOS_OK;
}
//删除任务同步信号
STATIC INLINE VOID OsTaskSyncDestroy(UINT32 syncSignal)
{
#if (LOSCFG_KERNEL_SMP_TASK_SYNC == YES)
    (VOID)LOS_SemDelete(syncSignal);
#else
    (VOID)syncSignal;
#endif
}
/******************************************
等待任务的同步信号量,
A --发送syncSignal-- > B
B --回一个syncSignal-- > A
如此A就知道B此时还在
*******************************************/
LITE_OS_SEC_TEXT UINT32 OsTaskSyncWait(const LosTaskCB *taskCB)
{
#if (LOSCFG_KERNEL_SMP_TASK_SYNC == YES)
    UINT32 ret = LOS_OK;

    LOS_ASSERT(LOS_SpinHeld(&g_taskSpin));
    LOS_SpinUnlock(&g_taskSpin);
    /*
     * gc soft timer works every OS_MP_GC_PERIOD period, to prevent this timer
     * triggered right at the timeout has reached, we set the timeout as double
     * of the gc peroid.
     */
    if (LOS_SemPend(taskCB->syncSignal, OS_MP_GC_PERIOD * 2) != LOS_OK) {//发送同步信号量
        ret = LOS_ERRNO_TSK_MP_SYNC_FAILED;
    }

    LOS_SpinLock(&g_taskSpin);

    return ret;
#else
    (VOID)taskCB;
    return LOS_OK;
#endif
}
//任务同步唤醒
STATIC INLINE VOID OsTaskSyncWake(const LosTaskCB *taskCB)
{
#if (LOSCFG_KERNEL_SMP_TASK_SYNC == YES)
    (VOID)OsSemPostUnsafe(taskCB->syncSignal, NULL);//唤醒一个挂在信号量链表上的阻塞任务
#else
    (VOID)taskCB;
#endif
}
//释放任务内核资源
STATIC VOID OsTaskKernelResourcesToFree(UINT32 syncSignal, UINTPTR topOfStack)
{
    VOID *poolTmp = (VOID *)m_aucSysMem1;

    OsTaskSyncDestroy(syncSignal);

#ifdef LOSCFG_EXC_INTERACTION
    if (topOfStack < (UINTPTR)m_aucSysMem1) {
        poolTmp = (VOID *)m_aucSysMem0;
    }
#endif
    (VOID)LOS_MemFree(poolTmp, (VOID *)topOfStack);
}
//从回收链表中回收任务到空闲链表
LITE_OS_SEC_TEXT VOID OsTaskCBRecyleToFree()
{
    LosTaskCB *taskCB = NULL;
    UINT32 intSave;

    SCHEDULER_LOCK(intSave);
    while (!LOS_ListEmpty(&g_taskRecyleList)) {//不空就一个一个回收任务
        taskCB = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&g_taskRecyleList));//取出第一个待回收任务
        LOS_ListDelete(&taskCB->pendList);//从回收链表上将自己摘除
        SCHEDULER_UNLOCK(intSave);

        OsTaskResourcesToFree(taskCB);//释放任务资源

        SCHEDULER_LOCK(intSave);
    }
    SCHEDULER_UNLOCK(intSave);
}

LITE_OS_SEC_TEXT VOID OsTaskResourcesToFree(LosTaskCB *taskCB)
{
    LosProcessCB *processCB = OS_PCB_FROM_PID(taskCB->processID);//通过任务找到所属进程
    UINT32 syncSignal = LOSCFG_BASE_IPC_SEM_LIMIT;
    UINT32 mapSize, intSave;
    UINTPTR mapBase, topOfStack;
    UINT32 ret;

    if (OsProcessIsUserMode(processCB) && (taskCB->userMapBase != 0)) {//进程在用户态而且任务栈不为空
        SCHEDULER_LOCK(intSave);
        mapBase = (UINTPTR)taskCB->userMapBase;//先保存任务栈底位置
        mapSize = taskCB->userMapSize;//先保存任务栈大小
        taskCB->userMapBase = 0;
        taskCB->userArea = 0;
        SCHEDULER_UNLOCK(intSave);

        LOS_ASSERT(!(processCB->vmSpace == NULL));
        ret = OsUnMMap(processCB->vmSpace, (UINTPTR)mapBase, mapSize);
        if ((ret != LOS_OK) && (mapBase != 0) && !(processCB->processStatus & OS_PROCESS_STATUS_INIT)) {
            PRINT_ERR("process(%u) ummap user task(%u) stack failed! mapbase: 0x%x size :0x%x, error: %d\n",
                      processCB->processID, taskCB->taskID, mapBase, mapSize, ret);
        }

#if (LOSCFG_KERNEL_LITEIPC == YES)
        LiteIpcRemoveServiceHandle(taskCB);
#endif
    }

    if (taskCB->taskStatus & OS_TASK_STATUS_UNUSED) {
        topOfStack = taskCB->topOfStack;
        taskCB->topOfStack = 0;
#if (LOSCFG_KERNEL_SMP_TASK_SYNC == YES)
        syncSignal = taskCB->syncSignal;
        taskCB->syncSignal = LOSCFG_BASE_IPC_SEM_LIMIT;
#endif
        OsTaskKernelResourcesToFree(syncSignal, topOfStack);

        SCHEDULER_LOCK(intSave);
        OsInsertTCBToFreeList(taskCB);
        SCHEDULER_UNLOCK(intSave);
    }
    return;
}
//任务基本信息的初始化
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

#if (LOSCFG_KERNEL_SMP == YES)
    taskCB->currCpu      = OS_TASK_INVALID_CPUID;
    taskCB->cpuAffiMask  = (initParam->usCpuAffiMask) ?
                            initParam->usCpuAffiMask : LOSCFG_KERNEL_CPU_MASK;
#endif
#if (LOSCFG_KERNEL_LITEIPC == YES)
    LOS_ListInit(&(taskCB->msgListHead));//初始化 liteipc的消息链表 
    (VOID)memset_s(taskCB->accessMap, sizeof(taskCB->accessMap), 0, sizeof(taskCB->accessMap));
#endif
    taskCB->policy = (initParam->policy == LOS_SCHED_FIFO) ? LOS_SCHED_FIFO : LOS_SCHED_RR;//调度模式
    taskCB->taskStatus = OS_TASK_STATUS_INIT;//任务初始状态
    if (initParam->uwResved & OS_TASK_FLAG_DETACHED) {//分离模式 代表任务与其他任务的关系
        taskCB->taskStatus |= OS_TASK_FLAG_DETACHED;//任务状态贴上分离标签
    } else {//参与模式
        LOS_ListInit(&taskCB->joinList);
        taskCB->taskStatus |= OS_TASK_FLAG_PTHREAD_JOIN;//任务状态贴上联合标签
    }

    taskCB->futex.index = OS_INVALID_VALUE;
    LOS_ListInit(&taskCB->lockList);//初始化互斥锁链表
}
//任务初始化
LITE_OS_SEC_TEXT_INIT STATIC UINT32 OsTaskCBInit(LosTaskCB *taskCB, const TSK_INIT_PARAM_S *initParam,
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
        OsUserTaskStackInit(taskCB->stackPointer, taskCB->taskEntry, initParam->userParam.userSP);//初始化用户态任务栈
        //这里要注意,任务的上下文是始终保存在内核栈空间,而用户态时运行在用户态栈空间.(context->SP = userSP 指向了用户态栈空间)
    }

    if (!processCB->threadNumber) {//进程线程数量为0时，
        processCB->threadGroupID = taskCB->taskID;//任务为线程组 组长
    }
    processCB->threadNumber++;//这里说明 线程和TASK是一个意思 threadNumber代表活动线程数,thread消亡的时候会 threadNumber--

    numCount = processCB->threadCount;//代表总线程数，包括销毁的，只要存在过的都算，这个值也就是在这里用下，
    processCB->threadCount++;//线程总数++,注意这个数会一直累加的,哪怕thread最后退出了,这个统计这个进程曾经存在过的线程数量
    SCHEDULER_UNLOCK(intSave);

    if (initParam->pcName == NULL) {
        (VOID)memset_s(taskCB->taskName, sizeof(CHAR) * OS_TCB_NAME_LEN, 0, sizeof(CHAR) * OS_TCB_NAME_LEN);
        (VOID)snprintf_s(taskCB->taskName, sizeof(CHAR) * OS_TCB_NAME_LEN,
                         (sizeof(CHAR) * OS_TCB_NAME_LEN) - 1, "thread%u", numCount);
        return LOS_OK;
    }

    if (mode == OS_KERNEL_MODE) {//内核模式的情况
        ret = memcpy_s(taskCB->taskName, sizeof(CHAR) * OS_TCB_NAME_LEN, initParam->pcName, strlen(initParam->pcName));
        if (ret != EOK) {
            taskCB->taskName[0] = '\0';
        }
    }

    return LOS_OK;
}
//获取一个空闲TCB
LITE_OS_SEC_TEXT LosTaskCB *OsGetFreeTaskCB(VOID)
{
    UINT32 intSave;
    LosTaskCB *taskCB = NULL;

    SCHEDULER_LOCK(intSave);
    if (LOS_ListEmpty(&g_losFreeTask)) {//全局空闲task为空
        SCHEDULER_UNLOCK(intSave);
        PRINT_ERR("No idle TCB in the system!\n");
        return NULL;
    }

    taskCB = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&g_losFreeTask));//拿到第一节点并通过pendlist拿到完整的TCB
    LOS_ListDelete(LOS_DL_LIST_FIRST(&g_losFreeTask));//从g_losFreeTask链表中摘除自己
    SCHEDULER_UNLOCK(intSave);

    return taskCB;
}
//创建任务，并使该任务进入suspend状态，不对该任务进行调度。如果需要调度，可以调用LOS_TaskResume使该任务进入ready状态
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
    return LOS_OK;

LOS_ERREND_TCB_INIT:
    (VOID)LOS_MemFree(pool, topStack);
LOS_ERREND_REWIND_SYNC:
#if (LOSCFG_KERNEL_SMP_TASK_SYNC == YES)
    OsTaskSyncDestroy(taskCB->syncSignal);
#endif
LOS_ERREND_REWIND_TCB:
    SCHEDULER_LOCK(intSave);
    OsInsertTCBToFreeList(taskCB);//归还freetask
    SCHEDULER_UNLOCK(intSave);
LOS_ERREND:
    return errRet;
}
//创建任务，并使该任务进入ready状态，如果就绪队列中没有更高优先级的任务，则运行该任务
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

    if (initParam->uwResved & OS_TASK_FLAG_IDLEFLAG) {//OS_TASK_FLAG_IDLEFLAG 是属于内核 idle进程专用的
        initParam->processID = OsGetIdleProcessID();//获取空闲进程
    } else if (OsProcessIsUserMode(OsCurrProcessGet())) {//当前进程是否为用户模式
        initParam->processID = OsGetKernelInitProcessID();//任务归属于"Kernel"进程
    } else {
        initParam->processID = OsCurrProcessGet()->processID;//获取当前进程 ID赋值
    }
    initParam->uwResved &= ~OS_TASK_FLAG_IDLEFLAG;//不能是 OS_TASK_FLAG_IDLEFLAG
    initParam->uwResved &= ~OS_TASK_FLAG_PTHREAD_JOIN;//不能是 OS_TASK_FLAG_PTHREAD_JOIN
    if (initParam->uwResved & LOS_TASK_STATUS_DETACHED) {//是否设置了自动删除
        initParam->uwResved = OS_TASK_FLAG_DETACHED;//自动删除,注意这里是 = ,也就是说只有 OS_TASK_FLAG_DETACHED 一个标签了
    }

    ret = LOS_TaskCreateOnly(taskID, initParam);//创建一个任务,这是任务创建的实体,前面都只是前期准备工作
    if (ret != LOS_OK) {
        return ret;
    }
    taskCB = OS_TCB_FROM_TID(*taskID);//通过ID拿到task实体

    SCHEDULER_LOCK(intSave);
    taskCB->taskStatus &= ~OS_TASK_STATUS_INIT;//任务不再是初始化
    OS_TASK_SCHED_QUEUE_ENQUEUE(taskCB, 0);//加入调度就绪队列,新任务是直接进入就绪队列的
    SCHEDULER_UNLOCK(intSave);

    /* in case created task not running on this core,
       schedule or not depends on other schedulers status. */
    LOS_MpSchedule(OS_MP_CPU_ALL);//如果创建的任务没有在这个核心上运行，是否调度取决于其他调度程序的状态。
    if (OS_SCHEDULER_ACTIVE) {//当前CPU核处于可调度状态
        LOS_Schedule();//发起调度
    }

    return LOS_OK;
}
//恢复挂起的任务，使该任务进入ready状态
LITE_OS_SEC_TEXT_INIT UINT32 LOS_TaskResume(UINT32 taskID)
{
    UINT32 intSave;
    UINT16 tempStatus;
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

    tempStatus = taskCB->taskStatus;
    if (tempStatus & OS_TASK_STATUS_UNUSED) {//有未使用的标签
        errRet = LOS_ERRNO_TSK_NOT_CREATED;
        OS_GOTO_ERREND();
    } else if (!(tempStatus & OS_TASK_STATUS_SUSPEND)) {
        errRet = LOS_ERRNO_TSK_NOT_SUSPENDED;
        OS_GOTO_ERREND();
    }

    taskCB->taskStatus &= ~OS_TASK_STATUS_SUSPEND;
    if (!(taskCB->taskStatus & OS_CHECK_TASK_BLOCK)) {
        OS_TASK_SCHED_QUEUE_ENQUEUE(taskCB, OS_PROCESS_STATUS_PEND);
        if (OS_SCHEDULER_ACTIVE) {
            needSched = TRUE;
        }
    }

    SCHEDULER_UNLOCK(intSave);

    if (needSched) {//需要调度
        LOS_MpSchedule(OS_MP_CPU_ALL);//
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

#if (LOSCFG_KERNEL_SMP == YES)
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
//任务暂停,参数可以不是当前任务，也就是说 A任务可以让B任务处于阻塞状态,挂起指定的任务，然后切换任务
LITE_OS_SEC_TEXT STATIC UINT32 OsTaskSuspend(LosTaskCB *taskCB)
{
    UINT32 errRet;
    UINT16 tempStatus;
    LosTaskCB *runTask = NULL;

    tempStatus = taskCB->taskStatus;
    if (tempStatus & OS_TASK_STATUS_UNUSED) {
        return LOS_ERRNO_TSK_NOT_CREATED;
    }

    if (tempStatus & OS_TASK_STATUS_SUSPEND) {
        return LOS_ERRNO_TSK_ALREADY_SUSPENDED;
    }

    if ((tempStatus & OS_TASK_STATUS_RUNNING) && //如果参数任务正在运行，注意多Cpu core情况下，贴着正在运行标签的任务并不一定是当前CPU的执行任务，
        !OsTaskSuspendCheckOnRun(taskCB, &errRet)) {//很有可能是别的CPU core在跑的任务
        return errRet;
    }

    if (tempStatus & OS_TASK_STATUS_READY) {//就绪状态下怎样？
        OS_TASK_SCHED_QUEUE_DEQUEUE(taskCB, OS_PROCESS_STATUS_PEND);//从就绪队列中删除，并给任务所属进程贴上阻塞标签
    }

    taskCB->taskStatus |= OS_TASK_STATUS_SUSPEND;//任务贴上阻塞标签

    runTask = OsCurrTaskGet();//获取当前任务
    if (taskCB == runTask) {//如果任务是当前任务，要怎样？
        OsSchedResched();//申请调度，自己把自己阻了还不调度等啥呢
    }

    return LOS_OK;
}
//外部接口，对OsTaskSuspend的封装
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
//设置任务为不使用状态
STATIC INLINE VOID OsTaskStatusUnusedSet(LosTaskCB *taskCB)
{
    taskCB->taskStatus |= OS_TASK_STATUS_UNUSED;
    taskCB->eventMask = 0;

    OS_MEM_CLEAR(taskCB->taskID);
}
//task释放持有的所有锁，一个任务可以持有很多把锁
STATIC INLINE VOID OsTaskReleaseHoldLock(LosProcessCB *processCB, LosTaskCB *taskCB)
{
    LosMux *mux = NULL;
    UINT32 ret;

    while (!LOS_ListEmpty(&taskCB->lockList)) {//轮询任务锁链表
        mux = LOS_DL_LIST_ENTRY(LOS_DL_LIST_FIRST(&taskCB->lockList), LosMux, holdList);//取出第一个互斥锁
        ret = OsMuxUnlockUnsafe(taskCB, mux, NULL);//还锁
        if (ret != LOS_OK) {//换锁成功
            LOS_ListDelete(&mux->holdList);//从锁链表中将自己摘除
            PRINT_ERR("mux ulock failed! : %u\n", ret);
        }
    }

    if (processCB->processMode == OS_USER_MODE) {//如果是用户模式，会怎样？
        OsTaskJoinPostUnsafe(taskCB);//把taskCB绑在一起的其他task唤醒.
        OsFutexNodeDeleteFromFutexHash(&taskCB->futex, TRUE, NULL, NULL);
    }

    OsTaskSyncWake(taskCB);//同步唤醒任务
}
////删除一个正在运行的任务
LITE_OS_SEC_TEXT VOID OsRunTaskToDelete(LosTaskCB *taskCB)
{
    LosProcessCB *processCB = OS_PCB_FROM_PID(taskCB->processID);//拿到task所属进程
    OsTaskReleaseHoldLock(processCB, taskCB);//task还锁
    OsTaskStatusUnusedSet(taskCB);//task重置为未使用状态，等待回收

    LOS_ListDelete(&taskCB->threadList);//从进程的线程链表中将自己摘除
    processCB->threadNumber--;//进程的活动task --，注意进程还有一个记录总task的变量 processCB->threadCount
    LOS_ListTailInsert(&g_taskRecyleList, &taskCB->pendList);//将task插入回收链表，等待回收资源再利用
    OsEventWriteUnsafe(&g_resourceEvent, OS_RESOURCE_EVENT_FREE, FALSE, NULL);//发送释放资源的事件,事件由 OsResourceRecoveryTask 消费

    OsSchedResched();//申请调度
    return;
}

/*
 * Check if needs to do the delete operation on the running task.
 * Return TRUE, if needs to do the deletion.
 * Rerturn FALSE, if meets following circumstances:
 * 1. Do the deletion across cores, if SMP is enabled
 * 2. Do the deletion when preemption is disabled
 * 3. Do the deletion in hard-irq
 * then LOS_TaskDelete will directly return with 'ret' value.
 */
 /****************************************************************
 检查是否需要对正在运行的任务执行删除操作,如果需要删除，则返回TRUE。
 如果满足以下情况，则返回FALSE：
 1.如果启用了SMP，则跨CPU执行删除
 2.禁用抢占时执行删除
 3.在硬irq中删除
 然后LOS_TaskDelete将直接返回ret值
 ****************************************************************/
STATIC BOOL OsRunTaskToDeleteCheckOnRun(LosTaskCB *taskCB, UINT32 *ret)
{
    /* init default out return value */
    *ret = LOS_OK;

#if (LOSCFG_KERNEL_SMP == YES)
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
//删除不活动的任务 !OS_TASK_STATUS_RUNNING 
STATIC VOID OsTaskDeleteInactive(LosProcessCB *processCB, LosTaskCB *taskCB)
{
    LosMux *mux = (LosMux *)taskCB->taskMux; //任务

    LOS_ASSERT(!(taskCB->taskStatus & OS_TASK_STATUS_RUNNING));//不活动任务的定义:非OS_TASK_STATUS_RUNNING状态的任务叫...

    OsTaskReleaseHoldLock(processCB, taskCB);//释放任务所持有的锁

    if (taskCB->taskStatus & OS_TASK_STATUS_READY) {//就绪状态时
        OS_TASK_SCHED_QUEUE_DEQUEUE(taskCB, 0);//从就绪队列中删除
    } else if (taskCB->taskStatus & OS_TASK_STATUS_PEND) {//阻塞状态时
        LOS_ListDelete(&taskCB->pendList);//从阻塞链表中摘除节点
        if (LOS_MuxIsValid(mux) == TRUE) {//如果是在等待锁
            OsMuxBitmapRestore(mux, taskCB, (LosTaskCB *)mux->owner);//恢复互斥锁位图
        }
    }

    if (taskCB->taskStatus & (OS_TASK_STATUS_DELAY | OS_TASK_STATUS_PEND_TIME)) {//定时任务
        OsTimerListDelete(taskCB);//从定时器列表中删除
    }
    OsTaskStatusUnusedSet(taskCB);//重置任务状态为未使用状态

    LOS_ListDelete(&taskCB->threadList);//将自己从进程的任务链表上摘除掉.
    processCB->threadNumber--;//任务数减少
    LOS_ListTailInsert(&g_taskRecyleList, &taskCB->pendList);//把自己挂到可回收链表上,等待系统的回收再利用.
    return;
}
//以不安全的方式删除参数任务
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

    if (mode == OS_USER_MODE) { //用户态模式
        SCHEDULER_UNLOCK(intSave);//先释放锁
        OsTaskResourcesToFree(taskCB);//释放任务资源
        SCHEDULER_LOCK(intSave);
    }

#if (LOSCFG_KERNEL_SMP == YES)
    LOS_ASSERT(OsPercpuGet()->taskLockCnt == 1);
#else
    LOS_ASSERT(OsPercpuGet()->taskLockCnt == 0);
#endif
    OsRunTaskToDelete(taskCB);//删除一个正在运行的任务

EXIT:
    SCHEDULER_UNLOCK(intSave);
    return errRet;
}
//删除指定的任务,回归任务池
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
//任务延时等待，释放CPU，等待时间到期后该任务会重新进入ready状态
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

    if (tick == 0) {//没有节拍的情况
        return LOS_TaskYield();//大公无私，主动让位，所谓主动让位是将自己放在就绪队列的末尾，可人还在就绪队列中的。
    } else {
        SCHEDULER_LOCK(intSave);
        OS_TASK_SCHED_QUEUE_DEQUEUE(runTask, OS_PROCESS_STATUS_PEND);//移出就绪队列，进程贴上阻塞标签，
        OsAdd2TimerList(runTask, tick);//加入定时器，等待超时触发。
        runTask->taskStatus |= OS_TASK_STATUS_DELAY;//任务贴上延期的标签
        OsSchedResched();//申请调度
        SCHEDULER_UNLOCK(intSave);
    }

    return LOS_OK;
}
//获取任务的优先级
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
//设置指定任务的优先级
LITE_OS_SEC_TEXT_MINOR UINT32 LOS_TaskPriSet(UINT32 taskID, UINT16 taskPrio)
{
    BOOL isReady = FALSE;
    UINT32 intSave;
    LosTaskCB *taskCB = NULL;
    UINT16 tempStatus;
    LosProcessCB *processCB = NULL;

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
    tempStatus = taskCB->taskStatus;
    if (tempStatus & OS_TASK_STATUS_UNUSED) {
        SCHEDULER_UNLOCK(intSave);
        return LOS_ERRNO_TSK_NOT_CREATED;
    }

    /* delete the task and insert with right priority into ready queue */
    isReady = tempStatus & OS_TASK_STATUS_READY;
    if (isReady) {//就绪状态下怎么设置优先级
        processCB = OS_PCB_FROM_PID(taskCB->processID);//获取进程实体
        OS_TASK_PRI_QUEUE_DEQUEUE(processCB, taskCB);//先出进程的就绪队列
        taskCB->priority = taskPrio;//设置任务优先级
        OS_TASK_PRI_QUEUE_ENQUEUE(processCB, taskCB);//再进进程的就绪队列，为什么要这样？因为进程有32个就绪队列，优先级变了，task所属的就绪队列就变了，所以要重新进入
    } else {
        taskCB->priority = taskPrio;//不在队列的情况，直接设置就好了。这里也可以看出，所谓队列其实就是只有就绪状态的队列，其他状态木有队列！记住这个对理解鸿蒙内核甚是重要。
        if (tempStatus & OS_TASK_STATUS_RUNNING) {
            isReady = TRUE;
        }
    }

    SCHEDULER_UNLOCK(intSave);
    /* delete the task and insert with right priority into ready queue */	//什么鬼，官方这条注释跑这里来想表达什么？
    if (isReady) {
        LOS_MpSchedule(OS_MP_CPU_ALL);
        LOS_Schedule();
    }
    return LOS_OK;
}
//设置当前任务的优先级
LITE_OS_SEC_TEXT_MINOR UINT32 LOS_CurTaskPriSet(UINT16 taskPrio)
{
    return LOS_TaskPriSet(OsCurrTaskGet()->taskID, taskPrio);
}

/*
 * Description : pend a task in list
 * Input       : list       --- wait task list
 *               taskStatus --- task status
 *               timeOut    ---  Expiry time
 * Return      : LOS_OK on success or LOS_NOK on failure
 */	//任务等待,将当前任务挂到参数 list上
UINT32 OsTaskWait(LOS_DL_LIST *list, UINT32 timeout, BOOL needSched)
{
    LosTaskCB *runTask = NULL;
    LOS_DL_LIST *pendObj = NULL;

    runTask = OsCurrTaskGet();//获取当前任务
    OS_TASK_SCHED_QUEUE_DEQUEUE(runTask, OS_PROCESS_STATUS_PEND);//将任务从就绪队列摘除,并变成阻塞状态
    pendObj = &runTask->pendList;
    runTask->taskStatus |= OS_TASK_STATUS_PEND;//给任务贴上阻塞任务标签
    LOS_ListTailInsert(list, pendObj);//将阻塞任务挂到list上,,这步很关键,很重要!
    if (timeout != LOS_WAIT_FOREVER) {//非永远等待的时候
        runTask->taskStatus |= OS_TASK_STATUS_PEND_TIME;//阻塞任务再贴上在一段时间内阻塞的标签
        OsAdd2TimerList(runTask, timeout);//把任务加到定时器链表中
    }

    if (needSched == TRUE) {//是否需要调度
        OsSchedResched();//申请调度,里面直接切换了任务上下文,至此任务不再往下执行了.
        if (runTask->taskStatus & OS_TASK_STATUS_TIMEOUT) {//这条语句是被调度再次选中时执行的,和上面的语句可能隔了很长时间,所以很可能已经超时了
            runTask->taskStatus &= ~OS_TASK_STATUS_TIMEOUT;//如果任务有timeout的标签,那么就去掉那个标签
            return LOS_ERRNO_TSK_TIMEOUT;
        }
    }
    return LOS_OK;
}

/*
 * Description : delete the task from pendlist and also add to the priqueue
 * Input       : resumedTask --- resumed task
 *               taskStatus  --- task status
 *///任务被唤醒
VOID OsTaskWake(LosTaskCB *resumedTask)//这个函数是被别的task唤醒的,当前任务可不是resumedTask,一定要明白这点.
{
    LOS_ListDelete(&resumedTask->pendList);//将自己从阻塞链表中摘除
    resumedTask->taskStatus &= ~OS_TASK_STATUS_PEND;//任务不再阻塞

    if (resumedTask->taskStatus & OS_TASK_STATUS_PEND_TIME) {//有阻塞时间标签的时候
        OsTimerListDelete(resumedTask);//从CPU的taskSortLink中摘除自己
        resumedTask->taskStatus &= ~OS_TASK_STATUS_PEND_TIME;//撕掉阻塞时间标签
    }
    if (!(resumedTask->taskStatus & OS_TASK_STATUS_SUSPEND)) {//任务不是挂起状态时
        OS_TASK_SCHED_QUEUE_ENQUEUE(resumedTask, OS_PROCESS_STATUS_PEND);//将任务加入调度队列,OS_PROCESS_STATUS_PEND表示加入就绪队列前的状态
    }//OS_TASK_SCHED_QUEUE_ENQUEUE 之后 resumedTask就变成了ready状态,等待被调度选中
}
//当前任务释放CPU，并将其移到具有相同优先级的就绪任务队列的末尾. 读懂这个函数 你就彻底搞懂了 yield
LITE_OS_SEC_TEXT_MINOR UINT32 LOS_TaskYield(VOID)
{
    UINT32 tskCount;
    UINT32 intSave;
    LosTaskCB *runTask = NULL;
    LosProcessCB *runProcess = NULL;

    if (OS_INT_ACTIVE) {
        return LOS_ERRNO_TSK_YIELD_IN_INT;
    }

    if (!OsPreemptable()) {
        return LOS_ERRNO_TSK_YIELD_IN_LOCK;
    }

    runTask = OsCurrTaskGet();//获取当前任务
    if (OS_TID_CHECK_INVALID(runTask->taskID)) {
        return LOS_ERRNO_TSK_ID_INVALID;
    }

    SCHEDULER_LOCK(intSave);

    /* reset timeslice of yeilded task */
    runTask->timeSlice = 0;//重置时间片
    runProcess = OS_PCB_FROM_PID(runTask->processID);//获取当前进程
    tskCount = OS_TASK_PRI_QUEUE_SIZE(runProcess, runTask);//获取进程中和当前任务一样的task数
    if (tskCount > 0) {
        OS_TASK_PRI_QUEUE_ENQUEUE(runProcess, runTask);//先出队列,再入就绪队列, 这句话总意思就是是把自己排到最后,给同级兄弟让位置,所以叫 yield 
        runTask->taskStatus |= OS_TASK_STATUS_READY;//任务状态变成就绪
    } else {
        SCHEDULER_UNLOCK(intSave);
        return LOS_OK;
    }
    OsSchedResched();//申请调度
    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;
}
//任务上锁
LITE_OS_SEC_TEXT_MINOR VOID LOS_TaskLock(VOID)
{
    UINT32 intSave;
    UINT32 *losTaskLock = NULL;

    intSave = LOS_IntLock();//禁止所有IRQ和FIQ中断
    losTaskLock = &OsPercpuGet()->taskLockCnt;//获取cpu 上锁任务数
    (*losTaskLock)++;//任务上锁数量自增
    LOS_IntRestore(intSave);//启用所有IRQ和FIQ中断
}
//解锁任务
LITE_OS_SEC_TEXT_MINOR VOID LOS_TaskUnlock(VOID)
{
    UINT32 intSave;
    UINT32 *losTaskLock = NULL;
    Percpu *percpu = NULL;

    intSave = LOS_IntLock();//禁止所有IRQ和FIQ中断

    percpu = OsPercpuGet();//获取当前CPU
    losTaskLock = &OsPercpuGet()->taskLockCnt;//获取cpu 上锁任务数
    if (*losTaskLock > 0) {//说明有存在上锁的任务
        (*losTaskLock)--;//减少一个上锁任务
        if ((*losTaskLock == 0) && (percpu->schedFlag == INT_PEND_RESCH) &&
            OS_SCHEDULER_ACTIVE) {
            percpu->schedFlag = INT_NO_RESCH;
            LOS_IntRestore(intSave);//启用所有IRQ和FIQ中断
            LOS_Schedule();
            return;
        }
    }

    LOS_IntRestore(intSave);//启用所有IRQ和FIQ中断
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
    taskInfo->uwStackSize = taskCB->stackSize;//内核态栈大小
    taskInfo->uwTopOfStack = taskCB->topOfStack;//内核态栈顶位置
    taskInfo->uwEventMask = taskCB->eventMask;
    taskInfo->taskEvent = taskCB->taskEvent;
    taskInfo->pTaskSem = taskCB->taskSem;
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
//CPU亲和性（affinity）就是进程要在某个给定的CPU上尽量长时间地运行而不被迁移到其他处理器
//把任务设置为由哪个CPU核调度,用于多核CPU情况,（该函数仅在SMP模式下支持）
LITE_OS_SEC_TEXT_MINOR UINT32 LOS_TaskCpuAffiSet(UINT32 taskID, UINT16 cpuAffiMask)
{
#if (LOSCFG_KERNEL_SMP == YES)
    LosTaskCB *taskCB = NULL;
    UINT32 intSave;
    BOOL needSched = FALSE;
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

    taskCB->cpuAffiMask = cpuAffiMask;//参数set给tcb
    currCpuMask = CPUID_TO_AFFI_MASK(taskCB->currCpu);
    if (!(currCpuMask & cpuAffiMask)) {
        needSched = TRUE;//需要调度
        taskCB->signal = SIGNAL_AFFI;//设置信号
    }
    SCHEDULER_UNLOCK(intSave);

    if (needSched && OS_SCHEDULER_ACTIVE) {
        LOS_MpSchedule(currCpuMask);//发送信号调度信号给currCpuMask 1位上的CPU
        LOS_Schedule();//申请调度
    }
#endif
    (VOID)taskID;
    (VOID)cpuAffiMask;
    return LOS_OK;
}
//CPU亲和性（affinity）就是进程要在某个给定的CPU上尽量长时间地运行而不被迁移到其他处理器
//获取task和CPU的亲和性信息
LITE_OS_SEC_TEXT_MINOR UINT16 LOS_TaskCpuAffiGet(UINT32 taskID)
{
#if (LOSCFG_KERNEL_SMP == YES)
#define INVALID_CPU_AFFI_MASK   0
    LosTaskCB *taskCB = NULL;
    UINT16 cpuAffiMask;
    UINT32 intSave;

    if (OS_TID_CHECK_INVALID(taskID)) {
        return INVALID_CPU_AFFI_MASK;
    }

    taskCB = OS_TCB_FROM_TID(taskID);
    SCHEDULER_LOCK(intSave);
    if (taskCB->taskStatus & OS_TASK_STATUS_UNUSED) {
        SCHEDULER_UNLOCK(intSave);
        return INVALID_CPU_AFFI_MASK;
    }

    cpuAffiMask = taskCB->cpuAffiMask;
    SCHEDULER_UNLOCK(intSave);

    return cpuAffiMask;
#else
    (VOID)taskID;
    return 1;
#endif
}

/*
 * Description : Process pending signals tagged by others cores
 */
 /******************************************************
 	由其他CPU核触发阻塞进程的信号
	函数由汇编代码调用 ..\arch\arm\arm\src\los_dispatch.S
 ******************************************************/
LITE_OS_SEC_TEXT_MINOR UINT32 OsTaskProcSignal(VOID)
{
    Percpu *percpu = NULL;
    LosTaskCB *runTask = NULL;
    UINT32 intSave, ret;
	//私有且不可中断，无需保护。这个任务在其他CPU核看到它时总是在运行，所以它在执行代码的同时也可以继续接收信号
    /*
     * private and uninterruptable, no protection needed.
     * while this task is always running when others cores see it,
     * so it keeps recieving signals while follow code excuting.
     */
    runTask = OsCurrTaskGet();//获取当前任务
    if (runTask->signal == SIGNAL_NONE) {//无任务信号
        goto EXIT;
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
#if (LOSCFG_KERNEL_SMP == YES)
    } else if (runTask->signal & SIGNAL_AFFI) {//意思是下次调度其他cpu要媾和你
        runTask->signal &= ~SIGNAL_AFFI;//任务贴上被其他CPU媾和的标签

        /* pri-queue has updated, notify the target cpu */
        LOS_MpSchedule((UINT32)runTask->cpuAffiMask);//发生调度,此任务将移交给媾和CPU运行.
#endif
    }

EXIT:
    /* check if needs to schedule */
    percpu = OsPercpuGet();//获取当前CPU信息
    if (OsPreemptable() && (percpu->schedFlag == INT_PEND_RESCH)) {//注意这是CPU的调度标识,而不是任务的.
        percpu->schedFlag = INT_NO_RESCH;//贴上不需要调度的标签
        return INT_PEND_RESCH;//返回需要调度,返回值后的指令是: CMP     R0, #0	,BLNE          OsSchedPreempt ,如此将发生一轮调度. 
    }

    return INT_NO_RESCH;
}
//设置任务名称
LITE_OS_SEC_TEXT INT32 OsSetCurrTaskName(const CHAR *name)
{
    UINT32 intSave;
    UINT32 strLen;
    errno_t err;
    LosTaskCB *runTask = NULL;
    LosProcessCB *runProcess = NULL;
    const CHAR *namePtr = NULL;
    CHAR nameBuff[OS_TCB_NAME_LEN] = { 0 };

    runTask = OsCurrTaskGet();
    runProcess = OS_PCB_FROM_PID(runTask->processID);
    if (runProcess->processMode == OS_USER_MODE) {
        (VOID)LOS_ArchCopyFromUser(nameBuff, (const VOID *)name, OS_TCB_NAME_LEN);
        strLen = strnlen(nameBuff, OS_TCB_NAME_LEN);
        namePtr = nameBuff;
    } else {
        strLen = strnlen(name, OS_TCB_NAME_LEN);
        namePtr = name;
    }

    if (strLen == 0) {
        err = EINVAL;
        PRINT_ERR("set task(%u) name failed! %d\n", OsCurrTaskGet()->taskID, err);
        return err;
    } else if (strLen == OS_TCB_NAME_LEN) {
        strLen = strLen - 1;
    }

    SCHEDULER_LOCK(intSave);

    err = memcpy_s(runTask->taskName, OS_TCB_NAME_LEN, (VOID *)namePtr, strLen);
    if (err != EOK) {
        runTask->taskName[0] = '\0';
        err = EINVAL;
        goto EXIT;
    }

    runTask->taskName[strLen] = '\0';

    /* if thread is main thread, then set processName as taskName */
    if (runTask->taskID == runProcess->threadGroupID) {
        (VOID)memcpy_s(runProcess->processName, OS_PCB_NAME_LEN, (VOID *)runTask->taskName, OS_TCB_NAME_LEN);
    }

    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;

EXIT:
    SCHEDULER_UNLOCK(intSave);
    return err;
}
//1.当前进程中的任务集体退群, 2.当前进程贴上退出标签
LITE_OS_SEC_TEXT VOID OsTaskExitGroup(UINT32 status)
{
    LosProcessCB *processCB = NULL;
    LosTaskCB *taskCB = NULL;
    LOS_DL_LIST *list = NULL;
    LOS_DL_LIST *head = NULL;
    LosTaskCB *runTask[LOSCFG_KERNEL_CORE_NUM] = { 0 };
    UINT32 intSave;
#if (LOSCFG_KERNEL_SMP == YES)
    UINT16 cpu;
#endif

    SCHEDULER_LOCK(intSave);//调度自旋锁,这块锁的代码有点多,这块容易出问题!出问题也不好复现,希望鸿蒙有充分测试这块的功能. @note_thinking
    processCB = OsCurrProcessGet();
    if (processCB->processStatus & OS_PROCESS_FLAG_EXIT) {//进程被贴上退出标记
        SCHEDULER_UNLOCK(intSave);
        return;
    }

    processCB->processStatus |= OS_PROCESS_FLAG_EXIT;//贴上进程要退出的标签
    runTask[ArchCurrCpuid()] = OsCurrTaskGet();//记录当前任务
    runTask[ArchCurrCpuid()]->sig.sigprocmask = OS_INVALID_VALUE;//当前进程不再接受任何信号

    list = &processCB->threadSiblingList;//拿到进程的任务列表
    head = list;
    do {
        taskCB = LOS_DL_LIST_ENTRY(list->pstNext, LosTaskCB, threadList);//遍历任务列表
        if (!(taskCB->taskStatus & OS_TASK_STATUS_RUNNING)) {//不在运行(活动)的任务
            OsTaskDeleteInactive(processCB, taskCB);//删除不活动的任务
        } else {
#if (LOSCFG_KERNEL_SMP == YES) //多CPU core时,注意一个进程的多个任务很可能是多个CPU在跑的,系统调度会尽量确保一个CPU跑一个任务,
            if (taskCB->currCpu != ArchCurrCpuid()) {//在任务被打断再次调度后由同一个CPU完成,但并不100%确保每次都是同一个cpu跑完一个任务的生命周期
                taskCB->signal = SIGNAL_KILL;//任务信号变成kill
                runTask[taskCB->currCpu] = taskCB;//将另一个cpu的运行任务强制变成taskCB
                LOS_MpSchedule(taskCB->currCpu);//给另一个CPU发送调度信号,另一个CPU接收到信号后立即调度,干掉taskCB
            }
#endif
            list = list->pstNext;//处理下一个任务
        }
    } while (head != list->pstNext);//双向循环链表的标准结束动作就是(head == list->pstNext)

#if (LOSCFG_KERNEL_SMP == YES)	//多cpu core的情况, 例如: ipcamera_hi3516dv300 平台是 2个core
    for (cpu = 0; cpu < LOSCFG_KERNEL_CORE_NUM; cpu++) {//这个循环很有意思,主要目的是用来确认task在其他CPU的执行情况
        if ((cpu == ArchCurrCpuid()) || (runTask[cpu] == NULL)) {//1. runTask数据记录了会在其他CPU执行的任务
            continue;//2.如果是当前CPU那已经在 OsTaskDeleteInactive 中就把task给干掉了.
        }//3.那问题来了,剩下的如果知道其他CPU也把相应的任务给干掉了呢?

        (VOID)OsTaskSyncWait(runTask[cpu]);//4.方法就是给要被干掉的任务发同步信号,如此另一个CPU调度到task,发现了上面的SIGNAL_KILL信号
    }//5.那是一个要干掉task的信号,task被干掉后自然发不出回复信号,OsTaskSyncWait中2个周期后等待时间到.此时返回LOS_ERRNO_TSK_MP_SYNC_FAILED
#endif
    SCHEDULER_UNLOCK(intSave);

    LOS_ASSERT(processCB->threadNumber == 1);//这一趟下来,断言进程只有一个正在活动的任务了
    return;
}
//任务退群并销毁,进入任务的回收链表之后再进入空闲链表,等着再次被分配使用.
LITE_OS_SEC_TEXT VOID OsExecDestroyTaskGroup(VOID)
{
    OsTaskExitGroup(OS_PRO_EXIT_OK);//进程
    OsTaskCBRecyleToFree();
}
//暂停当前进程的所有任务
LITE_OS_SEC_TEXT VOID OsProcessSuspendAllTask(VOID)
{
    LosProcessCB *process = NULL;
    LosTaskCB *taskCB = NULL;
    LosTaskCB *runTask = NULL;
    LOS_DL_LIST *list = NULL;
    LOS_DL_LIST *head = NULL;
    UINT32 intSave;
    UINT32 ret;

    SCHEDULER_LOCK(intSave);
    process = OsCurrProcessGet();//获取当前进程
    runTask = OsCurrTaskGet();//获取当前任务

    list = &process->threadSiblingList;//threadSiblingList上挂了进程下面的所有线程(task)
    head = list;
    do {
        taskCB = LOS_DL_LIST_ENTRY(list->pstNext, LosTaskCB, threadList);//通过threadList找到任务控制块(TCB)
        if (taskCB != runTask) {//只要不是当前任务就怎样?
            ret = OsTaskSuspend(taskCB);//暂停掉他们
            if ((ret != LOS_OK) && (ret != LOS_ERRNO_TSK_ALREADY_SUSPENDED)) {//暂停失败的处理
                PRINT_ERR("process(%d) suspend all task(%u) failed! ERROR: 0x%x\n",
                          process->processID, taskCB->taskID, ret);
            }
        }
        list = list->pstNext;//下一个
    } while (head != list->pstNext);//一直从头到尾的轮询一遍

    SCHEDULER_UNLOCK(intSave);
    return;
}

UINT32 OsUserTaskOperatePermissionsCheck(LosTaskCB *taskCB)
{
    if (taskCB == NULL) {
        return LOS_EINVAL;
    }

    if (taskCB->taskStatus & OS_TASK_STATUS_UNUSED) {
        return LOS_EINVAL;
    }

    if (OsCurrProcessGet()->processID != taskCB->processID) {
        return LOS_EPERM;
    }

    return LOS_OK;
}
//创建任务之前,检查用户态任务栈的参数,是否地址在用户空间
LITE_OS_SEC_TEXT_INIT STATIC UINT32 OsCreateUserTaskParamCheck(UINT32 processID, TSK_INIT_PARAM_S *param)
{
    UserTaskParam *userParam = NULL;

    if (param == NULL) {
        return OS_INVALID_VALUE;
    }

    userParam = &param->userParam;
    if ((processID == OS_INVALID_VALUE) && !LOS_IsUserAddress(userParam->userArea)) {
        return OS_INVALID_VALUE;
    }

    if (!LOS_IsUserAddress((UINTPTR)param->pfnTaskEntry)) {
        return OS_INVALID_VALUE;
    }

    if ((!userParam->userMapSize) || !LOS_IsUserAddressRange(userParam->userMapBase, userParam->userMapSize)) {
        return OS_INVALID_VALUE;
    }

    if (userParam->userArea &&
        ((userParam->userSP <= userParam->userMapBase) ||
        (userParam->userSP > (userParam->userMapBase + userParam->userMapSize)))) {
        return OS_INVALID_VALUE;
    }

    return LOS_OK;
}
//创建一个用户态任务
LITE_OS_SEC_TEXT_INIT INT32 OsCreateUserTask(UINT32 processID, TSK_INIT_PARAM_S *initParam)
{
    LosProcessCB *processCB = NULL;
    UINT32 taskID;
    UINT32 ret;
    UINT32 intSave;

    ret = OsCreateUserTaskParamCheck(processID, initParam);
    if (ret != LOS_OK) {
        return ret;
    }

    initParam->uwStackSize = OS_USER_TASK_SYSCALL_SATCK_SIZE;//设置任务栈大小,这里用了用户态下系统调用的栈大小(12K)      
    initParam->usTaskPrio = OS_TASK_PRIORITY_LOWEST;//设置默认优先级
    initParam->policy = LOS_SCHED_RR;//调度方式为抢占式,注意鸿蒙不仅仅只支持抢占式调度方式
    if (processID == OS_INVALID_VALUE) {//外面没指定进程ID的处理
        SCHEDULER_LOCK(intSave);
        processCB = OsCurrProcessGet();//拿当前运行的进程
        initParam->processID = processCB->processID;//进程ID赋值
        initParam->consoleID = processCB->consoleID;//任务控制台ID归属
        SCHEDULER_UNLOCK(intSave);
    } else {//进程已经创建
        processCB = OS_PCB_FROM_PID(processID);//通过ID拿到进程PCB
        if (!(processCB->processStatus & (OS_PROCESS_STATUS_INIT | OS_PROCESS_STATUS_RUNNING))) {//进程状态有初始和正在运行两个标签时
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
//获取任务的调度方式
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
//以不安全的方式设置任务的调度信息, 不安全指的是SCHEDULER_LOCK  在两个函数中SCHEDULER_UNLOCK 
LITE_OS_SEC_TEXT INT32 OsTaskSchedulerSetUnsafe(LosTaskCB *taskCB, UINT16 policy, UINT16 priority,
                                                BOOL policyFlag, UINT32 intSave)
{
    BOOL needSched = TRUE;
    if (taskCB->taskStatus & OS_TASK_STATUS_READY) {//就绪状态的处理
        OS_TASK_PRI_QUEUE_DEQUEUE(OS_PCB_FROM_PID(taskCB->processID), taskCB);//先出就绪队列,因为这里是要改变优先级的.
    }//一旦任务的调度优先级,将划到对应优先级的队列中,每个进程都有32个任务就绪队列. process.threadPriQueueList负责管理

    if (policyFlag == TRUE) {//TRUE表示 调度方式要改
        if (policy == LOS_SCHED_FIFO) {//如果是 FIFO 方式
            taskCB->timeSlice = 0;//不要时间片,只有抢占式才会需要时间片
        }
        taskCB->policy = policy;//改变调度方式
    }
    taskCB->priority = priority;//改变优先级

    if (taskCB->taskStatus & OS_TASK_STATUS_INIT) {//如果任务状态有初始状态的标签
        taskCB->taskStatus &= ~OS_TASK_STATUS_INIT;//去掉初始状态标签
        taskCB->taskStatus |= OS_TASK_STATUS_READY;//贴上就绪标签
    }

    if (taskCB->taskStatus & OS_TASK_STATUS_READY) {//如果有就绪标签
        taskCB->taskStatus &= ~OS_TASK_STATUS_READY;//去掉就绪标签,why这么做?因为只有非就绪状态的任务才可能加入 OS_TASK_SCHED_QUEUE_ENQUEUE
        OS_TASK_SCHED_QUEUE_ENQUEUE(taskCB, OS_PROCESS_STATUS_INIT);//任务加入到调度队列 ,具体看他的函数实现 OsTaskSchedQueueEnqueue
    } else if (taskCB->taskStatus & OS_TASK_STATUS_RUNNING) {//如果有运行标签,直接goto调度
        goto SCHEDULE;
    } else {
        needSched = FALSE;
    }

SCHEDULE:
    SCHEDULER_UNLOCK(intSave);

    LOS_MpSchedule(OS_MP_CPU_ALL);
    if (OS_SCHEDULER_ACTIVE && (needSched == TRUE)) {
        LOS_Schedule();//申请调度
    }

    return LOS_OK;
}
//设置任务的调度信息
LITE_OS_SEC_TEXT INT32 LOS_SetTaskScheduler(INT32 taskID, UINT16 policy, UINT16 priority)
{
    UINT32 intSave;
    LosTaskCB *taskCB = NULL;

    if (OS_TID_CHECK_INVALID(taskID)) {
        return LOS_ESRCH;
    }

    if (priority > OS_TASK_PRIORITY_LOWEST) {
        return LOS_EINVAL;
    }

    if ((policy != LOS_SCHED_FIFO) && (policy != LOS_SCHED_RR)) {
        return LOS_EINVAL;
    }

    SCHEDULER_LOCK(intSave);//拿到自旋锁
    taskCB = OS_TCB_FROM_TID(taskID);
    return OsTaskSchedulerSetUnsafe(taskCB, policy, priority, TRUE, intSave);//以不安全的方式设置任务的调度信息,为什么不安全? 因为自旋锁跨了一个函数 
}
//写一个资源事件
LITE_OS_SEC_TEXT VOID OsWriteResourceEvent(UINT32 events)
{
    (VOID)LOS_EventWrite(&g_resourceEvent, events);
}
//资源回收任务
STATIC VOID OsResourceRecoveryTask(VOID)
{
    UINT32 ret;

    while (1) {//死循环,回收资源不存在退出情况,只要系统在运行资源就需要回收
        ret = LOS_EventRead(&g_resourceEvent, OS_RESOURCE_EVENT_MASK,
                            LOS_WAITMODE_OR | LOS_WAITMODE_CLR, LOS_WAIT_FOREVER);//读取资源事件
        if (ret & (OS_RESOURCE_EVENT_FREE | OS_RESOURCE_EVENT_OOM)) {//收到资源释放或内存异常情况
            OsTaskCBRecyleToFree();//回收任务到空闲任务池
            OsProcessCBRecyleToFree();//回收进程到空闲进程池
        }

#ifdef LOSCFG_ENABLE_OOM_LOOP_TASK //内存溢出监测任务开关
        if (ret & OS_RESOURCE_EVENT_OOM) {//触发了这个事件
            (VOID)OomCheckProcess();//检查进程的内存溢出情况
        }
#endif
    }
}
//创建一个回收资源的任务
LITE_OS_SEC_TEXT UINT32 OsCreateResourceFreeTask(VOID)
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
    taskInitParam.uwStackSize = OS_TASK_RESOURCE_STATCI_SIZE;// 4K
    taskInitParam.pcName = "ResourcesTask";
    taskInitParam.usTaskPrio = OS_TASK_RESOURCE_FREE_PRIORITY;// 5 ,优先级很高
    return LOS_TaskCreate(&taskID, &taskInitParam);//创建任务，并加入就绪队列，立即调度
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */
