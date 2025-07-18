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

#ifdef LOSCFG_MNT_CONTAINER
#include <unistd.h>
#include "los_mnt_container_pri.h"
#include "los_container_pri.h"
#include "los_process_pri.h"
#include "sys/mount.h"
#include "vnode.h"
#include "internal.h"
STATIC UINT32 g_currentMntContainerNum;  // 当前挂载容器数量

/**
 * @brief 获取当前进程容器的挂载列表
 * @return 挂载列表头指针
 */
LIST_HEAD *GetContainerMntList(VOID)
{
    return &OsCurrProcessGet()->container->mntContainer->mountList;  // 返回当前进程容器的挂载列表
}

/**
 * @brief 创建新的挂载容器
 * @param parent 父挂载容器指针，可为NULL
 * @return 成功返回新创建的挂载容器指针，失败返回NULL
 */
STATIC MntContainer *CreateNewMntContainer(MntContainer *parent)
{
    MntContainer *mntContainer = (MntContainer *)LOS_MemAlloc(m_aucSysMem1, sizeof(MntContainer));  // 分配挂载容器内存
    if (mntContainer == NULL) {
        return NULL;  // 内存分配失败，返回NULL
    }
    mntContainer->containerID = OsAllocContainerID();  // 分配容器ID
    LOS_ListInit(&mntContainer->mountList);  // 初始化挂载列表

    if (parent != NULL) {
        LOS_AtomicSet(&mntContainer->rc, 1);  // 若有父容器，引用计数设为1
    } else {
        LOS_AtomicSet(&mntContainer->rc, 3);  // 若无父容器（根容器），引用计数设为3（对应3个系统进程）
    }
    return mntContainer;
}

/**
 * @brief 为子进程创建挂载容器
 * @param child 子进程控制块指针
 * @param parent 父进程控制块指针
 * @return 成功返回LOS_OK，失败返回错误码
 */
STATIC UINT32 CreateMntContainer(LosProcessCB *child, LosProcessCB *parent)
{
    UINT32 intSave;
    MntContainer *parentContainer = parent->container->mntContainer;  // 获取父进程的挂载容器
    MntContainer *newMntContainer = CreateNewMntContainer(parentContainer);  // 创建新挂载容器
    if (newMntContainer == NULL) {
        return ENOMEM;  // 容器创建失败，返回内存不足错误
    }

    SCHEDULER_LOCK(intSave);  // 关闭调度器
    g_currentMntContainerNum++;  // 增加当前挂载容器数量
    child->container->mntContainer = newMntContainer;  // 关联子进程与新挂载容器
    SCHEDULER_UNLOCK(intSave);  // 开启调度器
    return LOS_OK;
}

/**
 * @brief 初始化根挂载容器
 * @param mntContainer 输出参数，用于返回创建的根挂载容器指针
 * @return 成功返回LOS_OK，失败返回错误码
 */
UINT32 OsInitRootMntContainer(MntContainer **mntContainer)
{
    UINT32 intSave;
    MntContainer *newMntContainer = CreateNewMntContainer(NULL);  // 创建无父容器的新挂载容器（根容器）
    if (newMntContainer == NULL) {
        return ENOMEM;  // 容器创建失败，返回内存不足错误
    }

    SCHEDULER_LOCK(intSave);  // 关闭调度器
    g_currentMntContainerNum++;  // 增加当前挂载容器数量
    *mntContainer = newMntContainer;  // 返回根挂载容器指针
    SCHEDULER_UNLOCK(intSave);  // 开启调度器
    return LOS_OK;
}

/**
 * @brief 复制挂载列表
 * @param parentContainer 父挂载容器指针
 * @param newContainer 新挂载容器指针
 * @return 成功返回LOS_OK，失败返回错误码
 */
STATIC UINT32 CopyMountList(MntContainer *parentContainer, MntContainer *newContainer)
{
    struct Mount *mnt = NULL;
    VnodeHold();  // 持有Vnode锁
    LOS_DL_LIST_FOR_EACH_ENTRY(mnt, &parentContainer->mountList, struct Mount, mountList) {  // 遍历父容器挂载列表
        struct Mount *newMnt = (struct Mount *)zalloc(sizeof(struct Mount));  // 分配新挂载结构体内存
        if (newMnt == NULL) {
            VnodeDrop();  // 释放Vnode锁
            return ENOMEM;  // 内存分配失败，返回错误
        }
        *newMnt = *mnt;  // 复制挂载结构体内容
        LOS_ListTailInsert(&newContainer->mountList, &newMnt->mountList);  // 将新挂载项插入新容器挂载列表尾部
        newMnt->vnodeCovered->mntCount++;  // 增加覆盖Vnode的挂载计数
    }
    VnodeDrop();  // 释放Vnode锁
    return LOS_OK;
}

/**
 * @brief 复制挂载容器（用于进程克隆）
 * @param flags 克隆标志位
 * @param child 子进程控制块指针
 * @param parent 父进程控制块指针
 * @return 成功返回LOS_OK，失败返回错误码
 */
UINT32 OsCopyMntContainer(UINTPTR flags, LosProcessCB *child, LosProcessCB *parent)
{
    UINT32 ret;
    UINT32 intSave;
    MntContainer *currMntContainer = parent->container->mntContainer;  // 获取父进程的挂载容器

    if (!(flags & CLONE_NEWNS)) {  // 若未设置CLONE_NEWNS标志（不创建新的挂载命名空间）
        SCHEDULER_LOCK(intSave);  // 关闭调度器
        LOS_AtomicInc(&currMntContainer->rc);  // 增加容器引用计数
        child->container->mntContainer = currMntContainer;  // 子进程共享父进程的挂载容器
        SCHEDULER_UNLOCK(intSave);  // 开启调度器
        return LOS_OK;
    }

    if (OsContainerLimitCheck(MNT_CONTAINER, &g_currentMntContainerNum) != LOS_OK) {  // 检查挂载容器数量是否超限
        return EPERM;  // 超限返回权限错误
    }

    ret = CreateMntContainer(child, parent);  // 创建新的挂载容器
    if (ret != LOS_OK) {
        return ret;  // 创建失败，返回错误码
    }

    return CopyMountList(currMntContainer, child->container->mntContainer);  // 复制挂载列表到新容器
}

/**
 * @brief 取消共享挂载容器（用于unshare系统调用）
 * @param flags 标志位
 * @param curr 当前进程控制块指针
 * @param newContainer 新容器指针
 * @return 成功返回LOS_OK，失败返回错误码
 */
UINT32 OsUnshareMntContainer(UINTPTR flags, LosProcessCB *curr, Container *newContainer)
{
    UINT32 intSave;
    UINT32 ret;
    MntContainer *parentContainer = curr->container->mntContainer;  // 获取当前进程的挂载容器

    if (!(flags & CLONE_NEWNS)) {  // 若未设置CLONE_NEWNS标志
        SCHEDULER_LOCK(intSave);  // 关闭调度器
        newContainer->mntContainer = parentContainer;  // 新容器共享当前挂载容器
        LOS_AtomicInc(&parentContainer->rc);  // 增加容器引用计数
        SCHEDULER_UNLOCK(intSave);  // 开启调度器
        return LOS_OK;
    }

    if (OsContainerLimitCheck(MNT_CONTAINER, &g_currentMntContainerNum) != LOS_OK) {  // 检查挂载容器数量是否超限
        return EPERM;  // 超限返回权限错误
    }

    MntContainer *mntContainer = CreateNewMntContainer(parentContainer);  // 创建新的挂载容器
    if (mntContainer == NULL) {
        return ENOMEM;  // 容器创建失败，返回内存不足错误
    }

    ret = CopyMountList(parentContainer, mntContainer);  // 复制挂载列表到新容器
    if (ret != LOS_OK) {
        (VOID)LOS_MemFree(m_aucSysMem1, mntContainer);  // 复制失败，释放新容器内存
        return ret;
    }

    SCHEDULER_LOCK(intSave);  // 关闭调度器
    newContainer->mntContainer = mntContainer;  // 关联新容器与新挂载容器
    g_currentMntContainerNum++;  // 增加当前挂载容器数量
    SCHEDULER_UNLOCK(intSave);  // 开启调度器
    return LOS_OK;
}

/**
 * @brief 设置命名空间挂载容器（用于setns系统调用）
 * @param flags 标志位
 * @param container 目标容器指针
 * @param newContainer 新容器指针
 * @return 成功返回LOS_OK，失败返回错误码
 */
UINT32 OsSetNsMntContainer(UINT32 flags, Container *container, Container *newContainer)
{
    if (flags & CLONE_NEWNS) {  // 若设置CLONE_NEWNS标志
        newContainer->mntContainer = container->mntContainer;  // 新容器使用目标容器的挂载容器
        LOS_AtomicInc(&container->mntContainer->rc);  // 增加容器引用计数
        return LOS_OK;
    }

    newContainer->mntContainer = OsCurrProcessGet()->container->mntContainer;  // 新容器使用当前进程的挂载容器
    LOS_AtomicInc(&newContainer->mntContainer->rc);  // 增加容器引用计数
    return LOS_OK;
}

/**
 * @brief 释放挂载列表
 * @param mountList 挂载列表头指针
 */
STATIC VOID FreeMountList(LIST_HEAD *mountList)
{
    struct Mount *mnt = NULL;
    struct Mount *nextMnt = NULL;

    VnodeHold();  // 持有Vnode锁
    if (LOS_ListEmpty(mountList)) {  // 若挂载列表为空
        VnodeDrop();  // 释放Vnode锁
        return;
    }

    LOS_DL_LIST_FOR_EACH_ENTRY_SAFE(mnt, nextMnt, mountList, struct Mount, mountList) {  // 安全遍历挂载列表（防止删除当前节点后断链）
        if (mnt->vnodeCovered->mntCount > 0) {  // 若覆盖Vnode的挂载计数大于0
            mnt->vnodeCovered->mntCount--;  // 减少挂载计数
            LOS_ListDelete(&mnt->mountList);  // 从列表中删除挂载项
            free(mnt);  // 释放挂载结构体内存
        } else {
            umount(mnt->pathName);  // 卸载文件系统
        }
    }
    VnodeDrop();  // 释放Vnode锁
    return;
}

/**
 * @brief 销毁挂载容器
 * @param container 容器指针
 */
VOID OsMntContainerDestroy(Container *container)
{
    UINT32 intSave;
    if (container == NULL) {
        return;  // 容器为空，直接返回
    }

    SCHEDULER_LOCK(intSave);  // 关闭调度器
    MntContainer *mntContainer = container->mntContainer;  // 获取容器的挂载容器
    if (mntContainer == NULL) {
        SCHEDULER_UNLOCK(intSave);  // 挂载容器为空，开启调度器并返回
        return;
    }

    LOS_AtomicDec(&mntContainer->rc);  // 减少容器引用计数
    if (LOS_AtomicRead(&mntContainer->rc) > 0) {  // 若引用计数仍大于0
        SCHEDULER_UNLOCK(intSave);  // 开启调度器并返回
        return;
    }

    g_currentMntContainerNum--;  // 减少当前挂载容器数量
    SCHEDULER_UNLOCK(intSave);  // 开启调度器
    FreeMountList(&mntContainer->mountList);  // 释放挂载列表
    container->mntContainer = NULL;  // 解除容器与挂载容器的关联
    (VOID)LOS_MemFree(m_aucSysMem1, mntContainer);  // 释放挂载容器内存
    return;
}

/**
 * @brief 获取挂载容器ID
 * @param mntContainer 挂载容器指针
 * @return 成功返回容器ID，失败返回OS_INVALID_VALUE
 */
UINT32 OsGetMntContainerID(MntContainer *mntContainer)
{
    if (mntContainer == NULL) {
        return OS_INVALID_VALUE;  // 容器为空，返回无效值
    }

    return mntContainer->containerID;  // 返回容器ID
}

/**
 * @brief 获取当前挂载容器数量
 * @return 当前挂载容器数量
 */
UINT32 OsGetMntContainerCount(VOID)
{
    return g_currentMntContainerNum;  // 返回当前挂载容器数量
}
#endif
