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
// 进程控制块(PCB)数组，存储系统中所有进程的控制信息
LITE_OS_SEC_BSS LosProcessCB *g_processCBArray = NULL;
// 空闲进程链表，管理未使用的PCB节点
LITE_OS_SEC_DATA_INIT STATIC LOS_DL_LIST g_freeProcess;
// 进程回收链表，管理待回收的进程资源
LITE_OS_SEC_DATA_INIT STATIC LOS_DL_LIST g_processRecycleList;
// 系统支持的最大进程数量
LITE_OS_SEC_BSS UINT32 g_processMaxNum;
#ifndef LOSCFG_PID_CONTAINER  // 如果未启用PID容器功能
// 根进程组指针，指向系统中的根进程组
LITE_OS_SEC_BSS ProcessGroup *g_processGroup = NULL;
// 宏定义：获取根进程组（当未启用PID容器时）
#define OS_ROOT_PGRP(processCB) (g_processGroup)
#endif

/**
 * @brief 将进程控制块(PCB)插入空闲链表
 * @details 重置PCB内容并将其添加到空闲进程链表末尾，供后续进程创建时分配使用
 * @param processCB 待插入空闲链表的进程控制块指针
 * @return 无
 */
STATIC INLINE VOID OsInsertPCBToFreeList(LosProcessCB *processCB)
{
#ifdef LOSCFG_PID_CONTAINER  // 如果启用了PID容器功能
    OsPidContainerDestroy(processCB->container, processCB);  // 销毁进程的PID容器
#endif
    UINT32 pid = processCB->processID;  // 保存原进程ID
    (VOID)memset_s(processCB, sizeof(LosProcessCB), 0, sizeof(LosProcessCB));  // 清空PCB内存
    processCB->processID = pid;  // 恢复进程ID（保持PID复用）
    processCB->processStatus = OS_PROCESS_FLAG_UNUSED;  // 标记为未使用状态
    processCB->timerID = (timer_t)(UINTPTR)MAX_INVALID_TIMER_VID;  // 初始化定时器ID为无效值
    LOS_ListTailInsert(&g_freeProcess, &processCB->pendList);  // 将PCB插入空闲链表尾部
}

/**
 * @brief 从进程中删除任务并回收
 * @details 将指定任务从所属进程的任务列表中移除，减少进程的任务计数，并将任务插入回收链表
 * @param taskCB 待删除的任务控制块指针
 * @return 无
 */
VOID OsDeleteTaskFromProcess(LosTaskCB *taskCB)
{
    LosProcessCB *processCB = OS_PCB_FROM_TCB(taskCB);  // 通过任务控制块获取所属进程控制块

    LOS_ListDelete(&taskCB->threadList);  // 从进程的任务链表中删除该任务
    processCB->threadNumber--;  // 减少进程的任务数量计数
    OsTaskInsertToRecycleList(taskCB);  // 将任务插入任务回收链表
}

/**
 * @brief 向进程添加新任务
 * @details 将新任务加入进程的任务列表，设置任务的调度参数和进程关联信息，支持用户态进程和内核态进程
 * @param processID 进程ID（实际为进程控制块指针的UINTPTR类型转换）
 * @param taskCB 待添加的任务控制块指针
 * @param param 调度参数结构体指针，用于设置任务调度策略和优先级
 * @param numCount 输出参数，返回当前进程的任务计数
 * @return 操作结果，LOS_OK表示成功，其他值表示失败
 */
UINT32 OsProcessAddNewTask(UINTPTR processID, LosTaskCB *taskCB, SchedParam *param, UINT32 *numCount)
{
    UINT32 intSave;  // 用于保存中断状态
    LosProcessCB *processCB = (LosProcessCB *)processID;  // 将进程ID转换为进程控制块指针

    SCHEDULER_LOCK(intSave);  // 关闭调度器，防止并发访问冲突
#ifdef LOSCFG_PID_CONTAINER  // 如果启用了PID容器功能
    if (OsAllocVtid(taskCB, processCB) == OS_INVALID_VALUE) {  // 为任务分配虚拟线程ID
        SCHEDULER_UNLOCK(intSave);  // 恢复调度器
        PRINT_ERR("OsAllocVtid failed!\n");  // 打印分配失败错误信息
        return LOS_NOK;  // 返回失败
    }
#endif

    taskCB->processCB = (UINTPTR)processCB;  // 设置任务所属进程
    LOS_ListTailInsert(&(processCB->threadSiblingList), &(taskCB->threadList));  // 将任务插入进程的任务链表尾部
    if (OsProcessIsUserMode(processCB)) {  // 判断是否为用户态进程
        taskCB->taskStatus |= OS_TASK_FLAG_USER_MODE;  // 标记任务为用户态
        if (processCB->threadNumber > 0) {  // 如果进程已有其他任务
            LosTaskCB *task = processCB->threadGroup;  // 获取进程的主线程
            task->ops->schedParamGet(task, param);  // 继承主线程的调度参数
        } else {  // 如果是进程的第一个任务
            OsSchedProcessDefaultSchedParamGet(param->policy, param);  // 获取默认调度参数
        }
    } else {  // 内核态进程
        LosTaskCB *runTask = OsCurrTaskGet();  // 获取当前运行的任务
        runTask->ops->schedParamGet(runTask, param);  // 继承当前任务的调度参数
    }

#ifdef LOSCFG_KERNEL_VM  // 如果启用了内核虚拟内存
    taskCB->archMmu = (UINTPTR)&processCB->vmSpace->archMmu;  // 设置任务的MMU上下文
#endif
    if (!processCB->threadNumber) {  // 如果是进程的第一个任务
        processCB->threadGroup = taskCB;  // 设置为主线程
    }
    processCB->threadNumber++;  // 增加进程的任务数量

    *numCount = processCB->threadCount;  // 设置输出参数：当前进程的任务计数
    processCB->threadCount++;  // 增加进程的任务总数统计
    SCHEDULER_UNLOCK(intSave);  // 恢复调度器
    return LOS_OK;  // 返回成功
}

/**
 * @brief 创建进程组
 * @details 为指定进程创建新的进程组，初始化进程组链表，设置进程组leader，并将进程组添加到根进程组下
 * @param processCB 作为进程组leader的进程控制块指针
 * @return 新创建的进程组指针，NULL表示创建失败
 */
ProcessGroup *OsCreateProcessGroup(LosProcessCB *processCB)
{
    ProcessGroup *pgroup = LOS_MemAlloc(m_aucSysMem1, sizeof(ProcessGroup));  // 分配进程组内存
    if (pgroup == NULL) {  // 内存分配失败
        return NULL;  // 返回NULL
    }

    pgroup->pgroupLeader = (UINTPTR)processCB;  // 设置进程组leader为当前进程
    LOS_ListInit(&pgroup->processList);  // 初始化进程组内进程链表
    LOS_ListInit(&pgroup->exitProcessList);  // 初始化进程组内退出进程链表

    LOS_ListTailInsert(&pgroup->processList, &processCB->subordinateGroupList);  // 将进程加入进程组链表
    processCB->pgroup = pgroup;  // 设置进程的进程组指针
    processCB->processStatus |= OS_PROCESS_FLAG_GROUP_LEADER;  // 标记进程为进程组leader

    ProcessGroup *rootPGroup = OS_ROOT_PGRP(processCB);  // 获取根进程组
    if (rootPGroup == NULL) {  // 如果根进程组不存在（系统初始化时）
        OS_ROOT_PGRP(processCB) = pgroup;  // 将当前进程组设为根进程组
        LOS_ListInit(&pgroup->groupList);  // 初始化根进程组的子进程组链表
    } else {  // 如果根进程组已存在
        LOS_ListTailInsert(&rootPGroup->groupList, &pgroup->groupList);  // 将当前进程组添加到根进程组的子链表
    }
    return pgroup;  // 返回新创建的进程组
}

/**
 * @brief 退出进程组
 * @details 将进程从其所属进程组中移除，如果进程组变为空则销毁进程组，并回收相关资源
 * @param processCB 待退出进程组的进程控制块指针
 * @param pgroup 输出参数，用于返回需要销毁的进程组指针（如果进程组为空）
 * @return 无
 */
STATIC VOID ExitProcessGroup(LosProcessCB *processCB, ProcessGroup **pgroup)
{
    LosProcessCB *pgroupCB = OS_GET_PGROUP_LEADER(processCB->pgroup);  // 获取进程组leader
    LOS_ListDelete(&processCB->subordinateGroupList);  // 将进程从进程组链表中删除
    // 如果进程组内的进程链表和退出进程链表均为空
    if (LOS_ListEmpty(&processCB->pgroup->processList) && LOS_ListEmpty(&processCB->pgroup->exitProcessList)) {
#ifdef LOSCFG_PID_CONTAINER  // 如果启用了PID容器功能
        if (processCB->pgroup != OS_ROOT_PGRP(processCB)) {  // 非根进程组才需要销毁
#endif
            LOS_ListDelete(&processCB->pgroup->groupList);  // 将进程组从父进程组链表中删除
            *pgroup = processCB->pgroup;  // 设置输出参数：需要销毁的进程组
#ifdef LOSCFG_PID_CONTAINER
        }
#endif
        pgroupCB->processStatus &= ~OS_PROCESS_FLAG_GROUP_LEADER;  // 清除进程组leader标记
        // 如果进程组leader已 unused 且未退出
        if (OsProcessIsUnused(pgroupCB) && !(pgroupCB->processStatus & OS_PROCESS_FLAG_EXIT)) {
            LOS_ListDelete(&pgroupCB->pendList);  // 从链表中删除
            OsInsertPCBToFreeList(pgroupCB);  // 将PCB插入空闲链表
        }
    }

    processCB->pgroup = NULL;  // 清除进程的进程组指针
}

/**
 * @brief 根据进程组ID查找进程组
 * @details 从根进程组开始遍历所有子进程组，查找与指定进程组ID匹配的进程组
 * @param gid 要查找的进程组ID（即进程组leader的PID）
 * @return 找到的进程组指针，NULL表示未找到
 */
STATIC ProcessGroup *OsFindProcessGroup(UINT32 gid)
{
    ProcessGroup *pgroup = NULL;  // 进程组指针
    ProcessGroup *rootPGroup = OS_ROOT_PGRP(OsCurrProcessGet());  // 获取当前进程的根进程组
    LosProcessCB *processCB = OS_GET_PGROUP_LEADER(rootPGroup);  // 获取根进程组的leader
    if (processCB->processID == gid) {  // 如果根进程组leader的PID匹配
        return rootPGroup;  // 返回根进程组
    }

    // 遍历根进程组下的所有子进程组
    LOS_DL_LIST_FOR_EACH_ENTRY(pgroup, &rootPGroup->groupList, ProcessGroup, groupList) {
        processCB = OS_GET_PGROUP_LEADER(pgroup);  // 获取当前进程组的leader
        if (processCB->processID == gid) {  // 如果leader的PID匹配
            return pgroup;  // 返回找到的进程组
        }
    }

    PRINT_INFO("%s failed! pgroup id = %u\n", __FUNCTION__, gid);  // 打印查找失败信息
    return NULL;  // 返回NULL表示未找到
}

/**
 * @brief 向指定进程组发送信号
 * @details 遍历进程组内的所有进程，向每个进程发送指定信号，只要有一个进程成功接收信号则返回成功
 * @param pgroup 目标进程组指针
 * @param info 信号信息结构体指针，包含信号类型和附加信息
 * @param permission 发送信号的权限检查标志
 * @return 操作结果，LOS_OK表示至少有一个进程成功接收信号，其他值表示所有进程均失败
 */
STATIC INT32 OsSendSignalToSpecifyProcessGroup(ProcessGroup *pgroup, siginfo_t *info, INT32 permission)
{
    INT32 ret, success, err;  // ret:最终返回值, success:成功标志, err:临时错误码
    LosProcessCB *childCB = NULL;  // 子进程控制块指针

    success = 0;  // 初始化成功标志为0（失败）
    ret = -LOS_ESRCH;  // 初始返回值：未找到进程
    // 遍历进程组内的所有进程
    LOS_DL_LIST_FOR_EACH_ENTRY(childCB, &(pgroup->processList), LosProcessCB, subordinateGroupList) {
        if (childCB->processID == 0) {  // 跳过PID为0的无效进程
            continue;
        }

        err = OsDispatch(childCB->processID, info, permission);  // 向进程发送信号
        success |= !err;  // 如果发送成功，设置success为1
        ret = err;  // 更新返回值为最后一次发送的结果
    }
    /* At least one success. */
    return success ? LOS_OK : ret;  // 如果有至少一个成功则返回LOS_OK，否则返回最后一次错误
}

/**
 * @brief 向系统中所有进程发送信号
 * @details 遍历根进程组及其所有子进程组，向每个进程组内的所有进程发送指定信号
 * @param info 信号信息结构体指针，包含信号类型和附加信息
 * @param permission 发送信号的权限检查标志
 * @return 操作结果，LOS_OK表示至少有一个进程成功接收信号，其他值表示所有进程均失败
 */
LITE_OS_SEC_TEXT INT32 OsSendSignalToAllProcess(siginfo_t *info, INT32 permission)
{
    INT32 ret, success, err;  // ret:最终返回值, success:成功标志, err:临时错误码
    ProcessGroup *pgroup = NULL;  // 进程组指针
    ProcessGroup *rootPGroup = OS_ROOT_PGRP(OsCurrProcessGet());  // 获取当前进程的根进程组

    success = 0;  // 初始化成功标志为0（失败）
    err = OsSendSignalToSpecifyProcessGroup(rootPGroup, info, permission);  // 向根进程组发送信号
    success |= !err;  // 更新成功标志
    ret = err;  // 更新返回值
    /* all processes group */
    // 遍历根进程组下的所有子进程组
    LOS_DL_LIST_FOR_EACH_ENTRY(pgroup, &rootPGroup->groupList, ProcessGroup, groupList) {
        /* all processes in the process group. */
        err = OsSendSignalToSpecifyProcessGroup(pgroup, info, permission);  // 向子进程组发送信号
        success |= !err;  // 更新成功标志
        ret = err;  // 更新返回值
    }
    return success ? LOS_OK : ret;  // 如果有至少一个成功则返回LOS_OK，否则返回最后一次错误
}

/**
 * @brief 向指定进程组发送信号（外部接口）
 * @details 根据进程组ID查找进程组，并向该进程组内的所有进程发送信号
 * @param pid 进程组ID（如果为0，则使用当前进程的进程组ID；如果为负数，则取绝对值作为进程组ID）
 * @param info 信号信息结构体指针，包含信号类型和附加信息
 * @param permission 发送信号的权限检查标志
 * @return 操作结果，LOS_OK表示成功，-LOS_ESRCH表示进程组不存在，其他值表示信号发送失败
 */
LITE_OS_SEC_TEXT INT32 OsSendSignalToProcessGroup(INT32 pid, siginfo_t *info, INT32 permission)
{
    ProcessGroup *pgroup = NULL;  // 进程组指针
    /* Send SIG to all processes in process group PGRP.
       If PGRP is zero, send SIG to all processes in
       the current process's process group. */
    // 如果pid为0，则使用当前进程的进程组ID；否则使用-pid作为进程组ID
    pgroup = OsFindProcessGroup(pid ? -pid : LOS_GetCurrProcessGroupID());
    if (pgroup == NULL) {  // 进程组不存在
        return -LOS_ESRCH;  // 返回未找到错误
    }
    /* all processes in the process group. */
    return OsSendSignalToSpecifyProcessGroup(pgroup, info, permission);  // 向进程组发送信号
}

/**
 * @brief 在进程组的退出进程链表中查找指定进程
 * @details 遍历进程组的退出进程链表，查找PID匹配的进程或任意进程（当pid为OS_INVALID_VALUE时）
 * @param pgroup 目标进程组指针
 * @param pid 要查找的进程PID，OS_INVALID_VALUE表示查找任意退出进程
 * @return 找到的进程控制块指针，NULL表示未找到
 */
STATIC LosProcessCB *OsFindGroupExitProcess(ProcessGroup *pgroup, INT32 pid)
{
    LosProcessCB *childCB = NULL;  // 子进程控制块指针

    // 遍历进程组的退出进程链表
    LOS_DL_LIST_FOR_EACH_ENTRY(childCB, &(pgroup->exitProcessList), LosProcessCB, subordinateGroupList) {
        if ((childCB->processID == pid) || (pid == OS_INVALID_VALUE)) {  // PID匹配或查找任意进程
            return childCB;  // 返回找到的进程
        }
    }

    return NULL;  // 返回NULL表示未找到
}
/**
 * @brief 查找指定子进程
 * @details 遍历父进程的子进程链表，检查是否存在目标子进程
 * @param processCB 父进程控制块指针
 * @param wait 待查找的子进程控制块指针
 * @return 查找结果，LOS_OK表示找到，LOS_NOK表示未找到
 */
STATIC UINT32 OsFindChildProcess(const LosProcessCB *processCB, const LosProcessCB *wait)
{
    LosProcessCB *childCB = NULL;  // 子进程控制块指针

    // 遍历父进程的子进程链表
    LOS_DL_LIST_FOR_EACH_ENTRY(childCB, &(processCB->childrenList), LosProcessCB, siblingList) {
        if (childCB == wait) {  // 如果找到目标子进程
            return LOS_OK;  // 返回成功
        }
    }

    return LOS_NOK;  // 未找到目标子进程，返回失败
}

/**
 * @brief 查找已退出的子进程
 * @details 遍历父进程的退出子进程链表，查找指定子进程或任意退出子进程
 * @param processCB 父进程控制块指针
 * @param wait 待查找的子进程控制块指针（为NULL时查找任意退出子进程）
 * @return 找到的退出子进程控制块指针，NULL表示未找到
 */
STATIC LosProcessCB *OsFindExitChildProcess(const LosProcessCB *processCB, const LosProcessCB *wait)
{
    LosProcessCB *exitChild = NULL;  // 退出子进程控制块指针

    // 遍历父进程的退出子进程链表
    LOS_DL_LIST_FOR_EACH_ENTRY(exitChild, &(processCB->exitChildList), LosProcessCB, siblingList) {
        if ((wait == NULL) || (exitChild == wait)) {  // 匹配目标子进程或查找任意子进程
            return exitChild;  // 返回找到的退出子进程
        }
    }

    return NULL;  // 未找到退出子进程，返回NULL
}

/**
 * @brief 唤醒等待中的任务
 * @details 设置任务的等待ID并唤醒任务，在SMP配置下触发跨CPU调度
 * @param taskCB 待唤醒的任务控制块指针
 * @param wakePID 唤醒源进程ID（触发唤醒的进程ID）
 * @return 无
 */
VOID OsWaitWakeTask(LosTaskCB *taskCB, UINTPTR wakePID)
{
    taskCB->waitID = wakePID;  // 设置任务的等待ID为唤醒源进程ID
    taskCB->ops->wake(taskCB);  // 调用任务操作接口唤醒任务
#ifdef LOSCFG_KERNEL_SMP  // 如果启用了SMP（对称多处理）
    LOS_MpSchedule(OS_MP_CPU_ALL);  // 触发所有CPU的调度
#endif
}

/**
 * @brief 唤醒等待指定进程的任务
 * @details 遍历等待链表，唤醒所有等待指定进程的任务，并记录非特定等待任务的位置
 * @param head 等待链表头指针
 * @param processCB 目标进程控制块指针（被等待的进程）
 * @param anyList 输出参数，用于记录第一个非特定等待任务的链表节点
 * @return 是否找到并唤醒了指定等待任务，TRUE表示找到，FALSE表示未找到
 */
STATIC BOOL OsWaitWakeSpecifiedProcess(LOS_DL_LIST *head, const LosProcessCB *processCB, LOS_DL_LIST **anyList)
{
    LOS_DL_LIST *list = head;  // 链表遍历指针
    LosTaskCB *taskCB = NULL;  // 任务控制块指针
    UINTPTR processID = 0;  // 进程ID，用于记录唤醒源
    BOOL find = FALSE;  // 是否找到指定等待任务的标志

    while (list->pstNext != head) {  // 遍历等待链表
        taskCB = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(list));  // 获取链表中的第一个任务
        // 如果任务正在等待特定进程，且等待的进程ID匹配
        if ((taskCB->waitFlag == OS_PROCESS_WAIT_PRO) && (taskCB->waitID == (UINTPTR)processCB)) {
            if (processID == 0) {  // 首次找到匹配的等待任务
                processID = taskCB->waitID;  // 记录唤醒源进程ID
                find = TRUE;  // 设置找到标志
            } else {  // 找到多个匹配的等待任务
                processID = OS_INVALID_VALUE;  // 标记为无效ID（多个唤醒源）
            }

            OsWaitWakeTask(taskCB, processID);  // 唤醒当前任务
            continue;  // 继续处理下一个任务
        }

        if (taskCB->waitFlag != OS_PROCESS_WAIT_PRO) {  // 如果任务不是等待特定进程
            *anyList = list;  // 记录当前链表节点
            break;  // 退出循环
        }
        list = list->pstNext;  // 移动到下一个链表节点
    }

    return find;  // 返回是否找到指定等待任务
}

/**
 * @brief 检查并唤醒父进程中等待的任务
 * @details 当子进程退出时，唤醒父进程中等待该子进程或任意子进程的任务
 * @param parentCB 父进程控制块指针
 * @param processCB 退出的子进程控制块指针
 * @return 无
 */
STATIC VOID OsWaitCheckAndWakeParentProcess(LosProcessCB *parentCB, const LosProcessCB *processCB)
{
    LOS_DL_LIST *head = &parentCB->waitList;  // 父进程的等待链表头
    LOS_DL_LIST *list = NULL;  // 链表遍历指针
    LosTaskCB *taskCB = NULL;  // 任务控制块指针
    BOOL findSpecified = FALSE;  // 是否找到等待特定子进程的任务标志

    if (LOS_ListEmpty(&parentCB->waitList)) {  // 如果父进程的等待链表为空
        return;  // 直接返回
    }

    findSpecified = OsWaitWakeSpecifiedProcess(head, processCB, &list);  // 唤醒等待特定子进程的任务
    if (findSpecified == TRUE) {  // 如果找到并唤醒了等待特定子进程的任务
        /* No thread is waiting for any child process to finish */
        if (LOS_ListEmpty(&parentCB->waitList)) {  // 如果等待链表已为空
            return;  // 直接返回
        } else if (!LOS_ListEmpty(&parentCB->childrenList)) {  // 如果父进程还有其他子进程
            /* Other child processes exist, and other threads that are waiting
             * for the child to finish continue to wait
             */
            return;  // 其他等待任务继续等待，返回
        }
    }

    /* Waiting threads are waiting for a specified child process to finish */
    if (list == NULL) {  // 如果未找到非特定等待任务
        return;  // 返回
    }

    /* No child processes exist and all waiting threads are awakened */
    if (findSpecified == TRUE) {  // 如果已唤醒特定等待任务，且父进程无其他子进程
        while (list->pstNext != head) {  // 遍历剩余等待任务
            taskCB = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(list));  // 获取等待任务
            OsWaitWakeTask(taskCB, OS_INVALID_VALUE);  // 唤醒任务，设置无效唤醒源ID
        }
        return;  // 返回
    }

    while (list->pstNext != head) {  // 遍历非特定等待任务
        taskCB = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(list));  // 获取等待任务
        if (taskCB->waitFlag == OS_PROCESS_WAIT_GID) {  // 如果任务等待特定进程组
            // 检查进程组leader是否匹配
            if (taskCB->waitID != (UINTPTR)OS_GET_PGROUP_LEADER(processCB->pgroup)) {
                list = list->pstNext;  // 移动到下一个任务
                continue;  // 继续循环
            }
        }

        if (findSpecified == FALSE) {  // 如果是第一个唤醒的任务
            OsWaitWakeTask(taskCB, (UINTPTR)processCB);  // 唤醒任务，设置唤醒源为退出的子进程
            findSpecified = TRUE;  // 设置找到标志
        } else {  // 如果已唤醒过任务
            OsWaitWakeTask(taskCB, OS_INVALID_VALUE);  // 唤醒任务，设置无效唤醒源ID
        }

        if (!LOS_ListEmpty(&parentCB->childrenList)) {  // 如果父进程还有其他子进程
            break;  // 退出循环
        }
    }

    return;  // 返回
}

/**
 * @brief 释放进程资源
 * @details 根据配置选项，释放进程占用的虚拟内存空间、用户结构体、软件定时器和VID映射等资源
 * @param processCB 待释放资源的进程控制块指针
 * @return 无
 */
LITE_OS_SEC_TEXT VOID OsProcessResourcesToFree(LosProcessCB *processCB)
{
#ifdef LOSCFG_KERNEL_VM  // 如果启用了内核虚拟内存
    if (OsProcessIsUserMode(processCB)) {  // 如果是用户态进程
        (VOID)OsVmSpaceRegionFree(processCB->vmSpace);  // 释放进程的虚拟内存空间
    }
#endif

#ifdef LOSCFG_SECURITY_CAPABILITY  // 如果启用了安全能力
    if (processCB->user != NULL) {  // 如果用户结构体存在
        (VOID)LOS_MemFree(m_aucSysMem1, processCB->user);  // 释放用户结构体内存
        processCB->user = NULL;  // 清空用户结构体指针
    }
#endif

#ifdef LOSCFG_BASE_CORE_SWTMR_ENABLE  // 如果启用了软件定时器
    OsSwtmrRecycle((UINTPTR)processCB);  // 回收进程相关的软件定时器
    processCB->timerID = (timer_t)(UINTPTR)MAX_INVALID_TIMER_VID;  // 重置定时器ID为无效值
#endif

#ifdef LOSCFG_SECURITY_VID  // 如果启用了安全VID
    if (processCB->timerIdMap.bitMap != NULL) {  // 如果定时器ID映射表存在
        VidMapDestroy(processCB);  // 销毁VID映射
        processCB->timerIdMap.bitMap = NULL;  // 清空映射表指针
    }
#endif

#ifdef LOSCFG_KERNEL_LITEIPC  // 如果启用了轻量级IPC功能
    (VOID)LiteIpcPoolDestroy(processCB->processID);  // 销毁进程的IPC池
#endif

#ifdef LOSCFG_KERNEL_CPUP  // 如果启用了CPU性能统计功能
    UINT32 intSave;  // 用于保存中断状态
    OsCpupBase *processCpup = processCB->processCpup;  // 获取进程的CPU性能统计数据
    SCHEDULER_LOCK(intSave);  // 关闭调度器，防止并发访问冲突
    processCB->processCpup = NULL;  // 清空进程的CPU性能统计指针
    SCHEDULER_UNLOCK(intSave);  // 恢复调度器
    (VOID)LOS_MemFree(m_aucSysMem1, processCpup);  // 释放CPU性能统计数据内存
#endif

#ifdef LOSCFG_PROC_PROCESS_DIR  // 如果启用了proc文件系统进程目录
    ProcFreeProcessDir(processCB->procDir);  // 释放进程的proc目录资源
    processCB->procDir = NULL;  // 清空proc目录指针
#endif

#ifdef LOSCFG_KERNEL_CONTAINER  // 如果启用了容器功能
    OsOsContainersDestroyEarly(processCB);  // 提前销毁进程的容器资源
#endif

#ifdef LOSCFG_FS_VFS  // 如果启用了VFS文件系统
    if (OsProcessIsUserMode(processCB)) {  // 如果是用户态进程
        delete_files(processCB->files);  // 删除进程打开的文件
    }
    processCB->files = NULL;  // 清空文件指针
#endif

#ifdef LOSCFG_KERNEL_CONTAINER  // 如果启用了容器功能
    OsContainersDestroy(processCB);  // 销毁进程的容器
#endif

#ifdef LOSCFG_KERNEL_PLIMITS  // 如果启用了进程资源限制功能
    OsPLimitsDeleteProcess(processCB);  // 删除进程的资源限制
#endif
    if (processCB->resourceLimit != NULL) {  // 如果资源限制结构体存在
        (VOID)LOS_MemFree((VOID *)m_aucSysMem0, processCB->resourceLimit);  // 释放资源限制内存
        processCB->resourceLimit = NULL;  // 清空资源限制指针
    }
}

/**
 * @brief 回收僵尸进程
 * @details 将僵尸进程从进程组中移除，清理任务资源，并根据进程状态将PCB加入回收链表或空闲链表
 * @param childCB 待回收的子进程控制块指针
 * @param pgroup 输出参数，用于返回需要销毁的进程组指针
 * @return 无
 */
STATIC VOID OsRecycleZombiesProcess(LosProcessCB *childCB, ProcessGroup **pgroup)
{
    ExitProcessGroup(childCB, pgroup);  // 退出进程组
    LOS_ListDelete(&childCB->siblingList);  // 从父进程的子进程链表中删除
    if (OsProcessIsDead(childCB)) {  // 如果进程已死亡
        OsDeleteTaskFromProcess(childCB->threadGroup);  // 从进程中删除主线程
        childCB->processStatus &= ~OS_PROCESS_STATUS_ZOMBIES;  // 清除僵尸状态标志
        childCB->processStatus |= OS_PROCESS_FLAG_UNUSED;  // 设置为未使用状态
    }

    LOS_ListDelete(&childCB->pendList);  // 从当前链表中删除PCB
    if (childCB->processStatus & OS_PROCESS_FLAG_EXIT) {  // 如果进程已标记退出
        LOS_ListHeadInsert(&g_processRecycleList, &childCB->pendList);  // 插入进程回收链表头部
    } else if (OsProcessIsPGroupLeader(childCB)) {  // 如果是进程组leader
        LOS_ListTailInsert(&g_processRecycleList, &childCB->pendList);  // 插入进程回收链表尾部
    } else {  // 普通进程
        OsInsertPCBToFreeList(childCB);  // 插入PCB空闲链表
    }
}

/**
 * @brief 处理存活子进程
 * @details 将进程的存活子进程重新父进程化为根进程或内核初始化进程
 * @param processCB 父进程控制块指针
 * @return 无
 */
STATIC VOID OsDealAliveChildProcess(LosProcessCB *processCB)
{
    LosProcessCB *childCB = NULL;  // 子进程控制块指针
    LosProcessCB *parentCB = NULL;  // 新的父进程控制块指针
    LOS_DL_LIST *nextList = NULL;  // 链表遍历指针
    LOS_DL_LIST *childHead = NULL;  // 子进程链表头指针

#ifdef LOSCFG_PID_CONTAINER  // 如果启用了PID容器功能
    if (processCB->processID == OS_USER_ROOT_PROCESS_ID) {  // 如果是用户根进程
        return;  // 直接返回，不处理
    }
#endif

    if (!LOS_ListEmpty(&processCB->childrenList)) {  // 如果存在存活子进程
        childHead = processCB->childrenList.pstNext;  // 获取子进程链表头
        LOS_ListDelete(&(processCB->childrenList));  // 从原父进程中删除子进程链表
        if (OsProcessIsUserMode(processCB)) {  // 如果原父进程是用户态进程
            parentCB = OS_PCB_FROM_PID(OS_USER_ROOT_PROCESS_ID);  // 新父进程设为用户根进程
        } else {  // 内核态进程
            parentCB = OsGetKernelInitProcess();  // 新父进程设为内核初始化进程
        }

        for (nextList = childHead; ;) {  // 遍历所有子进程
            childCB = OS_PCB_FROM_SIBLIST(nextList);  // 获取子进程控制块
            childCB->parentProcess = parentCB;  // 更新子进程的父进程指针
            nextList = nextList->pstNext;  // 移动到下一个子进程
            if (nextList == childHead) {  // 遍历完成
                break;  // 退出循环
            }
        }

        LOS_ListTailInsertList(&parentCB->childrenList, childHead);  // 将子进程链表插入新父进程
    }

    return;  // 返回
}

/**
 * @brief 释放子进程资源
 * @details 遍历父进程的退出子进程链表，回收所有僵尸子进程并释放进程组资源
 * @param processCB 父进程控制块指针
 * @return 无
 */
STATIC VOID OsChildProcessResourcesFree(const LosProcessCB *processCB)
{
    LosProcessCB *childCB = NULL;  // 子进程控制块指针
    ProcessGroup *pgroup = NULL;  // 进程组指针

    // 循环处理所有退出的子进程
    while (!LOS_ListEmpty(&((LosProcessCB *)processCB)->exitChildList)) {
        // 获取链表中的第一个退出子进程
        childCB = LOS_DL_LIST_ENTRY(processCB->exitChildList.pstNext, LosProcessCB, siblingList);
        OsRecycleZombiesProcess(childCB, &pgroup);  // 回收僵尸进程
        (VOID)LOS_MemFree(m_aucSysMem1, pgroup);  // 释放进程组内存
    }
}

/**
 * @brief 进程正常退出处理
 * @details 释放子进程资源，通知父进程，处理僵尸状态，并将进程加入回收链表
 * @param processCB 退出的进程控制块指针
 * @param status 退出状态码
 * @return 无
 */
VOID OsProcessNaturalExit(LosProcessCB *processCB, UINT32 status)
{
    OsChildProcessResourcesFree(processCB);  // 释放子进程资源

    /* is a child process */
    if (processCB->parentProcess != NULL) {  // 如果存在父进程
        LosProcessCB *parentCB = processCB->parentProcess;  // 获取父进程控制块
        LOS_ListDelete(&processCB->siblingList);  // 从父进程的子进程链表中删除
        if (!OsProcessExitCodeSignalIsSet(processCB)) {  // 如果未设置退出码信号
            OsProcessExitCodeSet(processCB, status);  // 设置进程退出码
        }
        // 将进程加入父进程的退出子进程链表
        LOS_ListTailInsert(&parentCB->exitChildList, &processCB->siblingList);
        LOS_ListDelete(&processCB->subordinateGroupList);  // 从进程组链表中删除
        // 将进程加入进程组的退出进程链表
        LOS_ListTailInsert(&processCB->pgroup->exitProcessList, &processCB->subordinateGroupList);

        OsWaitCheckAndWakeParentProcess(parentCB, processCB);  // 唤醒等待的父进程任务

        OsDealAliveChildProcess(processCB);  // 处理存活子进程

        processCB->processStatus |= OS_PROCESS_STATUS_ZOMBIES;  // 标记为僵尸进程
#ifdef LOSCFG_KERNEL_VM  // 如果启用了内核虚拟内存
        (VOID)OsSendSigToProcess(parentCB, SIGCHLD, OS_KERNEL_KILL_PERMISSION);  // 发送SIGCHLD信号给父进程
#endif
        LOS_ListHeadInsert(&g_processRecycleList, &processCB->pendList);  // 将进程加入回收链表
        return;  // 返回
    }

    LOS_Panic("pid : %u is the root process exit!\n", processCB->processID);  // 根进程退出，触发系统恐慌
    return;  // 返回
}

/**
 * @brief 系统进程早期初始化
 * @details 从空闲链表中移除系统进程，初始化容器，并设置主任务进程
 * @param processCB 系统进程控制块指针
 * @return 无
 */
STATIC VOID SystemProcessEarlyInit(LosProcessCB *processCB)
{
    LOS_ListDelete(&processCB->pendList);  // 将系统进程从空闲链表中删除
#ifdef LOSCFG_KERNEL_CONTAINER  // 如果启用了容器功能
    OsContainerInitSystemProcess(processCB);  // 初始化系统进程的容器
#endif
    if (processCB == OsGetKernelInitProcess()) {  // 如果是内核初始化进程
        OsSetMainTaskProcess((UINTPTR)processCB);  // 设置为主任务进程
    }
}

/**
 * @brief 进程模块初始化
 * @details 分配进程控制块数组，初始化空闲和回收链表，创建系统进程
 * @return 操作结果，LOS_OK表示成功，其他值表示失败
 */
UINT32 OsProcessInit(VOID)
{
    UINT32 index;  // 循环索引
    UINT32 size;  // 内存大小
    UINT32 ret;  // 返回值

    g_processMaxNum = LOSCFG_BASE_CORE_PROCESS_LIMIT;  // 设置最大进程数
    size = (g_processMaxNum + 1) * sizeof(LosProcessCB);  // 计算进程控制块数组大小

    // 分配进程控制块数组内存
    g_processCBArray = (LosProcessCB *)LOS_MemAlloc(m_aucSysMem1, size);
    if (g_processCBArray == NULL) {  // 内存分配失败
        return LOS_NOK;  // 返回失败
    }
    (VOID)memset_s(g_processCBArray, size, 0, size);  // 清空进程控制块数组内存

    LOS_ListInit(&g_freeProcess);  // 初始化空闲进程链表
    LOS_ListInit(&g_processRecycleList);  // 初始化进程回收链表

    // 初始化所有普通进程控制块
    for (index = 0; index < g_processMaxNum; index++) {
        g_processCBArray[index].processID = index;  // 设置进程ID
        g_processCBArray[index].processStatus = OS_PROCESS_FLAG_UNUSED;  // 标记为未使用
        LOS_ListTailInsert(&g_freeProcess, &g_processCBArray[index].pendList);  // 插入空闲链表
    }

    /* Default process to prevent thread PCB from being empty */
    g_processCBArray[index].processID = index;  // 设置默认进程ID
    g_processCBArray[index].processStatus = OS_PROCESS_FLAG_UNUSED;  // 标记为未使用

    // 初始化默认进程的任务
    ret = OsTaskInit((UINTPTR)&g_processCBArray[g_processMaxNum]);
    if (ret != LOS_OK) {  // 任务初始化失败
        (VOID)LOS_MemFree(m_aucSysMem1, g_processCBArray);  // 释放进程控制块数组内存
        return LOS_OK;  // 返回失败
    }

#ifdef LOSCFG_KERNEL_CONTAINER  // 如果启用了容器功能
    OsInitRootContainer();  // 初始化根容器
#endif
#ifdef LOSCFG_KERNEL_PLIMITS  // 如果启用了进程资源限制功能
    OsProcLimiterSetInit();  // 初始化进程资源限制器
#endif
    SystemProcessEarlyInit(OsGetIdleProcess());  // 初始化空闲进程
    SystemProcessEarlyInit(OsGetUserInitProcess());  // 初始化用户初始化进程
    SystemProcessEarlyInit(OsGetKernelInitProcess());  // 初始化内核初始化进程
    return LOS_OK;  // 返回成功
}
/**
 * @brief 回收进程控制块到空闲链表
 * @details 将处于回收链表中的进程控制块（PCB）进行资源清理后移至空闲链表，供后续进程创建复用
 *          主要处理已退出的进程，释放其虚拟内存空间（如启用内核虚拟内存）并更新进程状态
 */
LITE_OS_SEC_TEXT VOID OsProcessCBRecycleToFree(VOID)
{
    UINT32 intSave;                          // 中断保存变量，用于临界区保护
    LosProcessCB *processCB = NULL;          // 指向当前待处理的进程控制块

    SCHEDULER_LOCK(intSave);                 // 加锁保护进程回收链表操作
    while (!LOS_ListEmpty(&g_processRecycleList)) {  // 遍历进程回收链表
        processCB = OS_PCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&g_processRecycleList));  // 获取链表头的PCB
        if (!(processCB->processStatus & OS_PROCESS_FLAG_EXIT)) {  // 检查进程是否已退出
            break;                           // 若存在未退出进程，终止回收流程
        }
        SCHEDULER_UNLOCK(intSave);           // 临时解锁，允许调度器切换

        OsTaskCBRecycleToFree();             // 回收与该进程关联的任务控制块

        SCHEDULER_LOCK(intSave);             // 重新加锁保护PCB操作
        processCB->processStatus &= ~OS_PROCESS_FLAG_EXIT;  // 清除进程退出标志
#ifdef LOSCFG_KERNEL_VM                      // 若启用内核虚拟内存管理
        LosVmSpace *space = NULL;            // 虚拟内存空间指针
        if (OsProcessIsUserMode(processCB)) {  // 检查是否为用户模式进程
            space = processCB->vmSpace;      // 保存用户进程的虚拟内存空间
        }
        processCB->vmSpace = NULL;           // 清除进程的虚拟内存空间引用
#endif
        /* OS_PROCESS_FLAG_GROUP_LEADER: 进程组组长的PCB在未销毁前不能回收
         * !OS_PROCESS_FLAG_UNUSED: 父进程未回收子进程资源
         */
        LOS_ListDelete(&processCB->pendList);  // 将PCB从回收链表中移除
        if (OsProcessIsPGroupLeader(processCB) || OsProcessIsDead(processCB)) {  // 检查是否为进程组组长或已死亡进程
            LOS_ListTailInsert(&g_processRecycleList, &processCB->pendList);  // 重新插入回收链表尾部等待后续处理
        } else {
            /* 清除进程状态的低4位 */
            OsInsertPCBToFreeList(processCB);  // 将PCB插入空闲链表，供新进程创建使用
        }
#ifdef LOSCFG_KERNEL_VM                      // 若启用内核虚拟内存管理
        SCHEDULER_UNLOCK(intSave);           // 解锁以释放虚拟内存空间
        (VOID)LOS_VmSpaceFree(space);        // 释放用户进程的虚拟内存空间
        SCHEDULER_LOCK(intSave);             // 重新加锁继续处理
#endif
    }

    SCHEDULER_UNLOCK(intSave);               // 解锁，结束临界区保护
}

/**
 * @brief 反初始化进程控制块
 * @details 清理进程资源，解除进程间关系（父子进程、进程组），并将进程控制块加入回收链表
 * @param processCB 待反初始化的进程控制块指针
 */
STATIC VOID OsDeInitPCB(LosProcessCB *processCB)
{
    UINT32 intSave;                          // 中断保存变量，用于临界区保护
    ProcessGroup *pgroup = NULL;             // 进程组指针

    if (processCB == NULL) {                 // 检查进程控制块指针有效性
        return;                              // 无效指针，直接返回
    }

#ifdef LOSCFG_KERNEL_CONTAINER               // 若启用内核容器功能
    if (OS_PID_CHECK_INVALID(processCB->processID)) {  // 检查进程ID是否有效
        return;                              // 无效PID，直接返回
    }
#endif

    OsProcessResourcesToFree(processCB);     // 释放进程关联的资源

    SCHEDULER_LOCK(intSave);                 // 加锁保护进程操作
    if (processCB->parentProcess != NULL) {  // 检查是否存在父进程
        LOS_ListDelete(&processCB->siblingList);  // 从父进程的子进程链表中移除
        processCB->parentProcess = NULL;     // 清除父进程引用
    }

    if (processCB->pgroup != NULL) {         // 检查是否属于某个进程组
        ExitProcessGroup(processCB, &pgroup);  // 退出进程组并获取进程组指针
    }

    processCB->processStatus &= ~OS_PROCESS_STATUS_INIT;  // 清除进程初始化状态
    processCB->processStatus |= OS_PROCESS_FLAG_EXIT;     // 设置进程退出标志
    LOS_ListHeadInsert(&g_processRecycleList, &processCB->pendList);  // 将PCB插入回收链表头部
    SCHEDULER_UNLOCK(intSave);               // 解锁，结束临界区保护

    (VOID)LOS_MemFree(m_aucSysMem1, pgroup); // 释放进程组内存
    OsWriteResourceEvent(OS_RESOURCE_EVENT_FREE);  // 写入资源释放事件
    return;
}

/**
 * @brief 设置进程名称
 * @details 根据输入名称或默认规则设置进程控制块中的进程名称
 * @param processCB 进程控制块指针
 * @param name 进程名称字符串，为NULL时使用默认命名规则
 * @return 操作结果，LOS_OK表示成功，其他值表示失败
 */
UINT32 OsSetProcessName(LosProcessCB *processCB, const CHAR *name)
{
    errno_t errRet;                          // 字符串操作返回值

    if (processCB == NULL) {                 // 检查进程控制块指针有效性
        return LOS_EINVAL;                   // 返回无效参数错误
    }

    if (name != NULL) {                      // 若提供了进程名称
        errRet = strncpy_s(processCB->processName, OS_PCB_NAME_LEN, name, OS_PCB_NAME_LEN - 1);  // 安全拷贝名称
        if (errRet == EOK) {                 // 拷贝成功
            return LOS_OK;                   // 返回成功
        }
    }

    // 若未提供名称或拷贝失败，使用默认命名规则
    switch (processCB->processMode) {
        case OS_KERNEL_MODE:                 // 内核模式进程
            errRet = snprintf_s(processCB->processName, OS_PCB_NAME_LEN, OS_PCB_NAME_LEN - 1,
                                "KerProcess%u", processCB->processID);  // 生成内核进程名称
            break;
        default:                             // 用户模式进程（默认）
            errRet = snprintf_s(processCB->processName, OS_PCB_NAME_LEN, OS_PCB_NAME_LEN - 1,
                                "UserProcess%u", processCB->processID);  // 生成用户进程名称
            break;
    }

    if (errRet < 0) {                        // 检查名称生成是否失败
        return LOS_NOK;                      // 返回失败
    }
    return LOS_OK;                           // 返回成功
}

/**
 * @brief 初始化进程控制块(PCB)
 * @details 对进程控制块的基本成员进行初始化，包括进程模式、状态、亲属关系、链表节点等，
 *          并根据配置选项初始化虚拟内存空间、CPU性能统计信息、VID映射列表和能力集
 * @param[in] processCB 指向待初始化的进程控制块指针
 * @param[in] mode 进程模式（内核态/用户态）
 * @param[in] name 进程名称
 * @return UINT32 初始化结果，LOS_OK表示成功，其他值表示失败
 */
STATIC UINT32 OsInitPCB(LosProcessCB *processCB, UINT32 mode, const CHAR *name)
{
    processCB->processMode = mode;  // 设置进程模式（内核态/用户态）
    processCB->processStatus = OS_PROCESS_STATUS_INIT;  // 初始化进程状态为初始态
    processCB->parentProcess = NULL;  // 父进程初始化为空
    processCB->threadGroup = NULL;  // 线程组初始化为空
    processCB->umask = OS_PROCESS_DEFAULT_UMASK;  // 设置进程默认文件创建掩码
    processCB->timerID = (timer_t)(UINTPTR)MAX_INVALID_TIMER_VID;  // 初始化定时器ID为无效值

    LOS_ListInit(&processCB->threadSiblingList);  // 初始化线程兄弟链表
    LOS_ListInit(&processCB->childrenList);  // 初始化子进程链表
    LOS_ListInit(&processCB->exitChildList);  // 初始化已退出子进程链表
    LOS_ListInit(&(processCB->waitList));  // 初始化进程等待链表

#ifdef LOSCFG_KERNEL_VM  // 若启用虚拟内存配置
    if (OsProcessIsUserMode(processCB)) {  // 判断是否为用户态进程
        processCB->vmSpace = OsCreateUserVmSpace();  // 创建用户态虚拟内存空间
        if (processCB->vmSpace == NULL) {  // 虚拟内存空间创建失败检查
            processCB->processStatus = OS_PROCESS_FLAG_UNUSED;  // 标记进程控制块为未使用
            return LOS_ENOMEM;  // 返回内存不足错误
        }
    } else {
        processCB->vmSpace = LOS_GetKVmSpace();  // 内核态进程使用内核虚拟内存空间
    }
#endif

#ifdef LOSCFG_KERNEL_CPUP  // 若启用CPU性能统计配置
    processCB->processCpup = (OsCpupBase *)LOS_MemAlloc(m_aucSysMem1, sizeof(OsCpupBase));  // 分配CPU性能统计结构内存
    if (processCB->processCpup == NULL) {  // 内存分配失败检查
        return LOS_ENOMEM;  // 返回内存不足错误
    }
    (VOID)memset_s(processCB->processCpup, sizeof(OsCpupBase), 0, sizeof(OsCpupBase));  // 初始化CPU性能统计结构
#endif
#ifdef LOSCFG_SECURITY_VID  // 若启用VID安全配置
    status_t status = VidMapListInit(processCB);  // 初始化VID映射列表
    if (status != LOS_OK) {  // 初始化失败检查
        return LOS_ENOMEM;  // 返回内存不足错误
    }
#endif
#ifdef LOSCFG_SECURITY_CAPABILITY  // 若启用能力集安全配置
    OsInitCapability(processCB);  // 初始化进程能力集
#endif

    if (OsSetProcessName(processCB, name) != LOS_OK) {  // 设置进程名称
        return LOS_ENOMEM;  // 返回内存不足错误
    }

    return LOS_OK;  // PCB初始化成功
}

#ifdef LOSCFG_SECURITY_CAPABILITY  // 若启用能力集安全配置
/**
 * @brief 创建用户结构体
 * @details 分配并初始化用户结构体，包括用户ID、组ID及附加组列表
 * @param[in] userID 用户ID
 * @param[in] gid 组ID
 * @param[in] size 组数量（包括初始组）
 * @return User* 创建成功的用户结构体指针，失败返回NULL
 */
STATIC User *OsCreateUser(UINT32 userID, UINT32 gid, UINT32 size)
{   // (size - 1) * sizeof(UINT32) 用于存储user->groups数组，节约内存
    User *user = LOS_MemAlloc(m_aucSysMem1, sizeof(User) + (size - 1) * sizeof(UINT32));  // 分配用户结构体内存
    if (user == NULL) {  // 内存分配失败检查
        return NULL;  // 返回NULL
    }

    user->userID = userID;  // 设置用户ID
    user->effUserID = userID;  // 设置有效用户ID
    user->gid = gid;  // 设置组ID
    user->effGid = gid;  // 设置有效组ID
    user->groupNumber = size;  // 设置组数量
    user->groups[0] = gid;  // 初始化第一个组为初始组ID
    return user;  // 返回创建的用户结构体
}

/**
 * @brief 检查当前用户是否属于指定组
 * @details 遍历当前用户的组列表，检查是否包含指定的组ID
 * @param[in] gid 要检查的组ID
 * @return BOOL 属于指定组返回TRUE，否则返回FALSE
 */
LITE_OS_SEC_TEXT BOOL LOS_CheckInGroups(UINT32 gid)
{   // (size - 1) * sizeof(UINT32) 用于存储user->groups数组，节约内存
    UINT32 intSave;  // 中断状态保存变量
    UINT32 count;  // 循环计数器
    User *user = NULL;  // 用户结构体指针

    SCHEDULER_LOCK(intSave);  // 关闭调度器
    user = OsCurrUserGet();  // 获取当前用户
    for (count = 0; count < user->groupNumber; count++) {  // 遍历用户组列表
        if (user->groups[count] == gid) {  // 检查是否为目标组
            SCHEDULER_UNLOCK(intSave);  // 恢复调度器
            return TRUE;  // 找到目标组，返回TRUE
        }
    }

    SCHEDULER_UNLOCK(intSave);  // 恢复调度器
    return FALSE;  // 未找到目标组，返回FALSE
}
#endif

/**
 * @brief 获取当前用户ID
 * @details 根据配置选项，获取当前进程的用户ID，支持用户容器功能
 * @return INT32 当前用户ID，未启用能力集时返回0
 */
LITE_OS_SEC_TEXT INT32 LOS_GetUserID(VOID)
{
#ifdef LOSCFG_SECURITY_CAPABILITY  // 若启用能力集安全配置
    UINT32 intSave;  // 中断状态保存变量
    INT32 uid;  // 用户ID变量

    SCHEDULER_LOCK(intSave);  // 关闭调度器
#ifdef LOSCFG_USER_CONTAINER  // 若启用用户容器配置
    uid = OsFromKuidMunged(OsCurrentUserContainer(), CurrentCredentials()->uid);  // 从容器中获取用户ID
#else
    uid = (INT32)OsCurrUserGet()->userID;  // 直接获取当前用户ID
#endif
    SCHEDULER_UNLOCK(intSave);  // 恢复调度器
    return uid;  // 返回用户ID
#else
    return 0;  // 未启用能力集，返回0
#endif
}

/**
 * @brief 获取当前组ID
 * @details 根据配置选项，获取当前进程的组ID，支持用户容器功能
 * @return INT32 当前组ID，未启用能力集时返回0
 */
LITE_OS_SEC_TEXT INT32 LOS_GetGroupID(VOID)
{
#ifdef LOSCFG_SECURITY_CAPABILITY  // 若启用能力集安全配置
    UINT32 intSave;  // 中断状态保存变量
    INT32 gid;  // 组ID变量

    SCHEDULER_LOCK(intSave);  // 关闭调度器
#ifdef LOSCFG_USER_CONTAINER  // 若启用用户容器配置
    gid = OsFromKgidMunged(OsCurrentUserContainer(), CurrentCredentials()->gid);  // 从容器中获取组ID
#else
    gid = (INT32)OsCurrUserGet()->gid;  // 直接获取当前组ID
#endif
    SCHEDULER_UNLOCK(intSave);  // 恢复调度器

    return gid;  // 返回组ID
#else
    return 0;  // 未启用能力集，返回0
#endif
}

/**
 * @brief 系统进程初始化
 * @details 初始化系统进程控制块，创建进程组及相关资源
 * @param processCB 进程控制块指针
 * @param flags 进程标志（内核态/用户态）
 * @param name 进程名称
 * @return 操作结果，LOS_OK表示成功，其他值表示失败
 */
STATIC UINT32 OsSystemProcessInit(LosProcessCB *processCB, UINT32 flags, const CHAR *name)
{
    UINT32 ret = OsInitPCB(processCB, flags, name);  // 初始化进程控制块
    if (ret != LOS_OK) {  // 初始化失败
        goto EXIT;  // 跳转到退出处理
    }

#ifdef LOSCFG_FS_VFS
    processCB->files = alloc_files();  // 分配文件描述符表
    if (processCB->files == NULL) {  // 分配失败
        ret = LOS_ENOMEM;  // 设置内存不足错误
        goto EXIT;  // 跳转到退出处理
    }
#endif

    ProcessGroup *pgroup = OsCreateProcessGroup(processCB);  // 创建进程组
    if (pgroup == NULL) {  // 创建失败
        ret = LOS_ENOMEM;  // 设置内存不足错误
        goto EXIT;  // 跳转到退出处理
    }

#ifdef LOSCFG_SECURITY_CAPABILITY
    processCB->user = OsCreateUser(0, 0, 1);  // 创建用户信息（UID=0，GID=0，组数量=1）
    if (processCB->user == NULL) {  // 创建失败
        ret = LOS_ENOMEM;  // 设置内存不足错误
        goto EXIT;  // 跳转到退出处理
    }
#endif

#ifdef LOSCFG_KERNEL_PLIMITS
    ret = OsPLimitsAddProcess(NULL, processCB);  // 添加进程到资源限制管理
    if (ret != LOS_OK) {  // 添加失败
        ret = LOS_ENOMEM;  // 设置内存不足错误
        goto EXIT;  // 跳转到退出处理
    }
#endif
    return LOS_OK;  // 返回成功

EXIT:
    OsDeInitPCB(processCB);  // 反初始化进程控制块
    return ret;  // 返回错误码
}

/**
 * @brief 创建系统进程
 * @details 创建内核初始化进程和空闲进程，建立进程间关系
 * @return 操作结果，LOS_OK表示成功，其他值表示失败
 */
LITE_OS_SEC_TEXT_INIT UINT32 OsSystemProcessCreate(VOID)
{
    LosProcessCB *kerInitProcess = OsGetKernelInitProcess();  // 获取内核初始化进程控制块
    UINT32 ret = OsSystemProcessInit(kerInitProcess, OS_KERNEL_MODE, "KProcess");  // 初始化内核进程
    if (ret != LOS_OK) {  // 初始化失败
        return ret;  // 返回错误码
    }
    kerInitProcess->processStatus &= ~OS_PROCESS_STATUS_INIT;  // 清除初始化状态标志

    LosProcessCB *idleProcess = OsGetIdleProcess();  // 获取空闲进程控制块
    ret = OsInitPCB(idleProcess, OS_KERNEL_MODE, "KIdle");  // 初始化空闲进程
    if (ret != LOS_OK) {  // 初始化失败
        return ret;  // 返回错误码
    }
    idleProcess->parentProcess = kerInitProcess;  // 设置父进程为内核初始化进程
    LOS_ListTailInsert(&kerInitProcess->childrenList, &idleProcess->siblingList);  // 将空闲进程添加到子进程列表
    idleProcess->pgroup = kerInitProcess->pgroup;  // 继承进程组
    LOS_ListTailInsert(&kerInitProcess->pgroup->processList, &idleProcess->subordinateGroupList);  // 添加到进程组列表
#ifdef LOSCFG_SECURITY_CAPABILITY
    idleProcess->user = kerInitProcess->user;  // 继承用户信息
#endif
#ifdef LOSCFG_FS_VFS
    idleProcess->files = kerInitProcess->files;  // 继承文件描述符表
#endif
    idleProcess->processStatus &= ~OS_PROCESS_STATUS_INIT;  // 清除初始化状态标志

    ret = OsIdleTaskCreate((UINTPTR)idleProcess);  // 创建空闲任务
    if (ret != LOS_OK) {  // 创建失败
        return ret;  // 返回错误码
    }
    return LOS_OK;  // 返回成功
}

/**
 * @brief 调度参数检查
 * @details 验证调度策略和参数的有效性
 * @param policy 调度策略（LOS_SCHED_RR/LOS_SCHED_FIFO/LOS_SCHED_DEADLINE）
 * @param isThread 是否为线程（TRUE表示线程，FALSE表示进程）
 * @param param 调度参数结构体指针
 * @return 操作结果，LOS_OK表示有效，其他值表示无效
 */
INT32 OsSchedulerParamCheck(UINT16 policy, BOOL isThread, const LosSchedParam *param)
{
    if (param == NULL) {  // 参数为空
        return LOS_EINVAL;  // 返回无效参数错误
    }

    if ((policy == LOS_SCHED_RR) || (isThread && (policy == LOS_SCHED_FIFO))) {  // RR策略或线程FIFO策略
        if ((param->priority < OS_PROCESS_PRIORITY_HIGHEST) ||  // 优先级低于最高值
            (param->priority > OS_PROCESS_PRIORITY_LOWEST)) {  // 优先级高于最低值
            return LOS_EINVAL;  // 返回无效参数错误
        }
        return LOS_OK;  // 参数有效
    }

    if (policy == LOS_SCHED_DEADLINE) {  // 截止期限调度策略
        if ((param->runTimeUs < OS_SCHED_EDF_MIN_RUNTIME) || (param->runTimeUs >= param->deadlineUs)) {  // 运行时间无效
            return LOS_EINVAL;  // 返回无效参数错误
        }
        if ((param->deadlineUs < OS_SCHED_EDF_MIN_DEADLINE) || (param->deadlineUs > OS_SCHED_EDF_MAX_DEADLINE)) {  // 截止期限无效
            return LOS_EINVAL;  // 返回无效参数错误
        }
        if (param->periodUs < param->deadlineUs) {  // 周期小于截止期限
            return LOS_EINVAL;  // 返回无效参数错误
        }
        return LOS_OK;  // 参数有效
    }

    return LOS_EINVAL;  // 不支持的调度策略
}

/**
 * @brief 进程调度参数检查（内联函数）
 * @details 验证进程调度参数的有效性
 * @param which 参数类型（必须为LOS_PRIO_PROCESS）
 * @param pid 进程ID
 * @param policy 调度策略
 * @param param 调度参数结构体指针
 * @return 操作结果，LOS_OK表示有效，其他值表示无效
 */
STATIC INLINE INT32 ProcessSchedulerParamCheck(INT32 which, INT32 pid, UINT16 policy, const LosSchedParam *param)
{
    if (OS_PID_CHECK_INVALID(pid)) {  // PID无效
        return LOS_EINVAL;  // 返回无效参数错误
    }

    if (which != LOS_PRIO_PROCESS) {  // 不是进程优先级类型
        return LOS_EINVAL;  // 返回无效参数错误
    }

    return OsSchedulerParamCheck(policy, FALSE, param);  // 调用调度参数检查函数（进程类型）
}

#ifdef LOSCFG_SECURITY_CAPABILITY
/**
 * @brief 进程调度权限检查
 * @details 检查当前进程是否有修改目标进程调度参数的权限
 * @param processCB 目标进程控制块
 * @param param 当前调度参数
 * @param policy 新调度策略
 * @param prio 新优先级
 * @return 权限检查结果，TRUE表示有权限，FALSE表示无权限
 */
STATIC BOOL OsProcessCapPermitCheck(const LosProcessCB *processCB, const SchedParam *param, UINT16 policy, UINT16 prio)
{
    LosProcessCB *runProcess = OsCurrProcessGet();  // 获取当前运行进程

    /* always trust kernel process */
    if (!OsProcessIsUserMode(runProcess)) {  // 内核进程拥有全部权限
        return TRUE;  // 权限允许
    }

    /* user mode process can reduce the priority of itself */
    if ((runProcess->processID == processCB->processID) && (policy == LOS_SCHED_RR) && (prio > param->basePrio)) {  // 自身进程降低优先级
        return TRUE;  // 权限允许
    }

    /* user mode process with privilege of CAP_SCHED_SETPRIORITY can change the priority */
    if (IsCapPermit(CAP_SCHED_SETPRIORITY)) {  // 拥有CAP_SCHED_SETPRIORITY capability
        return TRUE;  // 权限允许
    }
    return FALSE;  // 权限拒绝
}
#endif

/**
 * @brief 设置进程调度策略和参数
 * @details 修改指定进程的调度策略和相关参数，并在需要时触发调度
 * @param which 参数类型（LOS_PRIO_PROCESS）
 * @param pid 进程ID
 * @param policy 调度策略
 * @param schedParam 调度参数结构体指针
 * @return 操作结果，LOS_OK表示成功，其他值表示失败
 */
LITE_OS_SEC_TEXT INT32 OsSetProcessScheduler(INT32 which, INT32 pid, UINT16 policy, const LosSchedParam *schedParam)
{
    SchedParam param = { 0 };  // 调度参数结构体
    BOOL needSched = FALSE;  // 是否需要调度标志
    UINT32 intSave;  // 中断状态保存值

    INT32 ret = ProcessSchedulerParamCheck(which, pid, policy, schedParam);  // 检查调度参数有效性
    if (ret != LOS_OK) {  // 参数无效
        return -ret;  // 返回错误码
    }

    LosProcessCB *processCB = OS_PCB_FROM_PID(pid);  // 通过PID获取进程控制块
    SCHEDULER_LOCK(intSave);  // 锁定调度器
    if (OsProcessIsInactive(processCB)) {  // 进程未激活
        SCHEDULER_UNLOCK(intSave);  // 解锁调度器
        return -LOS_ESRCH;  // 返回进程不存在错误
    }

    LosTaskCB *taskCB = processCB->threadGroup;  // 获取进程的主线程控制块
    taskCB->ops->schedParamGet(taskCB, &param);  // 获取当前调度参数

#ifdef LOSCFG_SECURITY_CAPABILITY
    if (!OsProcessCapPermitCheck(processCB, &param, policy, schedParam->priority)) {  // 权限检查失败
        SCHEDULER_UNLOCK(intSave);  // 解锁调度器
        return -LOS_EPERM;  // 返回权限不足错误
    }
#endif

    if (param.policy != policy) {  // 调度策略变更
        if (policy == LOS_SCHED_DEADLINE) { /* HPF -> EDF */  // 从HPF策略切换到EDF策略
            if (processCB->threadNumber > 1) {  // 多线程进程不支持EDF
                SCHEDULER_UNLOCK(intSave);  // 解锁调度器
                return -LOS_EPERM;  // 返回操作不允许错误
            }
            OsSchedParamInit(taskCB, policy, NULL, schedParam);  // 初始化EDF调度参数
            needSched = TRUE;  // 需要重新调度
            goto TO_SCHED;  // 跳转到调度处理
        } else if (param.policy == LOS_SCHED_DEADLINE) { /* EDF -> HPF */  // 从EDF策略切换到HPF策略
            SCHEDULER_UNLOCK(intSave);  // 解锁调度器
            return -LOS_EPERM;  // 返回操作不允许错误
        }
    }

    if (policy == LOS_SCHED_DEADLINE) {  // EDF调度策略
        param.runTimeUs = schedParam->runTimeUs;  // 设置运行时间
        param.deadlineUs = schedParam->deadlineUs;  // 设置截止期限
        param.periodUs = schedParam->periodUs;  // 设置周期
    } else {  // RR/FIFO调度策略
        param.basePrio = schedParam->priority;  // 设置基础优先级
    }
    needSched = taskCB->ops->schedParamModify(taskCB, &param);  // 修改调度参数并判断是否需要调度

TO_SCHED:
    SCHEDULER_UNLOCK(intSave);  // 解锁调度器

    LOS_MpSchedule(OS_MP_CPU_ALL);  // 多CPU调度通知
    if (needSched && OS_SCHEDULER_ACTIVE) {  // 需要调度且调度器激活
        LOS_Schedule();  // 触发调度
    }
    return LOS_OK;  // 返回成功
}
/**
 * @brief 设置进程调度策略
 * @details 该函数用于设置指定进程的调度策略和调度参数
 * @param pid 进程ID
 * @param policy 调度策略
 * @param schedParam 调度参数结构体指针
 * @return 操作结果，LOS_OK表示成功，其他值表示失败
 */
LITE_OS_SEC_TEXT INT32 LOS_SetProcessScheduler(INT32 pid, UINT16 policy, const LosSchedParam *schedParam)
{
    return OsSetProcessScheduler(LOS_PRIO_PROCESS, pid, policy, schedParam);  // 调用内部函数设置进程调度策略
}

/**
 * @brief 获取进程调度策略和参数
 * @details 该函数用于获取指定进程的调度策略和调度参数信息
 * @param pid 进程ID
 * @param policy [输出] 调度策略指针
 * @param schedParam [输出] 调度参数结构体指针
 * @return 操作结果，LOS_OK表示成功，其他值表示失败
 */
LITE_OS_SEC_TEXT INT32 LOS_GetProcessScheduler(INT32 pid, INT32 *policy, LosSchedParam *schedParam)
{
    UINT32 intSave;  // 中断状态保存值
    SchedParam param = { 0 };  // 调度参数结构体

    if (OS_PID_CHECK_INVALID(pid)) {  // 检查PID是否有效
        return -LOS_EINVAL;  // 返回无效参数错误
    }

    if ((policy == NULL) && (schedParam == NULL)) {  // 检查输出参数是否至少提供一个
        return -LOS_EINVAL;  // 返回无效参数错误
    }

    SCHEDULER_LOCK(intSave);  // 锁定调度器
    LosProcessCB *processCB = OS_PCB_FROM_PID(pid);  // 通过PID获取进程控制块
    if (OsProcessIsUnused(processCB)) {  // 检查进程是否未使用
        SCHEDULER_UNLOCK(intSave);  // 解锁调度器
        return -LOS_ESRCH;  // 返回进程不存在错误
    }

    LosTaskCB *taskCB = processCB->threadGroup;  // 获取进程的主线程控制块
    taskCB->ops->schedParamGet(taskCB, &param);  // 获取线程调度参数
    SCHEDULER_UNLOCK(intSave);  // 解锁调度器

    if (policy != NULL) {  // 如果需要获取调度策略
        if (param.policy == LOS_SCHED_FIFO) {  // FIFO策略映射为RR
            *policy = LOS_SCHED_RR;  // 设置调度策略为RR
        } else {
            *policy = param.policy;  // 设置实际调度策略
        }
    }

    if (schedParam != NULL) {  // 如果需要获取调度参数
        if (param.policy == LOS_SCHED_DEADLINE) {  // 截止期限调度策略
            schedParam->runTimeUs = param.runTimeUs;  // 设置运行时间
            schedParam->deadlineUs = param.deadlineUs;  // 设置截止期限
            schedParam->periodUs = param.periodUs;  // 设置周期
        } else {
            schedParam->priority = param.basePrio;  // 设置优先级
        }
    }
    return LOS_OK;  // 返回成功
}

/**
 * @brief 设置进程优先级
 * @details 该函数用于设置指定进程的调度优先级
 * @param pid 进程ID
 * @param prio 优先级值
 * @return 操作结果，LOS_OK表示成功，其他值表示失败
 */
LITE_OS_SEC_TEXT INT32 LOS_SetProcessPriority(INT32 pid, INT32 prio)
{
    INT32 ret;  // 函数返回值
    INT32 policy;  // 调度策略
    LosSchedParam param = {  // 调度参数结构体
        .priority = prio,  // 设置优先级
    };

    ret = LOS_GetProcessScheduler(pid, &policy, NULL);  // 获取当前调度策略
    if (ret != LOS_OK) {  // 获取失败
        return ret;  // 返回错误码
    }

    if (policy == LOS_SCHED_DEADLINE) {  // 截止期限调度策略不支持优先级设置
        return -LOS_EINVAL;  // 返回无效参数错误
    }

    return OsSetProcessScheduler(LOS_PRIO_PROCESS, pid, (UINT16)policy, &param);  // 调用内部函数设置调度参数
}

/**
 * @brief 获取进程优先级（内部接口）
 * @details 该函数用于内部获取指定进程的优先级
 * @param which 指定获取类型，必须为LOS_PRIO_PROCESS
 * @param pid 进程ID
 * @return 成功返回优先级值，失败返回错误码
 */
LITE_OS_SEC_TEXT INT32 OsGetProcessPriority(INT32 which, INT32 pid)
{
    UINT32 intSave;  // 中断状态保存值
    SchedParam param = { 0 };  // 调度参数结构体
    (VOID)which;  // 未使用参数

    if (OS_PID_CHECK_INVALID(pid)) {  // 检查PID是否有效
        return -LOS_EINVAL;  // 返回无效参数错误
    }

    if (which != LOS_PRIO_PROCESS) {  // 检查类型是否为进程优先级
        return -LOS_EINVAL;  // 返回无效参数错误
    }

    LosProcessCB *processCB = OS_PCB_FROM_PID(pid);  // 通过PID获取进程控制块
    SCHEDULER_LOCK(intSave);  // 锁定调度器
    if (OsProcessIsUnused(processCB)) {  // 检查进程是否未使用
        SCHEDULER_UNLOCK(intSave);  // 解锁调度器
        return -LOS_ESRCH;  // 返回进程不存在错误
    }

    LosTaskCB *taskCB = processCB->threadGroup;  // 获取进程的主线程控制块
    taskCB->ops->schedParamGet(taskCB, &param);  // 获取线程调度参数

    if (param.policy == LOS_SCHED_DEADLINE) {  // 截止期限调度策略没有传统优先级
        SCHEDULER_UNLOCK(intSave);  // 解锁调度器
        return -LOS_EINVAL;  // 返回无效参数错误
    }

    SCHEDULER_UNLOCK(intSave);  // 解锁调度器
    return param.basePrio;  // 返回基础优先级
}

/**
 * @brief 获取进程优先级
 * @details 该函数用于获取指定进程的优先级
 * @param pid 进程ID
 * @return 成功返回优先级值，失败返回错误码
 */
LITE_OS_SEC_TEXT INT32 LOS_GetProcessPriority(INT32 pid)
{
    return OsGetProcessPriority(LOS_PRIO_PROCESS, pid);  // 调用内部函数获取进程优先级
}

/**
 * @brief 按顺序将任务插入等待列表
 * @details 该函数根据任务的等待标志将任务按特定顺序插入进程的等待列表
 * @param runTask 当前运行任务控制块
 * @param processCB 进程控制块
 */
STATIC VOID OsWaitInsertWaitListInOrder(LosTaskCB *runTask, LosProcessCB *processCB)
{
    LOS_DL_LIST *head = &processCB->waitList;  // 等待列表头节点
    LOS_DL_LIST *list = head;  // 列表遍历指针
    LosTaskCB *taskCB = NULL;  // 任务控制块指针

    if (runTask->waitFlag == OS_PROCESS_WAIT_GID) {  // 按进程组等待
        while (list->pstNext != head) {  // 遍历等待列表
            taskCB = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(list));  // 获取列表中的任务
            if (taskCB->waitFlag == OS_PROCESS_WAIT_PRO) {  // 如果是按进程等待的任务
                list = list->pstNext;  // 继续向后查找
                continue;
            }
            break;  // 找到插入位置
        }
    } else if (runTask->waitFlag == OS_PROCESS_WAIT_ANY) {  // 等待任意子进程
        while (list->pstNext != head) {  // 遍历等待列表
            taskCB = OS_TCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(list));  // 获取列表中的任务
            if (taskCB->waitFlag != OS_PROCESS_WAIT_ANY) {  // 如果不是等待任意子进程的任务
                list = list->pstNext;  // 继续向后查找
                continue;
            }
            break;  // 找到插入位置
        }
    }
    /* if runTask->waitFlag == OS_PROCESS_WAIT_PRO,
     * this node is inserted directly into the header of the waitList
     */
    (VOID)runTask->ops->wait(runTask, list->pstNext, LOS_WAIT_FOREVER);  // 将任务插入等待列表
    return;
}

/**
 * @brief 查找指定PID的子进程
 * @details 该函数查找指定PID的子进程是否存在并已退出，或设置等待标志
 * @param pid 要查找的进程PID
 * @param runTask 当前运行任务控制块
 * @param processCB 当前进程控制块
 * @param childCB [输出] 查找到的子进程控制块指针
 * @return 操作结果，LOS_OK表示成功，其他值表示失败
 */
STATIC UINT32 WaitFindSpecifiedProcess(UINT32 pid, LosTaskCB *runTask,
                                       const LosProcessCB *processCB, LosProcessCB **childCB)
{
    if (OS_PID_CHECK_INVALID((UINT32)pid)) {  // 检查PID是否有效
        return LOS_ECHILD;  // 返回子进程不存在错误
    }

    LosProcessCB *waitProcess = OS_PCB_FROM_PID(pid);  // 通过PID获取进程控制块
    if (OsProcessIsUnused(waitProcess)) {  // 检查进程是否未使用
        return LOS_ECHILD;  // 返回子进程不存在错误
    }

#ifdef LOSCFG_PID_CONTAINER
    if (OsPidContainerProcessParentIsRealParent(waitProcess, processCB)) {  // 检查是否为PID容器中的真实父进程
        *childCB = (LosProcessCB *)processCB;  // 设置子进程控制块
        return LOS_OK;  // 返回成功
    }
#endif
    /* Wait for the child process whose process number is pid. */
    *childCB = OsFindExitChildProcess(processCB, waitProcess);  // 查找已退出的子进程
        if (*childCB != NULL) {  // 找到已退出的子进程
        return LOS_OK;  // 返回成功
        }

        if (OsFindChildProcess(processCB, waitProcess) != LOS_OK) {  // 检查是否为当前进程的子进程
            return LOS_ECHILD;  // 返回子进程不存在错误
        }
        runTask->waitFlag = OS_PROCESS_WAIT_PRO;  // 设置等待标志为按进程等待
        runTask->waitID = (UINTPTR)waitProcess;  // 设置等待的进程控制块
    return LOS_OK;  // 返回成功
}

/**
 * @brief 根据PID设置等待标志并查找目标子进程
 * @details 该函数根据传入的PID参数类型，设置当前任务的等待标志，并查找符合条件的已退出子进程
 * @param processCB 当前进程控制块指针
 * @param pid 等待的进程ID，支持以下类型：
 *        - pid > 0: 等待指定PID的子进程
 *        - pid == 0: 等待同一进程组中的任何子进程
 *        - pid == -1: 等待任何子进程
 *        - pid < -1: 等待进程组ID为pid绝对值的任何子进程
 * @param child [输出] 查找到的子进程控制块指针
 * @return 操作结果，LOS_OK表示成功，其他值表示失败
 */
STATIC UINT32 OsWaitSetFlag(const LosProcessCB *processCB, INT32 pid, LosProcessCB **child)
{
    UINT32 ret;  // 函数返回值
    LosProcessCB *childCB = NULL;  // 子进程控制块指针
    LosTaskCB *runTask = OsCurrTaskGet();  // 获取当前运行任务控制块

    if (pid > 0) {  // 等待指定PID的子进程
        ret = WaitFindSpecifiedProcess((UINT32)pid, runTask, processCB, &childCB);  // 查找指定PID的子进程
        if (ret != LOS_OK) {  // 查找失败
            return ret;  // 返回错误码
        }
        if (childCB != NULL) {  // 找到已退出的子进程
            goto WAIT_BACK;  // 跳转到返回处理
        }
    } else if (pid == 0) {  // 等待同一进程组中的任何子进程
        /* Wait for any child process in the same process group */
        childCB = OsFindGroupExitProcess(processCB->pgroup, OS_INVALID_VALUE);  // 查找进程组中已退出的子进程
        if (childCB != NULL) {  // 找到已退出的子进程
            goto WAIT_BACK;  // 跳转到返回处理
        }
        runTask->waitID = (UINTPTR)OS_GET_PGROUP_LEADER(processCB->pgroup);  // 设置等待的进程组领导者ID
        runTask->waitFlag = OS_PROCESS_WAIT_GID;  // 设置等待标志为按进程组等待
    } else if (pid == -1) {  // 等待任何子进程
        /* Wait for any child process */
        childCB = OsFindExitChildProcess(processCB, NULL);  // 查找任意已退出的子进程
        if (childCB != NULL) {  // 找到已退出的子进程
            goto WAIT_BACK;  // 跳转到返回处理
        }
        runTask->waitID = pid;  // 设置等待的PID为-1
        runTask->waitFlag = OS_PROCESS_WAIT_ANY;  // 设置等待标志为等待任意子进程
    } else { /* pid < -1 */  // 等待指定进程组中的任何子进程
        /* Wait for any child process whose group number is the pid absolute value. */
        ProcessGroup *pgroup = OsFindProcessGroup(-pid);  // 查找PID绝对值对应的进程组
        if (pgroup == NULL) {  // 进程组不存在
            return LOS_ECHILD;  // 返回子进程不存在错误
        }

        childCB = OsFindGroupExitProcess(pgroup, OS_INVALID_VALUE);  // 查找进程组中已退出的子进程
        if (childCB != NULL) {  // 找到已退出的子进程
            goto WAIT_BACK;  // 跳转到返回处理
        }

        runTask->waitID = (UINTPTR)OS_GET_PGROUP_LEADER(pgroup);  // 设置等待的进程组领导者ID
        runTask->waitFlag = OS_PROCESS_WAIT_GID;  // 设置等待标志为按进程组等待
    }

WAIT_BACK:
    *child = childCB;  // 输出查找到的子进程控制块
    return LOS_OK;  // 返回成功
}

/**
 * @brief 回收子进程资源并设置退出状态
 * @details 该函数负责回收已退出子进程的资源，设置退出状态信息，并释放进程组资源
 * @param childCB 子进程控制块指针
 * @param intSave 中断状态保存值
 * @param status [输出] 子进程退出状态指针
 * @param info [输出] 子进程信号信息指针
 * @return 回收的子进程PID
 */
STATIC UINT32 OsWaitRecycleChildProcess(const LosProcessCB *childCB, UINT32 intSave, INT32 *status, siginfo_t *info)
{
    ProcessGroup *pgroup = NULL;  // 进程组指针
    UINT32 pid  = OsGetPid(childCB);  // 获取子进程PID
    UINT16 mode = childCB->processMode;  // 获取进程模式（用户态/内核态）
    INT32 exitCode = childCB->exitCode;  // 获取子进程退出码
    UINT32 uid = 0;  // 用户ID

#ifdef LOSCFG_SECURITY_CAPABILITY
    if (childCB->user != NULL) {  // 如果用户信息存在
        uid = childCB->user->userID;  // 获取用户ID
    }
#endif

    OsRecycleZombiesProcess((LosProcessCB *)childCB, &pgroup);  // 回收僵尸进程资源
    SCHEDULER_UNLOCK(intSave);  // 解锁调度器

    if (status != NULL) {  // 如果需要设置退出状态
        if (mode == OS_USER_MODE) {  // 用户态进程
            (VOID)LOS_ArchCopyToUser((VOID *)status, (const VOID *)(&(exitCode)), sizeof(INT32));  // 拷贝退出码到用户空间
        } else {  // 内核态进程
            *status = exitCode;  // 直接设置退出码
        }
    }
    /* get signal info */
    if (info != NULL) {  // 如果需要设置信号信息
        siginfo_t tempinfo = { 0 };  // 临时信号信息结构

        tempinfo.si_signo = SIGCHLD;  // 信号类型为子进程状态改变
        tempinfo.si_errno = 0;  // 错误码
        tempinfo.si_pid = pid;  // 子进程PID
        tempinfo.si_uid = uid;  // 子进程用户ID
        /*
         * Process exit code
         * 31	 15 		  8 		  7 	   0
         * |	 | exit code  | core dump | signal |
         */
        if ((exitCode & 0x7f) == 0) {  // 正常退出（低7位为0）
            tempinfo.si_code = CLD_EXITED;  // 退出码为正常退出
            tempinfo.si_status = (exitCode >> 8U);  // 提取退出状态码（8-15位）
        } else {  // 信号终止
            tempinfo.si_code = (exitCode & 0x80) ? CLD_DUMPED : CLD_KILLED;  // 判断是否产生core dump
            tempinfo.si_status = (exitCode & 0x7f);  // 提取终止信号（0-7位）
        }

        if (mode == OS_USER_MODE) {  // 用户态进程
            (VOID)LOS_ArchCopyToUser((VOID *)(info), (const VOID *)(&(tempinfo)), sizeof(siginfo_t));  // 拷贝信号信息到用户空间
        } else {  // 内核态进程
            (VOID)memcpy_s((VOID *)(info), sizeof(siginfo_t), (const VOID *)(&(tempinfo)), sizeof(siginfo_t));  // 拷贝信号信息
        }
    }
    (VOID)LOS_MemFree(m_aucSysMem1, pgroup);  // 释放进程组内存
    return pid;  // 返回回收的子进程PID
}

/**
 * @brief 检查是否有可等待的子进程
 * @details 该函数检查当前进程是否有子进程或已退出子进程，若无可等待子进程则返回错误
 * @param processCB 当前进程控制块指针
 * @param pid 等待的进程ID
 * @param childCB [输出] 查找到的子进程控制块指针
 * @return 操作结果，LOS_OK表示存在可等待子进程，LOS_ECHILD表示无子进程可等待
 */
STATIC UINT32 OsWaitChildProcessCheck(LosProcessCB *processCB, INT32 pid, LosProcessCB **childCB)
{	
    if (LOS_ListEmpty(&(processCB->childrenList)) && LOS_ListEmpty(&(processCB->exitChildList))) {  // 检查子进程列表和已退出子进程列表是否均为空
        return LOS_ECHILD;  // 无子进程可等待，返回错误
    }

    return OsWaitSetFlag(processCB, pid, childCB);  // 调用OsWaitSetFlag设置等待标志并查找子进程
}

/**
 * @brief 检查等待选项的有效性
 * @details 该函数验证等待选项参数的合法性，不支持的选项将返回错误
 * @param options 等待选项，支持LOS_WAIT_WNOHANG
 * @return 操作结果，LOS_OK表示选项有效，其他值表示无效
 */
STATIC UINT32 OsWaitOptionsCheck(UINT32 options)
{
    UINT32 flag = LOS_WAIT_WNOHANG | LOS_WAIT_WUNTRACED | LOS_WAIT_WCONTINUED;  // 支持的选项掩码

    flag = ~flag & options;  // 检查是否包含不支持的选项
    if (flag != 0) {  // 存在不支持的选项
        return LOS_EINVAL;  // 返回无效参数错误
    }

    if ((options & (LOS_WAIT_WCONTINUED | LOS_WAIT_WUNTRACED)) != 0) {  // 检查是否包含未实现的选项
        return LOS_EOPNOTSUPP;  // 返回操作不支持错误
    }

    if (OS_INT_ACTIVE) {  // 检查当前是否在中断上下文中
        return LOS_EINTR;  // 返回中断错误
    }

    return LOS_OK;  // 选项有效，返回成功
}

/**
 * @brief 等待子进程状态变化
 * @details 等待指定子进程退出或状态变化，支持非阻塞等待和回收子进程资源
 * @param pid 要等待的进程ID，-1表示等待任意子进程
 * @param status 用于存储子进程退出状态的指针
 * @param info 用于存储信号信息的指针
 * @param options 等待选项，如LOS_WAIT_WNOHANG表示非阻塞等待
 * @param rusage 资源使用信息指针（未使用）
 * @return 成功返回子进程ID，失败返回负数错误码
 */
STATIC INT32 OsWait(INT32 pid, USER INT32 *status, USER siginfo_t *info, UINT32 options, VOID *rusage)
{
    (VOID)rusage;  // 未使用的参数
    UINT32 ret;  // 函数返回值
    UINT32 intSave;  // 中断保存标志
    LosProcessCB *childCB = NULL;  // 子进程控制块指针

    LosProcessCB *processCB = OsCurrProcessGet();  // 获取当前进程控制块
    LosTaskCB *runTask = OsCurrTaskGet();  // 获取当前任务控制块
    SCHEDULER_LOCK(intSave);  // 关闭调度器
    ret = OsWaitChildProcessCheck(processCB, pid, &childCB);  // 检查等待的子进程是否存在
    if (ret != LOS_OK) {  // 检查子进程检查结果
        pid = -ret;  // 设置错误码
        goto ERROR;  // 跳转到错误处理
    }

    if (childCB != NULL) {  // 如果找到子进程
#ifdef LOSCFG_PID_CONTAINER
        if (childCB == processCB) {  // 检查是否为当前进程自身（容器场景）
            SCHEDULER_UNLOCK(intSave);  // 开启调度器
            if (status != NULL) {  // 如果状态指针有效
                (VOID)LOS_ArchCopyToUser((VOID *)status, (const VOID *)(&ret), sizeof(INT32));  // 拷贝状态到用户空间
            }
            return pid;  // 返回进程ID
        }
#endif
        return (INT32)OsWaitRecycleChildProcess(childCB, intSave, status, info);  // 回收子进程资源并返回
    }

    if ((options & LOS_WAIT_WNOHANG) != 0) {  // 如果是非阻塞等待
        runTask->waitFlag = 0;  // 清除等待标志
        pid = 0;  // 非阻塞模式下没有子进程退出，返回0
        goto ERROR;  // 跳转到错误处理
    }

    OsWaitInsertWaitListInOrder(runTask, processCB);  // 将任务插入等待列表

    runTask->waitFlag = 0;  // 清除等待标志
    if (runTask->waitID == OS_INVALID_VALUE) {  // 检查等待ID是否无效
        pid = -LOS_ECHILD;  // 设置没有子进程错误
        goto ERROR;  // 跳转到错误处理
    }

    childCB = (LosProcessCB *)runTask->waitID;  // 获取等待的子进程
    if (!OsProcessIsDead(childCB)) {  // 检查子进程是否已退出
        pid = -LOS_ESRCH;  // 设置进程不存在错误
        goto ERROR;  // 跳转到错误处理
    }

    return (INT32)OsWaitRecycleChildProcess(childCB, intSave, status, info);  // 回收子进程资源并返回

ERROR:
    SCHEDULER_UNLOCK(intSave);  // 开启调度器
    return pid;  // 返回错误码
}

/**
 * @brief 系统调用：等待子进程
 * @details 提供用户态接口，用于等待子进程退出并获取退出状态
 * @param pid 要等待的进程ID，-1表示等待任意子进程
 * @param status 用于存储子进程退出状态的指针
 * @param options 等待选项
 * @param rusage 资源使用信息指针（未使用）
 * @return 成功返回子进程ID，失败返回负数错误码
 */
LITE_OS_SEC_TEXT INT32 LOS_Wait(INT32 pid, USER INT32 *status, UINT32 options, VOID *rusage)
{
    (VOID)rusage;  // 未使用的参数
    UINT32 ret;  // 函数返回值

    ret = OsWaitOptionsCheck(options);  // 检查等待选项合法性
    if (ret != LOS_OK) {  // 检查选项检查结果
        return -ret;  // 返回错误码
    }

    return OsWait(pid, status, NULL, options, NULL);  // 调用OsWait执行等待操作
}

/**
 * @brief 检查waitid选项合法性
 * @details 验证waitid系统调用的选项参数是否支持
 * @param options 等待选项
 * @return 操作结果，LOS_OK表示成功，其他值表示失败
 */
STATIC UINT32 OsWaitidOptionsCheck(UINT32 options)
{
    UINT32 flag = LOS_WAIT_WNOHANG | LOS_WAIT_WSTOPPED | LOS_WAIT_WCONTINUED | LOS_WAIT_WEXITED | LOS_WAIT_WNOWAIT;  // 支持的选项掩码

    flag = ~flag & options;  // 检查是否包含不支持的选项
    if ((flag != 0) || (options == 0)) {  // 如果有不支持的选项或选项为空
        return LOS_EINVAL;  // 返回无效参数错误
    }

    /*
     * 仅支持 LOS_WAIT_WNOHANG | LOS_WAIT_WEXITED
     * 不支持 LOS_WAIT_WSTOPPED | LOS_WAIT_WCONTINUED | LOS_WAIT_WNOWAIT
     */
    if ((options & (LOS_WAIT_WSTOPPED | LOS_WAIT_WCONTINUED | LOS_WAIT_WNOWAIT)) != 0) {  // 检查是否包含不支持的选项
        return LOS_EOPNOTSUPP;  // 返回操作不支持错误
    }

    if (OS_INT_ACTIVE) {  // 检查是否在中断上下文中
        return LOS_EINTR;  // 返回中断错误
    }

    return LOS_OK;  // 返回成功
}

/**
 * @brief 系统调用：等待子进程状态变化（扩展版）
 * @details 提供更灵活的子进程等待接口，支持更多状态查询
 * @param pid 要等待的进程ID
 * @param info 用于存储信号信息的指针
 * @param options 等待选项
 * @param rusage 资源使用信息指针（未使用）
 * @return 成功返回0，失败返回负数错误码
 */
LITE_OS_SEC_TEXT INT32 LOS_Waitid(INT32 pid, USER siginfo_t *info, UINT32 options, VOID *rusage)
{
    (VOID)rusage;  // 未使用的参数
    UINT32 ret;  // 函数返回值

    /* 检查选项值 */
    ret = OsWaitidOptionsCheck(options);  // 检查waitid选项合法性
    if (ret != LOS_OK) {  // 检查选项检查结果
        return -ret;  // 返回错误码
    }

    return OsWait(pid, NULL, info, options, NULL);  // 调用OsWait执行等待操作
}

/**
 * @brief 获取进程组控制块
 * @details 根据进程ID获取其所属进程组的组长进程控制块
 * @param pid 进程ID
 * @param ppgroupLeader 用于返回进程组组长控制块指针的指针
 * @return 操作结果，LOS_OK表示成功，其他值表示失败
 */
UINT32 OsGetProcessGroupCB(UINT32 pid, UINTPTR *ppgroupLeader)
{
    UINT32 intSave;  // 中断保存标志

    if (OS_PID_CHECK_INVALID(pid) || (ppgroupLeader == NULL)) {  // 检查PID和指针参数合法性
        return LOS_EINVAL;  // 返回无效参数错误
    }

    SCHEDULER_LOCK(intSave);  // 关闭调度器
    LosProcessCB *processCB = OS_PCB_FROM_PID(pid);  // 通过PID获取进程控制块
    if (OsProcessIsUnused(processCB)) {  // 检查进程是否未使用
        SCHEDULER_UNLOCK(intSave);  // 开启调度器
        return LOS_ESRCH;  // 返回进程不存在错误
    }

    *ppgroupLeader = (UINTPTR)OS_GET_PGROUP_LEADER(processCB->pgroup);  // 获取进程组组长
    SCHEDULER_UNLOCK(intSave);  // 开启调度器
    return LOS_OK;  // 返回成功
}

/**
 * @brief 检查设置进程组的合法性
 * @details 验证将进程加入指定进程组的操作是否合法
 * @param processCB 要移动的进程控制块
 * @param pgroupCB 目标进程组的组长进程控制块
 * @return 操作结果，LOS_OK表示成功，其他值表示失败
 */
STATIC UINT32 OsSetProcessGroupCheck(const LosProcessCB *processCB, LosProcessCB *pgroupCB)
{
    LosProcessCB *runProcessCB = OsCurrProcessGet();  // 获取当前进程控制块

    if (OsProcessIsInactive(processCB)) {  // 检查进程是否处于非活动状态
        return LOS_ESRCH;  // 返回进程不存在错误
    }

#ifdef LOSCFG_PID_CONTAINER
    if ((processCB->processID == OS_USER_ROOT_PROCESS_ID) || OS_PROCESS_CONTAINER_CHECK(processCB, runProcessCB)) {  // 检查是否为根进程或容器进程
        return LOS_EPERM;  // 返回权限错误
    }
#endif

    if (!OsProcessIsUserMode(processCB) || !OsProcessIsUserMode(pgroupCB)) {  // 检查进程是否为用户态
        return LOS_EPERM;  // 返回权限错误
    }

    if (runProcessCB == processCB->parentProcess) {  // 如果当前进程是目标进程的父进程
        if (processCB->processStatus & OS_PROCESS_FLAG_ALREADY_EXEC) {  // 检查进程是否已执行exec
            return LOS_EACCES;  // 返回访问权限错误
        }
    } else if (processCB->processID != runProcessCB->processID) {  // 如果不是当前进程且不是父进程
        return LOS_ESRCH;  // 返回进程不存在错误
    }

    /* 将进程添加到另一个已存在的进程组 */
    if (processCB != pgroupCB) {  // 如果进程不是进程组组长
        if (!OsProcessIsPGroupLeader(pgroupCB)) {  // 检查目标进程是否为进程组组长
            return LOS_EPERM;  // 返回权限错误
        }

        if ((pgroupCB->parentProcess != processCB->parentProcess) && (pgroupCB != processCB->parentProcess)) {  // 检查进程组关系
            return LOS_EPERM;  // 返回权限错误
        }
    }

    return LOS_OK;  // 返回成功
}

/**
 * @brief 不安全地设置进程组ID
 * @details 不进行调度器加锁，直接修改进程的组ID，用于已在临界区中的场景
 * @param pid 要修改的进程ID
 * @param gid 目标进程组ID
 * @param pgroup 进程组指针的指针
 * @return 操作结果，LOS_OK表示成功，其他值表示失败
 */
STATIC UINT32 OsSetProcessGroupIDUnsafe(UINT32 pid, UINT32 gid, ProcessGroup **pgroup)
{
    LosProcessCB *processCB = OS_PCB_FROM_PID(pid);  // 通过PID获取进程控制块
    ProcessGroup *rootPGroup = OS_ROOT_PGRP(OsCurrProcessGet());  // 获取根进程组
    LosProcessCB *pgroupCB = OS_PCB_FROM_PID(gid);  // 通过组ID获取进程控制块
    UINT32 ret = OsSetProcessGroupCheck(processCB, pgroupCB);  // 检查设置进程组的合法性
    if (ret != LOS_OK) {  // 检查合法性检查结果
        return ret;  // 返回错误码
    }

    if (OS_GET_PGROUP_LEADER(processCB->pgroup) == pgroupCB) {  // 检查是否已是该进程组的成员
        return LOS_OK;  // 直接返回成功
    }

    ProcessGroup *oldPGroup = processCB->pgroup;  // 保存旧的进程组
    ExitProcessGroup(processCB, pgroup);  // 退出旧的进程组

    ProcessGroup *newPGroup = OsFindProcessGroup(gid);  // 查找目标进程组
    if (newPGroup != NULL) {  // 检查进程组是否存在
        LOS_ListTailInsert(&newPGroup->processList, &processCB->subordinateGroupList);  // 将进程添加到新进程组的进程列表
        processCB->pgroup = newPGroup;  // 更新进程的进程组指针
        return LOS_OK;  // 返回成功
    }

    newPGroup = OsCreateProcessGroup(pgroupCB);  // 创建新的进程组
    if (newPGroup == NULL) {  // 检查进程组创建是否成功
        LOS_ListTailInsert(&oldPGroup->processList, &processCB->subordinateGroupList);  // 将进程重新添加到旧进程组
        processCB->pgroup = oldPGroup;  // 恢复旧的进程组指针
        if (*pgroup != NULL) {  // 检查进程组指针是否有效
            LOS_ListTailInsert(&rootPGroup->groupList, &oldPGroup->groupList);  // 将旧进程组添加到根进程组列表
            processCB = OS_GET_PGROUP_LEADER(oldPGroup);  // 获取旧进程组的组长
            processCB->processStatus |= OS_PROCESS_FLAG_GROUP_LEADER;  // 设置组长标志
            *pgroup = NULL;  // 清空进程组指针
        }
        return LOS_EPERM;  // 返回权限错误
    }
    return LOS_OK;  // 返回成功
}

/**
 * @brief 设置进程组ID
 * @details 提供安全的进程组ID设置接口，包含调度器加锁和解锁操作
 * @param pid 要修改的进程ID
 * @param gid 目标进程组ID
 * @return 操作结果，0表示成功，负数表示失败
 */
LITE_OS_SEC_TEXT INT32 OsSetProcessGroupID(UINT32 pid, UINT32 gid)
{
    ProcessGroup *pgroup = NULL;  // 进程组指针
    UINT32 ret;  // 函数返回值
    UINT32 intSave;  // 中断保存标志

    if ((OS_PID_CHECK_INVALID(pid)) || (OS_PID_CHECK_INVALID(gid))) {  // 检查PID和GID是否合法
        return -LOS_EINVAL;  // 返回无效参数错误
    }

    SCHEDULER_LOCK(intSave);  // 关闭调度器
    ret = OsSetProcessGroupIDUnsafe(pid, gid, &pgroup);  // 不安全地设置进程组ID
    SCHEDULER_UNLOCK(intSave);  // 开启调度器
    (VOID)LOS_MemFree(m_aucSysMem1, pgroup);  // 释放进程组内存
    return -ret;  // 返回结果（转换为负数错误码）
}

/**
 * @brief 设置当前进程的进程组ID
 * @details 为当前运行的进程设置进程组ID
 * @param gid 目标进程组ID
 * @return 操作结果，0表示成功，负数表示失败
 */
LITE_OS_SEC_TEXT INT32 OsSetCurrProcessGroupID(UINT32 gid)
{
    return OsSetProcessGroupID(OsCurrProcessGet()->processID, gid);  // 调用OsSetProcessGroupID设置当前进程的组ID
}

/**
 * @brief 获取进程的进程组ID
 * @details 根据进程PID获取其所属进程组的ID
 * @param pid 进程ID
 * @return 进程组ID，负数表示失败
 */
LITE_OS_SEC_TEXT INT32 LOS_GetProcessGroupID(UINT32 pid)
{
    INT32 gid;  // 进程组ID
    UINT32 intSave;  // 中断保存标志

    if (OS_PID_CHECK_INVALID(pid)) {  // 检查PID是否合法
        return -LOS_EINVAL;  // 返回无效参数错误
    }

    SCHEDULER_LOCK(intSave);  // 关闭调度器
    LosProcessCB *processCB = OS_PCB_FROM_PID(pid);  // 通过PID获取进程控制块
    if (OsProcessIsUnused(processCB)) {  // 检查进程是否未使用
        gid = -LOS_ESRCH;  // 设置进程不存在错误
        goto EXIT;  // 跳转到退出标签
    }

    processCB = OS_GET_PGROUP_LEADER(processCB->pgroup);  // 获取进程组组长
    gid = (INT32)processCB->processID;  // 获取组长的PID作为进程组ID

EXIT:
    SCHEDULER_UNLOCK(intSave);  // 开启调度器
    return gid;  // 返回进程组ID
}

/**
 * @brief 获取当前进程的进程组ID
 * @details 获取当前运行进程的进程组ID
 * @return 进程组ID，负数表示失败
 */
LITE_OS_SEC_TEXT INT32 LOS_GetCurrProcessGroupID(VOID)
{
    return LOS_GetProcessGroupID(OsCurrProcessGet()->processID);  // 调用LOS_GetProcessGroupID获取当前进程的组ID
}

#ifdef LOSCFG_KERNEL_VM
/**
 * @brief 获取空闲的进程控制块
 * @details 从空闲进程列表中获取一个未使用的进程控制块
 * @return 空闲进程控制块指针，NULL表示获取失败
 */
STATIC LosProcessCB *OsGetFreePCB(VOID)
{
    LosProcessCB *processCB = NULL;  // 进程控制块指针
    UINT32 intSave;  // 中断保存标志

    SCHEDULER_LOCK(intSave);  // 关闭调度器
    if (LOS_ListEmpty(&g_freeProcess)) {  // 检查空闲进程列表是否为空
        SCHEDULER_UNLOCK(intSave);  // 开启调度器
        PRINT_ERR("No idle PCB in the system!\n");  // 打印错误信息
        return NULL;  // 返回空指针
    }

    processCB = OS_PCB_FROM_PENDLIST(LOS_DL_LIST_FIRST(&g_freeProcess));  // 获取第一个空闲进程控制块
    LOS_ListDelete(&processCB->pendList);  // 从空闲列表中删除该进程控制块
    SCHEDULER_UNLOCK(intSave);  // 开启调度器

    return processCB;  // 返回获取到的进程控制块
}

/**
 * @brief 为用户初始化进程分配栈空间
 * @details 为用户态初始化进程分配并设置栈空间
 * @param processCB 进程控制块指针
 * @param size 用于返回分配的栈大小的指针
 * @return 栈空间的基地址，NULL表示分配失败
 */
STATIC VOID *OsUserInitStackAlloc(LosProcessCB *processCB, UINT32 *size)
{
    LosVmMapRegion *region = NULL;  // 虚拟内存映射区域指针
    UINT32 stackSize = ALIGN(OS_USER_TASK_STACK_SIZE, PAGE_SIZE);  // 栈大小（按页对齐）

    region = LOS_RegionAlloc(processCB->vmSpace, 0, stackSize,  // 分配虚拟内存区域
                             VM_MAP_REGION_FLAG_PERM_USER | VM_MAP_REGION_FLAG_PERM_READ |  // 用户态权限，可读
                             VM_MAP_REGION_FLAG_PERM_WRITE, 0);  // 可写
    if (region == NULL) {  // 检查区域分配是否成功
        return NULL;  // 返回空指针
    }

    LOS_SetRegionTypeAnon(region);  // 设置区域类型为匿名映射
    region->regionFlags |= VM_MAP_REGION_FLAG_STACK;  // 设置区域为栈类型

    *size = stackSize;  // 返回分配的栈大小

    return (VOID *)(UINTPTR)region->range.base;  // 返回栈空间基地址
}

#ifdef LOSCFG_KERNEL_DYNLOAD
/**
 * @brief 恢复进程的虚拟内存空间
 * @details 将当前进程的虚拟内存空间恢复为之前保存的旧空间，并切换MMU上下文
 * @param oldSpace 待恢复的旧虚拟内存空间指针
 * @return 无
 */
LITE_OS_SEC_TEXT VOID OsExecProcessVmSpaceRestore(LosVmSpace *oldSpace)
{
    LosProcessCB *processCB = OsCurrProcessGet();  // 获取当前进程控制块
    LosTaskCB *runTask = OsCurrTaskGet();  // 获取当前运行任务控制块

    processCB->vmSpace = oldSpace;  // 恢复旧的虚拟内存空间
    runTask->archMmu = (UINTPTR)&processCB->vmSpace->archMmu;  // 更新任务的MMU上下文指针
    LOS_ArchMmuContextSwitch((LosArchMmu *)runTask->archMmu);  // 切换MMU上下文
}

/**
 * @brief 替换进程的虚拟内存空间
 * @details 销毁当前进程的线程组，回收任务控制块，用新的虚拟内存空间替换旧空间，并更新堆和映射区域信息
 * @param newSpace 新的虚拟内存空间指针
 * @param stackBase 栈基地址
 * @param randomDevFD 随机设备文件描述符
 * @return 旧的虚拟内存空间指针
 */
LITE_OS_SEC_TEXT LosVmSpace *OsExecProcessVmSpaceReplace(LosVmSpace *newSpace, UINTPTR stackBase, INT32 randomDevFD)
{
    LosProcessCB *processCB = OsCurrProcessGet();  // 获取当前进程控制块
    LosTaskCB *runTask = OsCurrTaskGet();  // 获取当前运行任务控制块

    OsProcessThreadGroupDestroy();  // 销毁进程的线程组
    OsTaskCBRecycleToFree();  // 回收任务控制块到空闲列表

    LosVmSpace *oldSpace = processCB->vmSpace;  // 保存旧的虚拟内存空间
    processCB->vmSpace = newSpace;  // 设置新的虚拟内存空间
    processCB->vmSpace->heapBase += OsGetRndOffset(randomDevFD);  // 调整堆基地址（增加随机偏移）
    processCB->vmSpace->heapNow = processCB->vmSpace->heapBase;  // 初始化当前堆指针为堆基地址
    processCB->vmSpace->mapBase += OsGetRndOffset(randomDevFD);  // 调整映射基地址（增加随机偏移）
    processCB->vmSpace->mapSize = stackBase - processCB->vmSpace->mapBase;  // 计算映射区域大小
    runTask->archMmu = (UINTPTR)&processCB->vmSpace->archMmu;  // 更新任务的MMU上下文指针
    LOS_ArchMmuContextSwitch((LosArchMmu *)runTask->archMmu);  // 切换MMU上下文
    return oldSpace;  // 返回旧的虚拟内存空间
}

/**
 * @brief 回收进程资源并初始化新状态
 * @details 重置进程信号处理、回收虚拟内存空间、文件资源和定时器，初始化新的进程状态
 * @param processCB 进程控制块指针
 * @param name 进程名称
 * @param oldSpace 旧的虚拟内存空间指针
 * @param oldFiles 旧的文件结构指针
 * @return 操作结果，LOS_OK表示成功，其他值表示失败
 */
LITE_OS_SEC_TEXT UINT32 OsExecRecycleAndInit(LosProcessCB *processCB, const CHAR *name,
                                             LosVmSpace *oldSpace, UINTPTR oldFiles)
{
    UINT32 ret;  // 函数返回值
    const CHAR *processName = NULL;  // 进程名称指针

    if ((processCB == NULL) || (name == NULL)) {  // 参数合法性检查
        return LOS_NOK;  // 返回失败
    }

    processName = strrchr(name, '/');  // 从路径中提取进程名称
    processName = (processName == NULL) ? name : (processName + 1); /* 1: 不包含 '/' */

    ret = (UINT32)OsSetTaskName(OsCurrTaskGet(), processName, TRUE);  // 设置任务名称
    if (ret != LOS_OK) {  // 检查任务名称设置结果
        return ret;  // 返回失败
    }

#ifdef LOSCFG_KERNEL_LITEIPC
    (VOID)LiteIpcPoolDestroy(processCB->processID);  // 销毁轻量级IPC池
#endif

    processCB->sigHandler = 0;  // 重置信号处理函数
    OsCurrTaskGet()->sig.sigprocmask = 0;  // 重置信号屏蔽字

    LOS_VmSpaceFree(oldSpace);  // 释放旧的虚拟内存空间
#ifdef LOSCFG_FS_VFS
    CloseOnExec((struct files_struct *)oldFiles);  // 关闭执行时需要关闭的文件
    delete_files_snapshot((struct files_struct *)oldFiles);  // 删除文件快照
#endif

#ifdef LOSCFG_BASE_CORE_SWTMR_ENABLE
    OsSwtmrRecycle((UINTPTR)processCB);  // 回收软件定时器
    processCB->timerID = (timer_t)(UINTPTR)MAX_INVALID_TIMER_VID;  // 设置无效定时器ID
#endif

#ifdef LOSCFG_SECURITY_VID
    VidMapDestroy(processCB);  // 销毁VID映射
    ret = VidMapListInit(processCB);  // 初始化VID映射列表
    if (ret != LOS_OK) {  // 检查初始化结果
        return LOS_NOK;  // 返回失败
    }
#endif

    processCB->processStatus &= ~OS_PROCESS_FLAG_EXIT;  // 清除进程退出标志
    processCB->processStatus |= OS_PROCESS_FLAG_ALREADY_EXEC;  // 设置进程已执行标志

    return LOS_OK;  // 返回成功
}

/**
 * @brief 启动执行新程序
 * @details 初始化任务栈上下文，设置用户态映射信息，准备执行新程序入口
 * @param entry 程序入口函数指针
 * @param sp 栈指针
 * @param mapBase 映射基地址
 * @param mapSize 映射大小
 * @return 操作结果，LOS_OK表示成功，其他值表示失败
 */
LITE_OS_SEC_TEXT UINT32 OsExecStart(const TSK_ENTRY_FUNC entry, UINTPTR sp, UINTPTR mapBase, UINT32 mapSize)
{
    UINT32 intSave;  // 中断保存标志

    if (entry == NULL) {  // 检查入口函数是否为空
        return LOS_NOK;  // 返回失败
    }

    if ((sp == 0) || (LOS_Align(sp, LOSCFG_STACK_POINT_ALIGN_SIZE) != sp)) {  // 检查栈指针是否对齐
        return LOS_NOK;  // 返回失败
    }
    // 注意: sp此时指向栈底,栈底地址要大于栈顶
    if ((mapBase == 0) || (mapSize == 0) || (sp <= mapBase) || (sp > (mapBase + mapSize))) {  // 参数合法性检查
        return LOS_NOK;  // 返回失败
    }

    LosTaskCB *taskCB = OsCurrTaskGet();  // 获取当前任务控制块
    SCHEDULER_LOCK(intSave);  // 关闭调度器

    taskCB->userMapBase = mapBase;  // 设置用户态映射基地址
    taskCB->userMapSize = mapSize;  // 设置用户态映射大小
    taskCB->taskEntry = (TSK_ENTRY_FUNC)entry;  // 设置任务入口函数
    // 初始化内核态栈
    TaskContext *taskContext = (TaskContext *)OsTaskStackInit(taskCB->taskID, taskCB->stackSize,
                                                              (VOID *)taskCB->topOfStack, FALSE);
    OsUserTaskStackInit(taskContext, (UINTPTR)taskCB->taskEntry, sp);  // 初始化用户栈
    // 这样做的目的是将用户栈SP保存到内核栈中
    SCHEDULER_UNLOCK(intSave);  // 开启调度器
    return LOS_OK;  // 返回成功
}
#endif

/**
 * @brief 启动用户初始化进程
 * @details 创建用户任务，设置进程优先级和调度策略，启动用户初始化进程
 * @param processCB 进程控制块指针
 * @param param 任务初始化参数结构体指针
 * @return 操作结果，LOS_OK表示成功，其他值表示失败
 */
STATIC UINT32 OsUserInitProcessStart(LosProcessCB *processCB, TSK_INIT_PARAM_S *param)
{
    UINT32 intSave;  // 中断保存标志
    INT32 ret;  // 函数返回值

    UINT32 taskID = OsCreateUserTask((UINTPTR)processCB, param);  // 创建用户任务
    if (taskID == OS_INVALID_VALUE) {  // 检查任务创建结果
        return LOS_NOK;  // 返回失败
    }

    ret = LOS_SetProcessPriority(processCB->processID, OS_PROCESS_USERINIT_PRIORITY);  // 设置进程优先级
    if (ret != LOS_OK) {  // 检查优先级设置结果
        PRINT_ERR("User init process set priority failed! ERROR:%d \n", ret);  // 打印错误信息
        goto EXIT;  // 跳转到退出标签
    }

    SCHEDULER_LOCK(intSave);  // 关闭调度器
    processCB->processStatus &= ~OS_PROCESS_STATUS_INIT;  // 清除进程初始化状态
    SCHEDULER_UNLOCK(intSave);  // 开启调度器

    ret = LOS_SetTaskScheduler(taskID, LOS_SCHED_RR, OS_TASK_PRIORITY_LOWEST);  // 设置任务调度策略和优先级
    if (ret != LOS_OK) {  // 检查调度器设置结果
        PRINT_ERR("User init process set scheduler failed! ERROR:%d \n", ret);  // 打印错误信息
        goto EXIT;  // 跳转到退出标签
    }

    return LOS_OK;  // 返回成功

EXIT:
    (VOID)LOS_TaskDelete(taskID);  // 删除已创建的任务
    return ret;  // 返回失败
}

/**
 * @brief 加载用户初始化程序
 * @details 分配物理内存，拷贝用户初始化代码段和数据段，映射虚拟地址，初始化BSS段
 * @param processCB 进程控制块指针
 * @return 操作结果，LOS_OK表示成功，其他值表示失败
 */
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
    errno_t errRet;  // 错误返回值
    INT32 ret;  // 函数返回值
    CHAR *userInitTextStart = (CHAR *)&__user_init_entry;  // 用户初始化代码段起始地址
    CHAR *userInitBssStart = (CHAR *)&__user_init_bss;  // 用户初始化BSS段起始地址
    CHAR *userInitEnd = (CHAR *)&__user_init_end;  // 用户初始化段结束地址
    UINT32 initBssSize = userInitEnd - userInitBssStart;  // BSS段大小
    UINT32 initSize = userInitEnd - userInitTextStart;  // 初始化段总大小
    VOID *userBss = NULL;  // BSS段指针
    VOID *userText = NULL;  // 代码段指针

    if ((LOS_Align((UINTPTR)userInitTextStart, PAGE_SIZE) != (UINTPTR)userInitTextStart) ||  // 检查代码段起始地址是否页对齐
        (LOS_Align((UINTPTR)userInitEnd, PAGE_SIZE) != (UINTPTR)userInitEnd)) {  // 检查结束地址是否页对齐
        return LOS_EINVAL;  // 返回无效参数错误
    }

    if ((initSize == 0) || (initSize <= initBssSize)) {  // 检查初始化段大小是否有效
        return LOS_EINVAL;  // 返回无效参数错误
    }

    userText = LOS_PhysPagesAllocContiguous(initSize >> PAGE_SHIFT);  // 分配连续物理内存页
    if (userText == NULL) {  // 检查内存分配是否成功
        return LOS_NOK;  // 返回分配失败
    }

    errRet = memcpy_s(userText, initSize, (VOID *)&__user_init_load_addr, initSize - initBssSize);  // 拷贝代码段和数据段
    if (errRet != EOK) {  // 检查拷贝是否成功
        PRINT_ERR("Load user init text, data and bss failed! err : %d\n", errRet);  // 打印错误信息
        goto ERROR;  // 跳转到错误处理
    }
    ret = LOS_VaddrToPaddrMmap(processCB->vmSpace, (VADDR_T)(UINTPTR)userInitTextStart, LOS_PaddrQuery(userText),  // 映射虚拟地址到物理地址
                               initSize, VM_MAP_REGION_FLAG_PERM_READ | VM_MAP_REGION_FLAG_PERM_WRITE |  // 设置内存权限：读写
                               VM_MAP_REGION_FLAG_FIXED | VM_MAP_REGION_FLAG_PERM_EXECUTE |  // 固定映射且可执行
                               VM_MAP_REGION_FLAG_PERM_USER);  // 用户态权限
    if (ret < 0) {  // 检查映射是否成功
        PRINT_ERR("Mmap user init text, data and bss failed! err : %d\n", ret);  // 打印错误信息
        goto ERROR;  // 跳转到错误处理
    }

    /* The User init boot segment may not actually exist */
    if (initBssSize != 0) {  // 检查BSS段是否存在
        userBss = (VOID *)((UINTPTR)userText + userInitBssStart - userInitTextStart);  // 计算BSS段在物理内存中的地址
        errRet = memset_s(userBss, initBssSize, 0, initBssSize);  // 将BSS段初始化为0
        if (errRet != EOK) {  // 检查初始化是否成功
            PRINT_ERR("memset user init bss failed! err : %d\n", errRet);  // 打印错误信息
            goto ERROR;  // 跳转到错误处理
        }
    }

    return LOS_OK;  // 返回成功

ERROR:
    (VOID)LOS_PhysPagesFreeContiguous(userText, initSize >> PAGE_SHIFT);  // 释放已分配的物理内存
    return LOS_NOK;  // 返回失败
}

/**
 * @brief 初始化用户进程创建函数
 * @details 负责初始化用户态Init进程，包括进程控制块初始化、用户代码加载、堆栈分配及任务启动
 * @return 成功返回LOS_OK，失败返回对应错误码
 */
LITE_OS_SEC_TEXT_INIT UINT32 OsUserInitProcess(VOID)
{
    UINT32 ret;  // 函数返回值
    UINT32 size;  // 堆栈大小
    TSK_INIT_PARAM_S param = { 0 };  // 任务初始化参数
    VOID *stack = NULL;  // 堆栈指针

    LosProcessCB *processCB = OsGetUserInitProcess();  // 获取初始化用户进程控制块
    ret = OsSystemProcessInit(processCB, OS_USER_MODE, "Init");  // 初始化系统进程
    if (ret != LOS_OK) {  // 检查初始化是否成功
        return ret;  // 返回错误码
    }

    ret = OsLoadUserInit(processCB);  // 加载用户初始化代码
    if (ret != LOS_OK) {  // 检查加载是否成功
        goto ERROR;  // 跳转到错误处理
    }

    stack = OsUserInitStackAlloc(processCB, &size);  // 分配用户初始化堆栈
    if (stack == NULL) {  // 检查堆栈分配是否成功
        PRINT_ERR("Alloc user init process user stack failed!\n");  // 打印错误信息
        goto ERROR;  // 跳转到错误处理
    }

    param.pfnTaskEntry = (TSK_ENTRY_FUNC)(CHAR *)&__user_init_entry;  // 设置任务入口函数
    param.userParam.userSP = (UINTPTR)stack + size;  // 设置用户态堆栈指针
    param.userParam.userMapBase = (UINTPTR)stack;  // 设置堆栈映射基地址
    param.userParam.userMapSize = size;  // 设置堆栈映射大小
    param.uwResved = OS_TASK_FLAG_PTHREAD_JOIN;  // 设置任务属性为可连接
    ret = OsUserInitProcessStart(processCB, &param);  // 启动用户初始化进程
    if (ret != LOS_OK) {  // 检查启动是否成功
        (VOID)OsUnMMap(processCB->vmSpace, param.userParam.userMapBase, param.userParam.userMapSize);  // 解除堆栈映射
        goto ERROR;  // 跳转到错误处理
    }

    return LOS_OK;  // 返回成功

ERROR:
    OsDeInitPCB(processCB);  // 反初始化进程控制块
    return ret;  // 返回错误码
}

/**
 * @brief 拷贝父进程用户信息到子进程
 * @details 当使能安全能力时，复制父进程的用户ID、组ID等安全信息到子进程控制块
 * @param childCB 子进程控制块指针
 * @param parentCB 父进程控制块指针
 * @return 成功返回LOS_OK，内存分配失败返回LOS_ENOMEM
 */
STATIC UINT32 OsCopyUser(LosProcessCB *childCB, LosProcessCB *parentCB)
{
#ifdef LOSCFG_SECURITY_CAPABILITY  // 安全能力配置宏
    UINT32 intSave;  // 中断状态保存
    UINT32 size;  // 用户信息大小
    SCHEDULER_LOCK(intSave);  // 关闭调度器
    size = sizeof(User) + sizeof(UINT32) * (parentCB->user->groupNumber - 1);  // 计算用户信息总大小
    childCB->user = LOS_MemAlloc(m_aucSysMem1, size);  // 分配用户信息内存
    if (childCB->user == NULL) {  // 检查内存分配是否成功
        SCHEDULER_UNLOCK(intSave);  // 恢复调度器
        return LOS_ENOMEM;  // 返回内存不足错误
    }

    (VOID)memcpy_s(childCB->user, size, parentCB->user, size);  // 拷贝用户信息
    SCHEDULER_UNLOCK(intSave);  // 恢复调度器
#endif
    return LOS_OK;  // 返回成功
}

/**
 * @brief 获取拷贝任务的参数
 * @details 根据父进程任务信息初始化子进程任务参数，区分用户态和内核态任务的不同处理
 * @param childProcessCB 子进程控制块指针
 * @param entry 内核态任务入口地址
 * @param size 内核态任务堆栈大小
 * @param taskParam 任务初始化参数结构体指针
 * @param param 调度参数结构体指针
 */
STATIC VOID GetCopyTaskParam(LosProcessCB *childProcessCB, UINTPTR entry, UINT32 size,  // 获取拷贝任务参数
                             TSK_INIT_PARAM_S *taskParam, SchedParam *param)
{
    UINT32 intSave;  // 中断状态保存
    LosTaskCB *runTask = OsCurrTaskGet();  // 获取当前运行任务控制块

    SCHEDULER_LOCK(intSave);  // 关闭调度器
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
    if (runTask->taskStatus & OS_TASK_FLAG_PTHREAD_JOIN) {  // 检查任务是否可连接
        taskParam->uwResved = LOS_TASK_ATTR_JOINABLE;  // 设置任务为可连接属性
    }

    runTask->ops->schedParamGet(runTask, param);  // 获取调度参数
    SCHEDULER_UNLOCK(intSave);  // 恢复调度器

    taskParam->policy = param->policy;  // 设置调度策略
    taskParam->runTimeUs = param->runTimeUs;  // 设置运行时间
    taskParam->deadlineUs = param->deadlineUs;  // 设置截止时间
    taskParam->periodUs = param->periodUs;  // 设置周期
    taskParam->usTaskPrio = param->priority;  // 设置优先级
    taskParam->processID = (UINTPTR)childProcessCB;  // 设置进程ID
}

/**
 * @brief 拷贝父进程任务创建子进程任务
 * @details 根据父进程当前任务信息克隆子进程任务，包括任务属性、调度参数和堆栈信息
 * @param flags 克隆标志位
 * @param childProcessCB 子进程控制块指针
 * @param name 任务名称
 * @param entry 内核态任务入口地址
 * @param size 内核态任务堆栈大小
 * @return 成功返回LOS_OK，任务创建失败返回对应错误码
 */
STATIC UINT32 OsCopyTask(UINT32 flags, LosProcessCB *childProcessCB, const CHAR *name, UINTPTR entry, UINT32 size)  // 拷贝任务
{
    LosTaskCB *runTask = OsCurrTaskGet();  // 获取当前运行任务
    TSK_INIT_PARAM_S taskParam = { 0 };  // 任务初始化参数
    UINT32 ret, taskID, intSave;  // 返回值、任务ID、中断状态
    SchedParam param = { 0 };  // 调度参数

    taskParam.pcName = (CHAR *)name;  // 设置任务名称
    GetCopyTaskParam(childProcessCB, entry, size, &taskParam, &param);  // 获取拷贝任务参数

    ret = LOS_TaskCreateOnly(&taskID, &taskParam);  // 创建任务(仅创建不调度)
    if (ret != LOS_OK) {  // 检查任务创建是否成功
        if (ret == LOS_ERRNO_TSK_TCB_UNAVAILABLE) {  // 检查是否是TCB不可用错误
            return LOS_EAGAIN;  // 返回重试错误
        }
        return LOS_ENOMEM;  // 返回内存不足错误
    }

    LosTaskCB *childTaskCB = childProcessCB->threadGroup;  // 获取子进程线程组
    childTaskCB->taskStatus = runTask->taskStatus;//任务状态先同步,注意这里是赋值操作. ...01101001 
    childTaskCB->ops->schedParamModify(childTaskCB, &param);  // 修改调度参数
    if (childTaskCB->taskStatus & OS_TASK_STATUS_RUNNING) {//因只能有一个运行的task,所以如果一样要改4号位
        childTaskCB->taskStatus &= ~OS_TASK_STATUS_RUNNING;//将四号位清0 ,变成 ...01100001 
    } else {//非运行状态下会发生什么?
        if (OS_SCHEDULER_ACTIVE) {//克隆线程发生错误未运行
            LOS_Panic("Clone thread status not running error status: 0x%x\n", childTaskCB->taskStatus);  // 触发系统 panic
        }
        childTaskCB->taskStatus &= ~OS_TASK_STATUS_UNUSED;//干净的Task
    }

    if (OsProcessIsUserMode(childProcessCB)) {//是否是用户进程
        SCHEDULER_LOCK(intSave);  // 关闭调度器
        OsUserCloneParentStack(childTaskCB->stackPointer, entry, runTask->topOfStack, runTask->stackSize);  // 克隆父进程堆栈
        SCHEDULER_UNLOCK(intSave);  // 恢复调度器
    }
    return LOS_OK;  // 返回成功
}

/**
 * @brief 设置子进程的父进程关系
 * @details 根据克隆标志设置子进程的父进程和进程组关系，维护进程家族树结构
 * @param flags 克隆标志位，包含CLONE_PARENT等标志
 * @param childProcessCB 子进程控制块指针
 * @param runProcessCB 当前运行进程控制块指针
 * @return 成功返回LOS_OK
 */
STATIC UINT32 OsCopyParent(UINT32 flags, LosProcessCB *childProcessCB, LosProcessCB *runProcessCB)  // 拷贝父进程信息
{
    UINT32 intSave;  // 中断状态保存
    LosProcessCB *parentProcessCB = NULL;  // 父进程控制块指针

    SCHEDULER_LOCK(intSave);  // 关闭调度器
    if (childProcessCB->parentProcess == NULL) {  // 检查子进程是否已有父进程
	    if (flags & CLONE_PARENT) { //这里指明 childProcessCB 和 runProcessCB 有同一个父亲，是兄弟关系
	        parentProcessCB = runProcessCB->parentProcess;  // 设置父进程为当前进程的父进程
	    } else {
	        parentProcessCB = runProcessCB;          // 设置父进程为当前进程
	    }
	        childProcessCB->parentProcess = parentProcessCB;//指认父亲，这个赋值代表从此是你儿了
	        LOS_ListTailInsert(&parentProcessCB->childrenList, &childProcessCB->siblingList);//通过我的兄弟姐妹节点，挂到父亲的孩子链表上，于我而言，父亲的这个链表上挂的都是我的兄弟姐妹   													
    }
    if (childProcessCB->pgroup == NULL) {  // 检查进程组是否为空
        childProcessCB->pgroup = parentProcessCB->pgroup;  // 继承父进程组
        LOS_ListTailInsert(&parentProcessCB->pgroup->processList, &childProcessCB->subordinateGroupList);  // 将子进程添加到进程组列表
    }
    SCHEDULER_UNLOCK(intSave);  // 恢复调度器
    return LOS_OK;  // 返回成功
}

/**
 * @brief 拷贝进程内存空间
 * @details 根据克隆标志决定是共享还是拷贝父进程的内存空间，内核进程无需拷贝内存空间
 * @param flags 克隆标志，决定内存空间处理方式
 * @param childProcessCB 子进程控制块指针
 * @param runProcessCB 父进程控制块指针
 * @return 成功返回LOS_OK，失败返回错误码
 */
STATIC UINT32 OsCopyMM(UINT32 flags, LosProcessCB *childProcessCB, LosProcessCB *runProcessCB)
{
    status_t status;  // 函数返回状态
    UINT32 intSave;  // 中断保存标志

    if (!OsProcessIsUserMode(childProcessCB)) {  // 检查是否为用户模式进程
        return LOS_OK;  // 内核模式进程无需拷贝内存空间，直接返回成功
    }

    if (flags & CLONE_VM) {  // 检查是否设置共享内存空间标志
        SCHEDULER_LOCK(intSave);  // 锁定调度器
        childProcessCB->vmSpace->archMmu.virtTtb = runProcessCB->vmSpace->archMmu.virtTtb;  // 共享虚拟TTB(页表基地址)
        childProcessCB->vmSpace->archMmu.physTtb = runProcessCB->vmSpace->archMmu.physTtb;  // 共享物理TTB
        SCHEDULER_UNLOCK(intSave);  // 解锁调度器
        return LOS_OK;  // 返回成功
    }

    status = LOS_VmSpaceClone(flags, runProcessCB->vmSpace, childProcessCB->vmSpace);  // 克隆父进程虚拟空间
    if (status != LOS_OK) {  // 检查克隆是否成功
        return LOS_ENOMEM;  // 返回内存不足错误
    }
    return LOS_OK;  // 返回成功
}

/**
 * @brief 拷贝进程文件资源
 * @details 根据克隆标志决定是共享还是拷贝文件描述符表，设置控制台ID和文件创建掩码
 * @param flags 克隆标志，决定文件资源处理方式
 * @param childProcessCB 子进程控制块指针
 * @param runProcessCB 父进程控制块指针
 * @return 成功返回LOS_OK，失败返回错误码
 */
STATIC UINT32 OsCopyFile(UINT32 flags, LosProcessCB *childProcessCB, LosProcessCB *runProcessCB)
{
#ifdef LOSCFG_FS_VFS
    if (flags & CLONE_FILES) {  // 检查是否设置共享文件描述符标志
        childProcessCB->files = runProcessCB->files;  // 共享父进程文件描述符表
    } else {
#ifdef LOSCFG_IPC_CONTAINER
        if (flags & CLONE_NEWIPC) {  // 检查是否设置新IPC命名空间标志
            OsCurrTaskGet()->cloneIpc = TRUE;  // 设置IPC克隆标志
        }
#endif
        childProcessCB->files = dup_fd(runProcessCB->files);  // 复制父进程文件描述符表
#ifdef LOSCFG_IPC_CONTAINER
        OsCurrTaskGet()->cloneIpc = FALSE;  // 清除IPC克隆标志
#endif
    }
    if (childProcessCB->files == NULL) {  // 检查文件描述符表是否复制成功
        return LOS_ENOMEM;  // 返回内存不足错误
    }

#ifdef LOSCFG_PROC_PROCESS_DIR
    INT32 ret = ProcCreateProcessDir(OsGetRootPid(childProcessCB), (UINTPTR)childProcessCB);  // 创建进程proc目录
    if (ret < 0) {  // 检查目录创建是否成功
        PRINT_ERR("ProcCreateProcessDir failed, pid = %u\n", childProcessCB->processID);  // 打印错误信息
        return LOS_EBADF;  // 返回文件描述符错误
    }
#endif
#endif

    childProcessCB->consoleID = runProcessCB->consoleID;  // 继承控制台ID
    childProcessCB->umask = runProcessCB->umask;  // 继承文件创建掩码
    return LOS_OK;  // 返回成功
}

/**
 * @brief 初始化新进程控制块
 * @details 拷贝父进程基本信息并创建初始任务
 * @param flags 克隆标志
 * @param child 子进程控制块指针
 * @param name 进程名称
 * @param sp 栈指针
 * @param size 栈大小
 * @return 成功返回LOS_OK，失败返回错误码
 */
STATIC UINT32 OsForkInitPCB(UINT32 flags, LosProcessCB *child, const CHAR *name, UINTPTR sp, UINT32 size)
{
    UINT32 ret;  // 函数返回值
    LosProcessCB *run = OsCurrProcessGet();  // 获取当前进程控制块

    ret = OsCopyParent(flags, child, run);  // 拷贝父进程基本信息
    if (ret != LOS_OK) {  // 检查拷贝是否成功
        return ret;  // 返回错误码
    }

    return OsCopyTask(flags, child, name, sp, size);  // 拷贝任务并返回结果
}

/**
 * @brief 设置子进程组和调度属性
 * @details 将子进程加入进程组并设置调度参数，使其可被调度执行
 * @param child 子进程控制块指针
 * @param run 父进程控制块指针
 * @return 成功返回LOS_OK，失败返回错误码
 */
STATIC UINT32 OsChildSetProcessGroupAndSched(LosProcessCB *child, LosProcessCB *run)
{
    UINT32 intSave;  // 中断保存标志
    UINT32 ret;  // 函数返回值
    ProcessGroup *pgroup = NULL;  // 进程组指针

    SCHEDULER_LOCK(intSave);  // 锁定调度器
    if ((UINTPTR)OS_GET_PGROUP_LEADER(run->pgroup) == OS_USER_PRIVILEGE_PROCESS_GROUP) {  // 检查父进程组是否为用户特权组
        ret = OsSetProcessGroupIDUnsafe(child->processID, child->processID, &pgroup);  // 创建新进程组
        if (ret != LOS_OK) {  // 检查进程组创建是否成功
            SCHEDULER_UNLOCK(intSave);  // 解锁调度器
            return LOS_ENOMEM;  // 返回内存不足错误
        }
    }

    child->processStatus &= ~OS_PROCESS_STATUS_INIT;  // 清除初始化状态标志
    LosTaskCB *taskCB = child->threadGroup;  // 获取子进程主线程
    taskCB->ops->enqueue(OsSchedRunqueue(), taskCB);  // 将线程加入调度队列
    SCHEDULER_UNLOCK(intSave);  // 解锁调度器

    (VOID)LOS_MemFree(m_aucSysMem1, pgroup);  // 释放进程组内存
    return LOS_OK;  // 返回成功
}

/**
 * @brief 拷贝进程资源
 * @details 拷贝用户信息、内存空间、文件资源、IPC信息和安全能力等进程资源
 * @param flags 克隆标志
 * @param child 子进程控制块指针
 * @param run 父进程控制块指针
 * @return 成功返回LOS_OK，失败返回错误码
 */
STATIC UINT32 OsCopyProcessResources(UINT32 flags, LosProcessCB *child, LosProcessCB *run)
{
    UINT32 ret;  // 函数返回值

    ret = OsCopyUser(child, run);  // 拷贝用户信息
    if (ret != LOS_OK) {  // 检查拷贝是否成功
        return ret;  // 返回错误码
    }

    ret = OsCopyMM(flags, child, run);  // 拷贝内存空间
    if (ret != LOS_OK) {  // 检查拷贝是否成功
        return ret;  // 返回错误码
    }

    ret = OsCopyFile(flags, child, run);  // 拷贝文件资源
    if (ret != LOS_OK) {  // 检查拷贝是否成功
        return ret;  // 返回错误码
    }

#ifdef LOSCFG_KERNEL_LITEIPC
    if (run->ipcInfo != NULL) {  // 检查父进程是否有IPC信息
        child->ipcInfo = LiteIpcPoolReInit((const ProcIpcInfo *)(run->ipcInfo));  // 重新初始化IPC池
        if (child->ipcInfo == NULL) {  // 检查IPC池初始化是否成功
            return LOS_ENOMEM;  // 返回内存不足错误
        }
    }
#endif

#ifdef LOSCFG_SECURITY_CAPABILITY
    OsCopyCapability(run, child);  // 拷贝安全能力
#endif

    return LOS_OK;  // 返回成功
}

/**
 * @brief 拷贝进程主函数
 * @details 创建新进程控制块，初始化并拷贝父进程资源，设置进程组和调度属性
 * @param flags 克隆标志
 * @param name 进程名称
 * @param sp 栈指针
 * @param size 栈大小
 * @return 成功返回新进程ID，失败返回负错误码
 */
STATIC INT32 OsCopyProcess(UINT32 flags, const CHAR *name, UINTPTR sp, UINT32 size)
{
    UINT32 ret, processID;  // 函数返回值和进程ID
    LosProcessCB *run = OsCurrProcessGet();  // 获取当前进程控制块

    LosProcessCB *child = OsGetFreePCB();  // 从进程池获取空闲进程控制块
    if (child == NULL) {  // 检查是否获取成功
        return -LOS_EAGAIN;  // 返回资源暂时不可用错误
    }
    processID = child->processID;  // 保存新进程ID

    ret = OsInitPCB(child, run->processMode, name);  // 初始化进程控制块
    if (ret != LOS_OK) {  // 检查初始化是否成功
        goto ERROR_INIT;  // 跳转到错误处理标签
    }

#ifdef LOSCFG_KERNEL_CONTAINER
    ret = OsCopyContainers(flags, child, run, &processID);  // 拷贝容器信息
    if (ret != LOS_OK) {  // 检查拷贝是否成功
        goto ERROR_INIT;  // 跳转到错误处理标签
    }
#ifdef LOSCFG_KERNEL_PLIMITS
    ret = OsPLimitsAddProcess(run->plimits, child);  // 添加进程到资源限制组
    if (ret != LOS_OK) {  // 检查添加是否成功
        goto ERROR_INIT;  // 跳转到错误处理标签
    }
#endif
#endif
    ret = OsForkInitPCB(flags, child, name, sp, size);  // 初始化进程控制块
    if (ret != LOS_OK) {  // 检查初始化是否成功
        goto ERROR_INIT;  // 跳转到错误处理标签
    }

    ret = OsCopyProcessResources(flags, child, run);  // 拷贝进程资源
    if (ret != LOS_OK) {  // 检查拷贝是否成功
        goto ERROR_TASK;  // 跳转到任务错误处理标签
    }

    ret = OsChildSetProcessGroupAndSched(child, run);  // 设置进程组和调度
    if (ret != LOS_OK) {  // 检查设置是否成功
        goto ERROR_TASK;  // 跳转到任务错误处理标签
    }

    LOS_MpSchedule(OS_MP_CPU_ALL);  // 通知所有CPU核心调度
    if (OS_SCHEDULER_ACTIVE) {  // 检查调度器是否激活
        LOS_Schedule();  // 触发调度
    }

    return processID;  // 返回新进程ID

ERROR_TASK:
    (VOID)LOS_TaskDelete(child->threadGroup->taskID);  // 删除子进程任务
ERROR_INIT:
    OsDeInitPCB(child);  // 反初始化进程控制块
    return -ret;  // 返回负错误码
}

/**
 * @brief 实现Clone系统调用
 * @details 根据克隆标志创建新进程或线程，支持多种容器隔离功能
 * @param flags 克隆标志，指定创建特性
 * @param sp 栈指针
 * @param size 栈大小
 * @return 成功返回新进程/线程ID，失败返回负错误码
 */
LITE_OS_SEC_TEXT INT32 OsClone(UINT32 flags, UINTPTR sp, UINT32 size)
{
    UINT32 cloneFlag = CLONE_PARENT | CLONE_THREAD | SIGCHLD;  // 支持的克隆标志组合
#ifdef LOSCFG_KERNEL_CONTAINER
#ifdef LOSCFG_PID_CONTAINER
    cloneFlag |= CLONE_NEWPID;  // 支持PID命名空间隔离
    LosProcessCB *curr = OsCurrProcessGet();  // 获取当前进程控制块
    if (((flags & CLONE_NEWPID) != 0) && ((flags & (CLONE_PARENT | CLONE_THREAD)) != 0)) {  // 检查PID命名空间与父进程共享标志冲突
        return -LOS_EINVAL;  // 返回无效参数错误
    }

    if (OS_PROCESS_PID_FOR_CONTAINER_CHECK(curr) && ((flags & CLONE_NEWPID) != 0)) {  // 检查容器进程是否创建新PID命名空间
        return -LOS_EINVAL;  // 返回无效参数错误
    }

    if (OS_PROCESS_PID_FOR_CONTAINER_CHECK(curr) && ((flags & (CLONE_PARENT | CLONE_THREAD)) != 0)) {  // 检查容器进程是否共享父进程
        return -LOS_EINVAL;  // 返回无效参数错误
    }
#endif
#ifdef LOSCFG_UTS_CONTAINER
    cloneFlag |= CLONE_NEWUTS;  // 支持UTS命名空间隔离
#endif
#ifdef LOSCFG_MNT_CONTAINER
    cloneFlag |= CLONE_NEWNS;  // 支持挂载命名空间隔离
#endif
#ifdef LOSCFG_IPC_CONTAINER
    cloneFlag |= CLONE_NEWIPC;  // 支持IPC命名空间隔离
    if (((flags & CLONE_NEWIPC) != 0) && ((flags & CLONE_FILES) != 0)) {  // 检查IPC命名空间与文件共享标志冲突
        return -LOS_EINVAL;  // 返回无效参数错误
    }
#endif
#ifdef LOSCFG_TIME_CONTAINER
    cloneFlag |= CLONE_NEWTIME;  // 支持时间命名空间隔离
#endif
#ifdef LOSCFG_USER_CONTAINER
    cloneFlag |= CLONE_NEWUSER;  // 支持用户命名空间隔离
#endif
#ifdef LOSCFG_NET_CONTAINER
    cloneFlag |= CLONE_NEWNET;  // 支持网络命名空间隔离
#endif
#endif

    if (flags & (~cloneFlag)) {  // 检查是否包含不支持的标志
        return -LOS_EOPNOTSUPP;  // 返回操作不支持错误
    }

    return OsCopyProcess(cloneFlag & flags, NULL, sp, size);  // 调用进程拷贝函数
}

//注：著名的 fork 函数, 您也记得前往 https://gitee.com/weharmony/kernel_liteos_a_note  fork 一下 :)
/**
 * @brief 创建新进程
 * @details 通过复制当前进程创建新进程，支持指定克隆标志、进程名、入口函数和栈大小
 * @param flags 克隆标志，指定进程创建的特性
 * @param name 新进程名称
 * @param entry 新进程入口函数
 * @param stackSize 新进程栈大小
 * @return 成功返回新进程ID，失败返回错误码
 */
LITE_OS_SEC_TEXT INT32 LOS_Fork(UINT32 flags, const CHAR *name, const TSK_ENTRY_FUNC entry, UINT32 stackSize)
{
    UINT32 cloneFlag = CLONE_PARENT | CLONE_THREAD | CLONE_VFORK | CLONE_FILES;  // 支持的克隆标志组合

    if (flags & (~cloneFlag)) {  // 检查是否包含不支持的标志
        PRINT_WARN("Clone dont support some flags!\n");  // 打印警告信息
    }

    flags |= CLONE_FILES;  // 强制设置文件描述符共享标志
    return OsCopyProcess(cloneFlag & flags, name, (UINTPTR)entry, stackSize);  // 调用进程复制函数创建新进程
}
#else
/**
 * @brief 用户进程初始化
 * @details 初始化用户进程环境，当前实现为空操作
 * @return 始终返回0表示成功
 */
LITE_OS_SEC_TEXT_INIT UINT32 OsUserInitProcess(VOID)
{
    return 0;  // 返回成功
}
#endif

/**
 * @brief 进程退出
 * @details 终止当前进程及其所有线程，释放相关资源
 * @param status 退出状态码（未使用）
 * @return 无
 */
LITE_OS_SEC_TEXT VOID LOS_Exit(INT32 status)
{
    UINT32 intSave;  // 中断保存标志

    (void)status;  // 未使用的参数
    /* 内核态进程的退出必须确保所有线程都已主动退出 */
    LosProcessCB *processCB = OsCurrProcessGet();  // 获取当前进程控制块
    SCHEDULER_LOCK(intSave);  // 锁定调度器
    if (!OsProcessIsUserMode(processCB) && (processCB->threadNumber != 1)) {  // 检查内核态进程是否还有多个线程
        SCHEDULER_UNLOCK(intSave);  // 解锁调度器
        PRINT_ERR("Kernel-state processes with multiple threads are not allowed to exit directly\n");  // 打印错误信息
        return;  // 返回
    }
    SCHEDULER_UNLOCK(intSave);  // 解锁调度器

    OsProcessThreadGroupDestroy();  // 销毁进程的线程组
    OsRunningTaskToExit(OsCurrTaskGet(), OS_PRO_EXIT_OK);  // 将当前运行任务标记为退出状态
}

/**
 * @brief 获取已使用的PID列表
 * @details 遍历进程池，收集所有已使用的进程ID到输出数组中
 * @param pidList 存储PID列表的输出数组
 * @param pidMaxNum 输出数组的最大容量
 * @return 成功收集的PID数量
 */
LITE_OS_SEC_TEXT INT32 LOS_GetUsedPIDList(UINT32 *pidList, INT32 pidMaxNum)
{
    LosProcessCB *pcb = NULL;  // 进程控制块指针
    INT32 num = 0;  // 已收集的PID数量
    UINT32 intSave;  // 中断保存标志
    UINT32 pid = 1;  // PID起始值（从1开始遍历）

    if (pidList == NULL) {  // 检查输入参数是否为空
        return 0;  // 返回0表示未收集到PID
    }
    SCHEDULER_LOCK(intSave);  // 锁定调度器
    while (OsProcessIDUserCheckInvalid(pid) == false) {  // 遍历所有有效的用户PID
        pcb = OS_PCB_FROM_PID(pid);  // 通过PID获取进程控制块
        pid++;  // 递增PID
        if (OsProcessIsUnused(pcb)) {  // 检查进程是否未使用
            continue;  // 跳过未使用的进程
        }
        pidList[num] = pcb->processID;  // 将进程ID存入输出数组
        num++;  // 递增计数
        if (num >= pidMaxNum) {  // 检查是否达到数组最大容量
            break;  // 跳出循环
        }
    }
    SCHEDULER_UNLOCK(intSave);  // 解锁调度器
    return num;  // 返回收集到的PID数量
}

#ifdef LOSCFG_FS_VFS
/**
 * @brief 获取进程的文件描述符表
 * @details 根据进程ID获取对应的文件描述符表，仅在VFS配置启用时有效
 * @param pid 进程ID
 * @return 成功返回文件描述符表指针，失败返回NULL
 */
LITE_OS_SEC_TEXT struct fd_table_s *LOS_GetFdTable(UINT32 pid)
{
    if (OS_PID_CHECK_INVALID(pid)) {  // 检查PID是否有效
        return NULL;  // 返回NULL表示无效PID
    }

    LosProcessCB *pcb = OS_PCB_FROM_PID(pid);  // 通过PID获取进程控制块
    struct files_struct *files = pcb->files;  // 获取进程的文件结构
    if (files == NULL) {  // 检查文件结构是否为空
        return NULL;  // 返回NULL表示文件结构未初始化
    }

    return files->fdt;  // 返回文件描述符表
}
#endif

/**
 * @brief 获取当前进程ID
 * @details 获取当前运行进程的ID
 * @return 当前进程ID
 */
LITE_OS_SEC_TEXT UINT32 LOS_GetCurrProcessID(VOID)
{
    return OsCurrProcessGet()->processID;  // 返回当前进程控制块中的进程ID
}

#ifdef LOSCFG_KERNEL_VM
/**
 * @brief 处理线程组中活跃任务被终止的情况
 * @details 当线程组中的活跃任务需要被终止时，设置任务退出标志并发送终止信号，
 *          若任务处于可运行状态则进行调度处理，否则直接调用不安全的任务终止函数。
 *          同时初始化任务的连接列表并等待任务退出。
 * @param taskCB 指向需要被终止的任务控制块结构体指针
 * @return 无
 */
STATIC VOID ThreadGroupActiveTaskKilled(LosTaskCB *taskCB)
{
    INT32 ret;  // 函数返回值
    LosProcessCB *processCB = OS_PCB_FROM_TCB(taskCB);  // 通过任务控制块获取进程控制块
    taskCB->taskStatus |= OS_TASK_FLAG_EXIT_KILL;  // 设置任务退出标志为被终止
#ifdef LOSCFG_KERNEL_SMP
    /** 线程正在运行的其他核心且当前运行在非系统调用中 */
    if (!taskCB->sig.sigIntLock && (taskCB->taskStatus & OS_TASK_STATUS_RUNNING)) {  // 检查任务是否可被中断且处于运行状态
        taskCB->signal = SIGNAL_KILL;  // 设置终止信号
        LOS_MpSchedule(taskCB->currCpu);  // 调度指定CPU核心
    } else
#endif
    {
        ret = OsTaskKillUnsafe(taskCB->taskID, SIGKILL);  // 调用不安全的任务终止函数
        if (ret != LOS_OK) {  // 检查任务终止是否成功
            PRINT_ERR("pid %u exit, Exit task group %u kill %u failed! ERROR: %d\n",
                      processCB->processID, OsCurrTaskGet()->taskID, taskCB->taskID, ret);  // 打印错误信息
        }
    }

    if (!(taskCB->taskStatus & OS_TASK_FLAG_PTHREAD_JOIN)) {  // 检查任务是否已设置 pthread join 标志
        taskCB->taskStatus |= OS_TASK_FLAG_PTHREAD_JOIN;  // 设置 pthread join 标志
        LOS_ListInit(&taskCB->joinList);  // 初始化任务的连接列表
    }

    ret = OsTaskJoinPendUnsafe(taskCB);  // 等待任务退出
    if (ret != LOS_OK) {  // 检查等待是否成功
        PRINT_ERR("pid %u exit, Exit task group %u to wait others task %u(0x%x) exit failed! ERROR: %d\n",
                  processCB->processID, OsCurrTaskGet()->taskID, taskCB->taskID, taskCB->taskStatus, ret);  // 打印错误信息
    }
}
#endif

/**
 * @brief 销毁进程的线程组
 * @details 当进程需要退出时，遍历并销毁进程的线程组中所有线程，
 *          对于非当前线程调用终止处理函数，对于当前线程则跳过处理，
 *          最终确保进程中仅剩余当前线程。
 * @return 无
 */
LITE_OS_SEC_TEXT VOID OsProcessThreadGroupDestroy(VOID)
{
#ifdef LOSCFG_KERNEL_VM
    UINT32 intSave;  // 中断保存标志

    LosProcessCB *processCB = OsCurrProcessGet();  // 获取当前进程控制块
    LosTaskCB *currTask = OsCurrTaskGet();  // 获取当前任务控制块
    SCHEDULER_LOCK(intSave);  // 锁定调度器
    if ((processCB->processStatus & OS_PROCESS_FLAG_EXIT) || !OsProcessIsUserMode(processCB)) {  // 检查进程是否已退出或非用户模式
        SCHEDULER_UNLOCK(intSave);  // 解锁调度器
        return;  // 返回
    }

    processCB->processStatus |= OS_PROCESS_FLAG_EXIT;  // 设置进程退出标志
    processCB->threadGroup = currTask;  // 设置当前任务为线程组主线程

    LOS_DL_LIST *list = &processCB->threadSiblingList;  // 获取线程兄弟链表
    LOS_DL_LIST *head = list;  // 保存链表头
    do {
        LosTaskCB *taskCB = LOS_DL_LIST_ENTRY(list->pstNext, LosTaskCB, threadList);  // 获取链表中的任务控制块
        if ((OsTaskIsInactive(taskCB) ||  // 检查任务是否非活跃
            ((taskCB->taskStatus & OS_TASK_STATUS_READY) && !taskCB->sig.sigIntLock)) &&  // 或任务就绪且可中断
            !(taskCB->taskStatus & OS_TASK_STATUS_RUNNING)) {  // 且不处于运行状态
            OsInactiveTaskDelete(taskCB);  // 删除非活跃任务
        } else if (taskCB != currTask) {  // 若任务不是当前任务
            ThreadGroupActiveTaskKilled(taskCB);  // 调用活跃任务终止处理函数
        } else {
            /* 跳过当前任务 */
            list = list->pstNext;  // 移动到下一个链表节点
        }
    } while (head != list->pstNext);  // 遍历整个链表

    SCHEDULER_UNLOCK(intSave);  // 解锁调度器

    LOS_ASSERT(processCB->threadNumber == 1);  // 断言进程中仅剩一个线程
#endif
    return;  // 返回
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
