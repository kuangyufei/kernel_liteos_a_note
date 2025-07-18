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
#ifdef LOSCFG_IPC_CONTAINER
#include "los_ipc_container_pri.h"
#include "los_config.h"
#include "los_queue_pri.h"
#include "los_vm_shm_pri.h"
#include "los_process_pri.h"
#include "vnode.h"
#include "proc_fs.h"
#include "pthread.h"
#define IPC_INIT_NUM 3  // IPC容器初始数量

STATIC UINT32 g_currentIpcContainerNum = 0;  // 当前IPC容器数量

/**
 * @brief 创建新的IPC容器
 * @param parent 父IPC容器指针，可为NULL
 * @return 成功返回新创建的IPC容器指针，失败返回NULL
 */
STATIC IpcContainer *CreateNewIpcContainer(IpcContainer *parent)
{
    pthread_mutexattr_t attr;
    UINT32 size = sizeof(IpcContainer);
    IpcContainer *ipcContainer = (IpcContainer *)LOS_MemAlloc(m_aucSysMem1, size);  // 分配IPC容器内存
    if (ipcContainer == NULL) {
        return NULL;  // 内存分配失败，返回NULL
    }
    (VOID)memset_s(ipcContainer, size, 0, size);  // 初始化IPC容器内存为0

    ipcContainer->allQueue = OsAllQueueCBInit(&ipcContainer->freeQueueList);  // 初始化队列控制块
    if (ipcContainer->allQueue == NULL) {
        (VOID)LOS_MemFree(m_aucSysMem1, ipcContainer);  // 队列初始化失败，释放已分配内存
        return NULL;
    }
    pthread_mutexattr_init(&attr);  // 初始化互斥锁属性
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE_NP);  // 设置互斥锁为可重入类型
    pthread_mutex_init(&ipcContainer->mqueueMutex, &attr);  // 初始化消息队列互斥锁

    ipcContainer->shmSegs = OsShmCBInit(&ipcContainer->sysvShmMux, &ipcContainer->shmInfo,  // 初始化共享内存控制块
                                        &ipcContainer->shmUsedPageCount);
    if (ipcContainer->shmSegs == NULL) {
        (VOID)LOS_MemFree(m_aucSysMem1, ipcContainer->allQueue);  // 共享内存初始化失败，释放队列内存
        (VOID)LOS_MemFree(m_aucSysMem1, ipcContainer);  // 释放IPC容器内存
        return NULL;
    }
    ipcContainer->containerID = OsAllocContainerID();  // 分配容器ID

    if (parent != NULL) {
        LOS_AtomicSet(&ipcContainer->rc, 1);  // 若有父容器，引用计数设为1
    } else {
        LOS_AtomicSet(&ipcContainer->rc, 3);  // 若无父容器（根容器），引用计数设为3（对应3个系统进程）
    }
    return ipcContainer;
}

/**
 * @brief 为子进程创建IPC容器
 * @param child 子进程控制块指针
 * @param parent 父进程控制块指针
 * @return 成功返回LOS_OK，失败返回错误码
 */
STATIC UINT32 CreateIpcContainer(LosProcessCB *child, LosProcessCB *parent)
{
    UINT32 intSave;
    IpcContainer *parentContainer = parent->container->ipcContainer;  // 获取父进程的IPC容器
    IpcContainer *newIpcContainer = CreateNewIpcContainer(parentContainer);  // 创建新IPC容器
    if (newIpcContainer == NULL) {
        return ENOMEM;  // 容器创建失败，返回内存不足错误
    }

    SCHEDULER_LOCK(intSave);  // 关闭调度器
    g_currentIpcContainerNum++;  // 增加当前IPC容器数量
    child->container->ipcContainer = newIpcContainer;  // 关联子进程与新IPC容器
    SCHEDULER_UNLOCK(intSave);  // 开启调度器
    return LOS_OK;
}

/**
 * @brief 初始化根IPC容器
 * @param ipcContainer 输出参数，用于返回创建的根IPC容器指针
 * @return 成功返回LOS_OK，失败返回错误码
 */
UINT32 OsInitRootIpcContainer(IpcContainer **ipcContainer)
{
    UINT32 intSave;
    IpcContainer *newIpcContainer = CreateNewIpcContainer(NULL);  // 创建无父容器的新IPC容器（根容器）
    if (newIpcContainer == NULL) {
        return ENOMEM;  // 容器创建失败，返回内存不足错误
    }

    SCHEDULER_LOCK(intSave);  // 关闭调度器
    g_currentIpcContainerNum++;  // 增加当前IPC容器数量
    *ipcContainer = newIpcContainer;  // 返回根IPC容器指针
    SCHEDULER_UNLOCK(intSave);  // 开启调度器
    return LOS_OK;
}

/**
 * @brief 复制IPC容器（用于进程克隆）
 * @param flags 克隆标志位
 * @param child 子进程控制块指针
 * @param parent 父进程控制块指针
 * @return 成功返回LOS_OK，失败返回错误码
 */
UINT32 OsCopyIpcContainer(UINTPTR flags, LosProcessCB *child, LosProcessCB *parent)
{
    UINT32 intSave;
    IpcContainer *currIpcContainer = parent->container->ipcContainer;  // 获取父进程的IPC容器

    if (!(flags & CLONE_NEWIPC)) {  // 若未设置CLONE_NEWIPC标志
        SCHEDULER_LOCK(intSave);  // 关闭调度器
        LOS_AtomicInc(&currIpcContainer->rc);  // 增加容器引用计数
        child->container->ipcContainer = currIpcContainer;  // 子进程共享父进程的IPC容器
        SCHEDULER_UNLOCK(intSave);  // 开启调度器
        return LOS_OK;
    }

    if (OsContainerLimitCheck(IPC_CONTAINER, &g_currentIpcContainerNum) != LOS_OK) {  // 检查容器数量是否超限
        return EPERM;  // 超限返回权限错误
    }

    return CreateIpcContainer(child, parent);  // 创建新的IPC容器
}

/**
 * @brief 取消共享IPC容器（用于unshare系统调用）
 * @param flags 标志位
 * @param curr 当前进程控制块指针
 * @param newContainer 新容器指针
 * @return 成功返回LOS_OK，失败返回错误码
 */
UINT32 OsUnshareIpcContainer(UINTPTR flags, LosProcessCB *curr, Container *newContainer)
{
    UINT32 intSave;
    IpcContainer *parentContainer = curr->container->ipcContainer;  // 获取当前进程的IPC容器

    if (!(flags & CLONE_NEWIPC)) {  // 若未设置CLONE_NEWIPC标志
        SCHEDULER_LOCK(intSave);  // 关闭调度器
        newContainer->ipcContainer = parentContainer;  // 新容器共享当前IPC容器
        LOS_AtomicInc(&parentContainer->rc);  // 增加容器引用计数
        SCHEDULER_UNLOCK(intSave);  // 开启调度器
        return LOS_OK;
    }

    if (OsContainerLimitCheck(IPC_CONTAINER, &g_currentIpcContainerNum) != LOS_OK) {  // 检查容器数量是否超限
        return EPERM;  // 超限返回权限错误
    }

    IpcContainer *ipcContainer = CreateNewIpcContainer(parentContainer);  // 创建新的IPC容器
    if (ipcContainer == NULL) {
        return ENOMEM;  // 容器创建失败，返回内存不足错误
    }

    SCHEDULER_LOCK(intSave);  // 关闭调度器
    newContainer->ipcContainer = ipcContainer;  // 关联新容器与新IPC容器
    g_currentIpcContainerNum++;  // 增加当前IPC容器数量
    SCHEDULER_UNLOCK(intSave);  // 开启调度器
    return LOS_OK;
}

/**
 * @brief 设置命名空间IPC容器（用于setns系统调用）
 * @param flags 标志位
 * @param container 目标容器指针
 * @param newContainer 新容器指针
 * @return 成功返回LOS_OK，失败返回错误码
 */
UINT32 OsSetNsIpcContainer(UINT32 flags, Container *container, Container *newContainer)
{
    if (flags & CLONE_NEWIPC) {  // 若设置CLONE_NEWIPC标志
        newContainer->ipcContainer = container->ipcContainer;  // 新容器使用目标容器的IPC容器
        LOS_AtomicInc(&container->ipcContainer->rc);  // 增加容器引用计数
        return LOS_OK;
    }

    newContainer->ipcContainer = OsCurrProcessGet()->container->ipcContainer;  // 新容器使用当前进程的IPC容器
    LOS_AtomicInc(&newContainer->ipcContainer->rc);  // 增加容器引用计数
    return LOS_OK;
}

/**
 * @brief 销毁IPC容器
 * @param container 容器指针
 */
VOID OsIpcContainerDestroy(Container *container)
{
    UINT32 intSave;
    if (container == NULL) {
        return;  // 容器为空，直接返回
    }

    SCHEDULER_LOCK(intSave);  // 关闭调度器
    IpcContainer *ipcContainer = container->ipcContainer;  // 获取容器的IPC容器
    if (ipcContainer == NULL) {
        SCHEDULER_UNLOCK(intSave);  // IPC容器为空，开启调度器并返回
        return;
    }

    LOS_AtomicDec(&ipcContainer->rc);  // 减少容器引用计数
    if (LOS_AtomicRead(&ipcContainer->rc) > 0) {  // 若引用计数仍大于0
        SCHEDULER_UNLOCK(intSave);  // 开启调度器并返回
        return;
    }

    g_currentIpcContainerNum--;  // 减少当前IPC容器数量
    container->ipcContainer = NULL;  // 解除容器与IPC容器的关联
    SCHEDULER_UNLOCK(intSave);  // 开启调度器
    OsShmCBDestroy(ipcContainer->shmSegs, &ipcContainer->shmInfo, &ipcContainer->sysvShmMux);  // 销毁共享内存控制块
    ipcContainer->shmSegs = NULL;
    OsMqueueCBDestroy(ipcContainer->queueTable);  // 销毁消息队列控制块
    (VOID)LOS_MemFree(m_aucSysMem1, ipcContainer->allQueue);  // 释放队列内存
    (VOID)LOS_MemFree(m_aucSysMem1, ipcContainer);  // 释放IPC容器内存
    return;
}

/**
 * @brief 获取IPC容器ID
 * @param ipcContainer IPC容器指针
 * @return 成功返回容器ID，失败返回OS_INVALID_VALUE
 */
UINT32 OsGetIpcContainerID(IpcContainer *ipcContainer)
{
    if (ipcContainer == NULL) {
        return OS_INVALID_VALUE;  // 容器为空，返回无效值
    }

    return ipcContainer->containerID;  // 返回容器ID
}

/**
 * @brief 获取当前进程的IPC容器
 * @return 当前进程的IPC容器指针
 */
IpcContainer *OsGetCurrIpcContainer(VOID)
{
    return OsCurrProcessGet()->container->ipcContainer;  // 返回当前进程的IPC容器
}

/**
 * @brief 获取当前IPC容器数量
 * @return 当前IPC容器数量
 */
UINT32 OsGetIpcContainerCount(VOID)
{
    return g_currentIpcContainerNum;  // 返回当前IPC容器数量
}
#endif
