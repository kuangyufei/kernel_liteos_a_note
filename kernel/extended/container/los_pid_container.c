/*
 * Copyright (c) 2023-2023 Huawei Device Co., Ltd. All rights reserved.
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

#include <sched.h>
#include "los_pid_container_pri.h"
#include "los_config.h"
#include "los_process_pri.h"
#include "los_container_pri.h"

#ifdef LOSCFG_PID_CONTAINER
// 全局变量定义
STATIC UINT32 g_currentPidContainerNum;  // 当前PID容器数量
STATIC LosProcessCB *g_defaultProcessCB = NULL;  // 默认进程控制块指针
STATIC LosTaskCB *g_defaultTaskCB = NULL;  // 默认任务控制块指针

/**
 * @brief 释放进程的虚拟PID资源
 * @param processCB 进程控制块指针
 * @details 遍历释放进程在各级PID容器中的虚拟PID资源，当容器引用计数为0时释放容器内存
 */
STATIC VOID FreeVpid(LosProcessCB *processCB)
{
    PidContainer *pidContainer = processCB->container->pidContainer;  // 获取进程所在的PID容器
    UINT32 vpid = processCB->processID;  // 获取进程的虚拟PID

    // 循环释放各级容器中的虚拟PID资源，直到容器为空或vpid无效
    while ((pidContainer != NULL) && !OS_PID_CHECK_INVALID(vpid)) {
        ProcessVid *processVid = &pidContainer->pidArray[vpid];  // 获取虚拟PID对应的数组元素
        processVid->cb = (UINTPTR)g_defaultProcessCB;  // 重置进程控制块指针为默认值
        vpid = processVid->vpid;  // 获取父容器中的虚拟PID
        processVid->vpid = OS_INVALID_VALUE;  // 重置父容器虚拟PID为无效值
        LOS_ListTailInsert(&pidContainer->pidFreeList, &processVid->node);  // 将节点插回空闲列表
        LOS_AtomicDec(&pidContainer->rc);  // 原子操作：减少容器引用计数
        PidContainer *parentPidContainer = pidContainer->parent;  // 获取父容器
        if (LOS_AtomicRead(&pidContainer->rc) > 0) {  // 检查容器引用计数是否仍大于0
            pidContainer = parentPidContainer;  // 切换到父容器继续处理
            continue;  // 继续循环
        }
        g_currentPidContainerNum--;  // 减少当前PID容器总数
        (VOID)LOS_MemFree(m_aucSysMem1, pidContainer->rootPGroup);  // 释放进程组根节点内存
        (VOID)LOS_MemFree(m_aucSysMem1, pidContainer);  // 释放PID容器内存
        if (pidContainer == processCB->container->pidContainer) {  // 检查是否为进程直接关联的容器
            processCB->container->pidContainer = NULL;  // 清空容器指针
        }
        pidContainer = parentPidContainer;  // 切换到父容器继续处理
    }
}

/**
 * @brief 获取PID容器中的空闲虚拟PID
 * @param pidContainer PID容器指针
 * @return 成功返回空闲虚拟PID结构体指针，失败返回NULL
 */
STATIC ProcessVid *OsGetFreeVpid(PidContainer *pidContainer)
{
    if (LOS_ListEmpty(&pidContainer->pidFreeList)) {  // 检查空闲列表是否为空
        return NULL;  // 空列表返回NULL
    }

    // 从空闲列表头部获取第一个空闲虚拟PID节点
    ProcessVid *vpid = LOS_DL_LIST_ENTRY(LOS_DL_LIST_FIRST(&pidContainer->pidFreeList), ProcessVid, node);
    LOS_ListDelete(&vpid->node);  // 从空闲列表中删除该节点
    return vpid;  // 返回获取到的虚拟PID结构体指针
}

/**
 * @brief 不安全地分配指定的虚拟PID（无锁保护）
 * @param vpid 要分配的虚拟PID值
 * @param pidContainer PID容器指针
 * @param processCB 进程控制块指针
 * @param parent 父进程控制块指针
 * @return 成功返回分配的虚拟PID，失败返回OS_INVALID_VALUE
 * @note 该函数不提供锁保护，需在外部确保线程安全
 */
UINT32 OsAllocSpecifiedVpidUnsafe(UINT32 vpid, PidContainer *pidContainer,
                                  LosProcessCB *processCB, LosProcessCB *parent)
{
    if ((pidContainer == NULL) || OS_PID_CHECK_INVALID(vpid)) {  // 参数合法性检查
        return OS_INVALID_VALUE;  // 参数无效返回错误
    }

    if (LOS_AtomicRead(&pidContainer->lock) > 0) {  // 检查容器是否被锁定
        return OS_INVALID_VALUE;  // 已锁定返回错误
    }

    ProcessVid *processVid = &pidContainer->pidArray[vpid];  // 获取虚拟PID对应的数组元素
    if (processVid->cb != (UINTPTR)g_defaultProcessCB) {  // 检查该虚拟PID是否空闲
        return OS_INVALID_VALUE;  // 非空闲返回错误
    }

    processVid->cb = (UINTPTR)processCB;  // 设置进程控制块指针
    processVid->realParent = parent;  // 设置实际父进程
    processCB->processID = vpid;  // 设置进程的虚拟PID
    LOS_ListDelete(&processVid->node);  // 从空闲列表中删除该节点
    LOS_AtomicInc(&pidContainer->rc);  // 原子操作：增加容器引用计数

    if (vpid == OS_USER_ROOT_PROCESS_ID) {  // 检查是否为用户根进程ID
        if (parent != NULL) {  // 如果父进程存在
            ProcessVid *vppidItem = &pidContainer->pidArray[0];  // 获取PPID(0)对应的元素
            LOS_ListDelete(&vppidItem->node);  // 从空闲列表中删除
            vppidItem->cb = (UINTPTR)parent;  // 设置PPID对应的进程控制块
        }

        if (OsCreateProcessGroup(processCB) == NULL) {  // 创建进程组
            return OS_INVALID_VALUE;  // 创建失败返回错误
        }
    }

    pidContainer = pidContainer->parent;  // 切换到父容器
    while (pidContainer != NULL) {  // 循环处理各级父容器
        ProcessVid *item = OsGetFreeVpid(pidContainer);  // 获取父容器中的空闲虚拟PID
        if (item == NULL) {  // 如果没有空闲虚拟PID
            break;  // 退出循环
        }

        item->cb = (UINTPTR)processCB;  // 设置进程控制块指针
        processVid->vpid = item->vid;  // 记录父容器中的虚拟PID
        LOS_AtomicInc(&pidContainer->rc);  // 原子操作：增加父容器引用计数
        processVid = item;  // 更新当前虚拟PID结构体指针
        pidContainer = pidContainer->parent;  // 移动到上一级父容器
    }
    return processCB->processID;  // 返回分配的虚拟PID
}

/**
 * @brief 分配虚拟PID
 * @param processCB 进程控制块指针
 * @param parent 父进程控制块指针
 * @return 成功返回分配的虚拟PID，失败返回OS_INVALID_VALUE
 */
STATIC UINT32 OsAllocVpid(LosProcessCB *processCB, LosProcessCB *parent)
{
    ProcessVid *oldProcessVid = NULL;  // 用于记录前一个虚拟PID结构体
    PidContainer *pidContainer = processCB->container->pidContainer;  // 获取进程所在的PID容器
    if ((pidContainer == NULL) || (LOS_AtomicRead(&pidContainer->lock) > 0)) {  // 参数检查和容器锁定检查
        return OS_INVALID_VALUE;  // 容器无效或已锁定返回错误
    }

    processCB->processID = OS_INVALID_VALUE;  // 初始化进程ID为无效值
    do {
        ProcessVid *vpid = OsGetFreeVpid(pidContainer);  // 获取空闲虚拟PID
        if (vpid == NULL) {  // 如果没有空闲虚拟PID
            break;  // 退出循环
        }
        vpid->cb = (UINTPTR)processCB;  // 设置进程控制块指针
        if (processCB->processID == OS_INVALID_VALUE) {  // 如果进程ID尚未设置
            processCB->processID = vpid->vid;  // 设置进程ID
            vpid->realParent = parent;  // 设置实际父进程
        } else {
            oldProcessVid->vpid = vpid->vid;  // 设置父容器中的虚拟PID
        }
        LOS_AtomicInc(&pidContainer->rc);  // 原子操作：增加容器引用计数
        oldProcessVid = vpid;  // 更新前一个虚拟PID结构体指针
        pidContainer = pidContainer->parent;  // 移动到父容器
    } while (pidContainer != NULL);  // 循环处理所有父容器
    return processCB->processID;  // 返回分配的虚拟PID
}

/**
 * @brief 获取PID容器中的空闲虚拟TID
 * @param pidContainer PID容器指针
 * @return 成功返回空闲虚拟TID结构体指针，失败返回NULL
 */
STATIC ProcessVid *OsGetFreeVtid(PidContainer *pidContainer)
{
    if (LOS_ListEmpty(&pidContainer->tidFreeList)) {  // 检查TID空闲列表是否为空
        return NULL;  // 空列表返回NULL
    }

    // 从TID空闲列表头部获取第一个空闲节点
    ProcessVid *vtid = LOS_DL_LIST_ENTRY(LOS_DL_LIST_FIRST(&pidContainer->tidFreeList), ProcessVid, node);
    LOS_ListDelete(&vtid->node);  // 从空闲列表中删除该节点
    return vtid;  // 返回获取到的虚拟TID结构体指针
}

/**
 * @brief 释放任务的虚拟TID资源
 * @param taskCB 任务控制块指针
 * @details 遍历释放任务在各级PID容器中的虚拟TID资源
 */
VOID OsFreeVtid(LosTaskCB *taskCB)
{
    PidContainer *pidContainer = taskCB->pidContainer;  // 获取任务所在的PID容器
    UINT32 vtid = taskCB->taskID;  // 获取任务的虚拟TID

    // 循环释放各级容器中的虚拟TID资源，直到容器为空或vtid无效
    while ((pidContainer != NULL) && !OS_TID_CHECK_INVALID(vtid)) {
        ProcessVid *item = &pidContainer->tidArray[vtid];  // 获取虚拟TID对应的数组元素
        item->cb = (UINTPTR)g_defaultTaskCB;  // 重置任务控制块指针为默认值
        vtid = item->vpid;  // 获取父容器中的虚拟TID
        item->vpid = OS_INVALID_VALUE;  // 重置父容器虚拟TID为无效值
        item->realParent = NULL;  // 清空实际父进程
        LOS_ListTailInsert(&pidContainer->tidFreeList, &item->node);  // 将节点插回TID空闲列表
        pidContainer = pidContainer->parent;  // 切换到父容器继续处理
    }
    taskCB->pidContainer = NULL;  // 清空任务的PID容器指针
}

/**
 * @brief 分配虚拟TID
 * @param taskCB 任务控制块指针
 * @param processCB 所属进程控制块指针
 * @return 成功返回分配的虚拟TID，失败返回OS_INVALID_VALUE
 */
UINT32 OsAllocVtid(LosTaskCB *taskCB, const LosProcessCB *processCB)
{
    PidContainer *pidContainer = processCB->container->pidContainer;  // 获取进程所在的PID容器
    ProcessVid *oldTaskVid = NULL;  // 用于记录前一个虚拟TID结构体

    do {
        ProcessVid *item = OsGetFreeVtid(pidContainer);  // 获取空闲虚拟TID
        if (item == NULL) {  // 如果没有空闲虚拟TID
            return OS_INVALID_VALUE;  // 返回错误
        }

        // 如果存在父容器且当前TID为0（表示init进程）
        if ((pidContainer->parent != NULL) && (item->vid == 0)) {
            item->cb = (UINTPTR)OsCurrTaskGet();  // 设置为当前任务
            item->vpid = OsCurrTaskGet()->taskID;  // 设置当前任务的TID
            continue;  // 继续下一次循环
        }

        item->cb = (UINTPTR)taskCB;  // 设置任务控制块指针
        if (taskCB->pidContainer == NULL) {  // 如果任务尚未关联PID容器
            taskCB->pidContainer = pidContainer;  // 设置任务的PID容器
            taskCB->taskID = item->vid;  // 设置任务的TID
        } else {
            oldTaskVid->vpid = item->vid;  // 设置父容器中的虚拟TID
        }
        oldTaskVid = item;  // 更新前一个虚拟TID结构体指针
        pidContainer = pidContainer->parent;  // 移动到父容器
    } while (pidContainer != NULL);  // 循环处理所有父容器

    return taskCB->taskID;  // 返回分配的虚拟TID
}

/**
 * @brief 销毁PID容器中的所有进程
 * @param curr 当前进程控制块指针
 * @details 遍历所有进程，将非当前进程的子进程重新父化到当前进程，并发送终止信号
 */
VOID OsPidContainerDestroyAllProcess(LosProcessCB *curr)
{
    INT32 ret;  // 返回值
    UINT32 intSave;  // 中断状态保存变量
    PidContainer *pidContainer = curr->container->pidContainer;  // 获取当前PID容器
    LOS_AtomicInc(&pidContainer->lock);  // 原子操作：锁定容器

    // 遍历所有可能的进程ID（从2开始，跳过系统进程）  // 2: 普通进程起始ID
    for (UINT32 index = 2; index < LOSCFG_BASE_CORE_PROCESS_LIMIT; index++) {
        SCHEDULER_LOCK(intSave);  // 关闭调度器
        LosProcessCB *processCB = OS_PCB_FROM_PID(index);  // 从PID获取进程控制块
        // 检查进程是否未使用或无父进程
        if (OsProcessIsUnused(processCB) || (processCB->parentProcess == NULL)) {
            SCHEDULER_UNLOCK(intSave);  // 打开调度器
            continue;  // 跳过该进程
        }

        if (curr != processCB->parentProcess) {  // 如果当前进程不是该进程的父进程
            LOS_ListDelete(&processCB->siblingList);  // 从原有兄弟链表中删除
            if (OsProcessIsInactive(processCB)) {  // 检查进程是否已终止
                // 插入到当前进程的退出子进程列表
                LOS_ListTailInsert(&curr->exitChildList, &processCB->siblingList);
            } else {
                // 插入到当前进程的子进程列表
                LOS_ListTailInsert(&curr->childrenList, &processCB->siblingList);
            }
            processCB->parentProcess = curr;  // 更新父进程为当前进程
        }
        SCHEDULER_UNLOCK(intSave);  // 打开调度器

        ret = OsKillLock(index, SIGKILL);  // 发送终止信号
        if (ret < 0) {  // 检查是否发送成功
            PRINT_ERR("Pid container kill all process failed, pid %u, errno=%d\n", index, -ret);
        }

        ret = LOS_Wait(index, NULL, 0, NULL);  // 等待进程退出
        if (ret != index) {  // 检查等待结果
            PRINT_ERR("Pid container wait pid %d failed, errno=%d\n", index, -ret);
        }
    }
}

/**
 * @brief 创建新的PID容器
 * @param parent 父PID容器指针
 * @return 成功返回新PID容器指针，失败返回NULL
 * @details 初始化PID和TID空闲列表，设置容器层级和引用状态
 */
STATIC PidContainer *CreateNewPidContainer(PidContainer *parent)
{
    UINT32 index;  // 循环索引
    // 分配PID容器内存
    PidContainer *newPidContainer = (PidContainer *)LOS_MemAlloc(m_aucSysMem1, sizeof(PidContainer));
    if (newPidContainer == NULL) {  // 检查内存分配是否成功
        return NULL;  // 分配失败返回NULL
    }
    (VOID)memset_s(newPidContainer, sizeof(PidContainer), 0, sizeof(PidContainer));  // 初始化内存为0

    newPidContainer->containerID = OsAllocContainerID();  // 分配容器ID
    LOS_ListInit(&newPidContainer->pidFreeList);  // 初始化PID空闲列表
    // 初始化PID数组并添加到空闲列表
    for (index = 0; index < LOSCFG_BASE_CORE_PROCESS_LIMIT; index++) {
        ProcessVid *vpid = &newPidContainer->pidArray[index];  // 获取数组元素
        vpid->vid = index;  // 设置虚拟ID
        vpid->vpid = OS_INVALID_VALUE;  // 初始化父容器虚拟ID为无效值
        vpid->cb = (UINTPTR)g_defaultProcessCB;  // 设置默认进程控制块
        // 将节点插入PID空闲列表尾部
        LOS_ListTailInsert(&newPidContainer->pidFreeList, &vpid->node);
    }

    LOS_ListInit(&newPidContainer->tidFreeList);  // 初始化TID空闲列表
    // 初始化TID数组并添加到空闲列表
    for (index = 0; index < LOSCFG_BASE_CORE_TSK_LIMIT; index++) {
        ProcessVid *vtid = &newPidContainer->tidArray[index];  // 获取数组元素
        vtid->vid = index;  // 设置虚拟ID
        vtid->vpid = OS_INVALID_VALUE;  // 初始化父容器虚拟ID为无效值
        vtid->cb = (UINTPTR)g_defaultTaskCB;  // 设置默认任务控制块
        // 将节点插入TID空闲列表尾部
        LOS_ListTailInsert(&newPidContainer->tidFreeList, &vtid->node);
    }

    newPidContainer->parent = parent;  // 设置父容器
    if (parent != NULL) {  // 如果存在父容器
        // 设置容器层级为父容器层级+1
        LOS_AtomicSet(&newPidContainer->level, parent->level + 1);
        newPidContainer->referenced = FALSE;  // 设置未被引用
    } else {  // 如果是根容器
        LOS_AtomicSet(&newPidContainer->level, 0);  // 设置层级为0
        newPidContainer->referenced = TRUE;  // 设置已被引用
    }
    return newPidContainer;  // 返回新创建的PID容器
}

/**
 * @brief 销毁PID容器
 * @param container 容器指针
 * @param processCB 进程控制块指针
 * @details 释放进程的虚拟PID资源，当容器引用计数为0时释放容器内存
 */
VOID OsPidContainerDestroy(Container *container, LosProcessCB *processCB)
{
    if (container == NULL) {  // 检查容器指针是否为空
        return;  // 直接返回
    }

    PidContainer *pidContainer = container->pidContainer;  // 获取PID容器
    if (pidContainer == NULL) {  // 检查PID容器是否为空
        return;  // 直接返回
    }

    if (processCB != NULL) {  // 如果进程控制块不为空
        FreeVpid(processCB);  // 释放进程的虚拟PID
    }

    // 如果存在子容器且与当前PID容器不同
    if ((container->pidForChildContainer != NULL) && (pidContainer != container->pidForChildContainer)) {
        LOS_AtomicDec(&container->pidForChildContainer->rc);  // 减少子容器引用计数
        // 如果子容器引用计数小于等于0
        if (LOS_AtomicRead(&container->pidForChildContainer->rc) <= 0) {
            g_currentPidContainerNum--;  // 减少当前PID容器总数
            // 释放子容器内存
            (VOID)LOS_MemFree(m_aucSysMem1, container->pidForChildContainer);
            container->pidForChildContainer = NULL;  // 清空子容器指针
        }
    }

    // 如果PID容器不为空且引用计数小于等于0
    if ((container->pidContainer != NULL) && (LOS_AtomicRead(&pidContainer->rc) <= 0)) {
        g_currentPidContainerNum--;  // 减少当前PID容器总数
        container->pidContainer = NULL;  // 清空PID容器指针
        container->pidForChildContainer = NULL;  // 清空子容器指针
        (VOID)LOS_MemFree(m_aucSysMem1, pidContainer->rootPGroup);  // 释放进程组根节点
        (VOID)LOS_MemFree(m_aucSysMem1, pidContainer);  // 释放PID容器内存
    }

    if (processCB != NULL) {  // 如果进程控制块不为空
        OsContainerFree(processCB);  // 释放容器资源
    }
}

/**
 * @brief 创建PID容器
 * @param child 子进程控制块指针
 * @param parent 父进程控制块指针
 * @return 成功返回LOS_OK，失败返回错误码
 * @details 为子进程创建新的PID容器，并分配根进程ID
 */
STATIC UINT32 CreatePidContainer(LosProcessCB *child, LosProcessCB *parent)
{
    UINT32 ret;  // 返回值
    UINT32 intSave;  // 中断状态保存变量

    PidContainer *parentContainer = parent->container->pidContainer;  // 获取父进程的PID容器
    // 创建新的PID容器
    PidContainer *newPidContainer = CreateNewPidContainer(parentContainer);
    if (newPidContainer == NULL) {  // 检查容器创建是否成功
        return ENOMEM;  // 内存不足返回错误
    }

    SCHEDULER_LOCK(intSave);  // 关闭调度器
    // 检查容器层级是否超过限制
    if ((parentContainer->level + 1) >= PID_CONTAINER_LEVEL_LIMIT) {
        (VOID)LOS_MemFree(m_aucSysMem1, newPidContainer);  // 释放新容器内存
        SCHEDULER_UNLOCK(intSave);  // 打开调度器
        return EINVAL;  // 层级超限返回错误
    }

    g_currentPidContainerNum++;  // 增加当前PID容器总数
    newPidContainer->referenced = TRUE;  // 设置容器为已引用
    child->container->pidContainer = newPidContainer;  // 设置子进程的PID容器
    child->container->pidForChildContainer = newPidContainer;  // 设置子容器指针
    // 分配根进程ID
    ret = OsAllocSpecifiedVpidUnsafe(OS_USER_ROOT_PROCESS_ID, newPidContainer, child, parent);
    if (ret == OS_INVALID_VALUE) {  // 检查分配是否成功
        g_currentPidContainerNum--;  // 减少当前PID容器总数
        FreeVpid(child);  // 释放子进程的虚拟PID
        child->container->pidContainer = NULL;  // 清空PID容器指针
        child->container->pidForChildContainer = NULL;  // 清空子容器指针
        SCHEDULER_UNLOCK(intSave);  // 打开调度器
        (VOID)LOS_MemFree(m_aucSysMem1, newPidContainer);  // 释放新容器内存
        return ENOSPC;  // 无可用PID返回错误
    }
    SCHEDULER_UNLOCK(intSave);  // 打开调度器
    return LOS_OK;  // 创建成功返回LOS_OK
}
/**
 * @brief 为子容器分配虚拟PID
 * @param child 子进程控制块指针
 * @param parent 父进程控制块指针
 * @return 成功返回LOS_OK，失败返回错误码
 * @details 根据容器引用状态分配根进程ID或普通虚拟PID，并设置进程间关系
 */
STATIC UINT32 AllocVpidFormPidForChildContainer(LosProcessCB *child, LosProcessCB *parent)
{
    UINT32 ret;  // 返回值
    // 获取父进程的子容器PID容器
    PidContainer *pidContainer = parent->container->pidForChildContainer;
    child->container->pidContainer = pidContainer;  // 设置子进程的PID容器
    child->container->pidForChildContainer = pidContainer;  // 设置子进程的子容器指针

    if (!pidContainer->referenced) {  // 如果容器未被引用
        pidContainer->referenced = TRUE;  // 标记为已引用
        // 分配根进程ID
        ret = OsAllocSpecifiedVpidUnsafe(OS_USER_ROOT_PROCESS_ID, pidContainer, child, parent);
    } else {  // 如果容器已被引用
        ret = OsAllocVpid(child, parent);  // 分配普通虚拟PID
        if (ret != OS_INVALID_VALUE) {  // 如果分配成功
            // 获取容器根进程控制块
            LosProcessCB *parentProcessCB = (LosProcessCB *)pidContainer->pidArray[OS_USER_ROOT_PROCESS_ID].cb;
            child->parentProcess = parentProcessCB;  // 设置父进程为容器根进程
            // 将子进程添加到根进程的子进程列表
            LOS_ListTailInsert(&parentProcessCB->childrenList, &child->siblingList);
            child->pgroup = parentProcessCB->pgroup;  // 继承进程组
            // 将子进程添加到进程组列表
            LOS_ListTailInsert(&parentProcessCB->pgroup->processList, &child->subordinateGroupList);
        }
    }

    if (ret == OS_INVALID_VALUE) {  // 如果分配失败
        FreeVpid(child);  // 释放已分配的虚拟PID
        child->container->pidContainer = NULL;  // 清空PID容器指针
        child->container->pidForChildContainer = NULL;  // 清空子容器指针
        return ENOSPC;  // 返回无可用资源错误
    }

    return LOS_OK;  // 成功返回
}

/**
 * @brief 复制PID容器
 * @param flags 克隆标志
 * @param child 子进程控制块指针
 * @param parent 父进程控制块指针
 * @param processID 输出参数，返回分配的进程ID
 * @return 成功返回LOS_OK，失败返回错误码
 * @details 根据克隆标志决定创建新容器或共享现有容器，并分配相应的虚拟PID
 */
UINT32 OsCopyPidContainer(UINTPTR flags, LosProcessCB *child, LosProcessCB *parent, UINT32 *processID)
{
    UINT32 ret;  // 返回值
    UINT32 intSave;  // 中断状态保存变量

    SCHEDULER_LOCK(intSave);  // 关闭调度器
    PidContainer *parentPidContainer = parent->container->pidContainer;  // 获取父进程PID容器
    // 获取父进程的子容器PID容器
    PidContainer *parentPidContainerForChild = parent->container->pidForChildContainer;
    if (parentPidContainer == parentPidContainerForChild) {  // 如果父进程未执行unshare
        /* 当前进程未执行unshare */
        if (!(flags & CLONE_NEWPID)) {  // 如果未设置新建PID命名空间标志
            child->container->pidContainer = parentPidContainer;  // 共享父进程PID容器
            child->container->pidForChildContainer = parentPidContainer;  // 共享父进程子容器
            ret = OsAllocVpid(child, parent);  // 分配虚拟PID
            SCHEDULER_UNLOCK(intSave);  // 打开调度器
            if (ret == OS_INVALID_VALUE) {  // 如果分配失败
                PRINT_ERR("[%s] alloc vpid failed\n", __FUNCTION__);
                return ENOSPC;  // 返回无可用资源错误
            }
            *processID = child->processID;  // 设置输出参数
            return LOS_OK;  // 成功返回
        }
        SCHEDULER_UNLOCK(intSave);  // 打开调度器

        // 检查容器数量限制
        if (OsContainerLimitCheck(PID_CONTAINER, &g_currentPidContainerNum) != LOS_OK) {
            return EPERM;  // 权限不足错误
        }

        ret = CreatePidContainer(child, parent);  // 创建新的PID容器
        if (ret != LOS_OK) {  // 如果创建失败
            return ret;  // 返回错误码
        }
    } else {  // 如果父进程已执行unshare
        /* 创建unshare后的第一个进程 */
        ret = AllocVpidFormPidForChildContainer(child, parent);  // 为子容器分配PID
        SCHEDULER_UNLOCK(intSave);  // 打开调度器
        if (ret != LOS_OK) {  // 如果分配失败
            return ret;  // 返回错误码
        }
    }

    PidContainer *pidContainer = child->container->pidContainer;  // 获取子进程PID容器
    // 检查是否存在父容器虚拟PID
    if (pidContainer->pidArray[child->processID].vpid == OS_INVALID_VALUE) {
        *processID = child->processID;  // 直接使用当前容器PID
    } else {
        // 使用父容器PID
        *processID = pidContainer->pidArray[child->processID].vpid;
    }
    return LOS_OK;  // 成功返回
}

/**
 * @brief 取消共享PID容器
 * @param flags 克隆标志
 * @param curr 当前进程控制块指针
 * @param newContainer 新容器指针
 * @return 成功返回LOS_OK，失败返回错误码
 * @details 根据标志创建新的PID容器或共享现有容器，实现命名空间隔离
 */
UINT32 OsUnsharePidContainer(UINTPTR flags, LosProcessCB *curr, Container *newContainer)
{
    UINT32 intSave;  // 中断状态保存变量
    if (!(flags & CLONE_NEWPID)) {  // 如果未设置新建PID命名空间标志
        SCHEDULER_LOCK(intSave);  // 关闭调度器
        // 共享当前进程的PID容器
        newContainer->pidContainer = curr->container->pidContainer;
        // 共享当前进程的子容器
        newContainer->pidForChildContainer = curr->container->pidForChildContainer;
        // 如果PID容器和子容器不同
        if (newContainer->pidContainer != newContainer->pidForChildContainer) {
            LOS_AtomicInc(&newContainer->pidForChildContainer->rc);  // 增加子容器引用计数
        }
        SCHEDULER_UNLOCK(intSave);  // 打开调度器
        return LOS_OK;  // 成功返回
    }

    // 检查容器数量限制
    if (OsContainerLimitCheck(PID_CONTAINER, &g_currentPidContainerNum) != LOS_OK) {
        return EPERM;  // 权限不足错误
    }

    // 创建新的PID容器作为当前容器的子容器
    PidContainer *pidForChild = CreateNewPidContainer(curr->container->pidContainer);
    if (pidForChild == NULL) {  // 如果创建失败
        return ENOMEM;  // 内存不足错误
    }

    SCHEDULER_LOCK(intSave);  // 关闭调度器
    // 如果当前进程的PID容器和子容器不同
    if (curr->container->pidContainer != curr->container->pidForChildContainer) {
        SCHEDULER_UNLOCK(intSave);  // 打开调度器
        (VOID)LOS_MemFree(m_aucSysMem1, pidForChild);  // 释放新容器内存
        return EINVAL;  // 参数无效错误
    }

    if (pidForChild->level >= PID_CONTAINER_LEVEL_LIMIT) {  // 检查容器层级是否超限
        SCHEDULER_UNLOCK(intSave);  // 打开调度器
        (VOID)LOS_MemFree(m_aucSysMem1, pidForChild);  // 释放新容器内存
        return EINVAL;  // 参数无效错误
    }

    g_currentPidContainerNum++;  // 增加当前PID容器总数
    newContainer->pidContainer = curr->container->pidContainer;  // 继承当前PID容器
    newContainer->pidForChildContainer = pidForChild;  // 设置新的子容器
    LOS_AtomicSet(&pidForChild->rc, 1);  // 设置引用计数为1
    SCHEDULER_UNLOCK(intSave);  // 打开调度器
    return LOS_OK;  // 成功返回
}

/**
 * @brief 设置PID命名空间容器
 * @param flags 标志位
 * @param container 目标容器指针
 * @param newContainer 新容器指针
 * @return 成功返回LOS_OK
 * @details 根据标志设置新容器的PID容器和子容器，实现命名空间切换
 */
UINT32 OsSetNsPidContainer(UINT32 flags, Container *container, Container *newContainer)
{
    UINT32 ret = LOS_OK;  // 返回值，默认为成功
    LosProcessCB *curr = OsCurrProcessGet();  // 获取当前进程
    newContainer->pidContainer = curr->container->pidContainer;  // 继承当前PID容器

    if (flags & CLONE_NEWPID) {  // 如果设置了新建PID命名空间标志
        newContainer->pidForChildContainer = container->pidContainer;  // 设置子容器为目标容器
        LOS_AtomicInc(&container->pidContainer->rc);  // 增加目标容器引用计数
        return ret;  // 返回成功
    }

    // 继承当前进程的子容器
    newContainer->pidForChildContainer = curr->container->pidForChildContainer;
    // 如果PID容器和子容器不同
    if (newContainer->pidContainer != newContainer->pidForChildContainer) {
        LOS_AtomicInc(&newContainer->pidForChildContainer->rc);  // 增加子容器引用计数
    }
    return LOS_OK;  // 成功返回
}

/**
 * @brief 初始化根PID容器
 * @param pidContainer 输出参数，返回创建的根PID容器指针
 * @return 成功返回LOS_OK，失败返回错误码
 * @details 创建并初始化系统根PID容器，设置默认进程和任务控制块
 */
UINT32 OsInitRootPidContainer(PidContainer **pidContainer)
{
    UINT32 intSave;  // 中断状态保存变量
    g_defaultTaskCB = OsGetDefaultTaskCB();  // 获取默认任务控制块
    g_defaultProcessCB = OsGetDefaultProcessCB();  // 获取默认进程控制块

    PidContainer *newPidContainer = CreateNewPidContainer(NULL);  // 创建无父容器的新PID容器
    if (newPidContainer == NULL) {  // 如果创建失败
        return ENOMEM;  // 内存不足错误
    }

    SCHEDULER_LOCK(intSave);  // 关闭调度器
    g_currentPidContainerNum++;  // 增加当前PID容器总数
    *pidContainer = newPidContainer;  // 设置输出参数
    SCHEDULER_UNLOCK(intSave);  // 打开调度器
    return LOS_OK;  // 成功返回
}

/**
 * @brief 从当前容器获取虚拟PID
 * @param processCB 进程控制块指针
 * @return 成功返回当前容器视角的虚拟PID，失败返回OS_INVALID_VALUE
 * @details 遍历容器层级，将进程真实PID转换为当前容器视角的虚拟PID
 */
UINT32 OsGetVpidFromCurrContainer(const LosProcessCB *processCB)
{
    UINT32 vpid = processCB->processID;  // 获取进程PID
    PidContainer *pidContainer = processCB->container->pidContainer;  // 获取进程所在容器
    PidContainer *currPidContainer = OsCurrTaskGet()->pidContainer;  // 获取当前任务所在容器
    while (pidContainer != NULL) {  // 遍历容器层级
        ProcessVid *vid = &pidContainer->pidArray[vpid];  // 获取虚拟ID结构体
        if (currPidContainer != pidContainer) {  // 如果不是当前容器
            vpid = vid->vpid;  // 获取父容器中的虚拟PID
            pidContainer = pidContainer->parent;  // 移动到父容器
            continue;  // 继续循环
        }

        return vid->vid;  // 返回当前容器视角的虚拟PID
    }
    return OS_INVALID_VALUE;  // 失败返回无效值
}

/**
 * @brief 从根容器获取虚拟PID
 * @param processCB 进程控制块指针
 * @return 成功返回根容器视角的虚拟PID，失败返回OS_INVALID_VALUE
 * @details 遍历容器层级直到根容器，返回根容器视角的虚拟PID
 */
UINT32 OsGetVpidFromRootContainer(const LosProcessCB *processCB)
{
    UINT32 vpid = processCB->processID;  // 获取进程PID
    PidContainer *pidContainer = processCB->container->pidContainer;  // 获取进程所在容器
    while (pidContainer != NULL) {  // 遍历容器层级
        ProcessVid *vid = &pidContainer->pidArray[vpid];  // 获取虚拟ID结构体
        if (pidContainer->parent != NULL) {  // 如果不是根容器
            vpid = vid->vpid;  // 获取父容器中的虚拟PID
            pidContainer = pidContainer->parent;  // 移动到父容器
            continue;  // 继续循环
        }

        return vid->vid;  // 返回根容器视角的虚拟PID
    }
    return OS_INVALID_VALUE;  // 失败返回无效值
}

/**
 * @brief 从当前容器获取虚拟TID
 * @param taskCB 任务控制块指针
 * @return 成功返回当前容器视角的虚拟TID，失败返回OS_INVALID_VALUE
 * @details 遍历容器层级，将任务真实TID转换为当前容器视角的虚拟TID
 */
UINT32 OsGetVtidFromCurrContainer(const LosTaskCB *taskCB)
{
    UINT32 vtid = taskCB->taskID;  // 获取任务TID
    PidContainer *pidContainer = taskCB->pidContainer;  // 获取任务所在容器
    PidContainer *currPidContainer = OsCurrTaskGet()->pidContainer;  // 获取当前任务所在容器
    while (pidContainer != NULL) {  // 遍历容器层级
        ProcessVid *vid = &pidContainer->tidArray[vtid];  // 获取虚拟ID结构体
        if (currPidContainer != pidContainer) {  // 如果不是当前容器
            vtid = vid->vpid;  // 获取父容器中的虚拟TID
            pidContainer = pidContainer->parent;  // 移动到父容器
            continue;  // 继续循环
        }
        return vid->vid;  // 返回当前容器视角的虚拟TID
    }
    return OS_INVALID_VALUE;  // 失败返回无效值
}

/**
 * @brief 通过虚拟PID获取进程控制块
 * @param vpid 虚拟PID
 * @return 成功返回进程控制块指针，失败返回NULL
 * @details 从当前容器中查找虚拟PID对应的进程控制块
 */
LosProcessCB *OsGetPCBFromVpid(UINT32 vpid)
{
    PidContainer *pidContainer = OsCurrTaskGet()->pidContainer;  // 获取当前容器
    ProcessVid *processVid = &pidContainer->pidArray[vpid];  // 获取虚拟PID结构体
    return (LosProcessCB *)processVid->cb;  // 返回进程控制块指针
}

/**
 * @brief 通过虚拟TID获取任务控制块
 * @param vtid 虚拟TID
 * @return 成功返回任务控制块指针，失败返回NULL
 * @details 从当前容器中查找虚拟TID对应的任务控制块
 */
LosTaskCB *OsGetTCBFromVtid(UINT32 vtid)
{
    PidContainer *pidContainer = OsCurrTaskGet()->pidContainer;  // 获取当前容器
    ProcessVid *taskVid = &pidContainer->tidArray[vtid];  // 获取虚拟TID结构体
    return (LosTaskCB *)taskVid->cb;  // 返回任务控制块指针
}

/**
 * @brief 检查进程的父进程是否为真实父进程
 * @param processCB 进程控制块指针
 * @param curr 当前进程控制块指针
 * @return 是真实父进程返回TRUE，否则返回FALSE
 * @details 判断当前进程是否是指定进程的真实父进程（非容器内父进程）
 */
BOOL OsPidContainerProcessParentIsRealParent(const LosProcessCB *processCB, const LosProcessCB *curr)
{
    if (curr == processCB->parentProcess) {  // 如果当前进程是容器内父进程
        return FALSE;  // 返回FALSE
    }

    PidContainer *pidContainer = processCB->container->pidContainer;  // 获取进程所在容器
    ProcessVid *processVid = &pidContainer->pidArray[processCB->processID];  // 获取虚拟PID结构体
    if (processVid->realParent == curr) {  // 检查真实父进程
        return TRUE;  // 是真实父进程，返回TRUE
    }
    return FALSE;  // 不是真实父进程，返回FALSE
}

/**
 * @brief 获取PID容器ID
 * @param pidContainer PID容器指针
 * @return 成功返回容器ID，失败返回OS_INVALID_VALUE
 */
UINT32 OsGetPidContainerID(PidContainer *pidContainer)
{
    if (pidContainer == NULL) {  // 检查容器指针是否为空
        return OS_INVALID_VALUE;  // 返回无效值
    }

    return pidContainer->containerID;  // 返回容器ID
}

/**
 * @brief 获取当前PID容器总数
 * @return 返回当前系统中的PID容器总数
 */
UINT32 OsGetPidContainerCount(VOID)
{
    return g_currentPidContainerNum;  // 返回全局容器计数
}
#endif
