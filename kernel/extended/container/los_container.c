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

#ifdef LOSCFG_KERNEL_CONTAINER
#include "los_container_pri.h"
#include "los_process_pri.h"
#include "internal.h"
STATIC Container g_rootContainer;                          // 根容器实例，所有容器的父容器
STATIC ContainerLimit g_containerLimit;                    // 容器资源限制结构体，存储各类容器的最大数量限制
STATIC Atomic g_containerCount = 0xF0000000U;              // 容器ID原子计数器，初始值0xF0000000U用于区分容器ID范围
#ifdef LOSCFG_USER_CONTAINER
STATIC Credentials *g_rootCredentials = NULL;               // 用户容器的根凭证，仅在启用用户容器时有效
#endif

/**
 * @brief 分配容器ID
 * @return 新分配的容器ID，通过原子自增保证唯一性
 */
UINT32 OsAllocContainerID(VOID)
{
    return LOS_AtomicIncRet(&g_containerCount);             // 原子自增容器计数器并返回新值作为容器ID
}

/**
 * @brief 初始化系统进程的容器信息
 * @param processCB 进程控制块指针
 * @return 无
 */
VOID OsContainerInitSystemProcess(LosProcessCB *processCB)
{
    processCB->container = &g_rootContainer;                // 将系统进程关联到根容器
#ifdef LOSCFG_USER_CONTAINER
    processCB->credentials = g_rootCredentials;             // 关联根用户凭证（仅用户容器启用时）
#endif
    LOS_AtomicInc(&processCB->container->rc);               // 增加根容器的引用计数
#ifdef LOSCFG_PID_CONTAINER
    // 为系统进程分配指定的虚拟PID（仅PID容器启用时）
    (VOID)OsAllocSpecifiedVpidUnsafe(processCB->processID, processCB->container->pidContainer, processCB, NULL);
#endif
    return;
}

/**
 * @brief 获取指定类型容器的数量限制
 * @param type 容器类型（PID_CONTAINER、USER_CONTAINER等）
 * @return 容器数量限制值，无效类型返回OS_INVALID_VALUE
 */
UINT32 OsGetContainerLimit(ContainerType type)
{
    switch (type) {
#ifdef LOSCFG_PID_CONTAINER
        case PID_CONTAINER:                                 // PID容器类型
        case PID_CHILD_CONTAINER:                           // PID子容器类型
            return g_containerLimit.pidLimit;               // 返回PID容器限制值
#endif
#ifdef LOSCFG_USER_CONTAINER
        case USER_CONTAINER:                                // 用户容器类型
            return g_containerLimit.userLimit;              // 返回用户容器限制值
#endif
#ifdef LOSCFG_UTS_CONTAINER
        case UTS_CONTAINER:                                 // UTS命名空间容器类型
            return g_containerLimit.utsLimit;               // 返回UTS容器限制值
#endif
#ifdef LOSCFG_MNT_CONTAINER
        case MNT_CONTAINER:                                 // 挂载命名空间容器类型
            return g_containerLimit.mntLimit;               // 返回挂载容器限制值
#endif
#ifdef LOSCFG_IPC_CONTAINER
        case IPC_CONTAINER:                                 // IPC命名空间容器类型
            return g_containerLimit.ipcLimit;               // 返回IPC容器限制值
#endif
#ifdef LOSCFG_TIME_CONTAINER
        case TIME_CONTAINER:                                // 时间命名空间容器类型
        case TIME_CHILD_CONTAINER:                          // 时间子命名空间容器类型
            return g_containerLimit.timeLimit;              // 返回时间容器限制值
#endif
#ifdef LOSCFG_NET_CONTAINER
        case NET_CONTAINER:                                 // 网络命名空间容器类型
            return g_containerLimit.netLimit;               // 返回网络容器限制值
#endif
        default:                                            // 无效容器类型
            break;
    }
    return OS_INVALID_VALUE;                                // 返回无效值
}

/**
 * @brief 检查容器数量是否超过限制
 * @param type 容器类型
 * @param containerCount 当前容器数量指针
 * @return 成功返回LOS_OK，超过限制返回EINVAL
 */
UINT32 OsContainerLimitCheck(ContainerType type, UINT32 *containerCount)
{
    UINT32 intSave;                                         // 中断状态保存变量
    SCHEDULER_LOCK(intSave);                                // 关闭调度器，保证操作原子性
    if ((*containerCount) >= OsGetContainerLimit(type)) {   // 检查当前数量是否超过限制
        SCHEDULER_UNLOCK(intSave);                          // 恢复调度器
        return EINVAL;                                      // 返回参数无效错误
    }
    SCHEDULER_UNLOCK(intSave);                              // 恢复调度器
    return LOS_OK;                                          // 返回成功
}

/**
 * @brief 设置指定类型容器的数量限制
 * @param type 容器类型
 * @param value 要设置的限制值
 * @return 成功返回LOS_OK，无效参数返回EINVAL
 */
UINT32 OsSetContainerLimit(ContainerType type, UINT32 value)
{
    UINT32 intSave;                                         // 中断状态保存变量

    if (value > LOSCFG_KERNEL_CONTAINER_DEFAULT_LIMIT) {    // 检查值是否超过内核默认最大限制
        return EINVAL;                                      // 返回参数无效错误
    }

    SCHEDULER_LOCK(intSave);                                // 关闭调度器，保证操作原子性
    switch (type) {
#ifdef LOSCFG_PID_CONTAINER
        case PID_CONTAINER:                                 // PID容器类型
        case PID_CHILD_CONTAINER:                           // PID子容器类型
            g_containerLimit.pidLimit = value;              // 设置PID容器限制值
            break;
#endif
#ifdef LOSCFG_USER_CONTAINER
        case USER_CONTAINER:                                // 用户容器类型
            g_containerLimit.userLimit = value;             // 设置用户容器限制值
            break;
#endif
#ifdef LOSCFG_UTS_CONTAINER
        case UTS_CONTAINER:                                 // UTS命名空间容器类型
            g_containerLimit.utsLimit = value;              // 设置UTS容器限制值
            break;
#endif
#ifdef LOSCFG_MNT_CONTAINER
        case MNT_CONTAINER:                                 // 挂载命名空间容器类型
            g_containerLimit.mntLimit = value;              // 设置挂载容器限制值
            break;
#endif
#ifdef LOSCFG_IPC_CONTAINER
        case IPC_CONTAINER:                                 // IPC命名空间容器类型
            g_containerLimit.ipcLimit = value;              // 设置IPC容器限制值
            break;
#endif
#ifdef LOSCFG_TIME_CONTAINER
        case TIME_CONTAINER:                                // 时间命名空间容器类型
        case TIME_CHILD_CONTAINER:                          // 时间子命名空间容器类型
            g_containerLimit.timeLimit = value;             // 设置时间容器限制值
            break;
#endif
#ifdef LOSCFG_NET_CONTAINER
        case NET_CONTAINER:                                 // 网络命名空间容器类型
            g_containerLimit.netLimit = value;              // 设置网络容器限制值
            break;
#endif
        default:                                            // 无效容器类型
            SCHEDULER_UNLOCK(intSave);                      // 恢复调度器
            return EINVAL;                                  // 返回参数无效错误
    }
    SCHEDULER_UNLOCK(intSave);                              // 恢复调度器
    return LOS_OK;                                          // 返回成功
}

/**
 * @brief 获取指定类型容器的当前数量
 * @param type 容器类型
 * @return 容器当前数量，无效类型返回OS_INVALID_VALUE
 */
UINT32 OsGetContainerCount(ContainerType type)
{
    switch (type) {
#ifdef LOSCFG_PID_CONTAINER
        case PID_CONTAINER:                                 // PID容器类型
        case PID_CHILD_CONTAINER:                           // PID子容器类型
            return OsGetPidContainerCount();                // 获取PID容器数量
#endif
#ifdef LOSCFG_USER_CONTAINER
        case USER_CONTAINER:                                // 用户容器类型
            return OsGetUserContainerCount();               // 获取用户容器数量
#endif
#ifdef LOSCFG_UTS_CONTAINER
        case UTS_CONTAINER:                                 // UTS命名空间容器类型
            return OsGetUtsContainerCount();                // 获取UTS容器数量
#endif
#ifdef LOSCFG_MNT_CONTAINER
        case MNT_CONTAINER:                                 // 挂载命名空间容器类型
            return OsGetMntContainerCount();                // 获取挂载容器数量
#endif
#ifdef LOSCFG_IPC_CONTAINER
        case IPC_CONTAINER:                                 // IPC命名空间容器类型
            return OsGetIpcContainerCount();                // 获取IPC容器数量
#endif
#ifdef LOSCFG_TIME_CONTAINER
        case TIME_CONTAINER:                                // 时间命名空间容器类型
        case TIME_CHILD_CONTAINER:                          // 时间子命名空间容器类型
            return OsGetTimeContainerCount();               // 获取时间容器数量
#endif
#ifdef LOSCFG_NET_CONTAINER
        case NET_CONTAINER:                                 // 网络命名空间容器类型
            return OsGetNetContainerCount();                // 获取网络容器数量
#endif
        default:                                            // 无效容器类型
            break;
    }
    return OS_INVALID_VALUE;                                // 返回无效值
}
/**
 * @brief 初始化根容器及其限制参数
 * @details 为各类容器（用户、PID、UTS、挂载、IPC、时间、网络）设置默认限制值，并初始化根容器实例
 */
VOID OsInitRootContainer(VOID)
{
#ifdef LOSCFG_USER_CONTAINER                     // 用户容器配置开关
    g_containerLimit.userLimit = LOSCFG_KERNEL_CONTAINER_DEFAULT_LIMIT;  // 设置用户容器默认限制值
    OsInitRootUserCredentials(&g_rootCredentials);                       // 初始化根用户凭证
#endif
#ifdef LOSCFG_PID_CONTAINER                      // PID容器配置开关
    g_containerLimit.pidLimit = LOSCFG_KERNEL_CONTAINER_DEFAULT_LIMIT;   // 设置PID容器默认限制值
    (VOID)OsInitRootPidContainer(&g_rootContainer.pidContainer);         // 初始化根PID容器
    g_rootContainer.pidForChildContainer = g_rootContainer.pidContainer; // 初始化子进程PID容器
#endif
#ifdef LOSCFG_UTS_CONTAINER                      // UTS容器配置开关
    g_containerLimit.utsLimit = LOSCFG_KERNEL_CONTAINER_DEFAULT_LIMIT;   // 设置UTS容器默认限制值
    (VOID)OsInitRootUtsContainer(&g_rootContainer.utsContainer);         // 初始化根UTS容器
#endif
#ifdef LOSCFG_MNT_CONTAINER                      // 挂载容器配置开关
    g_containerLimit.mntLimit = LOSCFG_KERNEL_CONTAINER_DEFAULT_LIMIT;   // 设置挂载容器默认限制值
    (VOID)OsInitRootMntContainer(&g_rootContainer.mntContainer);         // 初始化根挂载容器
#endif
#ifdef LOSCFG_IPC_CONTAINER                      // IPC容器配置开关
    g_containerLimit.ipcLimit = LOSCFG_KERNEL_CONTAINER_DEFAULT_LIMIT;   // 设置IPC容器默认限制值
    (VOID)OsInitRootIpcContainer(&g_rootContainer.ipcContainer);         // 初始化根IPC容器
#endif
#ifdef LOSCFG_TIME_CONTAINER                     // 时间容器配置开关
    g_containerLimit.timeLimit = LOSCFG_KERNEL_CONTAINER_DEFAULT_LIMIT;  // 设置时间容器默认限制值
    (VOID)OsInitRootTimeContainer(&g_rootContainer.timeContainer);       // 初始化根时间容器
    g_rootContainer.timeForChildContainer = g_rootContainer.timeContainer; // 初始化子进程时间容器
#endif
#ifdef LOSCFG_NET_CONTAINER                      // 网络容器配置开关
    g_containerLimit.netLimit = LOSCFG_KERNEL_CONTAINER_DEFAULT_LIMIT;   // 设置网络容器默认限制值
    (VOID)OsInitRootNetContainer(&g_rootContainer.netContainer);         // 初始化根网络容器
#endif
    return;
}

/**
 * @brief 创建新容器实例
 * @return Container* 成功返回容器指针，失败返回NULL
 * @note 使用系统内存池分配内存并初始化引用计数
 */
STATIC INLINE Container *CreateContainer(VOID)
{
    Container *container = (Container *)LOS_MemAlloc(m_aucSysMem1, sizeof(Container));  // 从系统内存池分配容器内存
    if (container == NULL) {                                                           // 内存分配失败检查
        return NULL;
    }

    (VOID)memset_s(container, sizeof(Container), 0, sizeof(Container));  // 初始化容器内存为0

    LOS_AtomicInc(&container->rc);  // 原子操作增加容器引用计数
    return container;
}

/**
 * @brief 复制父进程容器到子进程
 * @param[in] flags 容器复制标志位
 * @param[in] child 子进程控制块
 * @param[in] parent 父进程控制块
 * @param[out] processID 子进程ID
 * @return UINT32 成功返回LOS_OK，失败返回错误码
 * @note PID容器初始化必须优先于其他容器
 */
STATIC UINT32 CopyContainers(UINTPTR flags, LosProcessCB *child, LosProcessCB *parent, UINT32 *processID)
{
    UINT32 ret = LOS_OK;
    /* Pid容器初始化必须 precede other container initialization. */
#ifdef LOSCFG_PID_CONTAINER                      // PID容器配置开关
    ret = OsCopyPidContainer(flags, child, parent, processID);  // 复制PID容器
    if (ret != LOS_OK) {                                        // 错误检查
        return ret;
    }
#endif
#ifdef LOSCFG_USER_CONTAINER                     // 用户容器配置开关
    ret = OsCopyCredentials(flags,  child, parent);  // 复制用户凭证
    if (ret != LOS_OK) {                             // 错误检查
        return ret;
    }
#endif
#ifdef LOSCFG_UTS_CONTAINER                     // UTS容器配置开关
    ret = OsCopyUtsContainer(flags, child, parent);  // 复制UTS容器
    if (ret != LOS_OK) {                             // 错误检查
        return ret;
    }
#endif
#ifdef LOSCFG_MNT_CONTAINER                     // 挂载容器配置开关
    ret = OsCopyMntContainer(flags, child, parent);  // 复制挂载容器
    if (ret != LOS_OK) {                             // 错误检查
        return ret;
    }
#endif
#ifdef LOSCFG_IPC_CONTAINER                     // IPC容器配置开关
    ret = OsCopyIpcContainer(flags, child, parent);  // 复制IPC容器
    if (ret != LOS_OK) {                             // 错误检查
        return ret;
    }
#endif
#ifdef LOSCFG_TIME_CONTAINER                    // 时间容器配置开关
    ret = OsCopyTimeContainer(flags, child, parent);  // 复制时间容器
    if (ret != LOS_OK) {                              // 错误检查
        return ret;
    }
#endif
#ifdef LOSCFG_NET_CONTAINER                     // 网络容器配置开关
    ret = OsCopyNetContainer(flags, child, parent);  // 复制网络容器
    if (ret != LOS_OK) {                             // 错误检查
        return ret;
    }
#endif
    return ret;
}

/**
 * @brief 获取容器标志位掩码
 * @return UINT32 容器标志位掩码
 * @note 根据使能的容器类型生成对应的CLONE_*标志位组合
 */
STATIC INLINE UINT32 GetContainerFlagsMask(VOID)
{
    UINT32 mask = 0;  // 标志位掩码初始化为0
#ifdef LOSCFG_PID_CONTAINER
    mask |= CLONE_NEWPID;  // 添加PID容器标志位
#endif
#ifdef LOSCFG_MNT_CONTAINER
    mask |= CLONE_NEWNS;   // 添加挂载命名空间标志位
#endif
#ifdef LOSCFG_IPC_CONTAINER
    mask |= CLONE_NEWIPC;  // 添加IPC容器标志位
#endif
#ifdef LOSCFG_USER_CONTAINER
    mask |= CLONE_NEWUSER; // 添加用户容器标志位
#endif
#ifdef LOSCFG_TIME_CONTAINER
    mask |= CLONE_NEWTIME; // 添加时间容器标志位
#endif
#ifdef LOSCFG_NET_CONTAINER
    mask |= CLONE_NEWNET;  // 添加网络容器标志位
#endif
#ifdef LOSCFG_UTS_CONTAINER
    mask |= CLONE_NEWUTS;  // 添加UTS容器标志位
#endif
    return mask;
}

/**
 * @brief 处理容器复制逻辑（从clone系统调用触发）
 * @param[in] flags 容器复制标志位
 * @param[in] child 子进程控制块
 * @param[in] parent 父进程控制块
 * @param[out] processID 子进程ID
 * @return UINT32 成功返回LOS_OK，失败返回错误码
 * @note 处理容器及所有命名空间的复制逻辑
 */
UINT32 OsCopyContainers(UINTPTR flags, LosProcessCB *child, LosProcessCB *parent, UINT32 *processID)
{
    UINT32 intSave;  // 中断状态保存变量

#ifdef LOSCFG_TIME_CONTAINER
    flags &= ~CLONE_NEWTIME;  // 清除时间容器标志位（特殊处理）
#endif

    if (!(flags & GetContainerFlagsMask())) {  // 检查是否需要创建新容器
#ifdef LOSCFG_PID_CONTAINER
        // 如果父进程的PID容器与子进程PID容器不同，需要创建新容器
        if (parent->container->pidContainer != parent->container->pidForChildContainer) {
            goto CREATE_CONTAINER;  // 跳转到创建容器逻辑
        }
#endif
#ifdef LOSCFG_TIME_CONTAINER
        // 如果父进程的时间容器与子进程时间容器不同，需要创建新容器
        if (parent->container->timeContainer != parent->container->timeForChildContainer) {
            goto CREATE_CONTAINER;  // 跳转到创建容器逻辑
        }
#endif
        SCHEDULER_LOCK(intSave);  // 关闭调度器（临界区保护）
        child->container = parent->container;  // 直接共享父进程容器
        LOS_AtomicInc(&child->container->rc);  // 原子增加容器引用计数
        SCHEDULER_UNLOCK(intSave);  // 恢复调度器
        goto COPY_CONTAINERS;  // 跳转到复制容器内容逻辑
    }

#if defined(LOSCFG_PID_CONTAINER) || defined(LOSCFG_TIME_CONTAINER)
CREATE_CONTAINER:  // 创建容器标签
#endif
    child->container = CreateContainer();  // 创建新容器实例
    if (child->container == NULL) {        // 容器创建失败检查
        return ENOMEM;                     // 返回内存不足错误
    }

COPY_CONTAINERS:  // 复制容器内容标签
    return CopyContainers(flags, child, parent, processID);  // 调用容器复制实现函数
}

/**
 * @brief 释放容器资源
 * @param[in] processCB 进程控制块
 * @note 当容器引用计数为0时才真正释放内存
 */
VOID OsContainerFree(LosProcessCB *processCB)
{
    LOS_AtomicDec(&processCB->container->rc);  // 原子减少容器引用计数
    if (LOS_AtomicRead(&processCB->container->rc) == 0) {  // 检查引用计数是否为0
        (VOID)LOS_MemFree(m_aucSysMem1, processCB->container);  // 释放容器内存
        processCB->container = NULL;                            // 清空容器指针
    }
}

/**
 * @brief 早期销毁容器资源
 * @param[in] processCB 进程控制块
 * @note 在容器销毁前必须先销毁容器内的所有进程
 */
VOID OsOsContainersDestroyEarly(LosProcessCB *processCB)
{
   /* All processes in the container must be destroyed before the container is destroyed. */
#ifdef LOSCFG_PID_CONTAINER
    if (processCB->processID == OS_USER_ROOT_PROCESS_ID) {  // 检查是否为根进程
        OsPidContainerDestroyAllProcess(processCB);         // 销毁PID容器内所有进程
    }
#endif

#ifdef LOSCFG_MNT_CONTAINER
    OsMntContainerDestroy(processCB->container);  // 销毁挂载容器
#endif
}

/**
 * @brief 销毁容器资源
 * @param[in] processCB 进程控制块
 * @note 按类型销毁各类容器资源并释放容器内存
 */
VOID OsContainersDestroy(LosProcessCB *processCB)
{
#ifdef LOSCFG_USER_CONTAINER
    OsUserContainerDestroy(processCB);  // 销毁用户容器
#endif

#ifdef LOSCFG_UTS_CONTAINER
    OsUtsContainerDestroy(processCB->container);  // 销毁UTS容器
#endif

#ifdef LOSCFG_IPC_CONTAINER
    OsIpcContainerDestroy(processCB->container);  // 销毁IPC容器
#endif

#ifdef LOSCFG_TIME_CONTAINER
    OsTimeContainerDestroy(processCB->container);  // 销毁时间容器
#endif

#ifdef LOSCFG_NET_CONTAINER
    OsNetContainerDestroy(processCB->container);  // 销毁网络容器
#endif

#ifndef LOSCFG_PID_CONTAINER  // 如果未启用PID容器
    UINT32 intSave;
    SCHEDULER_LOCK(intSave);  // 关闭调度器（临界区保护）
    OsContainerFree(processCB);  // 释放容器资源
    SCHEDULER_UNLOCK(intSave);  // 恢复调度器
#endif
}

/**
 * @brief 反初始化容器资源
 * @param[in] flags 容器标志位
 * @param[in] container 容器指针
 * @param[in] processCB 进程控制块
 * @note 按标志位选择性销毁容器组件并释放内存
 */
STATIC VOID DeInitContainers(UINT32 flags, Container *container, LosProcessCB *processCB)
{
    UINT32 intSave;  // 中断状态保存变量
    if (container == NULL) {  // 容器指针空检查
        return;
    }

#ifdef LOSCFG_PID_CONTAINER
    SCHEDULER_LOCK(intSave);  // 关闭调度器（临界区保护）
    if ((flags & CLONE_NEWPID) != 0) {  // 检查是否需要销毁PID容器
        OsPidContainerDestroy(container, processCB);  // 带进程参数销毁PID容器
    } else {
        OsPidContainerDestroy(container, NULL);       // 不带进程参数销毁PID容器
    }
    SCHEDULER_UNLOCK(intSave);  // 恢复调度器
#endif

#ifdef LOSCFG_UTS_CONTAINER
    OsUtsContainerDestroy(container);  // 销毁UTS容器
#endif
#ifdef LOSCFG_MNT_CONTAINER
    OsMntContainerDestroy(container);  // 销毁挂载容器
#endif
#ifdef LOSCFG_IPC_CONTAINER
    OsIpcContainerDestroy(container);  // 销毁IPC容器
#endif

#ifdef LOSCFG_TIME_CONTAINER
    OsTimeContainerDestroy(container);  // 销毁时间容器
#endif

#ifdef LOSCFG_NET_CONTAINER
    OsNetContainerDestroy(container);  // 销毁网络容器
#endif

    SCHEDULER_LOCK(intSave);  // 关闭调度器（临界区保护）
    LOS_AtomicDec(&container->rc);  // 原子减少容器引用计数
    if (LOS_AtomicRead(&container->rc) == 0) {  // 检查引用计数是否为0
        (VOID)LOS_MemFree(m_aucSysMem1, container);  // 释放容器内存
    }
    SCHEDULER_UNLOCK(intSave);  // 恢复调度器
}

/**
 * @brief 获取指定类型的容器ID
 * @param[in] processCB 进程控制块
 * @param[in] type 容器类型
 * @return UINT32 成功返回容器ID，失败返回OS_INVALID_VALUE
 * @note 根据容器类型调用对应的ID获取函数
 */
UINT32 OsGetContainerID(LosProcessCB *processCB, ContainerType type)
{
    Container *container = processCB->container;  // 获取进程关联的容器
    if (container == NULL) {                      // 容器指针空检查
        return OS_INVALID_VALUE;                  // 返回无效值
    }

    switch (type) {  // 根据容器类型选择对应的ID获取方式
#ifdef LOSCFG_PID_CONTAINER
        case PID_CONTAINER:          // PID容器类型
            return OsGetPidContainerID(container->pidContainer);
        case PID_CHILD_CONTAINER:    // 子进程PID容器类型
            return OsGetPidContainerID(container->pidForChildContainer);
#endif
#ifdef LOSCFG_USER_CONTAINER
        case USER_CONTAINER:         // 用户容器类型
            return OsGetUserContainerID(processCB->credentials);
#endif
#ifdef LOSCFG_UTS_CONTAINER
        case UTS_CONTAINER:          // UTS容器类型
            return OsGetUtsContainerID(container->utsContainer);
#endif
#ifdef LOSCFG_MNT_CONTAINER
        case MNT_CONTAINER:          // 挂载容器类型
            return OsGetMntContainerID(container->mntContainer);
#endif
#ifdef LOSCFG_IPC_CONTAINER
        case IPC_CONTAINER:          // IPC容器类型
            return OsGetIpcContainerID(container->ipcContainer);
#endif
#ifdef LOSCFG_TIME_CONTAINER
        case TIME_CONTAINER:         // 时间容器类型
            return OsGetTimeContainerID(container->timeContainer);
        case TIME_CHILD_CONTAINER:   // 子进程时间容器类型
            return OsGetTimeContainerID(container->timeForChildContainer);
#endif
#ifdef LOSCFG_NET_CONTAINER
        case NET_CONTAINER:          // 网络容器类型
            return OsGetNetContainerID(container->netContainer);
#endif
        default:                     // 未知容器类型
            break;
    }
    return OS_INVALID_VALUE;  // 返回无效值
}
STATIC UINT32 UnshareCreateNewContainers(UINT32 flags, LosProcessCB *curr, Container *newContainer)
{
    UINT32 ret = LOS_OK;  // 初始化返回值为成功
#ifdef LOSCFG_PID_CONTAINER  // 如果启用PID容器配置
    ret = OsUnsharePidContainer(flags, curr, newContainer);  // 创建PID容器
    if (ret != LOS_OK) {  // 检查创建结果
        return ret;  // 创建失败，返回错误码
    }
#endif
#ifdef LOSCFG_UTS_CONTAINER  // 如果启用UTS容器配置
    ret = OsUnshareUtsContainer(flags, curr, newContainer);  // 创建UTS容器
    if (ret != LOS_OK) {  // 检查创建结果
        return ret;  // 创建失败，返回错误码
    }
#endif
#ifdef LOSCFG_MNT_CONTAINER  // 如果启用挂载容器配置
    ret = OsUnshareMntContainer(flags, curr, newContainer);  // 创建挂载容器
    if (ret != LOS_OK) {  // 检查创建结果
        return ret;  // 创建失败，返回错误码
    }
#endif
#ifdef LOSCFG_IPC_CONTAINER  // 如果启用IPC容器配置
    ret = OsUnshareIpcContainer(flags, curr, newContainer);  // 创建IPC容器
    if (ret != LOS_OK) {  // 检查创建结果
        return ret;  // 创建失败，返回错误码
    }
#endif
#ifdef LOSCFG_TIME_CONTAINER  // 如果启用时间容器配置
    ret = OsUnshareTimeContainer(flags, curr, newContainer);  // 创建时间容器
    if (ret != LOS_OK) {  // 检查创建结果
        return ret;  // 创建失败，返回错误码
    }
#endif
#ifdef LOSCFG_NET_CONTAINER  // 如果启用网络容器配置
    ret = OsUnshareNetContainer(flags, curr, newContainer);  // 创建网络容器
    if (ret != LOS_OK) {  // 检查创建结果
        return ret;  // 创建失败，返回错误码
    }
#endif
    return ret;  // 所有容器创建成功，返回成功
}

INT32 OsUnshare(UINT32 flags)
{
    UINT32 ret;  // 函数返回值
    UINT32 intSave;  // 中断保存标志
    LosProcessCB *curr = OsCurrProcessGet();  // 获取当前进程控制块
    Container *oldContainer = curr->container;  // 保存旧容器指针
    UINT32 unshareFlags = GetContainerFlagsMask();  // 获取支持的容器标志位掩码
    if (!(flags & unshareFlags) || ((flags & (~unshareFlags)) != 0)) {  // 检查标志位有效性
        return -EINVAL;  // 无效标志位，返回参数错误
    }

#ifdef LOSCFG_USER_CONTAINER  // 如果启用用户容器配置
    ret = OsUnshareUserCredentials(flags, curr);  // 分离用户凭证
    if (ret != LOS_OK) {  // 检查分离结果
        return ret;  // 分离失败，返回错误码
    }
    if (flags == CLONE_NEWUSER) {  // 如果仅分离用户命名空间
        return LOS_OK;  // 直接返回成功
    }
#endif

    Container *newContainer = CreateContainer();  // 创建新容器
    if (newContainer == NULL) {  // 检查容器创建结果
        return -ENOMEM;  // 内存不足，返回错误
    }

    ret = UnshareCreateNewContainers(flags, curr, newContainer);  // 根据标志位创建具体容器
    if (ret != LOS_OK) {  // 检查容器创建结果
        goto EXIT;  // 创建失败，跳转到退出处理
    }

    SCHEDULER_LOCK(intSave);  // 关闭调度器，保护临界区
    oldContainer = curr->container;  // 重新获取旧容器指针
    curr->container = newContainer;  // 更新当前进程容器为新容器
    SCHEDULER_UNLOCK(intSave);  // 开启调度器

    DeInitContainers(flags, oldContainer, NULL);  // 销毁旧容器
    return LOS_OK;  // 返回成功

EXIT:  // 错误处理标签
    DeInitContainers(flags, newContainer, NULL);  // 销毁已创建的新容器
    return -ret;  // 返回错误码
}

STATIC UINT32 SetNsGetFlagByContainerType(UINT32 containerType)
{
    if (containerType >= (UINT32)CONTAINER_MAX) {  // 检查容器类型有效性
        return 0;  // 无效类型，返回0
    }
    ContainerType type = (ContainerType)containerType;  // 转换为容器类型枚举
    switch (type) {  // 根据容器类型选择标志位
        case PID_CONTAINER:  // PID容器类型
        case PID_CHILD_CONTAINER:  // PID子容器类型
            return CLONE_NEWPID;  // 返回PID命名空间标志位
        case USER_CONTAINER:  // 用户容器类型
            return CLONE_NEWUSER;  // 返回用户命名空间标志位
        case UTS_CONTAINER:  // UTS容器类型
            return CLONE_NEWUTS;  // 返回UTS命名空间标志位
        case MNT_CONTAINER:  // 挂载容器类型
            return CLONE_NEWNS;  // 返回挂载命名空间标志位
        case IPC_CONTAINER:  // IPC容器类型
            return CLONE_NEWIPC;  // 返回IPC命名空间标志位
        case TIME_CONTAINER:  // 时间容器类型
        case TIME_CHILD_CONTAINER:  // 时间子容器类型
            return CLONE_NEWTIME;  // 返回时间命名空间标志位
        case NET_CONTAINER:  // 网络容器类型
            return CLONE_NEWNET;  // 返回网络命名空间标志位
        default:  // 默认情况
            break;  // 不处理
    }
    return 0;  // 未知类型，返回0
}

STATIC UINT32 SetNsCreateNewContainers(UINT32 flags, Container *newContainer, Container *container)
{
    UINT32 ret = LOS_OK;  // 初始化返回值为成功
#ifdef LOSCFG_PID_CONTAINER  // 如果启用PID容器配置
    ret = OsSetNsPidContainer(flags, container, newContainer);  // 设置PID容器
    if (ret != LOS_OK) {  // 检查设置结果
        return ret;  // 设置失败，返回错误码
    }
#endif
#ifdef LOSCFG_UTS_CONTAINER  // 如果启用UTS容器配置
    ret = OsSetNsUtsContainer(flags, container, newContainer);  // 设置UTS容器
    if (ret != LOS_OK) {  // 检查设置结果
        return ret;  // 设置失败，返回错误码
    }
#endif
#ifdef LOSCFG_MNT_CONTAINER  // 如果启用挂载容器配置
    ret = OsSetNsMntContainer(flags, container, newContainer);  // 设置挂载容器
    if (ret != LOS_OK) {  // 检查设置结果
        return ret;  // 设置失败，返回错误码
    }
#endif
#ifdef LOSCFG_IPC_CONTAINER  // 如果启用IPC容器配置
    ret = OsSetNsIpcContainer(flags, container, newContainer);  // 设置IPC容器
    if (ret != LOS_OK) {  // 检查设置结果
        return ret;  // 设置失败，返回错误码
    }
#endif
#ifdef LOSCFG_TIME_CONTAINER  // 如果启用时间容器配置
    ret = OsSetNsTimeContainer(flags, container, newContainer);  // 设置时间容器
    if (ret != LOS_OK) {  // 检查设置结果
        return ret;  // 设置失败，返回错误码
    }
#endif
#ifdef LOSCFG_NET_CONTAINER  // 如果启用网络容器配置
    ret = OsSetNsNetContainer(flags, container, newContainer);  // 设置网络容器
    if (ret != LOS_OK) {  // 检查设置结果
        return ret;  // 设置失败，返回错误码
    }
#endif
    return LOS_OK;
}

STATIC UINT32 SetNsParamCheck(INT32 fd, INT32 type, UINT32 *flag, LosProcessCB **target)
{
    UINT32 typeMask = GetContainerFlagsMask();  // 获取容器类型掩码
    *flag = (UINT32)(type & typeMask);  // 计算有效的标志位
    UINT32 containerType = 0;  // 容器类型变量

    if (((type & (~typeMask)) != 0)) {  // 检查类型是否有效
        return EINVAL;  // 无效类型，返回参数错误
    }

    LosProcessCB *processCB = (LosProcessCB *)ProcfsContainerGet(fd, &containerType);  // 通过文件描述符获取进程控制块
    if (processCB == NULL) {  // 检查进程控制块有效性
        return EBADF;  // 无效文件描述符，返回错误
    }

    if (*flag == 0) {  // 如果标志位未设置
        *flag = SetNsGetFlagByContainerType(containerType);  // 根据容器类型获取标志位
    }

    if ((*flag == 0) || (*flag != SetNsGetFlagByContainerType(containerType))) {  // 检查标志位有效性
        return EINVAL;  // 无效标志位，返回参数错误
    }

    if (processCB == OsCurrProcessGet()) {  // 检查是否为当前进程
        return EINVAL;  // 不能操作当前进程，返回错误
    }
    *target = processCB;  // 设置目标进程控制块
    return LOS_OK;  // 参数验证成功，返回成功
}

INT32 OsSetNs(INT32 fd, INT32 type)
{
    UINT32 intSave, ret;  // 中断保存标志和返回值
    UINT32 flag = 0;  // 容器标志位
    LosProcessCB *curr = OsCurrProcessGet();  // 获取当前进程控制块
    LosProcessCB *processCB = NULL;  // 目标进程控制块指针

    ret = SetNsParamCheck(fd, type, &flag, &processCB);  // 验证输入参数
    if (ret != LOS_OK) {  // 检查参数验证结果
        return -ret;  // 参数无效，返回错误码
    }

#ifdef LOSCFG_USER_CONTAINER  // 如果启用用户容器配置
    if (flag == CLONE_NEWUSER) {  // 如果是用户命名空间
        SCHEDULER_LOCK(intSave);  // 关闭调度器，保护临界区
        if ((processCB->credentials == NULL) || (processCB->credentials->userContainer == NULL)) {  // 检查用户凭证有效性
            SCHEDULER_UNLOCK(intSave);  // 开启调度器
            return -EBADF;  // 无效用户容器，返回错误
        }
        UserContainer *userContainer = processCB->credentials->userContainer;  // 获取用户容器
        ret = OsSetNsUserContainer(userContainer, curr);  // 设置用户容器
        SCHEDULER_UNLOCK(intSave);  // 开启调度器
        return ret;  // 返回设置结果
    }
#endif

    Container *newContainer = CreateContainer();  // 创建新容器
    if (newContainer == NULL) {  // 检查容器创建结果
        return -ENOMEM;  // 内存不足，返回错误
    }

    SCHEDULER_LOCK(intSave);  // 关闭调度器，保护临界区
    Container *targetContainer = processCB->container;  // 获取目标容器
    if (targetContainer == NULL) {  // 检查目标容器有效性
        SCHEDULER_UNLOCK(intSave);  // 开启调度器
        return -EBADF;  // 无效目标容器，返回错误
    }

    ret = SetNsCreateNewContainers(flag, newContainer, targetContainer);  // 创建新容器内容
    if (ret != LOS_OK) {  // 检查容器创建结果
        SCHEDULER_UNLOCK(intSave);  // 开启调度器
        goto EXIT;  // 创建失败，跳转到退出处理
    }

    Container *oldContainer = curr->container;  // 保存旧容器指针
    curr->container = newContainer;  // 更新当前进程容器为新容器
    SCHEDULER_UNLOCK(intSave);  // 开启调度器
    DeInitContainers(flag, oldContainer, NULL);  // 销毁旧容器
    return LOS_OK;  // 返回成功

EXIT:  // 错误处理标签
    DeInitContainers(flag, newContainer, curr);  // 销毁已创建的新容器
    return ret;  // 返回错误码
}
#endif
