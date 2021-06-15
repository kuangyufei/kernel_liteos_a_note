/*
 * Copyright (c) 2013-2019, Huawei Technologies Co., Ltd. All rights reserved.
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

#include "los_process_pri.h"
#include "los_sched_pri.h"
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


/******************************************************************************
 并发（Concurrent）:多个线程在单个核心运行，同一时间只能一个线程运行，内核不停切换线程，
 		看起来像同时运行，实际上是线程不停切换
 并行（Parallel）每个线程分配给独立的CPU核心，线程同时运行
 单核CPU多个进程或多个线程内能实现并发（微观上的串行，宏观上的并行）
 多核CPU线程间可以实现宏观和微观上的并行
 LITE_OS_SEC_BSS 和 LITE_OS_SEC_DATA_INIT 是告诉编译器这些全局变量放在哪个数据段
******************************************************************************/
LITE_OS_SEC_BSS LosProcessCB *g_processCBArray = NULL; // 进程池数组
LITE_OS_SEC_DATA_INIT STATIC LOS_DL_LIST g_freeProcess;// 空闲状态下的进程链表, .个人觉得应该取名为 g_freeProcessList  @note_thinking
LITE_OS_SEC_DATA_INIT STATIC LOS_DL_LIST g_processRecyleList;// 需要回收的进程列表
LITE_OS_SEC_BSS UINT32 g_userInitProcess = OS_INVALID_VALUE;// 用户态的初始init进程,用户态下其他进程由它 fork
LITE_OS_SEC_BSS UINT32 g_kernelInitProcess = OS_INVALID_VALUE;// 内核态初始Kprocess进程,内核态下其他进程由它 fork
LITE_OS_SEC_BSS UINT32 g_kernelIdleProcess = OS_INVALID_VALUE;// 内核态idle进程,由Kprocess fork
LITE_OS_SEC_BSS UINT32 g_processMaxNum;// 进程最大数量,默认64个
LITE_OS_SEC_BSS ProcessGroup *g_processGroup = NULL;// 全局进程组,负责管理所有进程组
//将task从该进程的就绪队列中摘除,如果需要进程也从进程就绪队列中摘除

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
//通过组ID找到进程组
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
//查找进程是否有指定孩子进程
STATIC UINT32 OsFindChildProcess(const LosProcessCB *processCB, INT32 childPid)
{
    LosProcessCB *childCB = NULL;

    if (childPid < 0) {
        goto ERR;
    }

    LOS_DL_LIST_FOR_EACH_ENTRY(childCB, &(processCB->childrenList), LosProcessCB, siblingList) {//
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
//唤醒等待wakePID结束的任务,
VOID OsWaitWakeTask(LosTaskCB *taskCB, UINT32 wakePID)
{
    taskCB->waitID = wakePID;
    OsSchedTaskWake(taskCB);
#if (LOSCFG_KERNEL_SMP == YES)
    LOS_MpSchedule(OS_MP_CPU_ALL);//向所有cpu发送调度指令
#endif
}
//唤醒等待参数进程结束的任务
STATIC BOOL OsWaitWakeSpecifiedProcess(LOS_DL_LIST *head, const LosProcessCB *processCB, LOS_DL_LIST **anyList)
{
    LOS_DL_LIST *list = head;
    LosTaskCB *taskCB = NULL;
    UINT32 pid = 0;
    BOOL find = FALSE;

    while (list->pstNext != head) {//遍历等待链表 processCB->waitList
        taskCB = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(list));//一个一个来
        if ((taskCB->waitFlag == OS_PROCESS_WAIT_PRO) && (taskCB->waitID == processCB->processID)) {//符合OS_PROCESS_WAIT_PRO方式的
            if (pid == 0) {
                pid = processCB->processID;//等待的进程
                find = TRUE;//找到了
            } else {// @note_thinking 这个代码有点多余吧,会执行到吗?
                pid = OS_INVALID_VALUE;
            }

            OsWaitWakeTask(taskCB, pid);//唤醒这个任务,此时会切到 LOS_Wait runTask->waitFlag = 0;处运行
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
//检查父进程的等待任务并唤醒父进程去处理等待任务
STATIC VOID OsWaitCheckAndWakeParentProcess(LosProcessCB *parentCB, const LosProcessCB *processCB)
{
    LOS_DL_LIST *head = &parentCB->waitList;
    LOS_DL_LIST *list = NULL;
    LosTaskCB *taskCB = NULL;
    BOOL findSpecified = FALSE;

    if (LOS_ListEmpty(&parentCB->waitList)) {//父进程中是否有在等待子进程退出的任务?
        return;//没有就退出
    }

    findSpecified = OsWaitWakeSpecifiedProcess(head, processCB, &list);//找到指定的任务
    if (findSpecified == TRUE) {
        /* No thread is waiting for any child process to finish */
        if (LOS_ListEmpty(&parentCB->waitList)) {//没有线程正在等待任何子进程结束
            return;//已经处理完了,注意在OsWaitWakeSpecifiedProcess中做了频繁的任务切换
        } else if (!LOS_ListEmpty(&parentCB->childrenList)) {
            /* Other child processes exist, and other threads that are waiting
             * for the child to finish continue to wait
             *///存在其他子进程，正在等待它们的子进程结束而将继续等待
            return;
        }
    }

    /* Waiting threads are waiting for a specified child process to finish */
    if (list == NULL) {//等待线程正在等待指定的子进程结束
        return;
    }

    /* No child processes exist and all waiting threads are awakened */
    if (findSpecified == TRUE) {//所有等待的任务都被一一唤醒
        while (list->pstNext != head) {
            taskCB = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(list));
            OsWaitWakeTask(taskCB, OS_INVALID_VALUE);
        }
        return;
    }

    while (list->pstNext != head) {//处理 OS_PROCESS_WAIT_GID 标签
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
//回收指定进程的资源
LITE_OS_SEC_TEXT VOID OsProcessResourcesToFree(LosProcessCB *processCB)
{
    if (!(processCB->processStatus & (OS_PROCESS_STATUS_INIT | OS_PROCESS_STATUS_RUNNING))) {//1.初始化阶段并没有使用到资源,所以不用回收
        PRINT_ERR("The process(%d) has no permission to release process(%d) resources!\n",//2.正在运行的进程不能回收
                  OsCurrProcessGet()->processID, processCB->processID);
    }

#ifdef LOSCFG_FS_VFS
    if (OsProcessIsUserMode(processCB)) {//用户进程
        delete_files(processCB, processCB->files);//删除与用户进程相关的文件
    }
    processCB->files = NULL;
#endif

#ifdef LOSCFG_SECURITY_CAPABILITY //安全开关
    if (processCB->user != NULL) {
        (VOID)LOS_MemFree(m_aucSysMem1, processCB->user);//删除用户
        processCB->user = NULL;
    }
#endif

    OsSwtmrRecycle(processCB->processID);//回收进程使用的定时器
    processCB->timerID = (timer_t)(UINTPTR)MAX_INVALID_TIMER_VID;

#ifdef LOSCFG_SECURITY_VID
    if (processCB->timerIdMap.bitMap != NULL) {
        VidMapDestroy(processCB);
        processCB->timerIdMap.bitMap = NULL;
    }
#endif

#if (LOSCFG_KERNEL_LITEIPC == YES)
    if (OsProcessIsUserMode(processCB)) {//用户进程
        LiteIpcPoolDelete(&(processCB->ipcInfo));//删除进程对lite IPC的开销
        (VOID)memset_s(&(processCB->ipcInfo), sizeof(ProcIpcInfo), 0, sizeof(ProcIpcInfo));
    }
#endif
}
//回收僵死状态进程的资源
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
//当一个进程自然退出的时候,它的孩子进程由两位老祖宗收养
STATIC VOID OsDealAliveChildProcess(LosProcessCB *processCB)
{
    UINT32 parentID;
    LosProcessCB *childCB = NULL;
    LosProcessCB *parentCB = NULL;
    LOS_DL_LIST *nextList = NULL;
    LOS_DL_LIST *childHead = NULL;

    if (!LOS_ListEmpty(&processCB->childrenList)) {//如果存在孩子进程
        childHead = processCB->childrenList.pstNext;//获取孩子链表
        LOS_ListDelete(&(processCB->childrenList));//清空自己的孩子链表
        if (OsProcessIsUserMode(processCB)) {//是用户态进程
            parentID = g_userInitProcess;//用户态进程老祖宗
        } else {
            parentID = g_kernelInitProcess;//内核态进程老祖宗
        }

        for (nextList = childHead; ;) {//遍历孩子链表
            childCB = OS_PCB_FROM_SIBLIST(nextList);//找到孩子的真身
            childCB->parentProcessID = parentID;//孩子磕头认老祖宗为爸爸
            nextList = nextList->pstNext;//找下一个孩子进程
            if (nextList == childHead) {//一圈下来,孩子们都磕完头了
                break;
            }
        }

        parentCB = OS_PCB_FROM_PID(parentID);//找个老祖宗的真身
        LOS_ListTailInsertList(&parentCB->childrenList, childHead);//挂到老祖宗的孩子链表上
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
//一个进程的自然消亡过程,参数是当前运行的任务
STATIC VOID OsProcessNaturalExit(LosTaskCB *runTask, UINT32 status)
{
    LosProcessCB *processCB = OS_PCB_FROM_PID(runTask->processID);//通过task找到所属PCB
    LosProcessCB *parentCB = NULL;

    LOS_ASSERT(processCB->processStatus & OS_PROCESS_STATUS_RUNNING);//断言必须为正在运行的进程

    OsChildProcessResourcesFree(processCB);//释放孩子进程的资源


    /* is a child process */
    if (processCB->parentProcessID != OS_INVALID_VALUE) {//判断是否有父进程
        parentCB = OS_PCB_FROM_PID(processCB->parentProcessID);//获取父进程实体
        LOS_ListDelete(&processCB->siblingList);//将自己从兄弟链表中摘除,家人们,永别了!
        if (!OsProcessExitCodeSignalIsSet(processCB)) {//是否设置了退出码?
            OsProcessExitCodeSet(processCB, status);//将进程状态设为退出码
        }
        LOS_ListTailInsert(&parentCB->exitChildList, &processCB->siblingList);//挂到父进程的孩子消亡链表,家人中,永别的可不止我一个.
        LOS_ListDelete(&processCB->subordinateGroupList);//和志同道合的朋友们永别了,注意家里可不一定是朋友的,所有各有链表.
        LOS_ListTailInsert(&processCB->group->exitProcessList, &processCB->subordinateGroupList);//挂到进程组消亡链表,朋友中,永别的可不止我一个.

        OsWaitCheckAndWakeParentProcess(parentCB, processCB);//检查父进程的等待任务链表并唤醒对应的任务,此处将会频繁的切到其他任务运行.

        OsDealAliveChildProcess(processCB);//孩子们要怎么处理,移交给(用户态和内核态)根进程

        processCB->processStatus |= OS_PROCESS_STATUS_ZOMBIES;//贴上僵死进程的标签

#ifdef LOSCFG_KERNEL_VM
        (VOID)OsKill(processCB->parentProcessID, SIGCHLD, OS_KERNEL_KILL_PERMISSION);//以内核权限发送SIGCHLD(子进程退出)信号.
#endif
        LOS_ListHeadInsert(&g_processRecyleList, &processCB->pendList);//将进程通过其阻塞节点挂入全局进程回收链表
        OsRunTaskToDelete(runTask);//删除正在运行的任务
        return;
    }

    LOS_Panic("pid : %u is the root process exit!\n", processCB->processID);
    return;
}
//进程模块初始化,被编译放在代码段 .init 中
STATIC UINT32 OsProcessInit(VOID)
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
        g_processCBArray[index].processID = index;//进程ID[0-g_processMaxNum-1]赋值
        g_processCBArray[index].processStatus = OS_PROCESS_FLAG_UNUSED;// 默认都是白纸一张,贴上未使用标签
        LOS_ListTailInsert(&g_freeProcess, &g_processCBArray[index].pendList);//注意g_freeProcess挂的是pendList节点,所以使用要通过OS_PCB_FROM_PENDLIST找到进程实体.
    }

    g_kernelIdleProcess = 0; /* 0: The idle process ID of the kernel-mode process is fixed at 0 *///内核态init进程,从名字可以看出来这是让cpu休息的进程.
    LOS_ListDelete(&OS_PCB_FROM_PID(g_kernelIdleProcess)->pendList);//从空闲链表中摘掉
    
    g_userInitProcess = 1; /* 1: The root process ID of the user-mode process is fixed at 1 *///用户态的根进程
    LOS_ListDelete(&OS_PCB_FROM_PID(g_userInitProcess)->pendList);//从空闲链表中摘掉

    g_kernelInitProcess = 2; /* 2: The root process ID of the kernel-mode process is fixed at 2 *///内核态的根进程
    LOS_ListDelete(&OS_PCB_FROM_PID(g_kernelInitProcess)->pendList);//从空闲链表中摘掉

	//注意:这波骚操作之后,g_freeProcess链表上还有[3,g_processMaxNum-1]号进程.创建进程是从g_freeProcess上申请
	//即下次申请到的将是0号进程,而 OsCreateIdleProcess 将占有0号进程.

    return LOS_OK;
}

//进程回收再利用过程
LITE_OS_SEC_TEXT VOID OsProcessCBRecyleToFree(VOID)
{
    UINT32 intSave;
    LosProcessCB *processCB = NULL;

    SCHEDULER_LOCK(intSave);
    while (!LOS_ListEmpty(&g_processRecyleList)) {//循环任务回收链表,直到为空
        processCB = OS_PCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&g_processRecyleList));//找到回收链表中第一个进程实体
        //OS_PCB_FROM_PENDLIST 代表的是通过pendlist节点找到 PCB实体,因为g_processRecyleList上面挂的是pendlist节点位置
        if (!(processCB->processStatus & OS_PROCESS_FLAG_EXIT)) {//进程没有退出标签
            break;
        }
        SCHEDULER_UNLOCK(intSave);

        OsTaskCBRecycleToFree();

        SCHEDULER_LOCK(intSave);
        processCB->processStatus &= ~OS_PROCESS_FLAG_EXIT;//给进程撕掉退出标签,(可能进程并没有这个标签)
#ifdef LOSCFG_KERNEL_VM
        LosVmSpace *space = NULL;
        if (OsProcessIsUserMode(processCB)) {//进程是否是用户态进程
            space = processCB->vmSpace;//只有用户态的进程才需要释放虚拟内存空间
        }
        processCB->vmSpace = NULL;
#endif
        /* OS_PROCESS_FLAG_GROUP_LEADER: The lead process group cannot be recycled without destroying the PCB.
         * !OS_PROCESS_FLAG_UNUSED: Parent process does not reclaim child process resources.
         */
        LOS_ListDelete(&processCB->pendList);//将进程从进程链表上摘除
        if ((processCB->processStatus & OS_PROCESS_FLAG_GROUP_LEADER) ||//如果进程是进程组组长或者处于僵死状态
            (processCB->processStatus & OS_PROCESS_STATUS_ZOMBIES)) {
            LOS_ListTailInsert(&g_processRecyleList, &processCB->pendList);//将进程挂到进程回收链表上,因为组长不能走啊
        } else {
            /* Clear the bottom 4 bits of process status */
            OsInsertPCBToFreeList(processCB);//进程回到可分配池中,再分配利用
        }
        SCHEDULER_UNLOCK(intSave);
#ifdef LOSCFG_KERNEL_VM
        (VOID)LOS_VmSpaceFree(space);//释放用户态进程的虚拟内存空间,因为内核只有一个虚拟空间,因此不需要释放虚拟空间.
#endif
        SCHEDULER_LOCK(intSave);
    }

    SCHEDULER_UNLOCK(intSave);
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
UINT32 OsSetProcessName(LosProcessCB *processCB, const CHAR *name)
{
    errno_t errRet;

    if (processCB == NULL) {
        return LOS_EINVAL;
    }

    if (name != NULL) {
        errRet = strncpy_s(processCB->processName, OS_PCB_NAME_LEN, name, OS_PCB_NAME_LEN - 1);
        if (errRet == EOK) {
            return LOS_OK;
        }
    }

    switch (processCB->processMode) {
        case OS_KERNEL_MODE:
            errRet = snprintf_s(processCB->processName, OS_PCB_NAME_LEN, OS_PCB_NAME_LEN - 1,
                                "KerProcess%u", processCB->processID);
            break;
        default:
            errRet = snprintf_s(processCB->processName, OS_PCB_NAME_LEN, OS_PCB_NAME_LEN - 1,
                                "UserProcess%u", processCB->processID);
            break;
    }

    if (errRet < 0) {
        return LOS_NOK;
    }
    return LOS_OK;
}
//初始化PCB块
STATIC UINT32 OsInitPCB(LosProcessCB *processCB, UINT32 mode, UINT16 priority, const CHAR *name)
{
    processCB->processMode = mode;						//用户态进程还是内核态进程
    processCB->processStatus = OS_PROCESS_STATUS_INIT;	//进程初始状态
    processCB->parentProcessID = OS_INVALID_VALUE;		//爸爸进程，外面指定
    processCB->threadGroupID = OS_INVALID_VALUE;		//所属线程组
    processCB->priority = priority;						//进程优先级
    processCB->umask = OS_PROCESS_DEFAULT_UMASK;		//掩码
    processCB->timerID = (timer_t)(UINTPTR)MAX_INVALID_TIMER_VID;

    LOS_ListInit(&processCB->threadSiblingList);//初始化孩子任务/线程链表，上面挂的都是由此fork的孩子线程 见于 OsTaskCBInit LOS_ListTailInsert(&(processCB->threadSiblingList), &(taskCB->threadList));
    LOS_ListInit(&processCB->childrenList);		//初始化孩子进程链表，上面挂的都是由此fork的孩子进程 见于 OsCopyParent LOS_ListTailInsert(&parentProcessCB->childrenList, &childProcessCB->siblingList);
    LOS_ListInit(&processCB->exitChildList);	//初始化记录退出孩子进程链表，上面挂的是哪些exit	见于 OsProcessNaturalExit LOS_ListTailInsert(&parentCB->exitChildList, &processCB->siblingList);
    LOS_ListInit(&(processCB->waitList));		//初始化等待任务链表 上面挂的是处于等待的 见于 OsWaitInsertWaitLIstInOrder LOS_ListHeadInsert(&processCB->waitList, &runTask->pendList);

#ifdef LOSCFG_KERNEL_VM
    if (OsProcessIsUserMode(processCB)) {
        processCB->vmSpace = OsCreateUserVmSapce();
        if (processCB->vmSpace == NULL) {
            processCB->processStatus = OS_PROCESS_FLAG_UNUSED;
            return LOS_ENOMEM;
        }
    } else {
        processCB->vmSpace = LOS_GetKVmSpace();
    }
#endif

#ifdef LOSCFG_SECURITY_VID
    status_t status = VidMapListInit(processCB);
    if (status != LOS_OK) {
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
STATIC User *OsCreateUser(UINT32 userID, UINT32 gid, UINT32 size)//参数size 表示组数量
{	//(size - 1) * sizeof(UINT32) 用于 user->groups[..],这种设计节约了内存,不造成不需要的浪费
    User *user = LOS_MemAlloc(m_aucSysMem1, sizeof(User) + (size - 1) * sizeof(UINT32));
    if (user == NULL) {
        return NULL;
    }

    user->userID = userID;
    user->effUserID = userID;
    user->gid = gid;
    user->effGid = gid;
    user->groupNumber = size;//用户组数量
    user->groups[0] = gid;	 //用户组列表,一个用户可以属于多个用户组
    return user;
}
//检查参数群组ID是否在当前用户所属群组中
LITE_OS_SEC_TEXT BOOL LOS_CheckInGroups(UINT32 gid)
{
    UINT32 intSave;
    UINT32 count;
    User *user = NULL;

    SCHEDULER_LOCK(intSave);
    user = OsCurrUserGet();//当前进程所属用户
    for (count = 0; count < user->groupNumber; count++) {//循环对比
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
    UINT32 ret = OsInitPCB(processCB, flags, priority, name);
    if (ret != LOS_OK) {
        goto EXIT;
    }

#if (LOSCFG_KERNEL_LITEIPC == YES)
    if (OsProcessIsUserMode(processCB)) {//是否在用户模式
        ret = LiteIpcPoolInit(&(processCB->ipcInfo));//IPC池初始化
        if (ret != LOS_OK) {//异常处理
            ret = LOS_ENOMEM;
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

    return LOS_OK;

EXIT:
    OsDeInitPCB(processCB);//删除进程控制块,归还内存
    return ret;
}
//创建2,0号进程,即内核态进程的老祖宗
LITE_OS_SEC_TEXT_INIT UINT32 OsSystemProcessCreate(VOID)
{
    UINT32 ret = OsProcessInit();
    if (ret != LOS_OK) {
        return ret;
    }

    LosProcessCB *kerInitProcess = OS_PCB_FROM_PID(g_kernelInitProcess);//获取进程池中2号实体
    ret = OsProcessCreateInit(kerInitProcess, OS_KERNEL_MODE, "KProcess", 0);//创建内核态祖宗进程
    if (ret != LOS_OK) {
        return ret;
    }

    kerInitProcess->processStatus &= ~OS_PROCESS_STATUS_INIT;//去掉初始化标签
    g_processGroup = kerInitProcess->group;//进程组ID就是2号进程本身
    LOS_ListInit(&g_processGroup->groupList);//初始化进程组链表
    OsCurrProcessSet(kerInitProcess);//设置为当前进程,注意当前进程是内核的视角,并不代表一旦设置就必须执行进程的任务.

    LosProcessCB *idleProcess = OS_PCB_FROM_PID(g_kernelIdleProcess);//获取进程池中0号实体
    ret = OsInitPCB(idleProcess, OS_KERNEL_MODE, OS_TASK_PRIORITY_LOWEST, "KIdle");//创建内核态0号进程
    if (ret != LOS_OK) {
        return ret;
    }
    idleProcess->parentProcessID = kerInitProcess->processID;//认2号进程为父,它可是长子.
    LOS_ListTailInsert(&kerInitProcess->childrenList, &idleProcess->siblingList);//挂到内核态祖宗进程的子孙链接上
    idleProcess->group = kerInitProcess->group;//和老祖宗一个进程组,注意是父子并不代表是朋友.
    LOS_ListTailInsert(&kerInitProcess->group->processList, &idleProcess->subordinateGroupList);//挂到老祖宗的进程组链表上,进入了老祖宗的朋友圈.
#ifdef LOSCFG_SECURITY_CAPABILITY
    idleProcess->user = kerInitProcess->user;//共享用户
#endif
#ifdef LOSCFG_FS_VFS
    idleProcess->files = kerInitProcess->files;//共享文件
#endif

    ret = OsIdleTaskCreate();//创建cpu的idle任务,从此当前CPU OsPercpuGet()->idleTaskID 有了休息的地方.
    if (ret != LOS_OK) {
        return ret;
    }
    idleProcess->threadGroupID = OsPercpuGet()->idleTaskID;//设置进程的多线程组长

    return LOS_OK;
}
//进程调度参数检查
STATIC INLINE INT32 OsProcessSchedlerParamCheck(INT32 which, INT32 pid, UINT16 prio, UINT16 policy)
{
    if (OS_PID_CHECK_INVALID(pid)) {//进程ID是否有效，默认 g_processMaxNum = 64
        return LOS_EINVAL;
    }

    if (which != LOS_PRIO_PROCESS) {//进程标识
        return LOS_EINVAL;
    }

    if (prio > OS_PROCESS_PRIORITY_LOWEST) {//鸿蒙优先级是 0 -31级,0级最大
        return LOS_EINVAL;//返回无效参数
    }

    if (policy != LOS_SCHED_RR) {
        return LOS_EINVAL;
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
LITE_OS_SEC_TEXT INT32 OsSetProcessScheduler(INT32 which, INT32 pid, UINT16 prio, UINT16 policy)
{
    LosProcessCB *processCB = NULL;
    BOOL needSched = FALSE;
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

    needSched = OsSchedModifyProcessSchedParam(processCB, policy, prio);
    SCHEDULER_UNLOCK(intSave);//还锁

    LOS_MpSchedule(OS_MP_CPU_ALL);//核间中断
    if (needSched && OS_SCHEDULER_ACTIVE) {
        LOS_Schedule();//发起调度
    }
    return LOS_OK;

EXIT:
    SCHEDULER_UNLOCK(intSave);//还锁
    return -ret;
}
//设置进程调度方式
LITE_OS_SEC_TEXT INT32 LOS_SetProcessScheduler(INT32 pid, UINT16 policy, UINT16 prio)
{
    return OsSetProcessScheduler(LOS_PRIO_PROCESS, pid, prio, policy);
}
//获得进程调度方式
LITE_OS_SEC_TEXT INT32 LOS_GetProcessScheduler(INT32 pid)
{
    UINT32 intSave;

    if (OS_PID_CHECK_INVALID(pid)) {
        return -LOS_EINVAL;
    }

    SCHEDULER_LOCK(intSave);
    LosProcessCB *processCB = OS_PCB_FROM_PID(pid);
    if (OsProcessIsUnused(processCB)) {
        SCHEDULER_UNLOCK(intSave);
        return -LOS_ESRCH;
    }

    SCHEDULER_UNLOCK(intSave);

    return LOS_SCHED_RR;
}
//接口封装 - 设置进程优先级
LITE_OS_SEC_TEXT INT32 LOS_SetProcessPriority(INT32 pid, UINT16 prio)
{
    return OsSetProcessScheduler(LOS_PRIO_PROCESS, pid, prio, LOS_GetProcessScheduler(pid));
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
        return -LOS_EINVAL;
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

//将任务挂入进程的waitList链表,表示这个任务在等待某个进程的退出
//当被等待进程退出时候会将自己挂到父进程的退出子进程链表和进程组的退出进程链表.
STATIC VOID OsWaitInsertWaitListInOrder(LosTaskCB *runTask, LosProcessCB *processCB)
{
    LOS_DL_LIST *head = &processCB->waitList;
    LOS_DL_LIST *list = head;
    LosTaskCB *taskCB = NULL;

    if (runTask->waitFlag == OS_PROCESS_WAIT_GID) {
        while (list->pstNext != head) {
            taskCB = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(list));
            if (taskCB->waitFlag == OS_PROCESS_WAIT_PRO) {
                list = list->pstNext;
                continue;
            }
            break;
        }
    } else if (runTask->waitFlag == OS_PROCESS_WAIT_ANY) {
        while (list->pstNext != head) {
            taskCB = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(list));
            if (taskCB->waitFlag != OS_PROCESS_WAIT_ANY) {
                list = list->pstNext;
                continue;
            }
            break;
        }
    }
    /* if runTask->waitFlag == OS_PROCESS_WAIT_PRO,
     * this node is inserted directly into the header of the waitList
     */

    (VOID)OsSchedTaskWait(list->pstNext, LOS_WAIT_FOREVER, TRUE);
    return;
}
//设置等待子进程退出方式方法
STATIC UINT32 OsWaitSetFlag(const LosProcessCB *processCB, INT32 pid, LosProcessCB **child)
{
    LosProcessCB *childCB = NULL;
    ProcessGroup *group = NULL;
    LosTaskCB *runTask = OsCurrTaskGet();
    UINT32 ret;

    if (pid > 0) {//等待进程号为pid的子进程结束
        /* Wait for the child process whose process number is pid. */
        childCB = OsFindExitChildProcess(processCB, pid);//看能否从退出的孩子链表中找到PID
        if (childCB != NULL) {//找到了,确实有一个已经退出的PID,注意一个进程退出时会挂到父进程的exitChildList上
            goto WAIT_BACK;//直接成功返回
        }

        ret = OsFindChildProcess(processCB, pid);//看能否从现有的孩子链表中找到PID
        if (ret != LOS_OK) {
            return LOS_ECHILD;//参数进程并没有这个PID孩子,返回孩子进程失败.
        }
        runTask->waitFlag = OS_PROCESS_WAIT_PRO;//设置当前任务的等待类型
        runTask->waitID = pid;	//当前任务要等待进程ID结束
    } else if (pid == 0) {//等待同一进程组中的任何子进程
        /* Wait for any child process in the same process group */
        childCB = OsFindGroupExitProcess(processCB->group, OS_INVALID_VALUE);//看能否从退出的孩子链表中找到PID
        if (childCB != NULL) {//找到了,确实有一个已经退出的PID
            goto WAIT_BACK;//直接成功返回
        }
        runTask->waitID = processCB->group->groupID;//等待进程组的任意一个子进程结束
        runTask->waitFlag = OS_PROCESS_WAIT_GID;//设置当前任务的等待类型
    } else if (pid == -1) {//等待任意子进程
        /* Wait for any child process */
        childCB = OsFindExitChildProcess(processCB, OS_INVALID_VALUE);//看能否从退出的孩子链表中找到PID
        if (childCB != NULL) {//找到了,确实有一个已经退出的PID
            goto WAIT_BACK;
        }
        runTask->waitID = pid;//等待PID,这个PID可以和当前进程没有任何关系
        runTask->waitFlag = OS_PROCESS_WAIT_ANY;//设置当前任务的等待类型
    } else { /* pid < -1 */ //等待指定进程组内为|pid|的所有子进程
        /* Wait for any child process whose group number is the pid absolute value. */
        group = OsFindProcessGroup(-pid);//先通过PID找到进程组
        if (group == NULL) {
            return LOS_ECHILD;
        }

        childCB = OsFindGroupExitProcess(group, OS_INVALID_VALUE);//在进程组里任意一个已经退出的子进程
        if (childCB != NULL) {
            goto WAIT_BACK;
        }

        runTask->waitID = -pid;//此处用负数是为了和(pid == 0)以示区别,因为二者的waitFlag都一样.
        runTask->waitFlag = OS_PROCESS_WAIT_GID;//设置当前任务的等待类型
    }

WAIT_BACK:
    *child = childCB;
    return LOS_OK;
}
//等待回收孩子进程 @note_thinking 这样写Porcess不太好吧
STATIC UINT32 OsWaitRecycleChildPorcess(const LosProcessCB *childCB, UINT32 intSave, INT32 *status)
{
    ProcessGroup *group = NULL;
    UINT32 pid = childCB->processID;
    UINT16 mode = childCB->processMode;
    INT32 exitCode = childCB->exitCode;

    OsRecycleZombiesProcess((LosProcessCB *)childCB, &group);//回收僵尸进程
    SCHEDULER_UNLOCK(intSave);

    if (status != NULL) {
        if (mode == OS_USER_MODE) {//孩子为用户态进程
            (VOID)LOS_ArchCopyToUser((VOID *)status, (const VOID *)(&(exitCode)), sizeof(INT32));//从内核空间拷贝退出码
        } else {
            *status = exitCode;
        }
    }

    (VOID)LOS_MemFree(m_aucSysMem1, group);
    return pid;
}
//检查要等待的孩子进程
STATIC UINT32 OsWaitChildProcessCheck(LosProcessCB *processCB, INT32 pid, LosProcessCB **childCB)
{	//当进程没有孩子且没有退出的孩子进程
    if (LOS_ListEmpty(&(processCB->childrenList)) && LOS_ListEmpty(&(processCB->exitChildList))) {
        return LOS_ECHILD;
    }

    return OsWaitSetFlag(processCB, pid, childCB);//设置等待子进程退出方式方法
}

STATIC UINT32 OsWaitOptionsCheck(UINT32 options)
{
    UINT32 flag = LOS_WAIT_WNOHANG | LOS_WAIT_WUNTRACED | LOS_WAIT_WCONTINUED;

    flag = ~flag & options;
    if (flag != 0) {//三种方式中一种都没有
        return LOS_EINVAL;//无效参数
    }

    if ((options & (LOS_WAIT_WCONTINUED | LOS_WAIT_WUNTRACED)) != 0) {//暂不支持这两种方式.
        return LOS_EOPNOTSUPP;//不支持
    }

    if (OS_INT_ACTIVE) {//中断发生期间
        return LOS_EINTR;//中断提示
    }

    return LOS_OK;
}
//返回已经终止的子进程的进程ID号，并清除僵死进程。
LITE_OS_SEC_TEXT INT32 LOS_Wait(INT32 pid, USER INT32 *status, UINT32 options, VOID *rusage)
{
    (VOID)rusage;
    UINT32 ret;
    UINT32 intSave;
    LosProcessCB *childCB = NULL;
    LosProcessCB *processCB = NULL;
    LosTaskCB *runTask = NULL;

    ret = OsWaitOptionsCheck(options);//参数检查,只支持LOS_WAIT_WNOHANG
    if (ret != LOS_OK) {
        return -ret;
    }

    SCHEDULER_LOCK(intSave);
    processCB = OsCurrProcessGet();	//获取当前进程
    runTask = OsCurrTaskGet();		//获取当前任务

    ret = OsWaitChildProcessCheck(processCB, pid, &childCB);//先检查下看能不能找到参数要求的退出子进程
    if (ret != LOS_OK) {
        pid = -ret;
        goto ERROR;
    }

    if (childCB != NULL) {//找到了进程
        return (INT32)OsWaitRecycleChildPorcess(childCB, intSave, status);
    }
	//没有找到,看是否要返回还是去做个登记
    if ((options & LOS_WAIT_WNOHANG) != 0) {//有LOS_WAIT_WNOHANG标签
        runTask->waitFlag = 0;//等待标识置0
        pid = 0;//这里置0,是为了 return 0
        goto ERROR;
    }
	//等待孩子进程退出
    OsWaitInsertWaitListInOrder(runTask, processCB);//将当前任务挂入进程waitList链表
	//发起调度的目的是为了让出CPU,让其他进程/任务运行
	
    runTask->waitFlag = 0;
    if (runTask->waitID == OS_INVALID_VALUE) {
        pid = -LOS_ECHILD;//没有此子进程
        goto ERROR;
    }

    childCB = OS_PCB_FROM_PID(runTask->waitID);//获取当前任务的等待子进程ID
    if (!(childCB->processStatus & OS_PROCESS_STATUS_ZOMBIES)) {//子进程非僵死进程
        pid = -LOS_ESRCH;//没有此进程
        goto ERROR;
    }
	//回收僵死进程
    return (INT32)OsWaitRecycleChildPorcess(childCB, intSave, status);

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
    UINT32 ret = OsSetProcessGroupCheck(processCB, gid);
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

    gid = (INT32)processCB->group->groupID;

EXIT:
    SCHEDULER_UNLOCK(intSave);
    return gid;
}
//获取当前进程的组ID
LITE_OS_SEC_TEXT INT32 LOS_GetCurrProcessGroupID(VOID)
{
    return LOS_GetProcessGroupID(OsCurrProcessGet()->processID);
}
//为用户态任务分配栈空间
#ifdef LOSCFG_KERNEL_VM
STATIC LosProcessCB *OsGetFreePCB(VOID)
{
    LosProcessCB *processCB = NULL;
    UINT32 intSave;

    SCHEDULER_LOCK(intSave);
    if (LOS_ListEmpty(&g_freeProcess)) {
        SCHEDULER_UNLOCK(intSave);
        PRINT_ERR("No idle PCB in the system!\n");
        return NULL;
    }

    processCB = OS_PCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&g_freeProcess));
    LOS_ListDelete(&processCB->pendList);
    SCHEDULER_UNLOCK(intSave);

    return processCB;
}

STATIC VOID *OsUserInitStackAlloc(LosProcessCB *processCB, UINT32 *size)
{
    LosVmMapRegion *region = NULL;
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
/**************************************************
进程的回收再利用,被LOS_DoExecveFile调用
**************************************************/
LITE_OS_SEC_TEXT UINT32 OsExecRecycleAndInit(LosProcessCB *processCB, const CHAR *name,
                                             LosVmSpace *oldSpace, UINTPTR oldFiles)
{
    UINT32 ret;
    const CHAR *processName = NULL;

    if ((processCB == NULL) || (name == NULL)) {
        return LOS_NOK;
    }

    processName = strrchr(name, '/');
    processName = (processName == NULL) ? name : (processName + 1); /* 1: Do not include '/' */

    ret = (UINT32)OsSetTaskName(OsCurrTaskGet(), processName, TRUE);
    if (ret != LOS_OK) {
        return ret;
    }

#if (LOSCFG_KERNEL_LITEIPC == YES)
    ret = LiteIpcPoolInit(&(processCB->ipcInfo));
    if (ret != LOS_OK) {
        return LOS_NOK;
    }
#endif

    processCB->sigHandler = 0;
    OsCurrTaskGet()->sig.sigprocmask = 0;

    LOS_VmSpaceFree(oldSpace);
#ifdef LOSCFG_FS_VFS
    delete_files(OsCurrProcessGet(), (struct files_struct *)oldFiles);//删除进程文件管理器快照
    alloc_std_fd(OsCurrProcessGet()->files->fdt);
#endif

    OsSwtmrRecycle(processCB->processID);//回收定时器
    processCB->timerID = (timer_t)(UINTPTR)MAX_INVALID_TIMER_VID;

#ifdef LOSCFG_SECURITY_VID
    VidMapDestroy(processCB);
    ret = VidMapListInit(processCB);
    if (ret != LOS_OK) {
        return LOS_NOK;
    }
#endif

    processCB->processStatus &= ~OS_PROCESS_FLAG_EXIT;	//去掉进程退出标签
    processCB->processStatus |= OS_PROCESS_FLAG_ALREADY_EXEC;//加上进程运行elf标签

    return LOS_OK;
}
//执行用户态任务, entry为入口函数 ,其中 创建好task,task上下文 等待调度真正执行, sp:栈指针 mapBase:栈底 mapSize:栈大小
LITE_OS_SEC_TEXT UINT32 OsExecStart(const TSK_ENTRY_FUNC entry, UINTPTR sp, UINTPTR mapBase, UINT32 mapSize)
{
    UINT32 intSave;

    if (entry == NULL) {
        return LOS_NOK;
    }

    if ((sp == 0) || (LOS_Align(sp, LOSCFG_STACK_POINT_ALIGN_SIZE) != sp)) {//对齐
        return LOS_NOK;
    }
	//注意 sp此时指向栈底,栈底地址要大于栈顶
    if ((mapBase == 0) || (mapSize == 0) || (sp <= mapBase) || (sp > (mapBase + mapSize))) {//参数检查
        return LOS_NOK;
    }

    LosTaskCB *taskCB = OsCurrTaskGet();//获取当前任务
    SCHEDULER_LOCK(intSave);//拿自旋锁

    taskCB->userMapBase = mapBase;//用户态栈顶位置
    taskCB->userMapSize = mapSize;//用户态栈
    taskCB->taskEntry = (TSK_ENTRY_FUNC)entry;//任务的入口函数
	//初始化内核态栈
    TaskContext *taskContext = (TaskContext *)OsTaskStackInit(taskCB->taskID, taskCB->stackSize,
                                                              (VOID *)taskCB->topOfStack, FALSE);
    OsUserTaskStackInit(taskContext, (UINTPTR)taskCB->taskEntry, sp);//初始化用户栈,将内核栈中上下文的 context->R[0] = sp ,context->sp = sp
    //这样做的目的是将用户栈SP保存到内核栈中,
    SCHEDULER_UNLOCK(intSave);//解锁
    return LOS_OK;
}
//用户进程开始初始化
STATIC UINT32 OsUserInitProcessStart(UINT32 processID, TSK_INIT_PARAM_S *param)
{
    UINT32 intSave;
    UINT32 taskID;
    INT32 ret;

    taskID = OsCreateUserTask(processID, param);//创建一个用户态任务
    if (taskID == OS_INVALID_VALUE) {
        return LOS_NOK;
    }

    ret = LOS_SetTaskScheduler(taskID, LOS_SCHED_RR, OS_TASK_PRIORITY_LOWEST);//调度器:设置为抢占式调度和最低任务优先级(31级)
    if (ret != LOS_OK) {
        PRINT_ERR("User init process set scheduler failed! ERROR:%d \n", ret);
        SCHEDULER_LOCK(intSave);
        (VOID)OsTaskDeleteUnsafe(OS_TCB_FROM_TID(taskID), OS_PRO_EXIT_OK, intSave);
        return LOS_NOK;
    }

    return LOS_OK;
}

STATIC UINT32 OsLoadUserInit(LosProcessCB *processCB)
{
    /*              userInitTextStart               -----
     * | user text |
     *
     * | user data |                                initSize
     *              userInitBssStart  ---
     * | user bss  |                  initBssSize
     *              userInitEnd       ---           -----
     */
    errno_t errRet;
    INT32 ret;
    CHAR *userInitTextStart = (CHAR *)&__user_init_entry;
    CHAR *userInitBssStart = (CHAR *)&__user_init_bss;
    CHAR *userInitEnd = (CHAR *)&__user_init_end;
    UINT32 initBssSize = userInitEnd - userInitBssStart;
    UINT32 initSize = userInitEnd - userInitTextStart;
    VOID *userBss = NULL;
    VOID *userText = NULL;

    if ((LOS_Align((UINTPTR)userInitTextStart, PAGE_SIZE) != (UINTPTR)userInitTextStart) ||
        (LOS_Align((UINTPTR)userInitEnd, PAGE_SIZE) != (UINTPTR)userInitEnd)) {
        return LOS_EINVAL;
    }

    if ((initSize == 0) || (initSize <= initBssSize)) {
        return LOS_EINVAL;
    }

    userText = LOS_PhysPagesAllocContiguous(initSize >> PAGE_SHIFT);
    if (userText == NULL) {
        return LOS_NOK;
    }

    errRet = memcpy_s(userText, initSize, (VOID *)&__user_init_load_addr, initSize - initBssSize);
    if (errRet != EOK) {
        PRINT_ERR("Load user init text, data and bss failed! err : %d\n", errRet);
        goto ERROR;
    }
    ret = LOS_VaddrToPaddrMmap(processCB->vmSpace, (VADDR_T)(UINTPTR)userInitTextStart, LOS_PaddrQuery(userText),
                               initSize, VM_MAP_REGION_FLAG_PERM_READ | VM_MAP_REGION_FLAG_PERM_WRITE |
                               VM_MAP_REGION_FLAG_FIXED | VM_MAP_REGION_FLAG_PERM_EXECUTE |
                               VM_MAP_REGION_FLAG_PERM_USER);
    if (ret < 0) {
        PRINT_ERR("Mmap user init text, data and bss failed! err : %d\n", ret);
        goto ERROR;
    }

    /* The User init boot segment may not actually exist */
    if (initBssSize != 0) {
        userBss = (VOID *)((UINTPTR)userText + userInitBssStart - userInitTextStart);
        errRet = memset_s(userBss, initBssSize, 0, initBssSize);
        if (errRet != EOK) {
            PRINT_ERR("memset user init bss failed! err : %d\n", errRet);
            goto ERROR;
        }
    }

    return LOS_OK;

ERROR:
    (VOID)LOS_PhysPagesFreeContiguous(userText, initSize >> PAGE_SHIFT);
    return LOS_NOK;
}

LITE_OS_SEC_TEXT_INIT UINT32 OsUserInitProcess(VOID)
{
    UINT32 ret;
    UINT32 size;
    TSK_INIT_PARAM_S param = { 0 };
    VOID *stack = NULL;

    LosProcessCB *processCB = OS_PCB_FROM_PID(g_userInitProcess);
    ret = OsProcessCreateInit(processCB, OS_USER_MODE, "Init", OS_PROCESS_USERINIT_PRIORITY);
    if (ret != LOS_OK) {
        return ret;
    }

    ret = OsLoadUserInit(processCB);
    if (ret != LOS_OK) {
        goto ERROR;
    }

    stack = OsUserInitStackAlloc(processCB, &size);
    if (stack == NULL) {
        PRINT_ERR("Alloc user init process user stack failed!\n");
        goto ERROR;
    }

    param.pfnTaskEntry = (TSK_ENTRY_FUNC)(CHAR *)&__user_init_entry;
    param.userParam.userSP = (UINTPTR)stack + size;
    param.userParam.userMapBase = (UINTPTR)stack;
    param.userParam.userMapSize = size;
    param.uwResved = OS_TASK_FLAG_PTHREAD_JOIN;
    ret = OsUserInitProcessStart(g_userInitProcess, &param);
    if (ret != LOS_OK) {
        (VOID)OsUnMMap(processCB->vmSpace, param.userParam.userMapBase, param.userParam.userMapSize);
        goto ERROR;
    }

    return LOS_OK;

ERROR:
    OsDeInitPCB(processCB);
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

    if (OsProcessIsUserMode(childProcessCB)) {//用户态进程
        childPara->pfnTaskEntry = mainThread->taskEntry;//拷贝当前任务入口地址
        childPara->uwStackSize = mainThread->stackSize;	//栈空间大小
        childPara->userParam.userArea = mainThread->userArea;		//用户态栈区栈顶位置
        childPara->userParam.userMapBase = mainThread->userMapBase;	//用户态栈底
        childPara->userParam.userMapSize = mainThread->userMapSize;	//用户态栈大小
    } else {//注意内核态进程创建任务的入口由外界指定,例如 OsCreateIdleProcess 指定了OsIdleTask
        childPara->pfnTaskEntry = (TSK_ENTRY_FUNC)entry;//参数(sp)为内核态入口地址
        childPara->uwStackSize = size;//参数(size)为内核态栈大小
    }
    childPara->pcName = (CHAR *)name;					//进程名字
    childPara->policy = mainThread->policy;				//调度模式
    childPara->usTaskPrio = mainThread->priority;		//优先级
    childPara->processID = childProcessCB->processID;	//进程ID
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
    LosTaskCB *runTask = OsCurrTaskGet();
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

    LosTaskCB *childTaskCB = OS_TCB_FROM_TID(taskID);
    childTaskCB->taskStatus = runTask->taskStatus;//任务状态先同步,注意这里是赋值操作. ...01101001 
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
        OsUserCloneParentStack(childTaskCB->stackPointer, runTask->topOfStack, runTask->stackSize);//拷贝当前任务上下文给新的任务
        SCHEDULER_UNLOCK(intSave);
    }
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

    if (flags & CLONE_PARENT) { //这里指明 childProcessCB 和 runProcessCB 有同一个父亲，是兄弟关系
        parentProcessCB = OS_PCB_FROM_PID(runProcessCB->parentProcessID);//找出当前进程的父亲大人
    } else {
        parentProcessCB = runProcessCB;         
    }
        childProcessCB->parentProcessID = parentProcessCB->processID;//指认父亲，这个赋值代表从此是你儿了
        LOS_ListTailInsert(&parentProcessCB->childrenList, &childProcessCB->siblingList);//通过我的兄弟姐妹节点，挂到父亲的孩子链表上，于我而言，父亲的这个链表上挂的都是我的兄弟姐妹
        													//不会被排序，老大，老二，老三 老天爷指定了。
        childProcessCB->group = parentProcessCB->group;//跟父亲大人在同一个进程组，注意父亲可能是组长，但更可能不是组长，
        LOS_ListTailInsert(&parentProcessCB->group->processList, &childProcessCB->subordinateGroupList);//自己去组里登记下，这个要自己登记，跟父亲没啥关系。
        ret = OsCopyUser(childProcessCB, parentProcessCB);

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

    ret = OsInitPCB(child, run->processMode, OS_PROCESS_PRIORITY_LOWEST, name);
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

    OsSchedTaskEnQueue(OS_TCB_FROM_TID(child->threadGroupID));
    SCHEDULER_UNLOCK(intSave);

    (VOID)LOS_MemFree(m_aucSysMem1, group);
    return LOS_OK;
}

STATIC UINT32 OsCopyProcessResources(UINT32 flags, LosProcessCB *child, LosProcessCB *run)
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
#else
LITE_OS_SEC_TEXT_INIT UINT32 OsUserInitProcess(VOID)
{
    return 0;
}
#endif

LITE_OS_SEC_TEXT VOID LOS_Exit(INT32 status)
{
    UINT32 intSave;

    /* The exit of a kernel - state process must be kernel - state and all threads must actively exit */
    LosProcessCB *processCB = OsCurrProcessGet();
    SCHEDULER_LOCK(intSave);
    if (!OsProcessIsUserMode(processCB) && (processCB->threadNumber != 1)) {
        SCHEDULER_UNLOCK(intSave);
        PRINT_ERR("Kernel-state processes with multiple threads are not allowed to exit directly\n");
        return;
    }
    SCHEDULER_UNLOCK(intSave);
    OsTaskExitGroup((UINT32)status);
    OsProcessExit(OsCurrTaskGet(), (UINT32)status);
}

LITE_OS_SEC_TEXT UINT32 LOS_GetCurrProcessID(VOID)
{
    return OsCurrProcessGet()->processID;
}

LITE_OS_SEC_TEXT VOID OsProcessExit(LosTaskCB *runTask, INT32 status)
{
    UINT32 intSave;
    LOS_ASSERT(runTask == OsCurrTaskGet());//只有当前进程才能调用这个函数,即进程最后的退出不假手他人

    OsTaskResourcesToFree(runTask);//释放任务资源
    OsProcessResourcesToFree(OsCurrProcessGet());//释放进程资源

    SCHEDULER_LOCK(intSave);
    OsProcessNaturalExit(runTask, status);//进程自然退出
    SCHEDULER_UNLOCK(intSave);
}

LITE_OS_SEC_TEXT UINT32 LOS_GetSystemProcessMaximum(VOID)
{
    return g_processMaxNum;
}
//获取用户态进程的根进程,所有用户进程都是g_processCBArray[g_userInitProcess] fork来的
LITE_OS_SEC_TEXT UINT32 OsGetUserInitProcessID(VOID)
{
    return g_userInitProcess;//用户态根进程 序号为 1
}

LITE_OS_SEC_TEXT UINT32 OsGetKernelInitProcessID(VOID)
{
    return g_kernelInitProcess;
}

LITE_OS_SEC_TEXT UINT32 OsGetIdleProcessID(VOID)
{
    return g_kernelIdleProcess;
}
//设置进程的信号处理函数
LITE_OS_SEC_TEXT VOID OsSetSigHandler(UINTPTR addr)
{
    OsCurrProcessGet()->sigHandler = addr;
}
//获取进程的信号处理函数
LITE_OS_SEC_TEXT UINTPTR OsGetSigHandler(VOID)
{
    return OsCurrProcessGet()->sigHandler;
}

