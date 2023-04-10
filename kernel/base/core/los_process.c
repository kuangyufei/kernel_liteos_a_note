/*!
 * @file  los_process.c
 * @brief 进程模块主文件
 * @link
   @verbatim
   
   并发（Concurrent）:多个线程在单个核心运行，同一时间只能一个线程运行，内核不停切换线程，
		  看起来像同时运行，实际上是线程不停切换
   并行（Parallel）每个线程分配给独立的CPU核心，线程同时运行
   单核CPU多个进程或多个线程内能实现并发（微观上的串行，宏观上的并行）
   多核CPU线程间可以实现宏观和微观上的并行
   LITE_OS_SEC_BSS 和 LITE_OS_SEC_DATA_INIT 是告诉编译器这些全局变量放在哪个数据段

   @endverbatim
 * @version 
 * @author  weharmonyos.com | 鸿蒙研究站 | 每天死磕一点点
 * @date    2021-12-15
 */
 
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
#include "fs/fs_operation.h"
#include "internal.h"
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
#ifdef LOSCFG_KERNEL_DYNLOAD
#include "los_load_elf.h"
#endif
#include "los_swtmr_pri.h"
#include "los_vm_map.h"
#include "los_vm_phys.h"
#include "los_vm_syscall.h"

LITE_OS_SEC_BSS LosProcessCB *g_processCBArray = NULL; ///< 进程池数组
LITE_OS_SEC_DATA_INIT STATIC LOS_DL_LIST g_freeProcess;///< 空闲状态下的进程链表, .个人觉得应该取名为 g_freeProcessList  @note_thinking
LITE_OS_SEC_DATA_INIT STATIC LOS_DL_LIST g_processRecycleList;///< 需要回收的进程列表
LITE_OS_SEC_BSS UINT32 g_processMaxNum;///< 进程最大数量,默认64个
#ifndef LOSCFG_PID_CONTAINER
LITE_OS_SEC_BSS ProcessGroup *g_processGroup = NULL;///< 全局进程组,负责管理所有进程组
#define OS_ROOT_PGRP(processCB) (g_processGroup)
#endif
/*
 * @brief 将进程插入到空闲链表中
 * @details 
 * @param argc 1
 * @param[LosProcessCB]  processCB  指定进程
 * @return  函数执行结果
 * - VOID   无
*/
STATIC INLINE VOID OsInsertPCBToFreeList(LosProcessCB *processCB)
{
#ifdef LOSCFG_PID_CONTAINER
    OsPidContainerDestroy(processCB->container, processCB);
#endif
    UINT32 pid = processCB->processID;//获取进程ID
    (VOID)memset_s(processCB, sizeof(LosProcessCB), 0, sizeof(LosProcessCB));//进程描述符数据清0
    processCB->processID = pid;//进程ID
    processCB->processStatus = OS_PROCESS_FLAG_UNUSED;//设置为进程未使用
    processCB->timerID = (timer_t)(UINTPTR)MAX_INVALID_TIMER_VID;//timeID初始化值
    LOS_ListTailInsert(&g_freeProcess, &processCB->pendList);//进程节点挂入g_freeProcess以分配给后续进程使用
}

VOID OsDeleteTaskFromProcess(LosTaskCB *taskCB)
{
    LosProcessCB *processCB = OS_PCB_FROM_TCB(taskCB);

    LOS_ListDelete(&taskCB->threadList);
    processCB->threadNumber--;
    OsTaskInsertToRecycleList(taskCB);
}

UINT32 OsProcessAddNewTask(UINTPTR processID, LosTaskCB *taskCB, SchedParam *param, UINT32 *numCount)
{
    UINT32 intSave;
    LosProcessCB *processCB = (LosProcessCB *)processID;

    SCHEDULER_LOCK(intSave);
#ifdef LOSCFG_PID_CONTAINER
    if (OsAllocVtid(taskCB, processCB) == OS_INVALID_VALUE) {
        SCHEDULER_UNLOCK(intSave);
        PRINT_ERR("OsAllocVtid failed!\n");
        return LOS_NOK;
    }
#endif

    taskCB->processCB = (UINTPTR)processCB;
    LOS_ListTailInsert(&(processCB->threadSiblingList), &(taskCB->threadList));
    if (OsProcessIsUserMode(processCB)) {
        taskCB->taskStatus |= OS_TASK_FLAG_USER_MODE;
        if (processCB->threadNumber > 0) {
            LosTaskCB *task = processCB->threadGroup;
            task->ops->schedParamGet(task, param);
        } else {
            OsSchedProcessDefaultSchedParamGet(param->policy, param);
        }
    } else {
        LosTaskCB *runTask = OsCurrTaskGet();
        runTask->ops->schedParamGet(runTask, param);
    }

#ifdef LOSCFG_KERNEL_VM
    taskCB->archMmu = (UINTPTR)&processCB->vmSpace->archMmu;
#endif
    if (!processCB->threadNumber) {
        processCB->threadGroup = taskCB;
    }
    processCB->threadNumber++;

    *numCount = processCB->threadCount;
    processCB->threadCount++;
    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;
}
/**
 * @brief 创建进程组
 * @details 
 * @param argc 1
 * @param[UINT32]  pid  进程ID
 * @return  函数执行结果
 * - ProcessGroup   返回进程组
*/
ProcessGroup *OsCreateProcessGroup(LosProcessCB *processCB)
{
    ProcessGroup *pgroup = LOS_MemAlloc(m_aucSysMem1, sizeof(ProcessGroup));//分配一个进程组
    if (pgroup == NULL) {
        return NULL;
    }

    pgroup->pgroupLeader = (UINTPTR)processCB;//指定进程组负责人
    LOS_ListInit(&pgroup->processList);//初始化组员链表
    LOS_ListInit(&pgroup->exitProcessList);//初始化僵死进程链表

    LOS_ListTailInsert(&pgroup->processList, &processCB->subordinateGroupList);
    processCB->pgroup = pgroup;
    processCB->processStatus |= OS_PROCESS_FLAG_GROUP_LEADER;//进程状态贴上当老大的标签

    ProcessGroup *rootPGroup = OS_ROOT_PGRP(processCB);
    if (rootPGroup == NULL) {
        OS_ROOT_PGRP(processCB) = pgroup;
        LOS_ListInit(&pgroup->groupList);
    } else {
        LOS_ListTailInsert(&rootPGroup->groupList, &pgroup->groupList);
    }
    return pgroup;
}

/*! 退出进程组,参数是进程地址和进程组地址的地址 */
STATIC VOID ExitProcessGroup(LosProcessCB *processCB, ProcessGroup **pgroup)
{
    LosProcessCB *pgroupCB = OS_GET_PGROUP_LEADER(processCB->pgroup);

    LOS_ListDelete(&processCB->subordinateGroupList);//从进程组进程链表上摘出去
    if (LOS_ListEmpty(&processCB->pgroup->processList) && LOS_ListEmpty(&processCB->pgroup->exitProcessList)) {//进程组进程链表和退出进程链表都为空时
#ifdef LOSCFG_PID_CONTAINER
        if (processCB->pgroup != OS_ROOT_PGRP(processCB)) {
#endif
        LOS_ListDelete(&processCB->pgroup->groupList);//从全局进程组链表上把自己摘出去 记住它是 LOS_ListTailInsert(&g_processGroup->groupList, &group->groupList) 挂上去的
        *pgroup = processCB->pgroup;//????? 这步操作没看明白,谁能告诉我为何要这么做?
#ifdef LOSCFG_PID_CONTAINER
        }
#endif
        pgroupCB->processStatus &= ~OS_PROCESS_FLAG_GROUP_LEADER;
        if (OsProcessIsUnused(pgroupCB) && !(pgroupCB->processStatus & OS_PROCESS_FLAG_EXIT)) {//组长进程时退出的标签
            LOS_ListDelete(&pgroupCB->pendList);//进程从全局进程链表上摘除
            OsInsertPCBToFreeList(pgroupCB);//释放进程的资源,回到freelist再利用
        }
    }

    processCB->pgroup = NULL;
}

/*! 通过指定组ID找到进程组 */
STATIC ProcessGroup *OsFindProcessGroup(UINT32 gid)
{
    ProcessGroup *pgroup = NULL;
    ProcessGroup *rootPGroup = OS_ROOT_PGRP(OsCurrProcessGet());
    LosProcessCB *processCB = OS_GET_PGROUP_LEADER(rootPGroup);
    if (processCB->processID == gid) {
        return rootPGroup;
    }

    LOS_DL_LIST_FOR_EACH_ENTRY(pgroup, &rootPGroup->groupList, ProcessGroup, groupList) {
        processCB = OS_GET_PGROUP_LEADER(pgroup);
        if (processCB->processID == gid) {
            return pgroup;
        }
    }

    PRINT_INFO("%s failed! pgroup id = %u\n", __FUNCTION__, gid);
    return NULL;
}

/*! 给指定进程组发送信号 */
STATIC INT32 OsSendSignalToSpecifyProcessGroup(ProcessGroup *pgroup, siginfo_t *info, INT32 permission)
{
    INT32 ret, success, err;
    LosProcessCB *childCB = NULL;

    success = 0;
    ret = -LOS_ESRCH;
    LOS_DL_LIST_FOR_EACH_ENTRY(childCB, &(pgroup->processList), LosProcessCB, subordinateGroupList) {
        if (childCB->processID == 0) {
            continue;
        }

        err = OsDispatch(childCB->processID, info, permission);
        success |= !err;
        ret = err;
    }
    /* At least one success. */
    return success ? LOS_OK : ret;
}

LITE_OS_SEC_TEXT INT32 OsSendSignalToAllProcess(siginfo_t *info, INT32 permission)
{
    INT32 ret, success, err;
    ProcessGroup *pgroup = NULL;
    ProcessGroup *rootPGroup = OS_ROOT_PGRP(OsCurrProcessGet());

    success = 0;
    err = OsSendSignalToSpecifyProcessGroup(rootPGroup, info, permission);
    success |= !err;
    ret = err;
    /* all processes group */
    LOS_DL_LIST_FOR_EACH_ENTRY(pgroup, &rootPGroup->groupList, ProcessGroup, groupList) {
        /* all processes in the process group. */
        err = OsSendSignalToSpecifyProcessGroup(pgroup, info, permission);
        success |= !err;
        ret = err;
    }
    return success ? LOS_OK : ret;
}

LITE_OS_SEC_TEXT INT32 OsSendSignalToProcessGroup(INT32 pid, siginfo_t *info, INT32 permission)
{
    ProcessGroup *pgroup = NULL;
    /* Send SIG to all processes in process group PGRP.
       If PGRP is zero, send SIG to all processes in
       the current process's process group. */
    pgroup = OsFindProcessGroup(pid ? -pid : LOS_GetCurrProcessGroupID());
    if (pgroup == NULL) {
        return -LOS_ESRCH;
    }
    /* all processes in the process group. */
    return OsSendSignalToSpecifyProcessGroup(pgroup, info, permission);
}

STATIC LosProcessCB *OsFindGroupExitProcess(ProcessGroup *pgroup, INT32 pid)
{
    LosProcessCB *childCB = NULL;

    LOS_DL_LIST_FOR_EACH_ENTRY(childCB, &(pgroup->exitProcessList), LosProcessCB, subordinateGroupList) {
        if ((childCB->processID == pid) || (pid == OS_INVALID_VALUE)) {
            return childCB;
        }
    }

    return NULL;
}

STATIC UINT32 OsFindChildProcess(const LosProcessCB *processCB, const LosProcessCB *wait)
{
    LosProcessCB *childCB = NULL;

    LOS_DL_LIST_FOR_EACH_ENTRY(childCB, &(processCB->childrenList), LosProcessCB, siblingList) {
        if (childCB == wait) {
            return LOS_OK;
        }
    }

    return LOS_NOK;
}

STATIC LosProcessCB *OsFindExitChildProcess(const LosProcessCB *processCB, const LosProcessCB *wait)
{
    LosProcessCB *exitChild = NULL;

    LOS_DL_LIST_FOR_EACH_ENTRY(exitChild, &(processCB->exitChildList), LosProcessCB, siblingList) {
        if ((wait == NULL) || (exitChild == wait)) {
            return exitChild;
        }
    }

    return NULL;
}

/*! 唤醒等待wakePID结束的任务 */
VOID OsWaitWakeTask(LosTaskCB *taskCB, UINTPTR wakePID)
{
    taskCB->waitID = wakePID;
    taskCB->ops->wake(taskCB);
#ifdef LOSCFG_KERNEL_SMP
    LOS_MpSchedule(OS_MP_CPU_ALL);//向所有cpu发送调度指令
#endif
}

/*! 唤醒等待参数进程结束的任务 */
STATIC BOOL OsWaitWakeSpecifiedProcess(LOS_DL_LIST *head, const LosProcessCB *processCB, LOS_DL_LIST **anyList)
{
    LOS_DL_LIST *list = head;
    LosTaskCB *taskCB = NULL;
    UINTPTR processID = 0;
    BOOL find = FALSE;

    while (list->pstNext != head) {//遍历等待链表 processCB->waitList
        taskCB = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(list));//一个一个来
        if ((taskCB->waitFlag == OS_PROCESS_WAIT_PRO) && (taskCB->waitID == (UINTPTR)processCB)) {
            if (processID == 0) {
                processID = taskCB->waitID;
                find = TRUE;//找到了
            } else {// @note_thinking 这个代码有点多余吧,会执行到吗?
                processID = OS_INVALID_VALUE;
            }

            OsWaitWakeTask(taskCB, processID);//唤醒这个任务,此时会切到 LOS_Wait runTask->waitFlag = 0;处运行
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

/*! 检查父进程的等待任务并唤醒父进程去处理等待任务 */
STATIC VOID OsWaitCheckAndWakeParentProcess(LosProcessCB *parentCB, const LosProcessCB *processCB)
{
    LOS_DL_LIST *head = &parentCB->waitList;
    LOS_DL_LIST *list = NULL;
    LosTaskCB *taskCB = NULL;
    BOOL findSpecified = FALSE;

    if (LOS_ListEmpty(&parentCB->waitList)) {//父进程中是否有在等待子进程退出的任务?
        return;//没有就退出
    }
    // TODO 
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
            if (taskCB->waitID != (UINTPTR)OS_GET_PGROUP_LEADER(processCB->pgroup)) {
                list = list->pstNext;
                continue;
            }
        }

        if (findSpecified == FALSE) {
            OsWaitWakeTask(taskCB, (UINTPTR)processCB);
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

/*! 回收指定进程的资源 */
LITE_OS_SEC_TEXT VOID OsProcessResourcesToFree(LosProcessCB *processCB)
{
#ifdef LOSCFG_KERNEL_VM
    if (OsProcessIsUserMode(processCB)) {
        (VOID)OsVmSpaceRegionFree(processCB->vmSpace);
    }
#endif

#ifdef LOSCFG_SECURITY_CAPABILITY //安全开关
    if (processCB->user != NULL) {
        (VOID)LOS_MemFree(m_aucSysMem1, processCB->user);//删除用户
        processCB->user = NULL;	//重置指针为空
    }
#endif

#ifdef LOSCFG_BASE_CORE_SWTMR_ENABLE
    OsSwtmrRecycle((UINTPTR)processCB);
    processCB->timerID = (timer_t)(UINTPTR)MAX_INVALID_TIMER_VID;
#endif

#ifdef LOSCFG_SECURITY_VID
    if (processCB->timerIdMap.bitMap != NULL) {
        VidMapDestroy(processCB);
        processCB->timerIdMap.bitMap = NULL;
    }
#endif

#ifdef LOSCFG_KERNEL_LITEIPC
    (VOID)LiteIpcPoolDestroy(processCB->processID);
#endif

#ifdef LOSCFG_KERNEL_CPUP
    UINT32 intSave;
    OsCpupBase *processCpup = processCB->processCpup;
    SCHEDULER_LOCK(intSave);
    processCB->processCpup = NULL;
    SCHEDULER_UNLOCK(intSave);
    (VOID)LOS_MemFree(m_aucSysMem1, processCpup);
#endif

#ifdef LOSCFG_PROC_PROCESS_DIR
    ProcFreeProcessDir(processCB->procDir);
    processCB->procDir = NULL;
#endif

#ifdef LOSCFG_KERNEL_CONTAINER
    OsOsContainersDestroyEarly(processCB);
#endif

#ifdef LOSCFG_FS_VFS
    if (OsProcessIsUserMode(processCB)) {
        delete_files(processCB->files);
    }
    processCB->files = NULL;
#endif

#ifdef LOSCFG_KERNEL_CONTAINER
    OsContainersDestroy(processCB);
#endif

#ifdef LOSCFG_KERNEL_PLIMITS
    OsPLimitsDeleteProcess(processCB);
#endif
    if (processCB->resourceLimit != NULL) {
        (VOID)LOS_MemFree((VOID *)m_aucSysMem0, processCB->resourceLimit);
        processCB->resourceLimit = NULL;
    }
}

/*! 回收僵死状态进程的资源 */
STATIC VOID OsRecycleZombiesProcess(LosProcessCB *childCB, ProcessGroup **pgroup)
{
    ExitProcessGroup(childCB, pgroup);//退出进程组
    LOS_ListDelete(&childCB->siblingList);//从父亲大人的子孙链表上摘除
    if (OsProcessIsDead(childCB)) {
        OsDeleteTaskFromProcess(childCB->threadGroup);
        childCB->processStatus &= ~OS_PROCESS_STATUS_ZOMBIES;//去掉僵死标签
        childCB->processStatus |= OS_PROCESS_FLAG_UNUSED;//贴上没使用标签，进程由进程池分配，进程退出后重新回到空闲进程池
    }

    LOS_ListDelete(&childCB->pendList);//将自己从阻塞链表上摘除，注意有很多原因引起阻塞，pendList挂在哪里就以为这属于哪类阻塞
    if (childCB->processStatus & OS_PROCESS_FLAG_EXIT) {//如果有退出标签
        LOS_ListHeadInsert(&g_processRecycleList, &childCB->pendList);//从头部插入，注意g_processRecyleList挂的是pendList节点,所以要通过OS_PCB_FROM_PENDLIST找.
    } else if (OsProcessIsPGroupLeader(childCB)) {
        LOS_ListTailInsert(&g_processRecycleList, &childCB->pendList);//从尾部插入，意思就是组长尽量最后一个处理
    } else {
        OsInsertPCBToFreeList(childCB);//直接插到freeList中去，可用于重新分配了。
    }
}

/*! 当一个进程自然退出的时候,它的孩子进程由两位老祖宗收养 */
STATIC VOID OsDealAliveChildProcess(LosProcessCB *processCB)
{
    LosProcessCB *childCB = NULL;
    LosProcessCB *parentCB = NULL;
    LOS_DL_LIST *nextList = NULL;
    LOS_DL_LIST *childHead = NULL;

#ifdef LOSCFG_PID_CONTAINER
    if (processCB->processID == OS_USER_ROOT_PROCESS_ID) {
        return;
    }
#endif
    if (!LOS_ListEmpty(&processCB->childrenList)) {//如果存在孩子进程
        childHead = processCB->childrenList.pstNext;//获取孩子链表
        LOS_ListDelete(&(processCB->childrenList));//清空自己的孩子链表
        if (OsProcessIsUserMode(processCB)) {//是用户态进程
            parentCB = OS_PCB_FROM_PID(OS_USER_ROOT_PROCESS_ID);
        } else {
            parentCB = OsGetKernelInitProcess();
        }

        for (nextList = childHead; ;) {//遍历孩子链表
            childCB = OS_PCB_FROM_SIBLIST(nextList);//找到孩子的真身
            childCB->parentProcess = parentCB;
            nextList = nextList->pstNext;//找下一个孩子进程
            if (nextList == childHead) {//一圈下来,孩子们都磕完头了
                break;
            }
        }

        LOS_ListTailInsertList(&parentCB->childrenList, childHead);//挂到老祖宗的孩子链表上
    }

    return;
}

/*! 回收指定进程的已经退出(死亡)的孩子进程所占资源 */
STATIC VOID OsChildProcessResourcesFree(const LosProcessCB *processCB)
{
    LosProcessCB *childCB = NULL;
    ProcessGroup *pgroup = NULL;

    while (!LOS_ListEmpty(&((LosProcessCB *)processCB)->exitChildList)) {//遍历直到没有了退出(死亡)的孩子进程
        childCB = LOS_DL_LIST_ENTRY(processCB->exitChildList.pstNext, LosProcessCB, siblingList);//获取孩子进程,
        OsRecycleZombiesProcess(childCB, &pgroup);//其中会将childCB从exitChildList链表上摘出去
        (VOID)LOS_MemFree(m_aucSysMem1, pgroup);
    }
}

/*! 一个进程的自然消亡过程,参数是当前运行的任务*/
VOID OsProcessNaturalExit(LosProcessCB *processCB, UINT32 status)
{
    OsChildProcessResourcesFree(processCB);//释放孩子进程的资源


    /* is a child process */
    if (processCB->parentProcess != NULL) {
        LosProcessCB *parentCB = processCB->parentProcess;
        LOS_ListDelete(&processCB->siblingList);//将自己从兄弟链表中摘除,家人们,永别了!
        if (!OsProcessExitCodeSignalIsSet(processCB)) {//是否设置了退出码?
            OsProcessExitCodeSet(processCB, status);//将进程状态设为退出码
        }
        LOS_ListTailInsert(&parentCB->exitChildList, &processCB->siblingList);//挂到父进程的孩子消亡链表,家人中,永别的可不止我一个.
        LOS_ListDelete(&processCB->subordinateGroupList);//和志同道合的朋友们永别了,注意家里可不一定是朋友的,所有各有链表.
        LOS_ListTailInsert(&processCB->pgroup->exitProcessList, &processCB->subordinateGroupList);//挂到进程组消亡链表,朋友中,永别的可不止我一个.

        OsWaitCheckAndWakeParentProcess(parentCB, processCB);//检查父进程的等待任务链表并唤醒对应的任务,此处将会频繁的切到其他任务运行.

        OsDealAliveChildProcess(processCB);//孩子们要怎么处理,移交给(用户态和内核态)根进程

        processCB->processStatus |= OS_PROCESS_STATUS_ZOMBIES;//贴上僵死进程的标签

#ifdef LOSCFG_KERNEL_VM
        (VOID)OsSendSigToProcess(parentCB, SIGCHLD, OS_KERNEL_KILL_PERMISSION);
#endif
        LOS_ListHeadInsert(&g_processRecycleList, &processCB->pendList);//将进程通过其阻塞节点挂入全局进程回收链表
        return;
    }

    LOS_Panic("pid : %u is the root process exit!\n", processCB->processID);
    return;
}

STATIC VOID SystemProcessEarlyInit(LosProcessCB *processCB)
{
    LOS_ListDelete(&processCB->pendList);
#ifdef LOSCFG_KERNEL_CONTAINER
    OsContainerInitSystemProcess(processCB);
#endif
    if (processCB == OsGetKernelInitProcess()) {//2号进程
        OsSetMainTaskProcess((UINTPTR)processCB);//将内核根进程设为主任务所属进程
    }
}
/*! 进程模块初始化,被编译放在代码段 .init 中*/
UINT32 OsProcessInit(VOID)
{
    UINT32 index;
    UINT32 size;
    UINT32 ret;

    g_processMaxNum = LOSCFG_BASE_CORE_PROCESS_LIMIT;//默认支持64个进程
    size = (g_processMaxNum + 1) * sizeof(LosProcessCB);

    g_processCBArray = (LosProcessCB *)LOS_MemAlloc(m_aucSysMem1, size);// 进程池，占用内核堆,内存池分配 
    if (g_processCBArray == NULL) {
        return LOS_NOK;
    }
    (VOID)memset_s(g_processCBArray, size, 0, size);//安全方式重置清0

    LOS_ListInit(&g_freeProcess);//进程空闲链表初始化，创建一个进程时从g_freeProcess中申请一个进程描述符使用
    LOS_ListInit(&g_processRecycleList);//进程回收链表初始化,回收完成后进入g_freeProcess等待再次被申请使用

    for (index = 0; index < g_processMaxNum; index++) {//进程池循环创建
        g_processCBArray[index].processID = index;//进程ID[0-g_processMaxNum-1]赋值
        g_processCBArray[index].processStatus = OS_PROCESS_FLAG_UNUSED;// 默认都是白纸一张,贴上未使用标签
        LOS_ListTailInsert(&g_freeProcess, &g_processCBArray[index].pendList);//注意g_freeProcess挂的是pendList节点,所以使用要通过OS_PCB_FROM_PENDLIST找到进程实体.
    }

    /* Default process to prevent thread PCB from being empty */
    g_processCBArray[index].processID = index;
    g_processCBArray[index].processStatus = OS_PROCESS_FLAG_UNUSED;

    ret = OsTaskInit((UINTPTR)&g_processCBArray[g_processMaxNum]);
    if (ret != LOS_OK) {
        (VOID)LOS_MemFree(m_aucSysMem1, g_processCBArray);
        return LOS_OK;
    }

#ifdef LOSCFG_KERNEL_CONTAINER
    OsInitRootContainer();
#endif
#ifdef LOSCFG_KERNEL_PLIMITS
    OsProcLimiterSetInit();
#endif
    SystemProcessEarlyInit(OsGetIdleProcess());//初始化 0,1,2号进程
    SystemProcessEarlyInit(OsGetUserInitProcess());
    SystemProcessEarlyInit(OsGetKernelInitProcess());
    return LOS_OK;
}

/*! 进程回收再利用过程*/
LITE_OS_SEC_TEXT VOID OsProcessCBRecycleToFree(VOID)
{
    UINT32 intSave;
    LosProcessCB *processCB = NULL;

    SCHEDULER_LOCK(intSave);
    while (!LOS_ListEmpty(&g_processRecycleList)) {//循环任务回收链表,直到为空
        processCB = OS_PCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&g_processRecycleList));//找到回收链表中第一个进程实体
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
        if (OsProcessIsPGroupLeader(processCB) || OsProcessIsDead(processCB)) {
            LOS_ListTailInsert(&g_processRecycleList, &processCB->pendList);//将进程挂到进程回收链表上,因为组长不能走啊
        } else {
            /* Clear the bottom 4 bits of process status */
            OsInsertPCBToFreeList(processCB);//进程回到可分配池中,再分配利用
        }
#ifdef LOSCFG_KERNEL_VM
        SCHEDULER_UNLOCK(intSave);
        (VOID)LOS_VmSpaceFree(space);//释放用户态进程的虚拟内存空间,因为内核只有一个虚拟空间,因此不需要释放虚拟空间.
        SCHEDULER_LOCK(intSave);
#endif
    }

    SCHEDULER_UNLOCK(intSave);
}

/*! 删除PCB块 其实是 PCB块回归进程池,先进入回收链表*/
STATIC VOID OsDeInitPCB(LosProcessCB *processCB)
{
    UINT32 intSave;
    ProcessGroup *pgroup = NULL;

    if (processCB == NULL) {
        return;
    }

#ifdef LOSCFG_KERNEL_CONTAINER
    if (OS_PID_CHECK_INVALID(processCB->processID)) {
        return;
    }
#endif
    OsProcessResourcesToFree(processCB);//释放进程所占用的资源

    SCHEDULER_LOCK(intSave);
    if (processCB->parentProcess != NULL) {
        LOS_ListDelete(&processCB->siblingList);//将进程从兄弟链表中摘除
        processCB->parentProcess = NULL;
    }

    if (processCB->pgroup != NULL) {
        ExitProcessGroup(processCB, &pgroup);
    }

    processCB->processStatus &= ~OS_PROCESS_STATUS_INIT;//设置进程状态为非初始化
    processCB->processStatus |= OS_PROCESS_FLAG_EXIT;	//设置进程状态为退出
    LOS_ListHeadInsert(&g_processRecycleList, &processCB->pendList);
    SCHEDULER_UNLOCK(intSave);

    (VOID)LOS_MemFree(m_aucSysMem1, pgroup);
    OsWriteResourceEvent(OS_RESOURCE_EVENT_FREE);
    return;
}

/*! 设置进程的名字*/
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

/*! 初始化PCB(进程控制块)*/
STATIC UINT32 OsInitPCB(LosProcessCB *processCB, UINT32 mode, const CHAR *name)
{
    processCB->processMode = mode;						//用户态进程还是内核态进程
    processCB->processStatus = OS_PROCESS_STATUS_INIT;	//进程初始状态
    processCB->parentProcess = NULL;
    processCB->threadGroup = NULL;
    processCB->umask = OS_PROCESS_DEFAULT_UMASK;		//掩码
    processCB->timerID = (timer_t)(UINTPTR)MAX_INVALID_TIMER_VID;

    LOS_ListInit(&processCB->threadSiblingList);//初始化孩子任务/线程链表，上面挂的都是由此fork的孩子线程 见于 OsTaskCBInit LOS_ListTailInsert(&(processCB->threadSiblingList), &(taskCB->threadList));
    LOS_ListInit(&processCB->childrenList);		//初始化孩子进程链表，上面挂的都是由此fork的孩子进程 见于 OsCopyParent LOS_ListTailInsert(&parentProcessCB->childrenList, &childProcessCB->siblingList);
    LOS_ListInit(&processCB->exitChildList);	//初始化记录退出孩子进程链表，上面挂的是哪些exit	见于 OsProcessNaturalExit LOS_ListTailInsert(&parentCB->exitChildList, &processCB->siblingList);
    LOS_ListInit(&(processCB->waitList));		//初始化等待任务链表 上面挂的是处于等待的 见于 OsWaitInsertWaitLIstInOrder LOS_ListHeadInsert(&processCB->waitList, &runTask->pendList);

#ifdef LOSCFG_KERNEL_VM	
    if (OsProcessIsUserMode(processCB)) {//如果是用户态进程
        processCB->vmSpace = OsCreateUserVmSpace();//创建用户空间
        if (processCB->vmSpace == NULL) {
            processCB->processStatus = OS_PROCESS_FLAG_UNUSED;
            return LOS_ENOMEM;
        }
    } else {
        processCB->vmSpace = LOS_GetKVmSpace();//从这里也可以看出,所有内核态进程是共享一个进程空间的
    }//在鸿蒙内核态进程只有kprocess 和 kidle 两个 
#endif

#ifdef LOSCFG_KERNEL_CPUP
    processCB->processCpup = (OsCpupBase *)LOS_MemAlloc(m_aucSysMem1, sizeof(OsCpupBase));
    if (processCB->processCpup == NULL) {
        return LOS_ENOMEM;
    }
    (VOID)memset_s(processCB->processCpup, sizeof(OsCpupBase), 0, sizeof(OsCpupBase));
#endif
#ifdef LOSCFG_SECURITY_VID
    status_t status = VidMapListInit(processCB);
    if (status != LOS_OK) {
        return LOS_ENOMEM;
    }
#endif
#ifdef LOSCFG_SECURITY_CAPABILITY
    OsInitCapability(processCB);//初始化进程安全相关功能
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

/*! 检查参数群组ID是否在当前用户所属群组中*/
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

/*! 获取当前进程的用户ID*/
LITE_OS_SEC_TEXT INT32 LOS_GetUserID(VOID)
{
#ifdef LOSCFG_SECURITY_CAPABILITY
    UINT32 intSave;
    INT32 uid;

    SCHEDULER_LOCK(intSave);
#ifdef LOSCFG_USER_CONTAINER
    uid = OsFromKuidMunged(OsCurrentUserContainer(), CurrentCredentials()->uid);
#else
    uid = (INT32)OsCurrUserGet()->userID;
#endif
    SCHEDULER_UNLOCK(intSave);
    return uid;
#else
    return 0;
#endif
}

/*! 获取当前进程的用户组ID*/
LITE_OS_SEC_TEXT INT32 LOS_GetGroupID(VOID)
{
#ifdef LOSCFG_SECURITY_CAPABILITY
    UINT32 intSave;
    INT32 gid;

    SCHEDULER_LOCK(intSave);
#ifdef LOSCFG_USER_CONTAINER
    gid = OsFromKgidMunged(OsCurrentUserContainer(), CurrentCredentials()->gid);
#else
    gid = (INT32)OsCurrUserGet()->gid;
#endif
    SCHEDULER_UNLOCK(intSave);

    return gid;
#else
    return 0;
#endif
}

/*! 进程创建初始化*/
STATIC UINT32 OsSystemProcessInit(LosProcessCB *processCB, UINT32 flags, const CHAR *name)
{
    UINT32 ret = OsInitPCB(processCB, flags, name);
    if (ret != LOS_OK) {
        goto EXIT;
    }

#ifdef LOSCFG_FS_VFS
    processCB->files = alloc_files();//分配进程的文件的管理器
    if (processCB->files == NULL) {
        ret = LOS_ENOMEM;
        goto EXIT;
    }
#endif

    ProcessGroup *pgroup = OsCreateProcessGroup(processCB);
    if (pgroup == NULL) {
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

#ifdef LOSCFG_KERNEL_PLIMITS
    ret = OsPLimitsAddProcess(NULL, processCB);
    if (ret != LOS_OK) {
        ret = LOS_ENOMEM;
        goto EXIT;
    }
#endif
    return LOS_OK;

EXIT:
    OsDeInitPCB(processCB);//删除进程控制块,归还内存
    return ret;
}
/*! 创建2,0号进程,即内核态进程的老祖宗*/
LITE_OS_SEC_TEXT_INIT UINT32 OsSystemProcessCreate(VOID)
{
    LosProcessCB *kerInitProcess = OsGetKernelInitProcess();
    UINT32 ret = OsSystemProcessInit(kerInitProcess, OS_KERNEL_MODE, "KProcess");
    if (ret != LOS_OK) {
        return ret;
    }
    kerInitProcess->processStatus &= ~OS_PROCESS_STATUS_INIT;//去掉初始化标签

    LosProcessCB *idleProcess = OsGetIdleProcess();
    ret = OsInitPCB(idleProcess, OS_KERNEL_MODE, "KIdle");//创建内核态0号进程
    if (ret != LOS_OK) {
        return ret;
    }
    idleProcess->parentProcess = kerInitProcess;
    LOS_ListTailInsert(&kerInitProcess->childrenList, &idleProcess->siblingList);//挂到内核态祖宗进程的子孙链接上
    idleProcess->pgroup = kerInitProcess->pgroup;
    LOS_ListTailInsert(&kerInitProcess->pgroup->processList, &idleProcess->subordinateGroupList);
#ifdef LOSCFG_SECURITY_CAPABILITY
    idleProcess->user = kerInitProcess->user;//共享用户
#endif
#ifdef LOSCFG_FS_VFS
    idleProcess->files = kerInitProcess->files;//共享文件
#endif
    idleProcess->processStatus &= ~OS_PROCESS_STATUS_INIT;

    ret = OsIdleTaskCreate((UINTPTR)idleProcess);
    if (ret != LOS_OK) {
        return ret;
    }
    return LOS_OK;
}
/// 进程调度参数检查
INT32 OsSchedulerParamCheck(UINT16 policy, BOOL isThread, const LosSchedParam *param)
{
    if (param == NULL) {
        return LOS_EINVAL;
    }

    if ((policy == LOS_SCHED_RR) || (isThread && (policy == LOS_SCHED_FIFO))) {
        if ((param->priority < OS_PROCESS_PRIORITY_HIGHEST) ||
            (param->priority > OS_PROCESS_PRIORITY_LOWEST)) {
            return LOS_EINVAL;
        }
        return LOS_OK;
    }

    if (policy == LOS_SCHED_DEADLINE) {
        if ((param->runTimeUs < OS_SCHED_EDF_MIN_RUNTIME) || (param->runTimeUs >= param->deadlineUs)) {
            return LOS_EINVAL;
        }
        if ((param->deadlineUs < OS_SCHED_EDF_MIN_DEADLINE) || (param->deadlineUs > OS_SCHED_EDF_MAX_DEADLINE)) {
            return LOS_EINVAL;
        }
        if (param->periodUs < param->deadlineUs) {
            return LOS_EINVAL;
        }
        return LOS_OK;
    }

    return LOS_EINVAL;
}

STATIC INLINE INT32 ProcessSchedulerParamCheck(INT32 which, INT32 pid, UINT16 policy, const LosSchedParam *param)
{
    if (OS_PID_CHECK_INVALID(pid)) {//进程ID是否有效，默认 g_processMaxNum = 64
        return LOS_EINVAL;
    }

    if (which != LOS_PRIO_PROCESS) {//进程标识
        return LOS_EINVAL;
    }

    return OsSchedulerParamCheck(policy, FALSE, param);
}

#ifdef LOSCFG_SECURITY_CAPABILITY //检查进程的安全许可证
STATIC BOOL OsProcessCapPermitCheck(const LosProcessCB *processCB, const SchedParam *param, UINT16 policy, UINT16 prio)
{
    LosProcessCB *runProcess = OsCurrProcessGet();//获得当前进程

    /* always trust kernel process */
    if (!OsProcessIsUserMode(runProcess)) {//进程必须在内核模式下,也就是说在内核模式下是安全的.
        return TRUE;
    }

    /* user mode process can reduce the priority of itself */
    if ((runProcess->processID == processCB->processID) && (policy == LOS_SCHED_RR) && (prio > param->basePrio)) {
        return TRUE;
    }

    /* user mode process with privilege of CAP_SCHED_SETPRIORITY can change the priority */
    if (IsCapPermit(CAP_SCHED_SETPRIORITY)) {
        return TRUE;
    }
    return FALSE;
}
#endif
/// 设置进程调度计划
LITE_OS_SEC_TEXT INT32 OsSetProcessScheduler(INT32 which, INT32 pid, UINT16 policy, const LosSchedParam *schedParam)
{
    SchedParam param = { 0 };
    BOOL needSched = FALSE;
    UINT32 intSave;

    INT32 ret = ProcessSchedulerParamCheck(which, pid, policy, schedParam);
    if (ret != LOS_OK) {
        return -ret;
    }

    LosProcessCB *processCB = OS_PCB_FROM_PID(pid);
    SCHEDULER_LOCK(intSave);//持有调度自旋锁,多CPU情况下调度期间需要原子处理
    if (OsProcessIsInactive(processCB)) {//进程未活动的处理
        SCHEDULER_UNLOCK(intSave);
        return -LOS_ESRCH;
    }

    LosTaskCB *taskCB = processCB->threadGroup;
    taskCB->ops->schedParamGet(taskCB, &param);

#ifdef LOSCFG_SECURITY_CAPABILITY
    if (!OsProcessCapPermitCheck(processCB, &param, policy, schedParam->priority)) {
        SCHEDULER_UNLOCK(intSave);
        return -LOS_EPERM;
    }
#endif

    if (param.policy != policy) {
        if (policy == LOS_SCHED_DEADLINE) { /* HPF -> EDF */
            if (processCB->threadNumber > 1) {
                SCHEDULER_UNLOCK(intSave);
                return -LOS_EPERM;
            }
            OsSchedParamInit(taskCB, policy, NULL, schedParam);
            needSched = TRUE;
            goto TO_SCHED;
        } else if (param.policy == LOS_SCHED_DEADLINE) { /* EDF -> HPF */
            SCHEDULER_UNLOCK(intSave);
            return -LOS_EPERM;
        }
    }

    if (policy == LOS_SCHED_DEADLINE) {
        param.runTimeUs = schedParam->runTimeUs;
        param.deadlineUs = schedParam->deadlineUs;
        param.periodUs = schedParam->periodUs;
    } else {
        param.basePrio = schedParam->priority;
    }
    needSched = taskCB->ops->schedParamModify(taskCB, &param);

TO_SCHED:
    SCHEDULER_UNLOCK(intSave);//还锁

    LOS_MpSchedule(OS_MP_CPU_ALL);//核间中断
    if (needSched && OS_SCHEDULER_ACTIVE) {
        LOS_Schedule();//发起调度
    }
    return LOS_OK;
}
/// 设置指定进程的调度参数，包括优先级和调度策略
LITE_OS_SEC_TEXT INT32 LOS_SetProcessScheduler(INT32 pid, UINT16 policy, const LosSchedParam *schedParam)
{
    return OsSetProcessScheduler(LOS_PRIO_PROCESS, pid, policy, schedParam);
}
/// 获得指定进程的调度策略
LITE_OS_SEC_TEXT INT32 LOS_GetProcessScheduler(INT32 pid, INT32 *policy, LosSchedParam *schedParam)
{
    UINT32 intSave;
    SchedParam param = { 0 };

    if (OS_PID_CHECK_INVALID(pid)) {
        return -LOS_EINVAL;
    }

    if ((policy == NULL) && (schedParam == NULL)) {
        return -LOS_EINVAL;
    }

    SCHEDULER_LOCK(intSave);
    LosProcessCB *processCB = OS_PCB_FROM_PID(pid);
    if (OsProcessIsUnused(processCB)) {
        SCHEDULER_UNLOCK(intSave);
        return -LOS_ESRCH;
    }

    LosTaskCB *taskCB = processCB->threadGroup;
    taskCB->ops->schedParamGet(taskCB, &param);
    SCHEDULER_UNLOCK(intSave);

    if (policy != NULL) {
        if (param.policy == LOS_SCHED_FIFO) {
            *policy = LOS_SCHED_RR;
        } else {
            *policy = param.policy;
        }
    }

    if (schedParam != NULL) {
        if (param.policy == LOS_SCHED_DEADLINE) {
            schedParam->runTimeUs = param.runTimeUs;
            schedParam->deadlineUs = param.deadlineUs;
            schedParam->periodUs = param.periodUs;
        } else {
            schedParam->priority = param.basePrio;
        }
    }
    return LOS_OK;
}
/// 接口封装 - 设置进程优先级
LITE_OS_SEC_TEXT INT32 LOS_SetProcessPriority(INT32 pid, INT32 prio)
{
    INT32 ret;
    INT32 policy;
    LosSchedParam param = {
        .priority = prio,
    };

    ret = LOS_GetProcessScheduler(pid, &policy, NULL);
    if (ret != LOS_OK) {
        return ret;
    }

    if (policy == LOS_SCHED_DEADLINE) {
        return -LOS_EINVAL;
    }

    return OsSetProcessScheduler(LOS_PRIO_PROCESS, pid, (UINT16)policy, &param);
}
/// 接口封装 - 获取进程优先级 which:标识进程,进程组,用户
LITE_OS_SEC_TEXT INT32 OsGetProcessPriority(INT32 which, INT32 pid)
{
    UINT32 intSave;
    SchedParam param = { 0 };
    (VOID)which;

    if (OS_PID_CHECK_INVALID(pid)) {
        return -LOS_EINVAL;
    }

    if (which != LOS_PRIO_PROCESS) {
        return -LOS_EINVAL;
    }

    LosProcessCB *processCB = OS_PCB_FROM_PID(pid);
    SCHEDULER_LOCK(intSave);
    if (OsProcessIsUnused(processCB)) {
        SCHEDULER_UNLOCK(intSave);
        return -LOS_ESRCH;
    }

    LosTaskCB *taskCB = processCB->threadGroup;
    taskCB->ops->schedParamGet(taskCB, &param);

    if (param.policy == LOS_SCHED_DEADLINE) {
        SCHEDULER_UNLOCK(intSave);
        return -LOS_EINVAL;
    }

    SCHEDULER_UNLOCK(intSave);
    return param.basePrio;
}
/// 接口封装 - 获取指定进程优先级
LITE_OS_SEC_TEXT INT32 LOS_GetProcessPriority(INT32 pid)
{
    return OsGetProcessPriority(LOS_PRIO_PROCESS, pid);
}

/*! 
* 将任务挂入进程的waitList链表,表示这个任务在等待某个进程的退出
* 当被等待进程退出时候会将自己挂到父进程的退出子进程链表和进程组的退出进程链表.
*/
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
    (VOID)runTask->ops->wait(runTask, list->pstNext, LOS_WAIT_FOREVER);
    return;
}
/// 设置等待子进程退出方式方法
STATIC UINT32 WaitFindSpecifiedProcess(UINT32 pid, LosTaskCB *runTask,
                                       const LosProcessCB *processCB, LosProcessCB **childCB)
{
    if (OS_PID_CHECK_INVALID((UINT32)pid)) {
        return LOS_ECHILD;
    }

    LosProcessCB *waitProcess = OS_PCB_FROM_PID(pid);
    if (OsProcessIsUnused(waitProcess)) {
        return LOS_ECHILD;
    }

#ifdef LOSCFG_PID_CONTAINER
    if (OsPidContainerProcessParentIsRealParent(waitProcess, processCB)) {
        *childCB = (LosProcessCB *)processCB;
        return LOS_OK;
    }
#endif
    /* Wait for the child process whose process number is pid. */
    *childCB = OsFindExitChildProcess(processCB, waitProcess);
        if (*childCB != NULL) {//找到了,确实有一个已经退出的PID,注意一个进程退出时会挂到父进程的exitChildList上
        return LOS_OK;
        }

        if (OsFindChildProcess(processCB, waitProcess) != LOS_OK) {
            return LOS_ECHILD;
        }
        runTask->waitFlag = OS_PROCESS_WAIT_PRO;//设置当前任务的等待类型
        runTask->waitID = (UINTPTR)waitProcess;
    return LOS_OK;
}

STATIC UINT32 OsWaitSetFlag(const LosProcessCB *processCB, INT32 pid, LosProcessCB **child)
{
    UINT32 ret;
    LosProcessCB *childCB = NULL;
    LosTaskCB *runTask = OsCurrTaskGet();

    if (pid > 0) {
        ret = WaitFindSpecifiedProcess((UINT32)pid, runTask, processCB, &childCB);
        if (ret != LOS_OK) {
            return ret;
        }
        if (childCB != NULL) {
            goto WAIT_BACK;
        }
    } else if (pid == 0) {//等待同一进程组中的任何子进程
        /* Wait for any child process in the same process group */
        childCB = OsFindGroupExitProcess(processCB->pgroup, OS_INVALID_VALUE);
        if (childCB != NULL) {//找到了,确实有一个已经退出的PID
            goto WAIT_BACK;//直接成功返回
        }
        runTask->waitID = (UINTPTR)OS_GET_PGROUP_LEADER(processCB->pgroup);
        runTask->waitFlag = OS_PROCESS_WAIT_GID;//设置当前任务的等待类型
    } else if (pid == -1) {//等待任意子进程
        /* Wait for any child process */
        childCB = OsFindExitChildProcess(processCB, NULL);
        if (childCB != NULL) {//找到了,确实有一个已经退出的PID
            goto WAIT_BACK;
        }
        runTask->waitID = pid;//等待PID,这个PID可以和当前进程没有任何关系
        runTask->waitFlag = OS_PROCESS_WAIT_ANY;//设置当前任务的等待类型
    } else { /* pid < -1 */ //等待指定进程组内为|pid|的所有子进程
        /* Wait for any child process whose group number is the pid absolute value. */
        ProcessGroup *pgroup = OsFindProcessGroup(-pid);
        if (pgroup == NULL) {
            return LOS_ECHILD;
        }

        childCB = OsFindGroupExitProcess(pgroup, OS_INVALID_VALUE);
        if (childCB != NULL) {
            goto WAIT_BACK;
        }

        runTask->waitID = (UINTPTR)OS_GET_PGROUP_LEADER(pgroup);
        runTask->waitFlag = OS_PROCESS_WAIT_GID;//设置当前任务的等待类型
    }

WAIT_BACK:
    *child = childCB;
    return LOS_OK;
}
/// 等待回收孩子进程 @note_thinking 这样写Porcess不太好吧
STATIC UINT32 OsWaitRecycleChildProcess(const LosProcessCB *childCB, UINT32 intSave, INT32 *status, siginfo_t *info)
{
    ProcessGroup *pgroup = NULL;
    UINT32 pid  = OsGetPid(childCB);
    UINT16 mode = childCB->processMode;
    INT32 exitCode = childCB->exitCode;
    UINT32 uid = 0;

#ifdef LOSCFG_SECURITY_CAPABILITY
    if (childCB->user != NULL) {
        uid = childCB->user->userID;
    }
#endif

    OsRecycleZombiesProcess((LosProcessCB *)childCB, &pgroup);
    SCHEDULER_UNLOCK(intSave);

    if (status != NULL) {
        if (mode == OS_USER_MODE) {//孩子为用户态进程
            (VOID)LOS_ArchCopyToUser((VOID *)status, (const VOID *)(&(exitCode)), sizeof(INT32));//从内核空间拷贝退出码
        } else {
            *status = exitCode;
        }
    }
    /* get signal info */
    if (info != NULL) {
        siginfo_t tempinfo = { 0 };

        tempinfo.si_signo = SIGCHLD;
        tempinfo.si_errno = 0;
        tempinfo.si_pid = pid;
        tempinfo.si_uid = uid;
        /*
         * Process exit code
         * 31	 15 		  8 		  7 	   0
         * |	 | exit code  | core dump | signal |
         */
        if ((exitCode & 0x7f) == 0) {
            tempinfo.si_code = CLD_EXITED;
            tempinfo.si_status = (exitCode >> 8U);
        } else {
            tempinfo.si_code = (exitCode & 0x80) ? CLD_DUMPED : CLD_KILLED;
            tempinfo.si_status = (exitCode & 0x7f);
        }

        if (mode == OS_USER_MODE) {
            (VOID)LOS_ArchCopyToUser((VOID *)(info), (const VOID *)(&(tempinfo)), sizeof(siginfo_t));
        } else {
            (VOID)memcpy_s((VOID *)(info), sizeof(siginfo_t), (const VOID *)(&(tempinfo)), sizeof(siginfo_t));
        }
    }
    (VOID)LOS_MemFree(m_aucSysMem1, pgroup);
    return pid;
}
/// 检查要等待的孩子进程
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
///等待子进程结束并回收子进程,返回已经终止的子进程的进程ID号，并清除僵死进程。
STATIC INT32 OsWait(INT32 pid, USER INT32 *status, USER siginfo_t *info, UINT32 options, VOID *rusage)
{
    (VOID)rusage;
    UINT32 ret;
    UINT32 intSave;
    LosProcessCB *childCB = NULL;

    LosProcessCB *processCB = OsCurrProcessGet();
    LosTaskCB *runTask = OsCurrTaskGet();
    SCHEDULER_LOCK(intSave);
    ret = OsWaitChildProcessCheck(processCB, pid, &childCB);//先检查下看能不能找到参数要求的退出子进程
    if (ret != LOS_OK) {
        pid = -ret;
        goto ERROR;
    }

    if (childCB != NULL) {//找到了进程
#ifdef LOSCFG_PID_CONTAINER
        if (childCB == processCB) {
            SCHEDULER_UNLOCK(intSave);
            if (status != NULL) {
                (VOID)LOS_ArchCopyToUser((VOID *)status, (const VOID *)(&ret), sizeof(INT32));
            }
            return pid;
        }
#endif
        return (INT32)OsWaitRecycleChildProcess(childCB, intSave, status, info);
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

    childCB = (LosProcessCB *)runTask->waitID;
    if (!OsProcessIsDead(childCB)) {
        pid = -LOS_ESRCH;
        goto ERROR;
    }
	//回收僵死进程
    return (INT32)OsWaitRecycleChildProcess(childCB, intSave, status, info);

ERROR:
    SCHEDULER_UNLOCK(intSave);
    return pid;
}

LITE_OS_SEC_TEXT INT32 LOS_Wait(INT32 pid, USER INT32 *status, UINT32 options, VOID *rusage)
{
    (VOID)rusage;
    UINT32 ret;

    ret = OsWaitOptionsCheck(options);
    if (ret != LOS_OK) {
        return -ret;
    }

    return OsWait(pid, status, NULL, options, NULL);
}

STATIC UINT32 OsWaitidOptionsCheck(UINT32 options)
{
    UINT32 flag = LOS_WAIT_WNOHANG | LOS_WAIT_WSTOPPED | LOS_WAIT_WCONTINUED | LOS_WAIT_WEXITED | LOS_WAIT_WNOWAIT;

    flag = ~flag & options;
    if ((flag != 0) || (options == 0)) {
        return LOS_EINVAL;
    }

    /*
     * only support LOS_WAIT_WNOHANG | LOS_WAIT_WEXITED
     * notsupport LOS_WAIT_WSTOPPED | LOS_WAIT_WCONTINUED | LOS_WAIT_WNOWAIT
     */
    if ((options & (LOS_WAIT_WSTOPPED | LOS_WAIT_WCONTINUED | LOS_WAIT_WNOWAIT)) != 0) {
        return LOS_EOPNOTSUPP;
    }

    if (OS_INT_ACTIVE) {
        return LOS_EINTR;
    }

    return LOS_OK;
}

LITE_OS_SEC_TEXT INT32 LOS_Waitid(INT32 pid, USER siginfo_t *info, UINT32 options, VOID *rusage)
{
    (VOID)rusage;
    UINT32 ret;

    /* check options value */
    ret = OsWaitidOptionsCheck(options);
    if (ret != LOS_OK) {
        return -ret;
    }

    return OsWait(pid, NULL, info, options, NULL);
}

UINT32 OsGetProcessGroupCB(UINT32 pid, UINTPTR *ppgroupLeader)
{
    UINT32 intSave;

    if (OS_PID_CHECK_INVALID(pid) || (ppgroupLeader == NULL)) {
        return LOS_EINVAL;
    }

    SCHEDULER_LOCK(intSave);
    LosProcessCB *processCB = OS_PCB_FROM_PID(pid);
    if (OsProcessIsUnused(processCB)) {
        SCHEDULER_UNLOCK(intSave);
        return LOS_ESRCH;
    }

    *ppgroupLeader = (UINTPTR)OS_GET_PGROUP_LEADER(processCB->pgroup);
    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;
}

STATIC UINT32 OsSetProcessGroupCheck(const LosProcessCB *processCB, LosProcessCB *pgroupCB)
{
    LosProcessCB *runProcessCB = OsCurrProcessGet();//拿到当前运行进程

    if (OsProcessIsInactive(processCB)) {//进程是否活动
        return LOS_ESRCH;
    }
	//参数进程不在用户态或者组长不在用户态
#ifdef LOSCFG_PID_CONTAINER
    if ((processCB->processID == OS_USER_ROOT_PROCESS_ID) || OS_PROCESS_CONTAINER_CHECK(processCB, runProcessCB)) {
        return LOS_EPERM;
    }
#endif

    if (!OsProcessIsUserMode(processCB) || !OsProcessIsUserMode(pgroupCB)) {
        return LOS_EPERM;
    }

    if (runProcessCB == processCB->parentProcess) {
        if (processCB->processStatus & OS_PROCESS_FLAG_ALREADY_EXEC) {
            return LOS_EACCES;
        }
    } else if (processCB->processID != runProcessCB->processID) {
        return LOS_ESRCH;
    }

    /* Add the process to another existing process group */
    if (processCB != pgroupCB) {
        if (!OsProcessIsPGroupLeader(pgroupCB)) {
            return LOS_EPERM;
        }

        if ((pgroupCB->parentProcess != processCB->parentProcess) && (pgroupCB != processCB->parentProcess)) {
            return LOS_EPERM;
        }
    }

    return LOS_OK;
}

STATIC UINT32 OsSetProcessGroupIDUnsafe(UINT32 pid, UINT32 gid, ProcessGroup **pgroup)
{
    LosProcessCB *processCB = OS_PCB_FROM_PID(pid);
    ProcessGroup *rootPGroup = OS_ROOT_PGRP(OsCurrProcessGet());
    LosProcessCB *pgroupCB = OS_PCB_FROM_PID(gid);
    UINT32 ret = OsSetProcessGroupCheck(processCB, pgroupCB);
    if (ret != LOS_OK) {
        return ret;
    }

    if (OS_GET_PGROUP_LEADER(processCB->pgroup) == pgroupCB) {
        return LOS_OK;
    }

    ProcessGroup *oldPGroup = processCB->pgroup;
    ExitProcessGroup(processCB, pgroup);

    ProcessGroup *newPGroup = OsFindProcessGroup(gid);
    if (newPGroup != NULL) {
        LOS_ListTailInsert(&newPGroup->processList, &processCB->subordinateGroupList);
        processCB->pgroup = newPGroup;
        return LOS_OK;
    }

    newPGroup = OsCreateProcessGroup(pgroupCB);
    if (newPGroup == NULL) {
        LOS_ListTailInsert(&oldPGroup->processList, &processCB->subordinateGroupList);
        processCB->pgroup = oldPGroup;
        if (*pgroup != NULL) {
            LOS_ListTailInsert(&rootPGroup->groupList, &oldPGroup->groupList);
            processCB = OS_GET_PGROUP_LEADER(oldPGroup);
            processCB->processStatus |= OS_PROCESS_FLAG_GROUP_LEADER;
            *pgroup = NULL;
        }
        return LOS_EPERM;
    }
    return LOS_OK;
}

LITE_OS_SEC_TEXT INT32 OsSetProcessGroupID(UINT32 pid, UINT32 gid)
{
    ProcessGroup *pgroup = NULL;
    UINT32 ret;
    UINT32 intSave;

    if ((OS_PID_CHECK_INVALID(pid)) || (OS_PID_CHECK_INVALID(gid))) {
        return -LOS_EINVAL;
    }

    SCHEDULER_LOCK(intSave);
    ret = OsSetProcessGroupIDUnsafe(pid, gid, &pgroup);
    SCHEDULER_UNLOCK(intSave);
    (VOID)LOS_MemFree(m_aucSysMem1, pgroup);
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

    if (OS_PID_CHECK_INVALID(pid)) {
        return -LOS_EINVAL;
    }

    SCHEDULER_LOCK(intSave);
    LosProcessCB *processCB = OS_PCB_FROM_PID(pid);
    if (OsProcessIsUnused(processCB)) {
        gid = -LOS_ESRCH;
        goto EXIT;
    }

    processCB = OS_GET_PGROUP_LEADER(processCB->pgroup);
    gid = (INT32)processCB->processID;

EXIT:
    SCHEDULER_UNLOCK(intSave);
    return gid;
}
/// 获取当前进程的组ID
LITE_OS_SEC_TEXT INT32 LOS_GetCurrProcessGroupID(VOID)
{
    return LOS_GetProcessGroupID(OsCurrProcessGet()->processID);
}
/// 为用户态任务分配栈空间
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

#ifdef LOSCFG_KERNEL_DYNLOAD
LITE_OS_SEC_TEXT VOID OsExecProcessVmSpaceRestore(LosVmSpace *oldSpace)
{
    LosProcessCB *processCB = OsCurrProcessGet();
    LosTaskCB *runTask = OsCurrTaskGet();

    processCB->vmSpace = oldSpace;
    runTask->archMmu = (UINTPTR)&processCB->vmSpace->archMmu;
    LOS_ArchMmuContextSwitch((LosArchMmu *)runTask->archMmu);
}

LITE_OS_SEC_TEXT LosVmSpace *OsExecProcessVmSpaceReplace(LosVmSpace *newSpace, UINTPTR stackBase, INT32 randomDevFD)
{
    LosProcessCB *processCB = OsCurrProcessGet();
    LosTaskCB *runTask = OsCurrTaskGet();

    OsProcessThreadGroupDestroy();
    OsTaskCBRecycleToFree();

    LosVmSpace *oldSpace = processCB->vmSpace;
    processCB->vmSpace = newSpace;
    processCB->vmSpace->heapBase += OsGetRndOffset(randomDevFD);
    processCB->vmSpace->heapNow = processCB->vmSpace->heapBase;
    processCB->vmSpace->mapBase += OsGetRndOffset(randomDevFD);
    processCB->vmSpace->mapSize = stackBase - processCB->vmSpace->mapBase;
    runTask->archMmu = (UINTPTR)&processCB->vmSpace->archMmu;
    LOS_ArchMmuContextSwitch((LosArchMmu *)runTask->archMmu);
    return oldSpace;
}

/**
 * @brief 进程的回收再利用,被LOS_DoExecveFile调用
 * @param processCB 
 * @param name 
 * @param oldSpace 
 * @param oldFiles 
 * @return LITE_OS_SEC_TEXT 
 */
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

#ifdef LOSCFG_KERNEL_LITEIPC
    (VOID)LiteIpcPoolDestroy(processCB->processID);
#endif

    processCB->sigHandler = 0;
    OsCurrTaskGet()->sig.sigprocmask = 0;

    LOS_VmSpaceFree(oldSpace);
#ifdef LOSCFG_FS_VFS
    CloseOnExec((struct files_struct *)oldFiles);
    delete_files_snapshot((struct files_struct *)oldFiles);
#endif

#ifdef LOSCFG_BASE_CORE_SWTMR_ENABLE
    OsSwtmrRecycle((UINTPTR)processCB);
    processCB->timerID = (timer_t)(UINTPTR)MAX_INVALID_TIMER_VID;
#endif

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
/// 执行用户态任务, entry为入口函数 ,其中 创建好task,task上下文 等待调度真正执行, sp:栈指针 mapBase:栈底 mapSize:栈大小
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
#endif
/// 用户进程开始初始化
STATIC UINT32 OsUserInitProcessStart(LosProcessCB *processCB, TSK_INIT_PARAM_S *param)
{
    UINT32 intSave;
    INT32 ret;

    UINT32 taskID = OsCreateUserTask((UINTPTR)processCB, param);
    if (taskID == OS_INVALID_VALUE) {
        return LOS_NOK;
    }

    ret = LOS_SetProcessPriority(processCB->processID, OS_PROCESS_USERINIT_PRIORITY);
    if (ret != LOS_OK) {
        PRINT_ERR("User init process set priority failed! ERROR:%d \n", ret);
        goto EXIT;
    }

    SCHEDULER_LOCK(intSave);
    processCB->processStatus &= ~OS_PROCESS_STATUS_INIT;
    SCHEDULER_UNLOCK(intSave);

    ret = LOS_SetTaskScheduler(taskID, LOS_SCHED_RR, OS_TASK_PRIORITY_LOWEST);//调度器:设置为抢占式调度和最低任务优先级(31级)
    if (ret != LOS_OK) {
        PRINT_ERR("User init process set scheduler failed! ERROR:%d \n", ret);
        goto EXIT;
    }

    return LOS_OK;

EXIT:
    (VOID)LOS_TaskDelete(taskID);
    return ret;
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

    LosProcessCB *processCB = OsGetUserInitProcess();
    ret = OsSystemProcessInit(processCB, OS_USER_MODE, "Init");
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
    ret = OsUserInitProcessStart(processCB, &param);
    if (ret != LOS_OK) {
        (VOID)OsUnMMap(processCB->vmSpace, param.userParam.userMapBase, param.userParam.userMapSize);
        goto ERROR;
    }

    return LOS_OK;

ERROR:
    OsDeInitPCB(processCB);
    return ret;
}
/// 拷贝用户信息 直接用memcpy_s
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

//拷贝一个Task过程
STATIC VOID GetCopyTaskParam(LosProcessCB *childProcessCB, UINTPTR entry, UINT32 size,
                             TSK_INIT_PARAM_S *taskParam, SchedParam *param)
{
    UINT32 intSave;
    LosTaskCB *runTask = OsCurrTaskGet();

    SCHEDULER_LOCK(intSave);
    if (OsProcessIsUserMode(childProcessCB)) {//用户态进程
        taskParam->pfnTaskEntry = runTask->taskEntry;//拷贝当前任务入口地址
        taskParam->uwStackSize = runTask->stackSize;	//栈空间大小
        taskParam->userParam.userArea = runTask->userArea;		//用户态栈区栈顶位置
        taskParam->userParam.userMapBase = runTask->userMapBase;	//用户态栈底
        taskParam->userParam.userMapSize = runTask->userMapSize;	//用户态栈大小
    } else {//注意内核态进程创建任务的入口由外界指定,例如 OsCreateIdleProcess 指定了OsIdleTask
        taskParam->pfnTaskEntry = (TSK_ENTRY_FUNC)entry;//参数(sp)为内核态入口地址
        taskParam->uwStackSize = size;//参数(size)为内核态栈大小
    }
    if (runTask->taskStatus & OS_TASK_FLAG_PTHREAD_JOIN) {
        taskParam->uwResved = LOS_TASK_ATTR_JOINABLE;
    }

    runTask->ops->schedParamGet(runTask, param);
    SCHEDULER_UNLOCK(intSave);

    taskParam->policy = param->policy;
    taskParam->runTimeUs = param->runTimeUs;
    taskParam->deadlineUs = param->deadlineUs;
    taskParam->periodUs = param->periodUs;
    taskParam->usTaskPrio = param->priority;
    taskParam->processID = (UINTPTR)childProcessCB;
}

STATIC UINT32 OsCopyTask(UINT32 flags, LosProcessCB *childProcessCB, const CHAR *name, UINTPTR entry, UINT32 size)
{
    LosTaskCB *runTask = OsCurrTaskGet();
    TSK_INIT_PARAM_S taskParam = { 0 };
    UINT32 ret, taskID, intSave;
    SchedParam param = { 0 };

    taskParam.pcName = (CHAR *)name;
    GetCopyTaskParam(childProcessCB, entry, size, &taskParam, &param);

    ret = LOS_TaskCreateOnly(&taskID, &taskParam);
    if (ret != LOS_OK) {
        if (ret == LOS_ERRNO_TSK_TCB_UNAVAILABLE) {
            return LOS_EAGAIN;
        }
        return LOS_ENOMEM;
    }

    LosTaskCB *childTaskCB = childProcessCB->threadGroup;
    childTaskCB->taskStatus = runTask->taskStatus;//任务状态先同步,注意这里是赋值操作. ...01101001 
    childTaskCB->ops->schedParamModify(childTaskCB, &param);
    if (childTaskCB->taskStatus & OS_TASK_STATUS_RUNNING) {//因只能有一个运行的task,所以如果一样要改4号位
        childTaskCB->taskStatus &= ~OS_TASK_STATUS_RUNNING;//将四号位清0 ,变成 ...01100001 
    } else {//非运行状态下会发生什么?
        if (OS_SCHEDULER_ACTIVE) {//克隆线程发生错误未运行
            LOS_Panic("Clone thread status not running error status: 0x%x\n", childTaskCB->taskStatus);
        }
        childTaskCB->taskStatus &= ~OS_TASK_STATUS_UNUSED;//干净的Task
    }

    if (OsProcessIsUserMode(childProcessCB)) {//是否是用户进程
        SCHEDULER_LOCK(intSave);
        OsUserCloneParentStack(childTaskCB->stackPointer, entry, runTask->topOfStack, runTask->stackSize);
        SCHEDULER_UNLOCK(intSave);
    }
    return LOS_OK;
}
//拷贝父亲大人的遗传基因信息
STATIC UINT32 OsCopyParent(UINT32 flags, LosProcessCB *childProcessCB, LosProcessCB *runProcessCB)
{
    UINT32 intSave;
    LosProcessCB *parentProcessCB = NULL;

    SCHEDULER_LOCK(intSave);
    if (childProcessCB->parentProcess == NULL) {
	    if (flags & CLONE_PARENT) { //这里指明 childProcessCB 和 runProcessCB 有同一个父亲，是兄弟关系
	        parentProcessCB = runProcessCB->parentProcess;
	    } else {
	        parentProcessCB = runProcessCB;         
	    }
	        childProcessCB->parentProcess = parentProcessCB;//指认父亲，这个赋值代表从此是你儿了
	        LOS_ListTailInsert(&parentProcessCB->childrenList, &childProcessCB->siblingList);//通过我的兄弟姐妹节点，挂到父亲的孩子链表上，于我而言，父亲的这个链表上挂的都是我的兄弟姐妹
	        													//不会被排序，老大，老二，老三 老天爷指定了。
    }
    if (childProcessCB->pgroup == NULL) {
        childProcessCB->pgroup = parentProcessCB->pgroup;
        LOS_ListTailInsert(&parentProcessCB->pgroup->processList, &childProcessCB->subordinateGroupList);
    }
    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;
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

    status = LOS_VmSpaceClone(flags, runProcessCB->vmSpace, childProcessCB->vmSpace);//虚拟空间clone
    if (status != LOS_OK) {
        return LOS_ENOMEM;
    }
    return LOS_OK;
}
/// 拷贝进程文件描述符(proc_fd)信息
STATIC UINT32 OsCopyFile(UINT32 flags, LosProcessCB *childProcessCB, LosProcessCB *runProcessCB)
{
#ifdef LOSCFG_FS_VFS
    if (flags & CLONE_FILES) {
        childProcessCB->files = runProcessCB->files;
    } else {
#ifdef LOSCFG_IPC_CONTAINER
        if (flags & CLONE_NEWIPC) {
            OsCurrTaskGet()->cloneIpc = TRUE;
        }
#endif
        childProcessCB->files = dup_fd(runProcessCB->files);
#ifdef LOSCFG_IPC_CONTAINER
        OsCurrTaskGet()->cloneIpc = FALSE;
#endif
    }
    if (childProcessCB->files == NULL) {
        return LOS_ENOMEM;
    }

#ifdef LOSCFG_PROC_PROCESS_DIR
    INT32 ret = ProcCreateProcessDir(OsGetRootPid(childProcessCB), (UINTPTR)childProcessCB);
    if (ret < 0) {
        PRINT_ERR("ProcCreateProcessDir failed, pid = %u\n", childProcessCB->processID);
        return LOS_EBADF;
    }
#endif
#endif

    childProcessCB->consoleID = runProcessCB->consoleID;//控制台也是文件
    childProcessCB->umask = runProcessCB->umask;
    return LOS_OK;
}

STATIC UINT32 OsForkInitPCB(UINT32 flags, LosProcessCB *child, const CHAR *name, UINTPTR sp, UINT32 size)
{
    UINT32 ret;
    LosProcessCB *run = OsCurrProcessGet();//获取当前进程

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
    ProcessGroup *pgroup = NULL;

    SCHEDULER_LOCK(intSave);
    if ((UINTPTR)OS_GET_PGROUP_LEADER(run->pgroup) == OS_USER_PRIVILEGE_PROCESS_GROUP) {
        ret = OsSetProcessGroupIDUnsafe(child->processID, child->processID, &pgroup);
        if (ret != LOS_OK) {
            SCHEDULER_UNLOCK(intSave);
            return LOS_ENOMEM;
        }
    }

    child->processStatus &= ~OS_PROCESS_STATUS_INIT;
    LosTaskCB *taskCB = child->threadGroup;
    taskCB->ops->enqueue(OsSchedRunqueue(), taskCB);
    SCHEDULER_UNLOCK(intSave);

    (VOID)LOS_MemFree(m_aucSysMem1, pgroup);
    return LOS_OK;
}
/// 拷贝进程资源
STATIC UINT32 OsCopyProcessResources(UINT32 flags, LosProcessCB *child, LosProcessCB *run)
{
    UINT32 ret;

    ret = OsCopyUser(child, run);
    if (ret != LOS_OK) {
        return ret;
    }

    ret = OsCopyMM(flags, child, run);//拷贝虚拟空间
    if (ret != LOS_OK) {
        return ret;
    }

    ret = OsCopyFile(flags, child, run);//拷贝文件信息
    if (ret != LOS_OK) {
        return ret;
    }

#ifdef LOSCFG_KERNEL_LITEIPC
    if (run->ipcInfo != NULL) { //重新初始化IPC池
        child->ipcInfo = LiteIpcPoolReInit((const ProcIpcInfo *)(run->ipcInfo));//@note_good 将沿用用户态空间地址(即线性区地址)
        if (child->ipcInfo == NULL) {//因为整个进程虚拟空间都是拷贝的,ipc的用户态虚拟地址当然可以拷贝,但因进程不同了,所以需要重新申请ipc池和重新
            return LOS_ENOMEM;//映射池中两个地址.
        }
    }
#endif

#ifdef LOSCFG_SECURITY_CAPABILITY
    OsCopyCapability(run, child);//拷贝安全能力
#endif

    return LOS_OK;
}
/// 拷贝进程
STATIC INT32 OsCopyProcess(UINT32 flags, const CHAR *name, UINTPTR sp, UINT32 size)
{
    UINT32 ret, processID;
    LosProcessCB *run = OsCurrProcessGet();//获取当前进程

    LosProcessCB *child = OsGetFreePCB();//从进程池中申请一个进程控制块，鸿蒙进程池默认64
    if (child == NULL) {
        return -LOS_EAGAIN;
    }
    processID = child->processID;

    ret = OsInitPCB(child, run->processMode, name);
    if (ret != LOS_OK) {
        goto ERROR_INIT;
    }

#ifdef LOSCFG_KERNEL_CONTAINER
    ret = OsCopyContainers(flags, child, run, &processID);
    if (ret != LOS_OK) {
        goto ERROR_INIT;
    }
#ifdef LOSCFG_KERNEL_PLIMITS
    ret = OsPLimitsAddProcess(run->plimits, child);
    if (ret != LOS_OK) {
        goto ERROR_INIT;
    }
#endif
#endif
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
    (VOID)LOS_TaskDelete(child->threadGroup->taskID);
ERROR_INIT:
    OsDeInitPCB(child);
    return -ret;
}

/*!
 * @brief OsClone	进程克隆
 *
 * @param flags	
 * @param size	进程主任务内核栈大小
 * @param sp 进程主任务的入口函数	
 * @return	
 *
 * @see
 */
LITE_OS_SEC_TEXT INT32 OsClone(UINT32 flags, UINTPTR sp, UINT32 size)
{
    UINT32 cloneFlag = CLONE_PARENT | CLONE_THREAD | SIGCHLD;
#ifdef LOSCFG_KERNEL_CONTAINER
#ifdef LOSCFG_PID_CONTAINER
    cloneFlag |= CLONE_NEWPID;
    LosProcessCB *curr = OsCurrProcessGet();
    if (((flags & CLONE_NEWPID) != 0) && ((flags & (CLONE_PARENT | CLONE_THREAD)) != 0)) {
        return -LOS_EINVAL;
    }

    if (OS_PROCESS_PID_FOR_CONTAINER_CHECK(curr) && ((flags & CLONE_NEWPID) != 0)) {
        return -LOS_EINVAL;
    }

    if (OS_PROCESS_PID_FOR_CONTAINER_CHECK(curr) && ((flags & (CLONE_PARENT | CLONE_THREAD)) != 0)) {
        return -LOS_EINVAL;
    }
#endif
#ifdef LOSCFG_UTS_CONTAINER
    cloneFlag |= CLONE_NEWUTS;
#endif
#ifdef LOSCFG_MNT_CONTAINER
    cloneFlag |= CLONE_NEWNS;
#endif
#ifdef LOSCFG_IPC_CONTAINER
    cloneFlag |= CLONE_NEWIPC;
    if (((flags & CLONE_NEWIPC) != 0) && ((flags & CLONE_FILES) != 0)) {
        return -LOS_EINVAL;
    }
#endif
#ifdef LOSCFG_TIME_CONTAINER
    cloneFlag |= CLONE_NEWTIME;
#endif
#ifdef LOSCFG_USER_CONTAINER
    cloneFlag |= CLONE_NEWUSER;
#endif
#ifdef LOSCFG_NET_CONTAINER
    cloneFlag |= CLONE_NEWNET;
#endif
#endif

    if (flags & (~cloneFlag)) {
        return -LOS_EOPNOTSUPP;
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

/*!
 * @brief LOS_Exit	
 * 进程退出
 * @param status	
 * @return	
 *
 * @see
 */
LITE_OS_SEC_TEXT VOID LOS_Exit(INT32 status)
{
    UINT32 intSave;

    (void)status;
    /* The exit of a kernel - state process must be kernel - state and all threads must actively exit */
    LosProcessCB *processCB = OsCurrProcessGet();
    SCHEDULER_LOCK(intSave);
    if (!OsProcessIsUserMode(processCB) && (processCB->threadNumber != 1)) {//内核态下进程的退出方式,必须是所有的任务都退出了
        SCHEDULER_UNLOCK(intSave);
        PRINT_ERR("Kernel-state processes with multiple threads are not allowed to exit directly\n");
        return;
    }
    SCHEDULER_UNLOCK(intSave);

    OsProcessThreadGroupDestroy();
    OsRunningTaskToExit(OsCurrTaskGet(), OS_PRO_EXIT_OK);
}


/*!
 * @brief LOS_GetUsedPIDList	
 * 获取使用中的进程列表
 * @param pidList	
 * @param pidMaxNum	
 * @return	
 *
 * @see
 */
LITE_OS_SEC_TEXT INT32 LOS_GetUsedPIDList(UINT32 *pidList, INT32 pidMaxNum)
{
    LosProcessCB *pcb = NULL;
    INT32 num = 0;
    UINT32 intSave;
    UINT32 pid = 1;

    if (pidList == NULL) {
        return 0;
    }
    SCHEDULER_LOCK(intSave);
    while (OsProcessIDUserCheckInvalid(pid) == false) {//遍历进程池
        pcb = OS_PCB_FROM_PID(pid);
        pid++;
        if (OsProcessIsUnused(pcb)) {//未使用的不算
            continue;
        }
        pidList[num] = pcb->processID;//由参数带走
        num++;
        if (num >= pidMaxNum) {
            break;
        }
    }
    SCHEDULER_UNLOCK(intSave);
    return num;
}

#ifdef LOSCFG_FS_VFS
LITE_OS_SEC_TEXT struct fd_table_s *LOS_GetFdTable(UINT32 pid)
{
    if (OS_PID_CHECK_INVALID(pid)) {
        return NULL;
    }

    LosProcessCB *pcb = OS_PCB_FROM_PID(pid);
    struct files_struct *files = pcb->files;
    if (files == NULL) {
        return NULL;
    }

    return files->fdt;
}
#endif
/// 获取当前进程的进程ID
LITE_OS_SEC_TEXT UINT32 LOS_GetCurrProcessID(VOID)
{
    return OsCurrProcessGet()->processID;
}
/// 按指定状态退出指定进程
#ifdef LOSCFG_KERNEL_VM
STATIC VOID ThreadGroupActiveTaskKilled(LosTaskCB *taskCB)
{
    INT32 ret;
    LosProcessCB *processCB = OS_PCB_FROM_TCB(taskCB);
    taskCB->taskStatus |= OS_TASK_FLAG_EXIT_KILL;
#ifdef LOSCFG_KERNEL_SMP
    /** The other core that the thread is running on and is currently running in a non-system call */
    if (!taskCB->sig.sigIntLock && (taskCB->taskStatus & OS_TASK_STATUS_RUNNING)) {
        taskCB->signal = SIGNAL_KILL;
        LOS_MpSchedule(taskCB->currCpu);
    } else
#endif
    {
        ret = OsTaskKillUnsafe(taskCB->taskID, SIGKILL);
        if (ret != LOS_OK) {
            PRINT_ERR("pid %u exit, Exit task group %u kill %u failed! ERROR: %d\n",
                      processCB->processID, OsCurrTaskGet()->taskID, taskCB->taskID, ret);
        }
    }

    if (!(taskCB->taskStatus & OS_TASK_FLAG_PTHREAD_JOIN)) {
        taskCB->taskStatus |= OS_TASK_FLAG_PTHREAD_JOIN;
        LOS_ListInit(&taskCB->joinList);
    }

    ret = OsTaskJoinPendUnsafe(taskCB);
    if (ret != LOS_OK) {
        PRINT_ERR("pid %u exit, Exit task group %u to wait others task %u(0x%x) exit failed! ERROR: %d\n",
                  processCB->processID, OsCurrTaskGet()->taskID, taskCB->taskID, taskCB->taskStatus, ret);
    }
}
#endif

LITE_OS_SEC_TEXT VOID OsProcessThreadGroupDestroy(VOID)
{
#ifdef LOSCFG_KERNEL_VM
    UINT32 intSave;

    LosProcessCB *processCB = OsCurrProcessGet();
    LosTaskCB *currTask = OsCurrTaskGet();
    SCHEDULER_LOCK(intSave);
    if ((processCB->processStatus & OS_PROCESS_FLAG_EXIT) || !OsProcessIsUserMode(processCB)) {
        SCHEDULER_UNLOCK(intSave);
        return;
    }

    processCB->processStatus |= OS_PROCESS_FLAG_EXIT;
    processCB->threadGroup = currTask;

    LOS_DL_LIST *list = &processCB->threadSiblingList;
    LOS_DL_LIST *head = list;
    do {
        LosTaskCB *taskCB = LOS_DL_LIST_ENTRY(list->pstNext, LosTaskCB, threadList);
        if ((OsTaskIsInactive(taskCB) ||
            ((taskCB->taskStatus & OS_TASK_STATUS_READY) && !taskCB->sig.sigIntLock)) &&
            !(taskCB->taskStatus & OS_TASK_STATUS_RUNNING)) {
            OsInactiveTaskDelete(taskCB);
        } else if (taskCB != currTask) {
            ThreadGroupActiveTaskKilled(taskCB);
        } else {
            /* Skip the current task */
            list = list->pstNext;
        }
    } while (head != list->pstNext);

    SCHEDULER_UNLOCK(intSave);

    LOS_ASSERT(processCB->threadNumber == 1);
#endif
    return;
}
/// 获取系统支持的最大进程数目
LITE_OS_SEC_TEXT UINT32 LOS_GetSystemProcessMaximum(VOID)
{
    return g_processMaxNum;
}
/// 获取用户态进程的根进程,所有用户进程都是g_processCBArray[g_userInitProcess] fork来的
LITE_OS_SEC_TEXT LosProcessCB *OsGetUserInitProcess(VOID)
{
    return &g_processCBArray[OS_USER_ROOT_PROCESS_ID];
}

LITE_OS_SEC_TEXT LosProcessCB *OsGetKernelInitProcess(VOID)
{
    return &g_processCBArray[OS_KERNEL_ROOT_PROCESS_ID];
}
/// 获取空闲进程，0号进程为空闲进程，该进程不干活，专给CPU休息的。
LITE_OS_SEC_TEXT LosProcessCB *OsGetIdleProcess(VOID)
{
    return &g_processCBArray[OS_KERNEL_IDLE_PROCESS_ID];
}

LITE_OS_SEC_TEXT VOID OsSetSigHandler(UINTPTR addr)
{
    OsCurrProcessGet()->sigHandler = addr;
}

LITE_OS_SEC_TEXT UINTPTR OsGetSigHandler(VOID)
{
    return OsCurrProcessGet()->sigHandler;
}

LosProcessCB *OsGetDefaultProcessCB(VOID)
{
    return &g_processCBArray[g_processMaxNum];
}
