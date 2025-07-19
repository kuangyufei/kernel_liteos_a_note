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

#ifndef _LOS_UTS_CONTAINER_PRI_H
#define _LOS_UTS_CONTAINER_PRI_H

#include "sys/utsname.h"
#include "sched.h"
#include "los_atomic.h"

#ifdef LOSCFG_UTS_CONTAINER
/**
 * @ingroup los_uts_container
 * @brief 进程控制块结构体前向声明
 * @details 用于表示系统中的一个进程，包含进程的所有控制信息
 */
typedef struct ProcessCB LosProcessCB;

/**
 * @ingroup los_uts_container
 * @brief 容器结构体前向声明
 * @details 通用容器结构体，可包含多种类型的命名空间容器
 */
struct Container;

/**
 * @ingroup los_uts_container
 * @brief UTS命名空间容器结构体
 * @details 存储UTS(UNIX Time-Sharing)命名空间相关信息，实现主机名和域名的隔离
 * https://unix.stackexchange.com/questions/183717/whats-a-uts-namespace
 * @note UTS命名空间是Linux容器技术的基础隔离机制之一，用于隔离系统标识信息
 */
typedef struct UtsContainer {
    Atomic  rc;             /* 引用计数 - 原子操作确保多线程安全的容器引用管理 */
    UINT32  containerID;    /* 容器ID - 全局唯一的UTS容器标识符 */
    struct  utsname utsName;/* UTS名称结构 - 存储主机名、域名等系统标识信息 */
} UtsContainer;  /* UTS容器核心结构体 */

/**
 * @ingroup los_uts_container
 * @brief 初始化根UTS容器
 * @param[out] utsContainer 根容器指针的指针 - 用于返回创建的根UTS容器
 * @retval LOS_OK 成功
 * @retval LOS_ENOMEM 内存分配失败
 * @retval 其他错误码 具体错误原因见错误码定义
 * @details 创建并初始化系统第一个UTS容器(根容器)，设置默认系统标识信息
 * @note 根容器是所有其他UTS容器的祖先，其ID固定为0
 */
UINT32 OsInitRootUtsContainer(UtsContainer **utsContainer);

/**
 * @ingroup los_uts_container
 * @brief 复制UTS容器
 * @param[in] flags 复制标志 - 控制复制行为的标志位(未使用，预留)
 * @param[in,out] child 子进程控制块 - 需要关联新UTS容器的子进程
 * @param[in] parent 父进程控制块 - 提供源UTS容器的父进程
 * @retval LOS_OK 成功
 * @retval LOS_ENOMEM 内存分配失败
 * @details 为子进程创建新的UTS容器，复制父进程容器的UTS信息
 * @note 实现了fork系统调用中UTS命名空间的Copy-On-Write语义
 */
UINT32 OsCopyUtsContainer(UINTPTR flags, LosProcessCB *child, LosProcessCB *parent);

/**
 * @ingroup los_uts_container
 * @brief 取消共享UTS容器
 * @param[in] flags 取消共享标志 - 必须包含CLONE_NEWUTS标志
 * @param[in] curr 当前进程控制块 - 发起unshare操作的进程
 * @param[in,out] newContainer 新容器指针 - 存储创建的新UTS容器
 * @retval LOS_OK 成功
 * @retval LOS_EINVAL 参数无效
 * @retval LOS_ENOMEM 内存分配失败
 * @details 创建新的UTS容器并与当前进程关联，实现与父进程UTS命名空间的分离
 * @note 对应Linux的unshare(CLONE_NEWUTS)系统调用功能
 */
UINT32 OsUnshareUtsContainer(UINTPTR flags, LosProcessCB *curr, struct Container *newContainer);

/**
 * @ingroup los_uts_container
 * @brief 设置命名空间UTS容器
 * @param[in] flags 操作标志 - 预留参数，当前未使用
 * @param[in] container 目标容器指针 - 当前进程所在的容器
 * @param[in] newContainer 新容器指针 - 要切换到的UTS容器
 * @retval LOS_OK 成功
 * @retval LOS_EPERM 权限不足
 * @details 将当前进程切换到指定的UTS容器，实现命名空间切换
 * @note 对应Linux的setns系统调用功能，需要CAP_SYS_ADMIN权限
 */
UINT32 OsSetNsUtsContainer(UINT32 flags, struct Container *container, struct Container *newContainer);

/**
 * @ingroup los_uts_container
 * @brief 销毁UTS容器
 * @param[in] container 容器指针 - 包含要销毁UTS容器的通用容器
 * @details 递减容器引用计数，当计数为0时释放容器内存及关联资源
 * @note 线程安全的容器清理函数，仅在最后一个引用者释放时才真正销毁
 */
VOID OsUtsContainerDestroy(struct Container *container);

/**
 * @ingroup los_uts_container
 * @brief 获取当前进程的UTS名称结构
 * @return struct utsname* UTS名称结构指针 - 当前进程所在UTS容器的标识信息
 * @details 通过当前进程控制块找到关联的UTS容器，返回其utsName成员
 * @note 用于uname()系统调用实现，提供当前命名空间的系统标识
 */
struct utsname *OsGetCurrUtsName(VOID);

/**
 * @ingroup los_uts_container
 * @brief 获取UTS容器ID
 * @param[in] utsContainer UTS容器指针 - 目标UTS容器
 * @return UINT32 容器ID - 若参数为NULL返回0(根容器ID)
 * @details 安全地获取容器ID，提供空指针保护
 */
UINT32 OsGetUtsContainerID(UtsContainer *utsContainer);

/**
 * @ingroup los_uts_container
 * @brief 获取UTS容器总数
 * @return UINT32 容器总数 - 当前系统中活跃的UTS容器数量
 * @details 遍历容器链表统计所有引用计数大于0的UTS容器
 * @note 用于系统监控和资源管理，返回值包含根容器
 */
UINT32 OsGetUtsContainerCount(VOID);
#endif
#endif /* _LOS_UTS_CONTAINER_PRI_H */
