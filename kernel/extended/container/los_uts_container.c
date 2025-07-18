/*
 * Copyright (c) 2022-2022 Huawei Device Co., Ltd. All rights reserved.
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

#ifdef LOSCFG_UTS_CONTAINER
#include "internal.h"
#include "los_uts_container_pri.h"
#include "los_process_pri.h"

/* 全局变量：当前UTS容器数量计数器 */
STATIC UINT32 g_currentUtsContainerNum;  /* 用于跟踪系统中UTS容器的总数 */

/**
 * @brief 初始化UTS名称结构
 * @param[in,out] name UTS名称结构指针，用于存储系统标识信息
 * @return 成功返回LOS_OK，失败返回LOS_NOK
 * @note 填充utsname结构体的各个字段，包括系统名称、节点名称、版本信息等
 */
STATIC UINT32 InitUtsContainer(struct utsname *name)
{
    INT32 ret = sprintf_s(name->sysname, sizeof(name->sysname), "%s", KERNEL_NAME);  /* 设置系统名称（如"LiteOS"） */
    if (ret < 0) {
        return LOS_NOK;
    }
    ret = sprintf_s(name->nodename, sizeof(name->nodename), "%s", KERNEL_NODE_NAME);  /* 设置节点名称（主机名） */
    if (ret < 0) {
        return LOS_NOK;
    }
    ret = sprintf_s(name->version, sizeof(name->version), "%s %u.%u.%u.%u %s %s",
        KERNEL_NAME, KERNEL_MAJOR, KERNEL_MINOR, KERNEL_PATCH, KERNEL_ITRE, __DATE__, __TIME__);  /* 版本信息格式：内核名 主版本.次版本.补丁.迭代 编译日期 编译时间 */
    if (ret < 0) {
        return LOS_NOK;
    }
    const char *cpuInfo = LOS_CpuInfo();  /* 获取CPU架构信息 */
    ret = sprintf_s(name->machine, sizeof(name->machine), "%s", cpuInfo);  /* 设置机器架构信息（如"armv7-a"） */
    if (ret < 0) {
        return LOS_NOK;
    }
    ret = sprintf_s(name->release, sizeof(name->release), "%u.%u.%u.%u",
        KERNEL_MAJOR, KERNEL_MINOR, KERNEL_PATCH, KERNEL_ITRE);  /* 发布版本号格式：主版本.次版本.补丁.迭代 */
    if (ret < 0) {
        return LOS_NOK;
    }
    name->domainname[0] = '\0';  /* 域名初始化为空字符串 */
    return LOS_OK;
}

/**
 * @brief 创建新的UTS容器
 * @param[in] parent 父UTS容器指针，可为NULL（创建根容器时）
 * @return 成功返回新UTS容器指针，失败返回NULL
 * @note 负责内存分配、容器ID分配和引用计数初始化
 */
STATIC UtsContainer *CreateNewUtsContainer(UtsContainer *parent)
{
    UINT32 ret;
    UINT32 size = sizeof(UtsContainer);
    UtsContainer *utsContainer = (UtsContainer *)LOS_MemAlloc(m_aucSysMem1, size);  /* 从系统内存池分配UTS容器结构 */
    if (utsContainer == NULL) {
        return NULL;  /* 内存分配失败 */
    }
    (VOID)memset_s(utsContainer, sizeof(UtsContainer), 0, sizeof(UtsContainer));  /* 初始化容器内存为0 */

    utsContainer->containerID = OsAllocContainerID();  /* 分配唯一的容器ID */
    if (parent != NULL) {  /* 非根容器（有父容器） */
        LOS_AtomicSet(&utsContainer->rc, 1);  /* 设置引用计数为1（初始引用） */
        return utsContainer;
    }
    LOS_AtomicSet(&utsContainer->rc, 3);  /* 3: 根容器初始引用计数为3（对应三个系统进程） */
    ret = InitUtsContainer(&utsContainer->utsName);  /* 初始化根容器的UTS名称信息 */
    if (ret != LOS_OK) {
        (VOID)LOS_MemFree(m_aucSysMem1, utsContainer);  /* 初始化失败，释放已分配内存 */
        return NULL;
    }
    return utsContainer;
}

/**
 * @brief 为子进程创建UTS容器
 * @param[in] child 子进程控制块指针
 * @param[in] parent 父进程控制块指针
 * @return 成功返回LOS_OK，失败返回ENOMEM
 * @note 从父进程继承UTS名称信息并创建新容器关联到子进程
 */
STATIC UINT32 CreateUtsContainer(LosProcessCB *child, LosProcessCB *parent)
{
    UINT32 intSave;
    UtsContainer *parentContainer = parent->container->utsContainer;  /* 获取父进程的UTS容器 */
    UtsContainer *newUtsContainer = CreateNewUtsContainer(parentContainer);  /* 创建新UTS容器 */
    if (newUtsContainer == NULL) {
        return ENOMEM;  /* 容器创建失败（内存不足） */
    }

    SCHEDULER_LOCK(intSave);  /* 关闭调度器，防止并发访问冲突 */
    g_currentUtsContainerNum++;  /* 增加全局UTS容器计数 */
    (VOID)memcpy_s(&newUtsContainer->utsName, sizeof(newUtsContainer->utsName),
                   &parentContainer->utsName, sizeof(parentContainer->utsName));  /* 复制父容器的UTS名称信息 */
    child->container->utsContainer = newUtsContainer;  /* 将新容器关联到子进程 */
    SCHEDULER_UNLOCK(intSave);  /* 恢复调度器 */
    return LOS_OK;
}

/**
 * @brief 初始化根UTS容器
 * @param[out] utsContainer 输出参数，用于返回根UTS容器指针
 * @return 成功返回LOS_OK，失败返回ENOMEM
 * @note 系统启动时创建第一个UTS容器（根容器）
 */
UINT32 OsInitRootUtsContainer(UtsContainer **utsContainer)
{
    UINT32 intSave;
    UtsContainer *newUtsContainer = CreateNewUtsContainer(NULL);  /* 创建无父容器的新UTS容器（根容器） */
    if (newUtsContainer == NULL) {
        return ENOMEM;
    }

    SCHEDULER_LOCK(intSave);  /* 关闭调度器，确保操作原子性 */
    g_currentUtsContainerNum++;  /* 根容器计数初始化为1 */
    *utsContainer = newUtsContainer;  /* 输出根容器指针 */
    SCHEDULER_UNLOCK(intSave);  /* 恢复调度器 */
    return LOS_OK;
}

/**
 * @brief 复制UTS容器（进程克隆时）
 * @param[in] flags 克隆标志位，包含CLONE_NEWUTS时创建新容器
 * @param[in] child 子进程控制块指针
 * @param[in] parent 父进程控制块指针
 * @return 成功返回LOS_OK，失败返回错误码
 * @note 根据克隆标志决定是共享父容器还是创建新容器
 */
UINT32 OsCopyUtsContainer(UINTPTR flags, LosProcessCB *child, LosProcessCB *parent)
{
    UINT32 intSave;
    UtsContainer *currUtsContainer = parent->container->utsContainer;  /* 获取父进程的UTS容器 */

    if (!(flags & CLONE_NEWUTS)) {  /* 未设置CLONE_NEWUTS标志，共享父容器 */
        SCHEDULER_LOCK(intSave);
        LOS_AtomicInc(&currUtsContainer->rc);  /* 增加父容器引用计数 */
        child->container->utsContainer = currUtsContainer;  /* 子进程共享父容器 */
        SCHEDULER_UNLOCK(intSave);
        return LOS_OK;
    }

    if (OsContainerLimitCheck(UTS_CONTAINER, &g_currentUtsContainerNum) != LOS_OK) {  /* 检查容器数量是否超过限制 */
        return EPERM;  /* 超过限制，返回权限错误 */
    }

    return CreateUtsContainer(child, parent);  /* 创建新UTS容器并关联到子进程 */
}

/**
 * @brief 取消共享UTS容器（unshare系统调用）
 * @param[in] flags unshare标志位，包含CLONE_NEWUTS时创建新容器
 * @param[in] curr 当前进程控制块指针
 * @param[in] newContainer 新容器结构指针
 * @return 成功返回LOS_OK，失败返回错误码
 * @note 为当前进程创建新的UTS容器并解除与原容器的关联
 */
UINT32 OsUnshareUtsContainer(UINTPTR flags, LosProcessCB *curr, Container *newContainer)
{
    UINT32 intSave;
    UtsContainer *parentContainer = curr->container->utsContainer;  /* 获取当前进程的UTS容器 */

    if (!(flags & CLONE_NEWUTS)) {  /* 未设置CLONE_NEWUTS标志，继续使用原容器 */
        SCHEDULER_LOCK(intSave);
        newContainer->utsContainer = parentContainer;
        LOS_AtomicInc(&parentContainer->rc);  /* 增加原容器引用计数 */
        SCHEDULER_UNLOCK(intSave);
        return LOS_OK;
    }

    if (OsContainerLimitCheck(UTS_CONTAINER, &g_currentUtsContainerNum) != LOS_OK) {  /* 检查容器数量限制 */
        return EPERM;
    }

    UtsContainer *utsContainer = CreateNewUtsContainer(parentContainer);  /* 创建新UTS容器 */
    if (utsContainer == NULL) {
        return ENOMEM;
    }

    SCHEDULER_LOCK(intSave);
    newContainer->utsContainer = utsContainer;  /* 新容器关联到进程 */
    g_currentUtsContainerNum++;  /* 增加容器计数 */
    (VOID)memcpy_s(&utsContainer->utsName, sizeof(utsContainer->utsName),
                   &parentContainer->utsName, sizeof(parentContainer->utsName));  /* 复制原容器的UTS名称 */
    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;
}

/**
 * @brief 设置命名空间UTS容器（setns系统调用）
 * @param[in] flags 命名空间标志位
 * @param[in] container 目标命名空间容器指针
 * @param[in] newContainer 新容器结构指针
 * @return 成功返回LOS_OK
 * @note 将进程加入指定的UTS命名空间容器
 */
UINT32 OsSetNsUtsContainer(UINT32 flags, Container *container, Container *newContainer)
{
    if (flags & CLONE_NEWUTS) {  /* 设置CLONE_NEWUTS标志，加入指定UTS容器 */
        newContainer->utsContainer = container->utsContainer;
        LOS_AtomicInc(&container->utsContainer->rc);  /* 增加目标容器引用计数 */
        return LOS_OK;
    }

    newContainer->utsContainer = OsCurrProcessGet()->container->utsContainer;  /* 使用当前进程的UTS容器 */
    LOS_AtomicInc(&newContainer->utsContainer->rc);  /* 增加引用计数 */
    return LOS_OK;
}

/**
 * @brief 销毁UTS容器
 * @param[in] container 包含UTS容器的进程容器结构指针
 * @return 无
 * @note 当容器引用计数为0时释放内存并减少容器总数
 */
VOID OsUtsContainerDestroy(Container *container)
{
    UINT32 intSave;
    if (container == NULL) {
        return;  /* 参数检查：容器结构为空直接返回 */
    }

    SCHEDULER_LOCK(intSave);  /* 关闭调度器，确保引用计数操作原子性 */
    UtsContainer *utsContainer = container->utsContainer;
    if (utsContainer == NULL) {
        SCHEDULER_UNLOCK(intSave);
        return;  /* UTS容器为空，无需销毁 */
    }

    LOS_AtomicDec(&utsContainer->rc);  /* 减少引用计数 */
    if (LOS_AtomicRead(&utsContainer->rc) > 0) {  /* 引用计数仍大于0，尚有进程使用 */
        SCHEDULER_UNLOCK(intSave);
        return;
    }
    g_currentUtsContainerNum--;  /* 引用计数为0，减少容器总数 */
    container->utsContainer = NULL;  /* 解除容器关联 */
    SCHEDULER_UNLOCK(intSave);  /* 恢复调度器 */
    (VOID)LOS_MemFree(m_aucSysMem1, utsContainer);  /* 释放UTS容器内存 */
    return;
}

/**
 * @brief 获取当前进程的UTS名称结构
 * @return 成功返回utsname结构指针，失败返回NULL
 * @note 从当前进程的容器中获取UTS信息
 */
struct utsname *OsGetCurrUtsName(VOID)
{
    Container *container = OsCurrProcessGet()->container;  /* 获取当前进程的容器结构 */
    if (container == NULL) {
        return NULL;
    }
    UtsContainer *utsContainer = container->utsContainer;  /* 获取UTS容器 */
    if (utsContainer == NULL) {
        return NULL;
    }
    return &utsContainer->utsName;  /* 返回UTS名称结构指针 */
}

/**
 * @brief 获取UTS容器ID
 * @param[in] utsContainer UTS容器指针
 * @return 成功返回容器ID，失败返回OS_INVALID_VALUE
 * @note 容器ID在创建时通过OsAllocContainerID分配
 */
UINT32 OsGetUtsContainerID(UtsContainer *utsContainer)
{
    if (utsContainer == NULL) {
        return OS_INVALID_VALUE;  /* 参数为空，返回无效值 */
    }

    return utsContainer->containerID;  /* 返回容器ID */
}

/**
 * @brief 获取当前UTS容器总数
 * @return 返回全局UTS容器计数器的值
 * @note 该值通过g_currentUtsContainerNum全局变量维护
 */
UINT32 OsGetUtsContainerCount(VOID)
{
    return g_currentUtsContainerNum;  /* 返回当前UTS容器数量 */
}

#endif
