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

#include "los_process_pri.h"
#include "los_task_pri.h"
#include "los_hw_pri.h"
#include "los_sem_pri.h"
#include "los_mp.h"
#include "los_exc.h"
#include "asm/page.h"
#ifdef LOSCFG_FS_VFS
#include "fs/fd_table.h"
#endif
#include "time.h"
#include "user_copy.h"
#include "los_signal.h"
#ifdef LOSCFG_KERNEL_CPUP
#include "los_cpup_pri.h"
#endif
#ifdef LOSCFG_SECURITY_VID
#include "vid_api.h"
#endif
#ifdef LOSCFG_SECURITY_CAPABILITY
#include "capability_api.h"
#endif
#include "los_swtmr_pri.h"
#include "los_vm_map.h"
#include "los_vm_phys.h"
#include "los_vm_syscall.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */
//并发（Concurrent）:多个线程在单个核心运行，同一时间一个线程运行，系统不停切换线程，看起来像同时运行，实际上是线程不停切换
//并行（Parallel）每个线程分配给独立的CPU核心，线程同时运行
//单核CPU多个进程或多个线程内能实现并发（微观上的串行，宏观上的并行）；多核CPU线程间可以实现宏观和微观上的并行
//LITE_OS_SEC_BSS 和 LITE_OS_SEC_DATA_INIT 是告诉编译器这些全局变量放在哪个数据段
LITE_OS_SEC_BSS LosProcessCB *g_runProcess[LOSCFG_KERNEL_CORE_NUM];// CPU内核个数,超过一个就实现了并行
LITE_OS_SEC_BSS LosProcessCB *g_processCBArray = NULL; // 进程池数组
LITE_OS_SEC_DATA_INIT STATIC LOS_DL_LIST g_freeProcess;// 空闲状态下的进程链表, .个人觉得应该取名为 g_freeProcessList
LITE_OS_SEC_DATA_INIT STATIC LOS_DL_LIST g_processRecyleList;// 需要回收的进程列表
LITE_OS_SEC_BSS UINT32 g_userInitProcess = OS_INVALID_VALUE;// 用户态的初始init进程,用户态下其他进程由它 fork
LITE_OS_SEC_BSS UINT32 g_kernelInitProcess = OS_INVALID_VALUE;// 内核态初始Kprocess进程,内核态下其他进程由它 fork
LITE_OS_SEC_BSS UINT32 g_kernelIdleProcess = OS_INVALID_VALUE;// 内核态idle进程,由Kprocess fork
LITE_OS_SEC_BSS UINT32 g_processMaxNum;// 进程最大数量
LITE_OS_SEC_BSS ProcessGroup *g_processGroup = NULL;// 全局进程组,负责管理所有进程组
//将task从该进程的就绪队列中摘除,如果需要进程也从进程就绪队列中摘除
LITE_OS_SEC_TEXT_INIT VOID OsTaskSchedQueueDequeue(LosTaskCB *taskCB, UINT16 status)
{
    LosProcessCB *processCB = OS_PCB_FROM_PID(taskCB->processID);//从进程池中取进程
    if (taskCB->taskStatus & OS_TASK_STATUS_READY) {//判断task是否是就绪状态
        OS_TASK_PRI_QUEUE_DEQUEUE(processCB, taskCB);//从进程就绪队列中删除
        taskCB->taskStatus &= ~OS_TASK_STATUS_READY;//置task为非就绪状态
    }

    if (processCB->threadScheduleMap != 0) {//判断所属进程是否还有就绪状态的task
        return;
    }

    if (processCB->processStatus & OS_PROCESS_STATUS_READY) {//判断进程是否处于就绪状态
        processCB->processStatus &= ~OS_PROCESS_STATUS_READY;//置process为非就绪状态
        OS_PROCESS_PRI_QUEUE_DEQUEUE(processCB);//进程出进程的就绪队列
    }

#if (LOSCFG_KERNEL_SMP == YES)//
    if (OS_PROCESS_GET_RUNTASK_COUNT(processCB->processStatus) == 1) {
#endif
        processCB->processStatus |= status;
#if (LOSCFG_KERNEL_SMP == YES)
    }
#endif
}
//将task加入进程的就绪队列
STATIC INLINE VOID OsSchedTaskEnqueue(LosProcessCB *processCB, LosTaskCB *taskCB)
{
    if (((taskCB->policy == LOS_SCHED_RR) && (taskCB->timeSlice != 0)) ||//调度方式为抢断且时间片没用完
        ((taskCB->taskStatus & OS_TASK_STATUS_RUNNING) && (taskCB->policy == LOS_SCHED_FIFO))) {//或处于运行的FIFO调度方式的task
        OS_TASK_PRI_QUEUE_ENQUEUE_HEAD(processCB, taskCB);//加入就绪队列头部
    } else {
        OS_TASK_PRI_QUEUE_ENQUEUE(processCB, taskCB);//默认插入尾部
    }
	
    taskCB->taskStatus |= OS_TASK_STATUS_READY;// 已进入就绪队列,改变task状态为就绪状态
}
// 任务调度入 g_priQueueList 队 
LITE_OS_SEC_TEXT_INIT VOID OsTaskSchedQueueEnqueue(LosTaskCB *taskCB, UINT16 status)
{
    LosProcessCB *processCB = NULL;
	
    LOS_ASSERT(!(taskCB->taskStatus & OS_TASK_STATUS_READY));// 只有非就绪状态任务才能入队
	
    processCB = OS_PCB_FROM_PID(taskCB->processID);// 通过一个任务得到这个任务所在的进程
    if (!(processCB->processStatus & OS_PROCESS_STATUS_READY)) {//task状态为就绪状态
        if (((processCB->policy == LOS_SCHED_RR) && (processCB->timeSlice != 0)) ||//调度方式为抢断且时间片没用完
            ((processCB->processStatus & OS_PROCESS_STATUS_RUNNING) && (processCB->policy == LOS_SCHED_FIFO))) {//或处于运行的FIFO调度方式的task
            OS_PROCESS_PRI_QUEUE_ENQUEUE_HEAD(processCB);//进程入g_priQueueList就绪队列头部
        } else {
            OS_PROCESS_PRI_QUEUE_ENQUEUE(processCB);//入进程入g_priQueueList就绪队列
        }
        processCB->processStatus &= ~(status | OS_PROCESS_STATUS_PEND);//去掉外传标签和阻塞标签
        processCB->processStatus |= OS_PROCESS_STATUS_READY;
    } else {
        LOS_ASSERT(!(processCB->processStatus & OS_PROCESS_STATUS_PEND));
        LOS_ASSERT((UINTPTR)processCB->pendList.pstNext);
        if ((processCB->timeSlice == 0) && (processCB->policy == LOS_SCHED_RR)) {//没有时间片且采用抢占式调度算法的情况
            OS_PROCESS_PRI_QUEUE_DEQUEUE(processCB);//进程先出队列
            OS_PROCESS_PRI_QUEUE_ENQUEUE(processCB);//进程再入队列,区别是排到了最后.这可是队列前面还有很多人等着被调度呢.
        }
    }

    OsSchedTaskEnqueue(processCB, taskCB); // 加入进程的任务就绪队列,这个队列里排的都是task
}
//插入进程到空闲链表中
STATIC INLINE VOID OsInsertPCBToFreeList(LosProcessCB *processCB)
{
    UINT32 pid = processCB->processID;//获取进程ID
    (VOID)memset_s(processCB, sizeof(LosProcessCB), 0, sizeof(LosProcessCB));//进程描述符数据清0
    processCB->processID = pid;//进程ID
    processCB->processStatus = OS_PROCESS_FLAG_UNUSED;//设置为进程未使用
    processCB->timerID = (timer_t)(UINTPTR)MAX_INVALID_TIMER_VID;//timeID初始化值
    LOS_ListTailInsert(&g_freeProcess, &processCB->pendList);//进程节点挂入g_freeProcess以分配给后续进程使用
}
//创建进程组
STATIC ProcessGroup *OsCreateProcessGroup(UINT32 pid)
{
    LosProcessCB *processCB = NULL;
    ProcessGroup *group = LOS_MemAlloc(m_aucSysMem1, sizeof(ProcessGroup));//从内存池中分配进程组结构体
    if (group == NULL) {
        return NULL;
    }

    group->groupID = pid;//参数当进程组ID
    LOS_ListInit(&group->processList);//初始化进程链表,这里把组内的进程都挂上去
    LOS_ListInit(&group->exitProcessList);//初始化退出进程链表,这里挂退出的进程

    processCB = OS_PCB_FROM_PID(pid);//通过pid获得进程实体
    LOS_ListTailInsert(&group->processList, &processCB->subordinateGroupList);//通过subordinateGroupList挂在进程组上,自然后续要通过它来找到进程实体
    processCB->group = group;//设置进程所属进程组
    processCB->processStatus |= OS_PROCESS_FLAG_GROUP_LEADER;//进程状态贴上当老大的标签
    if (g_processGroup != NULL) {//全局进程组链表判空,g_processGroup指向"Kernel"进程所在组,详见: OsKernelInitProcess
        LOS_ListTailInsert(&g_processGroup->groupList, &group->groupList);//把进程组挂到全局进程组链表上
    }

    return group;
}
//退出进程组,参数是进程地址和进程组地址的地址
STATIC VOID OsExitProcessGroup(LosProcessCB *processCB, ProcessGroup **group)//ProcessGroup *g_processGroup = NULL
{
    LosProcessCB *groupProcessCB = OS_PCB_FROM_PID(processCB->group->groupID);//找到进程组老大进程的实体

    LOS_ListDelete(&processCB->subordinateGroupList);//从进程组进程链表上摘出去
    if (LOS_ListEmpty(&processCB->group->processList) && LOS_ListEmpty(&processCB->group->exitProcessList)) {//进程组进程链表和退出进程链表都为空时
        LOS_ListDelete(&processCB->group->groupList);//从全局进程组链表上把自己摘出去 记住它是 LOS_ListTailInsert(&g_processGroup->groupList, &group->groupList) 挂上去的
        groupProcessCB->processStatus &= ~OS_PROCESS_FLAG_GROUP_LEADER;//贴上不是组长的标签
        *group = processCB->group;//????? 这步操作没看明白,谁能告诉我为何要这么做?
        if (OsProcessIsUnused(groupProcessCB) && !(groupProcessCB->processStatus & OS_PROCESS_FLAG_EXIT)) {//组长进程时退出的标签
            LOS_ListDelete(&groupProcessCB->pendList);//进程从全局进程链表上摘除
            OsInsertPCBToFreeList(groupProcessCB);//释放进程的资源,回到freelist再利用
        }
    }

    processCB->group = NULL;
}
//通过组ID找个进程组
STATIC ProcessGroup *OsFindProcessGroup(UINT32 gid)
{
    ProcessGroup *group = NULL;
    if (g_processGroup->groupID == gid) {
        return g_processGroup;
    }

    LOS_DL_LIST_FOR_EACH_ENTRY(group, &g_processGroup->groupList, ProcessGroup, groupList) {
        if (group->groupID == gid) {
            return group;
        }
    }

    PRINT_INFO("%s is find group : %u failed!\n", __FUNCTION__, gid);
    return NULL;
}

STATIC LosProcessCB *OsFindGroupExitProcess(ProcessGroup *group, INT32 pid)
{
    LosProcessCB *childCB = NULL;

    LOS_DL_LIST_FOR_EACH_ENTRY(childCB, &(group->exitProcessList), LosProcessCB, subordinateGroupList) {
        if ((childCB->processID == pid) || (pid == OS_INVALID_VALUE)) {
            return childCB;
        }
    }

    PRINT_INFO("%s find exit process : %d failed in group : %u\n", __FUNCTION__, pid, group->groupID);
    return NULL;
}

STATIC UINT32 OsFindChildProcess(const LosProcessCB *processCB, INT32 childPid)
{
    LosProcessCB *childCB = NULL;

    if (childPid < 0) {
        goto ERR;
    }

    LOS_DL_LIST_FOR_EACH_ENTRY(childCB, &(processCB->childrenList), LosProcessCB, siblingList) {
        if (childCB->processID == childPid) {
            return LOS_OK;
        }
    }

ERR:
    PRINT_INFO("%s is find the child : %d failed in parent : %u\n", __FUNCTION__, childPid, processCB->processID);
    return LOS_NOK;
}

STATIC LosProcessCB *OsFindExitChildProcess(const LosProcessCB *processCB, INT32 childPid)
{
    LosProcessCB *exitChild = NULL;

    LOS_DL_LIST_FOR_EACH_ENTRY(exitChild, &(processCB->exitChildList), LosProcessCB, siblingList) {
        if ((childPid == OS_INVALID_VALUE) || (exitChild->processID == childPid)) {
            return exitChild;
        }
    }

    PRINT_INFO("%s is find the exit child : %d failed in parent : %u\n", __FUNCTION__, childPid, processCB->processID);
    return NULL;
}

STATIC INLINE VOID OsWaitWakeTask(LosTaskCB *taskCB, UINT32 wakePID)
{
    taskCB->waitID = wakePID;
    OsTaskWake(taskCB);
#if (LOSCFG_KERNEL_SMP == YES)
    LOS_MpSchedule(OS_MP_CPU_ALL);
#endif
}

STATIC BOOL OsWaitWakeSpecifiedProcess(LOS_DL_LIST *head, const LosProcessCB *processCB, LOS_DL_LIST **anyList)
{
    LOS_DL_LIST *list = head;
    LosTaskCB *taskCB = NULL;
    UINT32 pid = 0;
    BOOL find = FALSE;

    while (list->pstNext != head) {
        taskCB = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(list));
        if ((taskCB->waitFlag == OS_PROCESS_WAIT_PRO) && (taskCB->waitID == processCB->processID)) {
            if (pid == 0) {
                pid = processCB->processID;
                find = TRUE;
            } else {
                pid = OS_INVALID_VALUE;
            }

            OsWaitWakeTask(taskCB, pid);
            continue;
        }

        if (taskCB->waitFlag != OS_PROCESS_WAIT_PRO) {
            *anyList = list;
            break;
        }
        list = list->pstNext;
    }

    return find;
}

STATIC VOID OsWaitCheckAndWakeParentProcess(LosProcessCB *parentCB, const LosProcessCB *processCB)
{
    LOS_DL_LIST *head = &parentCB->waitList;
    LOS_DL_LIST *list = NULL;
    LosTaskCB *taskCB = NULL;
    BOOL findSpecified = FALSE;

    if (LOS_ListEmpty(&parentCB->waitList)) {
        return;
    }

    findSpecified = OsWaitWakeSpecifiedProcess(head, processCB, &list);
    if (findSpecified == TRUE) {
        /* No thread is waiting for any child process to finish */
        if (LOS_ListEmpty(&parentCB->waitList)) {
            return;
        } else if (!LOS_ListEmpty(&parentCB->childrenList)) {
            /* Other child processes exist, and other threads that are waiting
             * for the child to finish continue to wait
             */
            return;
        }
    }

    /* Waiting threads are waiting for a specified child process to finish */
    if (list == NULL) {
        return;
    }

    /* No child processes exist and all waiting threads are awakened */
    if (findSpecified == TRUE) {
        while (list->pstNext != head) {
            taskCB = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(list));
            OsWaitWakeTask(taskCB, OS_INVALID_VALUE);
        }
        return;
    }

    while (list->pstNext != head) {
        taskCB = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(list));
        if (taskCB->waitFlag == OS_PROCESS_WAIT_GID) {
            if (taskCB->waitID != processCB->group->groupID) {
                list = list->pstNext;
                continue;
            }
        }

        if (findSpecified == FALSE) {
            OsWaitWakeTask(taskCB, processCB->processID);
            findSpecified = TRUE;
        } else {
            OsWaitWakeTask(taskCB, OS_INVALID_VALUE);
        }

        if (!LOS_ListEmpty(&parentCB->childrenList)) {
            break;
        }
    }

    return;
}

LITE_OS_SEC_TEXT VOID OsProcessResourcesToFree(LosProcessCB *processCB)
{
    if (!(processCB->processStatus & (OS_PROCESS_STATUS_INIT | OS_PROCESS_STATUS_RUNNING))) {
        PRINT_ERR("The process(%d) has no permission to release process(%d) resources!\n",
                  OsCurrProcessGet()->processID, processCB->processID);
    }

#ifdef LOSCFG_FS_VFS
    if (OsProcessIsUserMode(processCB)) {//用户模式下
        delete_files(processCB, processCB->files);//删除文件
    }
    processCB->files = NULL;
#endif

#ifdef LOSCFG_SECURITY_CAPABILITY
    if (processCB->user != NULL) {
        (VOID)LOS_MemFree(m_aucSysMem1, processCB->user);
        processCB->user = NULL;
    }
#endif

    OsSwtmrRecycle(processCB->processID);//软件定时器回收
    processCB->timerID = (timer_t)(UINTPTR)MAX_INVALID_TIMER_VID;

#ifdef LOSCFG_SECURITY_VID
    if (processCB->timerIdMap.bitMap != NULL) {
        VidMapDestroy(processCB);
        processCB->timerIdMap.bitMap = NULL;
    }
#endif

#if (LOSCFG_KERNEL_LITEIPC == YES)
    if (OsProcessIsUserMode(processCB)) {//用户模式下
        LiteIpcPoolDelete(&(processCB->ipcInfo));//删除进程IPC
        (VOID)memset_s(&(processCB->ipcInfo), sizeof(ProcIpcInfo), 0, sizeof(ProcIpcInfo));
    }
#endif
}
//回收僵死状态进程流程
LITE_OS_SEC_TEXT STATIC VOID OsRecycleZombiesProcess(LosProcessCB *childCB, ProcessGroup **group)
{
    OsExitProcessGroup(childCB, group);//退出进程组
    LOS_ListDelete(&childCB->siblingList);//从父亲大人的子孙链表上摘除
    if (childCB->processStatus & OS_PROCESS_STATUS_ZOMBIES) {//如果身上僵死状态的标签
        childCB->processStatus &= ~OS_PROCESS_STATUS_ZOMBIES;//去掉僵死标签
        childCB->processStatus |= OS_PROCESS_FLAG_UNUSED;//贴上没使用标签，进程由进程池分配，进程退出后重新回到空闲进程池
    }

    LOS_ListDelete(&childCB->pendList);//将自己从阻塞链表上摘除，注意有很多原因引起阻塞，pendList挂在哪里就以为这属于哪类阻塞
    if (childCB->processStatus & OS_PROCESS_FLAG_EXIT) {//如果有退出标签
        LOS_ListHeadInsert(&g_processRecyleList, &childCB->pendList);//从头部插入，注意g_processRecyleList挂的是pendList节点,所以要通过OS_PCB_FROM_PENDLIST找.
    } else if (childCB->processStatus & OS_PROCESS_FLAG_GROUP_LEADER) {//如果是进程组的组长
        LOS_ListTailInsert(&g_processRecyleList, &childCB->pendList);//从尾部插入，意思就是组长尽量最后一个处理
    } else {
        OsInsertPCBToFreeList(childCB);//直接插到freeList中去，可用于重新分配了。
    }
}

STATIC VOID OsDealAliveChildProcess(LosProcessCB *processCB)
{
    UINT32 parentID;
    LosProcessCB *childCB = NULL;
    LosProcessCB *parentCB = NULL;
    LOS_DL_LIST *nextList = NULL;
    LOS_DL_LIST *childHead = NULL;

    if (!LOS_ListEmpty(&processCB->childrenList)) {
        childHead = processCB->childrenList.pstNext;
        LOS_ListDelete(&(processCB->childrenList));
        if (OsProcessIsUserMode(processCB)) {
            parentID = g_userInitProcess;
        } else {
            parentID = g_kernelInitProcess;
        }

        for (nextList = childHead; ;) {
            childCB = OS_PCB_FROM_SIBLIST(nextList);
            childCB->parentProcessID = parentID;
            nextList = nextList->pstNext;
            if (nextList == childHead) {
                break;
            }
        }

        parentCB = OS_PCB_FROM_PID(parentID);
        LOS_ListTailInsertList(&parentCB->childrenList, childHead);
    }

    return;
}
//孩子进程资源释放
STATIC VOID OsChildProcessResourcesFree(const LosProcessCB *processCB)
{
    LosProcessCB *childCB = NULL;
    ProcessGroup *group = NULL;

    while (!LOS_ListEmpty(&((LosProcessCB *)processCB)->exitChildList)) {//
        childCB = LOS_DL_LIST_ENTRY(processCB->exitChildList.pstNext, LosProcessCB, siblingList);
        OsRecycleZombiesProcess(childCB, &group);
        (VOID)LOS_MemFree(m_aucSysMem1, group);
    }
}
//进程的自然退出,参数是当前运行的任务
STATIC VOID OsProcessNaturalExit(LosTaskCB *runTask, UINT32 status)
{
    LosProcessCB *processCB = OS_PCB_FROM_PID(runTask->processID);//通过task找到所属PCB
    LosProcessCB *parentCB = NULL;

    LOS_ASSERT(!(processCB->threadScheduleMap != 0));//断言没有任务需要调度了,当前task是最后一个了
    LOS_ASSERT(processCB->processStatus & OS_PROCESS_STATUS_RUNNING);//断言必须为正在运行的进程

    OsChildProcessResourcesFree(processCB);//

#ifdef LOSCFG_KERNEL_CPUP
    OsCpupClean(processCB->processID);
#endif

    /* is a child process */
    if (processCB->parentProcessID != OS_INVALID_VALUE) {
        parentCB = OS_PCB_FROM_PID(processCB->parentProcessID);
        LOS_ListDelete(&processCB->siblingList);
        if (!OsProcessExitCodeSignalIsSet(processCB)) {
            OsProcessExitCodeSet(processCB, status);
        }
        LOS_ListTailInsert(&parentCB->exitChildList, &processCB->siblingList);
        LOS_ListDelete(&processCB->subordinateGroupList);
        LOS_ListTailInsert(&processCB->group->exitProcessList, &processCB->subordinateGroupList);

        OsWaitCheckAndWakeParentProcess(parentCB, processCB);

        OsDealAliveChildProcess(processCB);

        processCB->processStatus |= OS_PROCESS_STATUS_ZOMBIES;

        (VOID)OsKill(processCB->parentProcessID, SIGCHLD, OS_KERNEL_KILL_PERMISSION);
        LOS_ListHeadInsert(&g_processRecyleList, &processCB->pendList);
        OsRunTaskToDelete(runTask);
        return;
    }

    LOS_Panic("pid : %u is the root process exit!\n", processCB->processID);
    return;
}
//进程模块初始化,被编译放在代码段 .init 中
LITE_OS_SEC_TEXT_INIT UINT32 OsProcessInit(VOID)
{
    UINT32 index;
    UINT32 size;

    g_processMaxNum = LOSCFG_BASE_CORE_PROCESS_LIMIT;//默认支持64个进程
    size = g_processMaxNum * sizeof(LosProcessCB);//算出总大小

    g_processCBArray = (LosProcessCB *)LOS_MemAlloc(m_aucSysMem1, size);// 进程池，占用内核堆,内存池分配 
    if (g_processCBArray == NULL) {
        return LOS_NOK;
    }
    (VOID)memset_s(g_processCBArray, size, 0, size);//安全方式重置清0

    LOS_ListInit(&g_freeProcess);//进程空闲链表初始化，创建一个进程时从g_freeProcess中申请一个进程描述符使用
    LOS_ListInit(&g_processRecyleList);//进程回收链表初始化,回收完成后进入g_freeProcess等待再次被申请使用

    for (index = 0; index < g_processMaxNum; index++) {//进程池循环创建
        g_processCBArray[index].processID = index;//进程ID[0-g_processMaxNum]赋值
        g_processCBArray[index].processStatus = OS_PROCESS_FLAG_UNUSED;// 默认都是白纸一张，臣妾干净着呢
        LOS_ListTailInsert(&g_freeProcess, &g_processCBArray[index].pendList);//注意g_freeProcess挂的是pendList节点,所以使用要通过OS_PCB_FROM_PENDLIST找到进程实体.
    }
	// ????? 为啥用户模式的根进程 选1 ,内核模式的根进程选2 
    g_userInitProcess = 1; /* 1: The root process ID of the user-mode process is fixed at 1 *///用户模式的根进程
    LOS_ListDelete(&g_processCBArray[g_userInitProcess].pendList);// 清空g_userInitProcess pend链表

    g_kernelInitProcess = 2; /* 2: The root process ID of the kernel-mode process is fixed at 2 *///内核模式的根进程
    LOS_ListDelete(&g_processCBArray[g_kernelInitProcess].pendList);// 清空g_kernelInitProcess pend链表

    return LOS_OK;
}
//创建一个名叫"KIdle"的进程,给CPU空闲的时候使用
STATIC UINT32 OsCreateIdleProcess(VOID)
{
    UINT32 ret;
    CHAR *idleName = "Idle";
    LosProcessCB *idleProcess = NULL;
    Percpu *perCpu = OsPercpuGet();
    UINT32 *idleTaskID = &perCpu->idleTaskID;//得到CPU的idle task

    ret = OsCreateResourceFreeTask();// 创建一个资源回收任务,优先级为5 用于回收进程退出时的各种资源
    if (ret != LOS_OK) {
        return ret;
    }
	//创建一个名叫"KIdle"的进程,并创建一个idle task,CPU空闲的时候就待在 idle task中等待被唤醒
    ret = LOS_Fork(CLONE_FILES, "KIdle", (TSK_ENTRY_FUNC)OsIdleTask, LOSCFG_BASE_CORE_TSK_IDLE_STACK_SIZE);
    if (ret < 0) {
        return LOS_NOK;
    }
    g_kernelIdleProcess = (UINT32)ret;//返回进程ID

    idleProcess = OS_PCB_FROM_PID(g_kernelIdleProcess);//通过ID拿到进程实体
    *idleTaskID = idleProcess->threadGroupID;//绑定CPU的IdleTask,或者说改变CPU现有的idle任务
    OS_TCB_FROM_TID(*idleTaskID)->taskStatus |= OS_TASK_FLAG_SYSTEM_TASK;//设定Idle task 为一个系统任务
#if (LOSCFG_KERNEL_SMP == YES)
    OS_TCB_FROM_TID(*idleTaskID)->cpuAffiMask = CPUID_TO_AFFI_MASK(ArchCurrCpuid());//多核CPU的任务指定,防止乱串了,注意多核才会有并行处理
#endif
    (VOID)memset_s(OS_TCB_FROM_TID(*idleTaskID)->taskName, OS_TCB_NAME_LEN, 0, OS_TCB_NAME_LEN);//task 名字先清0
    (VOID)memcpy_s(OS_TCB_FROM_TID(*idleTaskID)->taskName, OS_TCB_NAME_LEN, idleName, strlen(idleName));//task 名字叫 idle
    return LOS_OK;
}
//进程回收再利用过程
LITE_OS_SEC_TEXT VOID OsProcessCBRecyleToFree(VOID)
{
    UINT32 intSave;
    LosVmSpace *space = NULL;
    LosProcessCB *processCB = NULL;

    SCHEDULER_LOCK(intSave);
    while (!LOS_ListEmpty(&g_processRecyleList)) {//循环回收链表,直到为空
        processCB = OS_PCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&g_processRecyleList));//找到回收链表中第一个进程实体
        //OS_PCB_FROM_PENDLIST 代表的是通过pendlist节点找到 PCB实体,因为g_processRecyleList上面挂的是pendlist节点位置
        if (!(processCB->processStatus & OS_PROCESS_FLAG_EXIT)) {
            break;
        }
        SCHEDULER_UNLOCK(intSave);

        OsTaskCBRecyleToFree();

        SCHEDULER_LOCK(intSave);
        processCB->processStatus &= ~OS_PROCESS_FLAG_EXIT;
        if (OsProcessIsUserMode(processCB)) {
            space = processCB->vmSpace;
        }
        processCB->vmSpace = NULL;
        /* OS_PROCESS_FLAG_GROUP_LEADER: The lead process group cannot be recycled without destroying the PCB.
         * !OS_PROCESS_FLAG_UNUSED: Parent process does not reclaim child process resources.
         */
        LOS_ListDelete(&processCB->pendList);
        if ((processCB->processStatus & OS_PROCESS_FLAG_GROUP_LEADER) ||
            (processCB->processStatus & OS_PROCESS_STATUS_ZOMBIES)) {
            LOS_ListTailInsert(&g_processRecyleList, &processCB->pendList);
        } else {
            /* Clear the bottom 4 bits of process status */
            OsInsertPCBToFreeList(processCB);
        }
        SCHEDULER_UNLOCK(intSave);

        (VOID)LOS_VmSpaceFree(space);

        SCHEDULER_LOCK(intSave);
    }

    SCHEDULER_UNLOCK(intSave);
}
//获取一个可用的PCB(进程控制块)
STATIC LosProcessCB *OsGetFreePCB(VOID)
{
    LosProcessCB *processCB = NULL;
    UINT32 intSave;

    SCHEDULER_LOCK(intSave);
    if (LOS_ListEmpty(&g_freeProcess)) {//空闲池里还有未分配的task?
        SCHEDULER_UNLOCK(intSave);
        PRINT_ERR("No idle PCB in the system!\n");
        return NULL;
    }

    processCB = OS_PCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&g_freeProcess));//拿到PCB,通过OS_PCB_FROM_PENDLIST是因为通过pendlist 节点挂在 freelist链表上.
    LOS_ListDelete(&processCB->pendList);//分配出来了就要在freelist将自己摘除
    SCHEDULER_UNLOCK(intSave);

    return processCB;
}
//删除PCB块 其实是 PCB块回归进程池,先进入回收链表
STATIC VOID OsDeInitPCB(LosProcessCB *processCB)
{
    UINT32 intSave;
    ProcessGroup *group = NULL;

    if (processCB == NULL) {
        return;
    }

    OsProcessResourcesToFree(processCB);//释放进程所占用的资源

    SCHEDULER_LOCK(intSave);
    if (processCB->parentProcessID != OS_INVALID_VALUE) {
        LOS_ListDelete(&processCB->siblingList);//将进程从兄弟链表中摘除
        processCB->parentProcessID = OS_INVALID_VALUE;
    }

    if (processCB->group != NULL) {
        OsExitProcessGroup(processCB, &group);//退出进程组
    }

    processCB->processStatus &= ~OS_PROCESS_STATUS_INIT;//设置进程状态为非初始化
    processCB->processStatus |= OS_PROCESS_FLAG_EXIT;	//设置进程状态为退出
    LOS_ListHeadInsert(&g_processRecyleList, &processCB->pendList);//
    SCHEDULER_UNLOCK(intSave);

    (VOID)LOS_MemFree(m_aucSysMem1, group);//释放内存
    OsWriteResourceEvent(OS_RESOURCE_EVENT_FREE);
    return;
}
//设置进程的名字
STATIC UINT32 OsSetProcessName(LosProcessCB *processCB, const CHAR *name)
{
    errno_t errRet;
    UINT32 len;

    if (name != NULL) {
        len = strlen(name);
        if (len >= OS_PCB_NAME_LEN) {
            len = OS_PCB_NAME_LEN - 1; /* 1: Truncate, reserving the termination operator for character turns */
        }
        errRet = memcpy_s(processCB->processName, sizeof(CHAR) * OS_PCB_NAME_LEN, name, len);
        if (errRet != EOK) {
            processCB->processName[0] = '\0';
            return LOS_NOK;
        }
        processCB->processName[len] = '\0';
        return LOS_OK;
    }

    (VOID)memset_s(processCB->processName, sizeof(CHAR) * OS_PCB_NAME_LEN, 0, sizeof(CHAR) * OS_PCB_NAME_LEN);
    switch (processCB->processMode) {
        case OS_KERNEL_MODE:
            (VOID)snprintf_s(processCB->processName, sizeof(CHAR) * OS_PCB_NAME_LEN,
                             (sizeof(CHAR) * OS_PCB_NAME_LEN) - 1, "KerProcess%u", processCB->processID);
            break;
        default:
            (VOID)snprintf_s(processCB->processName, sizeof(CHAR) * OS_PCB_NAME_LEN,
                             (sizeof(CHAR) * OS_PCB_NAME_LEN) - 1, "UserProcess%u", processCB->processID);
            break;
    }
    return LOS_OK;
}
//初始化PCB块
STATIC UINT32 OsInitPCB(LosProcessCB *processCB, UINT32 mode, UINT16 priority, UINT16 policy, const CHAR *name)
{
    UINT32 count;
    LosVmSpace *space = NULL;
    LosVmPage *vmPage = NULL;
    status_t status;
    BOOL retVal = FALSE;

    processCB->processMode = mode;//用户态进程还是内核态进程
    processCB->processStatus = OS_PROCESS_STATUS_INIT;//进程初始状态
    processCB->parentProcessID = OS_INVALID_VALUE;//爸爸进程，外面指定
    processCB->threadGroupID = OS_INVALID_VALUE;//所属线程组
    processCB->priority = priority;//进程优先级
    processCB->policy = policy;//调度算法 LOS_SCHED_RR
    processCB->umask = OS_PROCESS_DEFAULT_UMASK;//掩码
    processCB->timerID = (timer_t)(UINTPTR)MAX_INVALID_TIMER_VID;

    LOS_ListInit(&processCB->threadSiblingList);//初始化孩子任务/线程链表，上面挂的都是由此fork的孩子线程 见于 OsTaskCBInit LOS_ListTailInsert(&(processCB->threadSiblingList), &(taskCB->threadList));
    LOS_ListInit(&processCB->childrenList);		//初始化孩子进程链表，上面挂的都是由此fork的孩子进程 见于 OsCopyParent LOS_ListTailInsert(&parentProcessCB->childrenList, &childProcessCB->siblingList);
    LOS_ListInit(&processCB->exitChildList);	//初始化记录退出孩子进程链表，上面挂的是哪些exit	见于 OsProcessNaturalExit LOS_ListTailInsert(&parentCB->exitChildList, &processCB->siblingList);
    LOS_ListInit(&(processCB->waitList));		//初始化等待任务链表 上面挂的是处于等待的 见于 OsWaitInsertWaitLIstInOrder LOS_ListHeadInsert(&processCB->waitList, &runTask->pendList);

    for (count = 0; count < OS_PRIORITY_QUEUE_NUM; ++count) { //根据 priority数 创建对应个数的队列
        LOS_ListInit(&processCB->threadPriQueueList[count]); //初始化一个个线程队列，队列中存放就绪状态的线程/task 
    }//在鸿蒙内核中 task就是thread,在鸿蒙源码分析系列篇中有详细阐释 见于 https://my.oschina.net/u/3751245

    if (OsProcessIsUserMode(processCB)) {// 是否为用户模式进程
        space = LOS_MemAlloc(m_aucSysMem0, sizeof(LosVmSpace));//分配一个虚拟空间
        if (space == NULL) {
            PRINT_ERR("%s %d, alloc space failed\n", __FUNCTION__, __LINE__);
            return LOS_ENOMEM;
        }
        VADDR_T *ttb = LOS_PhysPagesAllocContiguous(1);//分配一个物理页用于存储L1页表 4G虚拟内存分成 （4096*1M）
        if (ttb == NULL) {//这里直接获取物理页ttb
            PRINT_ERR("%s %d, alloc ttb or space failed\n", __FUNCTION__, __LINE__);
            (VOID)LOS_MemFree(m_aucSysMem0, space);
            return LOS_ENOMEM;
        }
        (VOID)memset_s(ttb, PAGE_SIZE, 0, PAGE_SIZE);//内存清0
        retVal = OsUserVmSpaceInit(space, ttb);//初始化虚拟空间和本进程 mmu
        vmPage = OsVmVaddrToPage(ttb);//通过虚拟地址拿到page
        if ((retVal == FALSE) || (vmPage == NULL)) {//异常处理
            PRINT_ERR("create space failed! ret: %d, vmPage: %#x\n", retVal, vmPage);
            processCB->processStatus = OS_PROCESS_FLAG_UNUSED;//进程未使用,干净
            (VOID)LOS_MemFree(m_aucSysMem0, space);//释放虚拟空间
            LOS_PhysPagesFreeContiguous(ttb, 1);//释放物理页,4K
            return LOS_EAGAIN;
        }
        processCB->vmSpace = space;//设为进程虚拟空间
        LOS_ListAdd(&processCB->vmSpace->archMmu.ptList, &(vmPage->node));//将空间映射页表挂在 空间的mmu L1页表, L1为表头
    } else {
        processCB->vmSpace = LOS_GetKVmSpace();//内核共用一个虚拟空间,内核进程 常驻内存
    }

#ifdef LOSCFG_SECURITY_VID
    status = VidMapListInit(processCB);
    if (status != LOS_OK) {
        PRINT_ERR("VidMapListInit failed!\n");
        return LOS_ENOMEM;
    }
#endif
#ifdef LOSCFG_SECURITY_CAPABILITY
    OsInitCapability(processCB);
#endif

    if (OsSetProcessName(processCB, name) != LOS_OK) {
        return LOS_ENOMEM;
    }

    return LOS_OK;
}
//创建用户
#ifdef LOSCFG_SECURITY_CAPABILITY
STATIC User *OsCreateUser(UINT32 userID, UINT32 gid, UINT32 size)
{
    User *user = LOS_MemAlloc(m_aucSysMem1, sizeof(User) + (size - 1) * sizeof(UINT32));
    if (user == NULL) {
        return NULL;
    }

    user->userID = userID;
    user->effUserID = userID;
    user->gid = gid;
    user->effGid = gid;
    user->groupNumber = size;
    user->groups[0] = gid;
    return user;
}

LITE_OS_SEC_TEXT BOOL LOS_CheckInGroups(UINT32 gid)
{
    UINT32 intSave;
    UINT32 count;
    User *user = NULL;

    SCHEDULER_LOCK(intSave);
    user = OsCurrUserGet();
    for (count = 0; count < user->groupNumber; count++) {
        if (user->groups[count] == gid) {
            SCHEDULER_UNLOCK(intSave);
            return TRUE;
        }
    }

    SCHEDULER_UNLOCK(intSave);
    return FALSE;
}
#endif

LITE_OS_SEC_TEXT INT32 LOS_GetUserID(VOID)
{
#ifdef LOSCFG_SECURITY_CAPABILITY
    UINT32 intSave;
    INT32 uid;

    SCHEDULER_LOCK(intSave);
    uid = (INT32)OsCurrUserGet()->userID;
    SCHEDULER_UNLOCK(intSave);
    return uid;
#else
    return 0;
#endif
}

LITE_OS_SEC_TEXT INT32 LOS_GetGroupID(VOID)
{
#ifdef LOSCFG_SECURITY_CAPABILITY
    UINT32 intSave;
    INT32 gid;

    SCHEDULER_LOCK(intSave);
    gid = (INT32)OsCurrUserGet()->gid;
    SCHEDULER_UNLOCK(intSave);

    return gid;
#else
    return 0;
#endif
}
//进程创建初始化
STATIC UINT32 OsProcessCreateInit(LosProcessCB *processCB, UINT32 flags, const CHAR *name, UINT16 priority)
{
    ProcessGroup *group = NULL;
    UINT32 ret = OsInitPCB(processCB, flags, priority, LOS_SCHED_RR, name);//初始化进程控制块
    if (ret != LOS_OK) {
        goto EXIT;
    }

#if (LOSCFG_KERNEL_LITEIPC == YES)
    if (OsProcessIsUserMode(processCB)) {//是否在用户模式
        ret = LiteIpcPoolInit(&(processCB->ipcInfo));//IPC池初始化
        if (ret != LOS_OK) {//异常处理
            ret = LOS_ENOMEM;
            PRINT_ERR("LiteIpcPoolInit failed!\n");
            goto EXIT;
        }
    }
#endif

#ifdef LOSCFG_FS_VFS
    processCB->files = alloc_files();//分配进程的文件的管理器
    if (processCB->files == NULL) {
        ret = LOS_ENOMEM;
        goto EXIT;
    }
#endif

    group = OsCreateProcessGroup(processCB->processID);//创建进程组
    if (group == NULL) {
        ret = LOS_ENOMEM;
        goto EXIT;
    }

#ifdef LOSCFG_SECURITY_CAPABILITY	//用户安全宏
    processCB->user = OsCreateUser(0, 0, 1);//创建用户
    if (processCB->user == NULL) {
        ret = LOS_ENOMEM;
        goto EXIT;
    }
#endif

#ifdef LOSCFG_KERNEL_CPUP
    OsCpupSet(processCB->processID);
#endif

    return LOS_OK;

EXIT:
    OsDeInitPCB(processCB);//删除进程控制块
    return ret;
}

LITE_OS_SEC_TEXT_INIT UINT32 OsKernelInitProcess(VOID)
{
    LosProcessCB *processCB = NULL;
    UINT32 ret;

    ret = OsProcessInit();// 初始化进程模块全部变量,创建各循环双向链表
    if (ret != LOS_OK) {
        return ret;
    }

    processCB = OS_PCB_FROM_PID(g_kernelInitProcess);// 以PID方式得到一个进程
    ret = OsProcessCreateInit(processCB, OS_KERNEL_MODE, "KProcess", 0);// 初始化进程,最高优先级0,鸿蒙进程一共有32个优先级(0-31) 其中0-9级为内核进程,用户进程可配置的优先级有22个(10-31)
    if (ret != LOS_OK) {
        return ret;
    }

    processCB->processStatus &= ~OS_PROCESS_STATUS_INIT;// 进程初始化位 置1
    g_processGroup = processCB->group;//全局进程组指向了KProcess所在的进程组
    LOS_ListInit(&g_processGroup->groupList);// 进程组链表初始化
    OsCurrProcessSet(processCB);// 设置为当前进程

    return OsCreateIdleProcess();// 创建一个空闲状态的进程
}
//进程主动让出CPU 
LITE_OS_SEC_TEXT UINT32 LOS_ProcessYield(VOID)
{
    UINT32 count;
    UINT32 intSave;
    LosProcessCB *runProcessCB = NULL;

    if (OS_INT_ACTIVE) {//系统是否处于激活状态
        return LOS_ERRNO_TSK_YIELD_IN_INT;
    }

    if (!OsPreemptable()) {//调度算法是否抢占式
        return LOS_ERRNO_TSK_YIELD_IN_LOCK;
    }

    SCHEDULER_LOCK(intSave);
    runProcessCB = OsCurrProcessGet();//获取当前进程

    /* reset timeslice of yeilded task */
    runProcessCB->timeSlice = 0;//剩余时间片清0

    count = OS_PROCESS_PRI_QUEUE_SIZE(runProcessCB);//进程就绪队列大小
    if (count > 0) {
        if (runProcessCB->processStatus & OS_PROCESS_STATUS_READY) {//进程状态是否为就绪状态,这里加判断很可能进程此时没有在就绪队列
            OS_PROCESS_PRI_QUEUE_DEQUEUE(runProcessCB);//从就绪队列删除
        }
        OS_PROCESS_PRI_QUEUE_ENQUEUE(runProcessCB);//加入就绪队列-尾部插入-排最后
        runProcessCB->processStatus |= OS_PROCESS_STATUS_READY;//重新变成就绪状态
        OsSchedTaskEnqueue(runProcessCB, OsCurrTaskGet());//将当前task加入就绪队列
    } else {
        SCHEDULER_UNLOCK(intSave);
        return LOS_OK;
    }

    OsSchedResched();//申请调度
    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;
}
//进程调度参数检查
STATIC INLINE INT32 OsProcessSchedlerParamCheck(INT32 which, INT32 pid, UINT16 prio, UINT16 policy)
{
    if (OS_PID_CHECK_INVALID(pid)) {//进程ID是否有效，默认 g_processMaxNum = 64
        return LOS_EINVAL;
    }

    if (which != LOS_PRIO_PROCESS) {//进程标识
        return LOS_EOPNOTSUPP;//返回操作不支持
    }

    if (prio > OS_PROCESS_PRIORITY_LOWEST) {//鸿蒙优先级是 0 -31级,0级最大
        return LOS_EINVAL;//返回无效参数
    }

    if ((policy != LOS_SCHED_FIFO) && (policy != LOS_SCHED_RR)) {//调度方式既不是先进先得,也不是抢占式
        return LOS_EOPNOTSUPP;//返回操作不支持
    }

    return LOS_OK;
}

#ifdef LOSCFG_SECURITY_CAPABILITY //检查进程的安全许可证
STATIC BOOL OsProcessCapPermitCheck(const LosProcessCB *processCB, UINT16 prio)
{
    LosProcessCB *runProcess = OsCurrProcessGet();//获得当前进程

    /* always trust kernel process */
    if (!OsProcessIsUserMode(runProcess)) {//进程必须在内核模式下,也就是说在内核模式下是安全的.
        return TRUE;
    }

    /* user mode process can reduce the priority of itself */
    if ((runProcess->processID == processCB->processID) && (prio > processCB->priority)) {//用户模式下进程阔以降低自己的优先级
        return TRUE;
    }

    /* user mode process with privilege of CAP_SCHED_SETPRIORITY can change the priority */
    if (IsCapPermit(CAP_SCHED_SETPRIORITY)) {
        return TRUE;
    }
    return FALSE;
}
#endif
//设置进程调度计划
LITE_OS_SEC_TEXT INT32 OsSetProcessScheduler(INT32 which, INT32 pid, UINT16 prio, UINT16 policy, BOOL policyFlag)
{
    LosProcessCB *processCB = NULL;
    UINT32 intSave;
    INT32 ret;

    ret = OsProcessSchedlerParamCheck(which, pid, prio, policy);//先参数检查
    if (ret != LOS_OK) {
        return -ret;
    }

    SCHEDULER_LOCK(intSave);//持有调度自旋锁,多CPU情况下调度期间需要原子处理
    processCB = OS_PCB_FROM_PID(pid);
    if (OsProcessIsInactive(processCB)) {//进程未活动的处理
        ret = LOS_ESRCH;
        goto EXIT;
    }

#ifdef LOSCFG_SECURITY_CAPABILITY
    if (!OsProcessCapPermitCheck(processCB, prio)) {//检查是否安全
        ret = LOS_EPERM;
        goto EXIT;
    }
#endif

    if (policyFlag == TRUE) {//参数 policyFlag 表示调度方式要变吗?
        if (policy == LOS_SCHED_FIFO) {//先进先出调度方式下
            processCB->timeSlice = 0;//不需要时间片
        }
        processCB->policy = policy;//改变调度方式
    }

    if (processCB->processStatus & OS_PROCESS_STATUS_READY) {//进程处于就绪状态的处理
        OS_PROCESS_PRI_QUEUE_DEQUEUE(processCB);//先出进程队列
        processCB->priority = prio;//改变进程的优先级
        OS_PROCESS_PRI_QUEUE_ENQUEUE(processCB);//再进入队列,排到新优先级队列的尾部
    } else {
        processCB->priority = prio;//改变优先级,没其他操作了,为什么?因为队列就是给就绪状态准备的,其他状态是没有队列的!
        if (!(processCB->processStatus & OS_PROCESS_STATUS_RUNNING)) {//不是就绪也不是运行状态
            ret = LOS_OK;
            goto EXIT;//直接goto退出
        }
    }

    SCHEDULER_UNLOCK(intSave);//还锁

    LOS_MpSchedule(OS_MP_CPU_ALL);//
    if (OS_SCHEDULER_ACTIVE) {//当前CPU是否激活了,激活才能调度
        LOS_Schedule();//真正的任务调度算法从LOS_Schedule始
    }
    return LOS_OK;

EXIT:
    SCHEDULER_UNLOCK(intSave);//还锁
    return -ret;
}
//接口封装 - 设置进程调度参数
LITE_OS_SEC_TEXT INT32 LOS_SetProcessScheduler(INT32 pid, UINT16 policy, UINT16 prio)
{
    return OsSetProcessScheduler(LOS_PRIO_PROCESS, pid, prio, policy, TRUE);
}
//接口封装 - 获得进程调度参数
LITE_OS_SEC_TEXT INT32 LOS_GetProcessScheduler(INT32 pid)
{
    LosProcessCB *processCB = NULL;
    UINT32 intSave;
    INT32 policy;

    if (OS_PID_CHECK_INVALID(pid)) {//检查PID不能越界
        return -LOS_EINVAL;
    }

    SCHEDULER_LOCK(intSave);
    processCB = OS_PCB_FROM_PID(pid);//通过pid获取进程实体
    if (OsProcessIsUnused(processCB)) {//进程是否在使用判断
        policy = -LOS_ESRCH;
        goto OUT;
    }

    policy = processCB->policy;//获取进程调度方式

OUT:
    SCHEDULER_UNLOCK(intSave);
    return policy;
}
//接口封装 - 设置进程优先级
LITE_OS_SEC_TEXT INT32 LOS_SetProcessPriority(INT32 pid, UINT16 prio)
{
    return OsSetProcessScheduler(LOS_PRIO_PROCESS, pid, prio, LOS_SCHED_RR, FALSE);//抢占式调度
}
//接口封装 - 获取进程优先级 which:标识进程,进程组,用户
LITE_OS_SEC_TEXT INT32 OsGetProcessPriority(INT32 which, INT32 pid)
{
    LosProcessCB *processCB = NULL;
    INT32 prio;
    UINT32 intSave;
    (VOID)which;

    if (OS_PID_CHECK_INVALID(pid)) {
        return -LOS_EINVAL;
    }

    if (which != LOS_PRIO_PROCESS) {
        return -LOS_EOPNOTSUPP;
    }

    SCHEDULER_LOCK(intSave);
    processCB = OS_PCB_FROM_PID(pid);
    if (OsProcessIsUnused(processCB)) {
        prio = -LOS_ESRCH;
        goto OUT;
    }

    prio = (INT32)processCB->priority;

OUT:
    SCHEDULER_UNLOCK(intSave);
    return prio;
}
//接口封装 - 获取进程优先级
LITE_OS_SEC_TEXT INT32 LOS_GetProcessPriority(INT32 pid)
{
    return OsGetProcessPriority(LOS_PRIO_PROCESS, pid);
}
//等待唤醒进程的信号
LITE_OS_SEC_TEXT VOID OsWaitSignalToWakeProcess(LosProcessCB *processCB)
{
    LosTaskCB *taskCB = NULL;

    if (processCB == NULL) {
        return;
    }

    /* only suspend process can continue */
    if (!(processCB->processStatus & OS_PROCESS_STATUS_PEND)) {//进程必须处于阻塞状态
        return;
    }

    if (!LOS_ListEmpty(&processCB->waitList)) {//waitList链表不为空
        taskCB = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&processCB->waitList));//从链表上摘下第一个task,通过节点获取task主体
        OsWaitWakeTask(taskCB, OS_INVALID_VALUE);//唤醒这个task
    }

    return;
}

STATIC VOID OsWaitInsertWaitListInOrder(LosTaskCB *runTask, LosProcessCB *processCB)
{
    LOS_DL_LIST *head = &processCB->waitList;
    LOS_DL_LIST *list = head;
    LosTaskCB *taskCB = NULL;

    (VOID)OsTaskWait(&processCB->waitList, LOS_WAIT_FOREVER, FALSE);
    LOS_ListDelete(&runTask->pendList);
    if (runTask->waitFlag == OS_PROCESS_WAIT_PRO) {
        LOS_ListHeadInsert(&processCB->waitList, &runTask->pendList);
        return;
    } else if (runTask->waitFlag == OS_PROCESS_WAIT_GID) {
        while (list->pstNext != head) {
            taskCB = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(list));
            if (taskCB->waitFlag == OS_PROCESS_WAIT_PRO) {
                list = list->pstNext;
                continue;
            }
            break;
        }
        LOS_ListHeadInsert(list, &runTask->pendList);
        return;
    }

    while (list->pstNext != head) {
        taskCB = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(list));
        if (taskCB->waitFlag != OS_PROCESS_WAIT_ANY) {
            list = list->pstNext;
            continue;
        }
        break;
    }

    LOS_ListHeadInsert(list, &runTask->pendList);
    return;
}

STATIC UINT32 OsWaitSetFlag(const LosProcessCB *processCB, INT32 pid, LosProcessCB **child)
{
    LosProcessCB *childCB = NULL;
    ProcessGroup *group = NULL;
    LosTaskCB *runTask = OsCurrTaskGet();
    UINT32 ret;

    if (pid > 0) {
        /* Wait for the child process whose process number is pid. */
        childCB = OsFindExitChildProcess(processCB, pid);
        if (childCB != NULL) {
            goto WAIT_BACK;
        }

        ret = OsFindChildProcess(processCB, pid);
        if (ret != LOS_OK) {
            return LOS_ECHILD;
        }
        runTask->waitFlag = OS_PROCESS_WAIT_PRO;
        runTask->waitID = pid;
    } else if (pid == 0) {
        /* Wait for any child process in the same process group */
        childCB = OsFindGroupExitProcess(processCB->group, OS_INVALID_VALUE);
        if (childCB != NULL) {
            goto WAIT_BACK;
        }
        runTask->waitID = processCB->group->groupID;
        runTask->waitFlag = OS_PROCESS_WAIT_GID;
    } else if (pid == -1) {
        /* Wait for any child process */
        childCB = OsFindExitChildProcess(processCB, OS_INVALID_VALUE);
        if (childCB != NULL) {
            goto WAIT_BACK;
        }
        runTask->waitID = pid;
        runTask->waitFlag = OS_PROCESS_WAIT_ANY;
    } else { /* pid < -1 */
        /* Wait for any child process whose group number is the pid absolute value. */
        group = OsFindProcessGroup(-pid);
        if (group == NULL) {
            return LOS_ECHILD;
        }

        childCB = OsFindGroupExitProcess(group, OS_INVALID_VALUE);
        if (childCB != NULL) {
            goto WAIT_BACK;
        }

        runTask->waitID = -pid;
        runTask->waitFlag = OS_PROCESS_WAIT_GID;
    }

WAIT_BACK:
    *child = childCB;
    return LOS_OK;
}

STATIC INT32 OsWaitRecycleChildPorcess(const LosProcessCB *childCB, UINT32 intSave, INT32 *status)
{
    ProcessGroup *group = NULL;
    UINT32 pid = childCB->processID;
    UINT16 mode = childCB->processMode;
    INT32 exitCode = childCB->exitCode;

    OsRecycleZombiesProcess((LosProcessCB *)childCB, &group);
    SCHEDULER_UNLOCK(intSave);

    if (status != NULL) {
        if (mode == OS_USER_MODE) {
            (VOID)LOS_ArchCopyToUser((VOID *)status, (const VOID *)(&(exitCode)), sizeof(INT32));
        } else {
            *status = exitCode;
        }
    }

    (VOID)LOS_MemFree(m_aucSysMem1, group);
    return pid;
}

STATIC INT32 OsWaitChildProcessCheck(LosProcessCB *processCB, INT32 pid, LosProcessCB **childCB)
{
    if (LOS_ListEmpty(&(processCB->childrenList)) && LOS_ListEmpty(&(processCB->exitChildList))) {
        return LOS_ECHILD;
    }

    return OsWaitSetFlag(processCB, pid, childCB);
}

STATIC UINT32 OsWaitOptionsCheck(UINT32 options)
{
    UINT32 flag = LOS_WAIT_WNOHANG | LOS_WAIT_WUNTRACED | LOS_WAIT_WCONTINUED;

    flag = ~flag & options;
    if (flag != 0) {
        return LOS_EINVAL;
    }

    if ((options & (LOS_WAIT_WCONTINUED | LOS_WAIT_WUNTRACED)) != 0) {
        return LOS_EOPNOTSUPP;
    }

    if (OS_INT_ACTIVE) {
        return LOS_EINTR;
    }

    return LOS_OK;
}

LITE_OS_SEC_TEXT INT32 LOS_Wait(INT32 pid, USER INT32 *status, UINT32 options, VOID *rusage)
{
    (VOID)rusage;
    UINT32 ret;
    UINT32 intSave;
    LosProcessCB *childCB = NULL;
    LosProcessCB *processCB = NULL;
    LosTaskCB *runTask = NULL;

    ret = OsWaitOptionsCheck(options);
    if (ret != LOS_OK) {
        return -ret;
    }

    SCHEDULER_LOCK(intSave);
    processCB = OsCurrProcessGet();
    runTask = OsCurrTaskGet();

    ret = OsWaitChildProcessCheck(processCB, pid, &childCB);
    if (ret != LOS_OK) {
        pid = -ret;
        goto ERROR;
    }

    if (childCB != NULL) {
        return OsWaitRecycleChildPorcess(childCB, intSave, status);
    }

    if ((options & LOS_WAIT_WNOHANG) != 0) {
        runTask->waitFlag = 0;
        pid = 0;
        goto ERROR;
    }

    OsWaitInsertWaitListInOrder(runTask, processCB);

    OsSchedResched();

    runTask->waitFlag = 0;
    if (runTask->waitID == OS_INVALID_VALUE) {
        pid = -LOS_ECHILD;
        goto ERROR;
    }

    childCB = OS_PCB_FROM_PID(runTask->waitID);
    if (!(childCB->processStatus & OS_PROCESS_STATUS_ZOMBIES)) {
        pid = -LOS_ESRCH;
        goto ERROR;
    }

    return OsWaitRecycleChildPorcess(childCB, intSave, status);

ERROR:
    SCHEDULER_UNLOCK(intSave);
    return pid;
}
//设置进程组检查
STATIC UINT32 OsSetProcessGroupCheck(const LosProcessCB *processCB, UINT32 gid)
{
    LosProcessCB *runProcessCB = OsCurrProcessGet();//拿到当前运行进程
    LosProcessCB *groupProcessCB = OS_PCB_FROM_PID(gid);//通过组ID拿到组长PCB实体

    if (OsProcessIsInactive(processCB)) {//进程是否活动
        return LOS_ESRCH;
    }
	//参数进程不在用户态或者组长不在用户态
    if (!OsProcessIsUserMode(processCB) || !OsProcessIsUserMode(groupProcessCB)) {
        return LOS_EPERM;
    }

    if (runProcessCB->processID == processCB->parentProcessID) {
        if (processCB->processStatus & OS_PROCESS_FLAG_ALREADY_EXEC) {
            return LOS_EACCES;
        }
    } else if (processCB->processID != runProcessCB->processID) {
        return LOS_ESRCH;
    }

    /* Add the process to another existing process group */
    if (processCB->processID != gid) {
        if (!(groupProcessCB->processStatus & OS_PROCESS_FLAG_GROUP_LEADER)) {
            return LOS_EPERM;
        }

        if ((groupProcessCB->parentProcessID != processCB->parentProcessID) && (gid != processCB->parentProcessID)) {
            return LOS_EPERM;
        }
    }

    return LOS_OK;
}

STATIC UINT32 OsSetProcessGroupIDUnsafe(UINT32 pid, UINT32 gid, ProcessGroup **group)
{
    ProcessGroup *oldGroup = NULL;
    ProcessGroup *newGroup = NULL;
    LosProcessCB *processCB = OS_PCB_FROM_PID(pid);
    INT32 ret = OsSetProcessGroupCheck(processCB, gid);
    if (ret != LOS_OK) {
        return ret;
    }

    if (processCB->group->groupID == gid) {
        return LOS_OK;
    }

    oldGroup = processCB->group;
    OsExitProcessGroup(processCB, group);

    newGroup = OsFindProcessGroup(gid);
    if (newGroup != NULL) {
        LOS_ListTailInsert(&newGroup->processList, &processCB->subordinateGroupList);
        processCB->group = newGroup;
        return LOS_OK;
    }

    newGroup = OsCreateProcessGroup(gid);
    if (newGroup == NULL) {
        LOS_ListTailInsert(&oldGroup->processList, &processCB->subordinateGroupList);
        processCB->group = oldGroup;
        if (*group != NULL) {
            LOS_ListTailInsert(&g_processGroup->groupList, &oldGroup->groupList);
            processCB = OS_PCB_FROM_PID(oldGroup->groupID);
            processCB->processStatus |= OS_PROCESS_FLAG_GROUP_LEADER;
            *group = NULL;
        }
        return LOS_EPERM;
    }
    return LOS_OK;
}

LITE_OS_SEC_TEXT INT32 OsSetProcessGroupID(UINT32 pid, UINT32 gid)
{
    ProcessGroup *group = NULL;
    UINT32 ret;
    UINT32 intSave;

    if ((OS_PID_CHECK_INVALID(pid)) || (OS_PID_CHECK_INVALID(gid))) {
        return -LOS_EINVAL;
    }

    SCHEDULER_LOCK(intSave);
    ret = OsSetProcessGroupIDUnsafe(pid, gid, &group);
    SCHEDULER_UNLOCK(intSave);
    (VOID)LOS_MemFree(m_aucSysMem1, group);
    return -ret;
}

LITE_OS_SEC_TEXT INT32 OsSetCurrProcessGroupID(UINT32 gid)
{
    return OsSetProcessGroupID(OsCurrProcessGet()->processID, gid);
}

LITE_OS_SEC_TEXT INT32 LOS_GetProcessGroupID(UINT32 pid)
{
    INT32 gid;
    UINT32 intSave;
    LosProcessCB *processCB = NULL;

    if (OS_PID_CHECK_INVALID(pid)) {
        return -LOS_EINVAL;
    }

    SCHEDULER_LOCK(intSave);
    processCB = OS_PCB_FROM_PID(pid);
    if (OsProcessIsUnused(processCB)) {
        gid = -LOS_ESRCH;
        goto EXIT;
    }

    gid = processCB->group->groupID;

EXIT:
    SCHEDULER_UNLOCK(intSave);
    return gid;
}
//获取当前进程的组ID
LITE_OS_SEC_TEXT INT32 LOS_GetCurrProcessGroupID(VOID)
{
    return LOS_GetProcessGroupID(OsCurrProcessGet()->processID);
}
//用户进程分配栈并初始化
STATIC VOID *OsUserInitStackAlloc(UINT32 processID, UINT32 *size)
{
    LosVmMapRegion *region = NULL;
    LosProcessCB *processCB = OS_PCB_FROM_PID(processID);//获取当前进程实体
    UINT32 stackSize = ALIGN(OS_USER_TASK_STACK_SIZE, PAGE_SIZE);//1M栈空间 按页对齐
	//线性区分配虚拟内存
    region = LOS_RegionAlloc(processCB->vmSpace, 0, stackSize,
                             VM_MAP_REGION_FLAG_PERM_USER | VM_MAP_REGION_FLAG_PERM_READ |
                             VM_MAP_REGION_FLAG_PERM_WRITE, 0);//可使用可读可写区
    if (region == NULL) {
        return NULL;
    }

    LOS_SetRegionTypeAnon(region);//匿名映射
    region->regionFlags |= VM_MAP_REGION_FLAG_STACK;//标记该线性区为栈区

    *size = stackSize;//记录栈大小

    return (VOID *)(UINTPTR)region->range.base;
}
//执行回收和初始化,再利用
LITE_OS_SEC_TEXT UINT32 OsExecRecycleAndInit(LosProcessCB *processCB, const CHAR *name,
                                             LosVmSpace *oldSpace, UINTPTR oldFiles)
{
    UINT32 ret;
    errno_t errRet;
    const CHAR *processName = NULL;

    if ((processCB == NULL) || (name == NULL)) {
        return LOS_NOK;
    }

    processName = strrchr(name, '/');
    processName = (processName == NULL) ? name : (processName + 1); /* 1: Do not include '/' */

    ret = OsSetProcessName(processCB, processName);//设置进程名称
    if (ret != LOS_OK) {
        return ret;
    }
    errRet = memcpy_s(OsCurrTaskGet()->taskName, OS_TCB_NAME_LEN, processCB->processName, OS_PCB_NAME_LEN);
    if (errRet != EOK) {
        OsCurrTaskGet()->taskName[0] = '\0';
        return LOS_NOK;
    }

#if (LOSCFG_KERNEL_LITEIPC == YES)
    ret = LiteIpcPoolInit(&(processCB->ipcInfo));
    if (ret != LOS_OK) {
        PRINT_ERR("LiteIpcPoolInit failed!\n");
        return LOS_NOK;
    }
#endif

    processCB->sigHandler = 0;
    OsCurrTaskGet()->sig.sigprocmask = 0;

#ifdef LOSCFG_FS_VFS
    delete_files(OsCurrProcessGet(), (struct files_struct *)oldFiles);//删除进程文件管理器快照
#endif

    OsSwtmrRecycle(processCB->processID);//定时器回收
    processCB->timerID = (timer_t)(UINTPTR)MAX_INVALID_TIMER_VID;

#ifdef LOSCFG_SECURITY_VID
    VidMapDestroy(processCB);
    ret = VidMapListInit(processCB);
    if (ret != LOS_OK) {
        PRINT_ERR("VidMapListInit failed!\n");
        return LOS_NOK;
    }
#endif

    processCB->processStatus &= ~OS_PROCESS_FLAG_EXIT;
    processCB->processStatus |= OS_PROCESS_FLAG_ALREADY_EXEC;

    LOS_VmSpaceFree(oldSpace);
    return LOS_OK;
}
//进程层面的开始执行, entry为入口函数 ,其中 创建好task,task上下文 等待调度真正执行, sp:栈指针 mapBase:栈底 mapSize:栈大小
LITE_OS_SEC_TEXT UINT32 OsExecStart(const TSK_ENTRY_FUNC entry, UINTPTR sp, UINTPTR mapBase, UINT32 mapSize)
{
    LosProcessCB *processCB = NULL;
    LosTaskCB *taskCB = NULL;
    TaskContext *taskContext = NULL;
    UINT32 intSave;

    if (entry == NULL) {
        return LOS_NOK;
    }

    if ((sp == 0) || (LOS_Align(sp, LOSCFG_STACK_POINT_ALIGN_SIZE) != sp)) {//对齐
        return LOS_NOK;
    }

    if ((mapBase == 0) || (mapSize == 0) || (sp <= mapBase) || (sp > (mapBase + mapSize))) {//参数检查
        return LOS_NOK;
    }

    SCHEDULER_LOCK(intSave);//拿自旋锁
    processCB = OsCurrProcessGet();//获取当前进程
    taskCB = OsCurrTaskGet();//获取当前任务

    processCB->threadGroupID = taskCB->taskID;//threadGroupID是进程的主线程ID,也就是应用程序 main函数线程
    taskCB->userMapBase = mapBase;//任务栈底地址
    taskCB->userMapSize = mapSize;//任务栈大小
    taskCB->taskEntry = (TSK_ENTRY_FUNC)entry;//任务的入口函数

    taskContext = (TaskContext *)OsTaskStackInit(taskCB->taskID, taskCB->stackSize, (VOID *)taskCB->topOfStack, FALSE);//创建任务上下文
    OsUserTaskStackInit(taskContext, taskCB->taskEntry, sp);//用户进程任务栈初始化
    SCHEDULER_UNLOCK(intSave);//解锁
    return LOS_OK;
}
//用户进程开始初始化
STATIC UINT32 OsUserInitProcessStart(UINT32 processID, TSK_INIT_PARAM_S *param)
{
    UINT32 intSave;
    INT32 taskID;
    INT32 ret;

    taskID = OsCreateUserTask(processID, param);//创建一个用户态任务
    if (taskID < 0) {
        return LOS_NOK;
    }

    ret = LOS_SetTaskScheduler(taskID, LOS_SCHED_RR, OS_TASK_PRIORITY_LOWEST);
    if (ret < 0) {
        PRINT_ERR("User init process set scheduler failed! ERROR:%d \n", ret);
        SCHEDULER_LOCK(intSave);
        (VOID)OsTaskDeleteUnsafe(OS_TCB_FROM_TID(taskID), OS_PRO_EXIT_OK, intSave);
        return -ret;
    }

    return LOS_OK;
}
//所有的用户进程都是使用同一个用户代码段描述符和用户数据段描述符，它们是__USER_CS和__USER_DS，
//也就是每个进程处于用户态时，它们的CS寄存器和DS寄存器中的值是相同的。当任何进程或者中断异常进入内核后，
//都是使用相同的内核代码段描述符和内核数据段描述符，它们是__KERNEL_CS和__KERNEL_DS。这里要明确记得，内核数据段实际上就是内核态堆栈段。
LITE_OS_SEC_TEXT_INIT UINT32 OsUserInitProcess(VOID)
{
    INT32 ret;
    UINT32 size;
    TSK_INIT_PARAM_S param = { 0 };
    VOID *stack = NULL;
    VOID *userText = NULL;
    CHAR *userInitTextStart = (CHAR *)&__user_init_entry;//代码区开始位置 ,所有进程
    CHAR *userInitBssStart = (CHAR *)&__user_init_bss;// 未初始化数据区（BSS）。在运行时改变其值
    CHAR *userInitEnd = (CHAR *)&__user_init_end;// 结束地址
    UINT32 initBssSize = userInitEnd - userInitBssStart;
    UINT32 initSize = userInitEnd - userInitTextStart;

    LosProcessCB *processCB = OS_PCB_FROM_PID(g_userInitProcess);
    ret = OsProcessCreateInit(processCB, OS_USER_MODE, "Init", OS_PROCESS_USERINIT_PRIORITY);// 初始化用户进程,它将是所有应用程序的父进程
    if (ret != LOS_OK) {
        return ret;
    }

    userText = LOS_PhysPagesAllocContiguous(initSize >> PAGE_SHIFT);// 分配连续的物理页
    if (userText == NULL) {
        ret = LOS_NOK;
        goto ERROR;
    }

    (VOID)memcpy_s(userText, initSize, (VOID *)&__user_init_load_addr, initSize);// 安全copy 经加载器load的结果 __user_init_load_addr -> userText
    ret = LOS_VaddrToPaddrMmap(processCB->vmSpace, (VADDR_T)(UINTPTR)userInitTextStart, LOS_PaddrQuery(userText),
                               initSize, VM_MAP_REGION_FLAG_PERM_READ | VM_MAP_REGION_FLAG_PERM_WRITE |
                               VM_MAP_REGION_FLAG_PERM_EXECUTE | VM_MAP_REGION_FLAG_PERM_USER);// 虚拟地址与物理地址的映射
    if (ret < 0) {
        goto ERROR;
    }

    (VOID)memset_s((VOID *)((UINTPTR)userText + userInitBssStart - userInitTextStart), initBssSize, 0, initBssSize);// 除了代码段，其余都清0

    stack = OsUserInitStackAlloc(g_userInitProcess, &size);// 初始化堆栈区
    if (stack == NULL) {
        PRINTK("user init process malloc user stack failed!\n");
        ret = LOS_NOK;
        goto ERROR;
    }

    param.pfnTaskEntry = (TSK_ENTRY_FUNC)userInitTextStart;// 从代码区开始执行，也就是应用程序main 函数的位置
    param.userParam.userSP = (UINTPTR)stack + size;// 指向栈顶
    param.userParam.userMapBase = (UINTPTR)stack;// 栈底
    param.userParam.userMapSize = size;// 栈大小
    param.uwResved = OS_TASK_FLAG_PTHREAD_JOIN;// 可结合的（joinable）能够被其他线程收回其资源和杀死
    ret = OsUserInitProcessStart(g_userInitProcess, &param);// 创建一个任务，来运行main函数
    if (ret != LOS_OK) {
        (VOID)OsUnMMap(processCB->vmSpace, param.userParam.userMapBase, param.userParam.userMapSize);
        goto ERROR;
    }

    return LOS_OK;

ERROR:
    (VOID)LOS_PhysPagesFreeContiguous(userText, initSize >> PAGE_SHIFT);//释放物理内存块
    OsDeInitPCB(processCB);//删除PCB块
    return ret;
}
//拷贝用户信息 直接用memcpy_s
STATIC UINT32 OsCopyUser(LosProcessCB *childCB, LosProcessCB *parentCB)
{
#ifdef LOSCFG_SECURITY_CAPABILITY
    UINT32 size = sizeof(User) + sizeof(UINT32) * (parentCB->user->groupNumber - 1);
    childCB->user = LOS_MemAlloc(m_aucSysMem1, size);
    if (childCB->user == NULL) {
        return LOS_ENOMEM;
    }

    (VOID)memcpy_s(childCB->user, size, parentCB->user, size);
#endif
    return LOS_OK;
}
//任务初始化时拷贝任务信息
STATIC VOID OsInitCopyTaskParam(LosProcessCB *childProcessCB, const CHAR *name, UINTPTR entry, UINT32 size,
                                TSK_INIT_PARAM_S *childPara)
{
    LosTaskCB *mainThread = NULL;
    UINT32 intSave;

    SCHEDULER_LOCK(intSave);
    mainThread = OsCurrTaskGet();//获取当前task,注意变量名从这里也可以看出 thread 和 task 是一个概念,只是内核常说task,上层应用说thread ,概念的映射.

    if (OsProcessIsUserMode(childProcessCB)) {//用户模式进程
        childPara->pfnTaskEntry = mainThread->taskEntry;
        childPara->uwStackSize = mainThread->stackSize;
        childPara->userParam.userArea = mainThread->userArea;
        childPara->userParam.userMapBase = mainThread->userMapBase;
        childPara->userParam.userMapSize = mainThread->userMapSize;
    } else {
        childPara->pfnTaskEntry = (TSK_ENTRY_FUNC)entry;
        childPara->uwStackSize = size;
    }
    childPara->pcName = (CHAR *)name;
    childPara->policy = mainThread->policy;
    childPara->usTaskPrio = mainThread->priority;
    childPara->processID = childProcessCB->processID;
    if (mainThread->taskStatus & OS_TASK_FLAG_PTHREAD_JOIN) {
        childPara->uwResved = OS_TASK_FLAG_PTHREAD_JOIN;
    } else if (mainThread->taskStatus & OS_TASK_FLAG_DETACHED) {
        childPara->uwResved = OS_TASK_FLAG_DETACHED;
    }

    SCHEDULER_UNLOCK(intSave);
}
//拷贝一个Task过程
STATIC UINT32 OsCopyTask(UINT32 flags, LosProcessCB *childProcessCB, const CHAR *name, UINTPTR entry, UINT32 size)
{
    LosTaskCB *childTaskCB = NULL;
    TSK_INIT_PARAM_S childPara = { 0 };
    UINT32 ret;
    UINT32 intSave;
    UINT32 taskID;

    OsInitCopyTaskParam(childProcessCB, name, entry, size, &childPara);//初始化Task参数

    ret = LOS_TaskCreateOnly(&taskID, &childPara);//只创建任务,不调度
    if (ret != LOS_OK) {
        if (ret == LOS_ERRNO_TSK_TCB_UNAVAILABLE) {
            return LOS_EAGAIN;
        }
        return LOS_ENOMEM;
    }

    childTaskCB = OS_TCB_FROM_TID(taskID);//通过taskId获取task实体
    childTaskCB->taskStatus = OsCurrTaskGet()->taskStatus;//任务状态先同步,注意这里是赋值操作. ...01101001 
    if (childTaskCB->taskStatus & OS_TASK_STATUS_RUNNING) {//因只能有一个运行的task,所以如果一样要改4号位
        childTaskCB->taskStatus &= ~OS_TASK_STATUS_RUNNING;//将四号位清0 ,变成 ...01100001 
    } else {//非运行状态下会发生什么?
        if (OS_SCHEDULER_ACTIVE) {//克隆线程发生错误未运行
            LOS_Panic("Clone thread status not running error status: 0x%x\n", childTaskCB->taskStatus);
        }
        childTaskCB->taskStatus &= ~OS_TASK_STATUS_UNUSED;//干净的Task
        childProcessCB->priority = OS_PROCESS_PRIORITY_LOWEST;//进程设为最低优先级
    }

    if (OsProcessIsUserMode(childProcessCB)) {//是否是用户进程
        SCHEDULER_LOCK(intSave);
        OsUserCloneParentStack(childTaskCB, OsCurrTaskGet());//任务栈拷贝
        SCHEDULER_UNLOCK(intSave);
    }
    OS_TASK_PRI_QUEUE_ENQUEUE(childProcessCB, childTaskCB);//将task加入进程的就绪队列
    childTaskCB->taskStatus |= OS_TASK_STATUS_READY;//任务状态贴上就绪标签
    return LOS_OK;
}
//拷贝父亲大人的遗传基因信息
STATIC UINT32 OsCopyParent(UINT32 flags, LosProcessCB *childProcessCB, LosProcessCB *runProcessCB)
{
    UINT32 ret;
    UINT32 intSave;
    LosProcessCB *parentProcessCB = NULL;

    SCHEDULER_LOCK(intSave);
    childProcessCB->priority = runProcessCB->priority;	//当前进程所处阶级
    childProcessCB->policy = runProcessCB->policy;		//当前进程参与的调度方式

    if (flags & CLONE_PARENT) { //这里指明 childProcessCB 和 runProcessCB 有同一个父亲，是兄弟关系
        parentProcessCB = OS_PCB_FROM_PID(runProcessCB->parentProcessID);//找出当前进程的父亲大人
        childProcessCB->parentProcessID = parentProcessCB->processID;//指认父亲，这个赋值代表从此是你儿了
        LOS_ListTailInsert(&parentProcessCB->childrenList, &childProcessCB->siblingList);//通过我的兄弟姐妹节点，挂到父亲的孩子链表上，于我而言，父亲的这个链表上挂的都是我的兄弟姐妹
        													//不会被排序，老大，老二，老三 老天爷指定了。
        childProcessCB->group = parentProcessCB->group;//跟父亲大人在同一个进程组，注意父亲可能是组长，但更可能不是组长，
        LOS_ListTailInsert(&parentProcessCB->group->processList, &childProcessCB->subordinateGroupList);//自己去组里登记下，这个要自己登记，跟父亲没啥关系。
        ret = OsCopyUser(childProcessCB, parentProcessCB);
    } else {//这里指明 childProcessCB 和 runProcessCB 是父子关系
        childProcessCB->parentProcessID = runProcessCB->processID;//runProcessCB就是父亲，这个赋值代表从此是你儿了
        LOS_ListTailInsert(&runProcessCB->childrenList, &childProcessCB->siblingList);//同上理解
        childProcessCB->group = runProcessCB->group;//同上理解
        LOS_ListTailInsert(&runProcessCB->group->processList, &childProcessCB->subordinateGroupList);//同上理解
        ret = OsCopyUser(childProcessCB, runProcessCB);//同上理解
    }
    SCHEDULER_UNLOCK(intSave);
    return ret;
}
//拷贝虚拟空间
STATIC UINT32 OsCopyMM(UINT32 flags, LosProcessCB *childProcessCB, LosProcessCB *runProcessCB)
{
    status_t status;
    UINT32 intSave;

    if (!OsProcessIsUserMode(childProcessCB)) {//不是用户模式，直接返回，什么意思？内核虚拟空间只有一个，无需COPY ！！！
        return LOS_OK;
    }

    if (flags & CLONE_VM) {//贴有虚拟内存的标签
        SCHEDULER_LOCK(intSave);
        childProcessCB->vmSpace->archMmu.virtTtb = runProcessCB->vmSpace->archMmu.virtTtb;//TTB虚拟地址基地址,即L1表存放位置,virtTtb是个指针,进程的虚拟空间是指定的范围的
        childProcessCB->vmSpace->archMmu.physTtb = runProcessCB->vmSpace->archMmu.physTtb;//TTB物理地址基地址,physTtb是个值,取决于运行时映射到物理内存的具体哪个位置.
        SCHEDULER_UNLOCK(intSave);
        return LOS_OK;
    }

    status = LOS_VmSpaceClone(runProcessCB->vmSpace, childProcessCB->vmSpace);//虚拟空间clone
    if (status != LOS_OK) {
        return LOS_ENOMEM;
    }
    return LOS_OK;
}
//拷贝文件信息
STATIC UINT32 OsCopyFile(UINT32 flags, LosProcessCB *childProcessCB, LosProcessCB *runProcessCB)
{
#ifdef LOSCFG_FS_VFS
    if (flags & CLONE_FILES) {
        childProcessCB->files = runProcessCB->files;
    } else {
        childProcessCB->files = dup_fd(runProcessCB->files);
    }
    if (childProcessCB->files == NULL) {
        return LOS_ENOMEM;
    }
#endif

    childProcessCB->consoleID = runProcessCB->consoleID;
    childProcessCB->umask = runProcessCB->umask;
    return LOS_OK;
}

STATIC UINT32 OsForkInitPCB(UINT32 flags, LosProcessCB *child, const CHAR *name, UINTPTR sp, UINT32 size)
{
    UINT32 ret;
    LosProcessCB *run = OsCurrProcessGet();//获取当前进程

    ret = OsInitPCB(child, run->processMode, OS_PROCESS_PRIORITY_LOWEST, LOS_SCHED_RR, name);//初始化PCB信息，进程模式，优先级，调度方式，名称 == 信息
    if (ret != LOS_OK) {
        return ret;
    }

    ret = OsCopyParent(flags, child, run);//拷贝父亲大人的基因信息
    if (ret != LOS_OK) {
        return ret;
    }

    return OsCopyTask(flags, child, name, sp, size);//拷贝任务，设置任务入口函数，栈大小
}
//设置进程组和加入进程调度就绪队列
STATIC UINT32 OsChildSetProcessGroupAndSched(LosProcessCB *child, LosProcessCB *run)
{
    UINT32 intSave;
    UINT32 ret;
    ProcessGroup *group = NULL;

    SCHEDULER_LOCK(intSave);
    if (run->group->groupID == OS_USER_PRIVILEGE_PROCESS_GROUP) {//如果是有用户特权进程组
        ret = OsSetProcessGroupIDUnsafe(child->processID, child->processID, &group);//设置组ID,存在不安全的风险
        if (ret != LOS_OK) {
            SCHEDULER_UNLOCK(intSave);
            return LOS_ENOMEM;
        }
    }

    OS_PROCESS_PRI_QUEUE_ENQUEUE(child);//
    child->processStatus &= ~OS_PROCESS_STATUS_INIT;//去掉初始化标签
    child->processStatus |= OS_PROCESS_STATUS_READY;//贴上就绪标签

#ifdef LOSCFG_KERNEL_CPUP
    OsCpupSet(child->processID);
#endif
    SCHEDULER_UNLOCK(intSave);

    (VOID)LOS_MemFree(m_aucSysMem1, group);
    return LOS_OK;
}

STATIC INT32 OsCopyProcessResources(UINT32 flags, LosProcessCB *child, LosProcessCB *run)
{
    UINT32 ret;

    ret = OsCopyMM(flags, child, run);//拷贝虚拟空间
    if (ret != LOS_OK) {
        return ret;
    }

    ret = OsCopyFile(flags, child, run);//拷贝文件信息
    if (ret != LOS_OK) {
        return ret;
    }

#if (LOSCFG_KERNEL_LITEIPC == YES)
    if (OsProcessIsUserMode(child)) {//用户模式下
        ret = LiteIpcPoolReInit(&child->ipcInfo, (const ProcIpcInfo *)(&run->ipcInfo));//重新初始化IPC池
        if (ret != LOS_OK) {
            return LOS_ENOMEM;
        }
    }
#endif

#ifdef LOSCFG_SECURITY_CAPABILITY
    OsCopyCapability(run, child);//拷贝安全能力
#endif

    return LOS_OK;
}

STATIC INT32 OsCopyProcess(UINT32 flags, const CHAR *name, UINTPTR sp, UINT32 size)
{
    UINT32 intSave, ret, processID;
    LosProcessCB *run = OsCurrProcessGet();//获取当前进程

    LosProcessCB *child = OsGetFreePCB();//从进程池中申请一个进程控制块，鸿蒙进程池默认64
    if (child == NULL) {
        return -LOS_EAGAIN;
    }
    processID = child->processID;

    ret = OsForkInitPCB(flags, child, name, sp, size);//初始化进程控制块
    if (ret != LOS_OK) {
        goto ERROR_INIT;
    }

    ret = OsCopyProcessResources(flags, child, run);//拷贝进程的资源,包括虚拟空间,文件,安全,IPC ==
    if (ret != LOS_OK) {
        goto ERROR_TASK;
    }

    ret = OsChildSetProcessGroupAndSched(child, run);//设置进程组和加入进程调度就绪队列
    if (ret != LOS_OK) {
        goto ERROR_TASK;
    }

    LOS_MpSchedule(OS_MP_CPU_ALL);//给各CPU发送准备接受调度信号
    if (OS_SCHEDULER_ACTIVE) {//当前CPU core处于活动状态
        LOS_Schedule();// 申请调度
    }

    return processID;

ERROR_TASK:
    SCHEDULER_LOCK(intSave);
    (VOID)OsTaskDeleteUnsafe(OS_TCB_FROM_TID(child->threadGroupID), OS_PRO_EXIT_OK, intSave);
ERROR_INIT:
    OsDeInitPCB(child);
    return -ret;
}

LITE_OS_SEC_TEXT INT32 OsClone(UINT32 flags, UINTPTR sp, UINT32 size)
{
    UINT32 cloneFlag = CLONE_PARENT | CLONE_THREAD | CLONE_VFORK | CLONE_VM;

    if (flags & (~cloneFlag)) {
        PRINT_WARN("Clone dont support some flags!\n");
    }

    return OsCopyProcess(cloneFlag & flags, NULL, sp, size);
}
//著名的 fork 函数 记得前往 https://gitee.com/weharmony/kernel_liteos_a_note  fork一下 :)
LITE_OS_SEC_TEXT INT32 LOS_Fork(UINT32 flags, const CHAR *name, const TSK_ENTRY_FUNC entry, UINT32 stackSize)
{
    UINT32 cloneFlag = CLONE_PARENT | CLONE_THREAD | CLONE_VFORK | CLONE_FILES;

    if (flags & (~cloneFlag)) {
        PRINT_WARN("Clone dont support some flags!\n");
    }

    flags |= CLONE_FILES;
    return OsCopyProcess(cloneFlag & flags, name, (UINTPTR)entry, stackSize);//拷贝一个进程
}

LITE_OS_SEC_TEXT UINT32 LOS_GetCurrProcessID(VOID)
{
    return OsCurrProcessGet()->processID;
}

LITE_OS_SEC_TEXT VOID OsProcessExit(LosTaskCB *runTask, INT32 status)
{
    UINT32 intSave;
    LOS_ASSERT(runTask == OsCurrTaskGet());//只有当前进程才能调用这个函数

    OsTaskResourcesToFree(runTask);//释放任务资源
    OsProcessResourcesToFree(OsCurrProcessGet());//释放进程资源

    SCHEDULER_LOCK(intSave);
    OsProcessNaturalExit(runTask, status);//进程自然退出
    SCHEDULER_UNLOCK(intSave);
}
//接口封装 进程退出
LITE_OS_SEC_TEXT VOID LOS_Exit(INT32 status)
{
    OsTaskExitGroup((UINT32)status);
    OsProcessExit(OsCurrTaskGet(), (UINT32)status);
}
//获取用户进程的根进程,所有用户进程都是g_processCBArray[g_userInitProcess] fork来的
LITE_OS_SEC_TEXT UINT32 OsGetUserInitProcessID(VOID)
{
    return g_userInitProcess;
}
//获取Idel进程,CPU不公正时待的地方,等待被事件唤醒
LITE_OS_SEC_TEXT UINT32 OsGetIdleProcessID(VOID)
{
    return g_kernelIdleProcess;
}
//获取内核进程的根进程,所有内核进程都是g_processCBArray[g_kernelInitProcess] fork来的,包括g_processCBArray[g_kernelIdleProcess]进程
LITE_OS_SEC_TEXT UINT32 OsGetKernelInitProcessID(VOID)
{
    return g_kernelInitProcess;
}
//设置进程的中断处理函数
LITE_OS_SEC_TEXT VOID OsSetSigHandler(UINTPTR addr)
{
    OsCurrProcessGet()->sigHandler = addr;
}
//获取进程的中断处理函数
LITE_OS_SEC_TEXT UINTPTR OsGetSigHandler(VOID)
{
    return OsCurrProcessGet()->sigHandler;
}
#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif
