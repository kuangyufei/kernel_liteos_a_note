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

#ifndef _LOS_CONTAINER_PRI_H
#define _LOS_CONTAINER_PRI_H

#include "los_atomic.h"
#ifdef LOSCFG_KERNEL_CONTAINER
#ifdef LOSCFG_PID_CONTAINER
#include "los_pid_container_pri.h"
#endif
#ifdef LOSCFG_UTS_CONTAINER
#include "los_uts_container_pri.h"
#endif
#ifdef LOSCFG_MNT_CONTAINER
#include "los_mnt_container_pri.h"
#endif
#ifdef LOSCFG_IPC_CONTAINER
#include "los_ipc_container_pri.h"
#endif
#ifdef LOSCFG_USER_CONTAINER
#include "los_user_container_pri.h"
#endif
#ifdef LOSCFG_TIME_CONTAINER
#include "los_time_container_pri.h"
#endif
#ifdef LOSCFG_NET_CONTAINER
#include "los_net_container_pri.h"
#endif

/**
 * @brief 容器类型枚举定义
 * @details 定义了系统支持的各类容器类型，用于容器管理和标识
 */
typedef enum {
    CONTAINER = 0,                  ///< 基础容器类型，作为其他容器的基类
    PID_CONTAINER,                  ///< 进程ID容器，管理进程ID命名空间
    PID_CHILD_CONTAINER,            ///< 子进程容器，管理子进程的命名空间
    UTS_CONTAINER,                  ///< UTS容器，管理主机名和域名等系统标识信息
    MNT_CONTAINER,                  ///< 挂载容器，管理文件系统挂载点
    IPC_CONTAINER,                  ///< IPC容器，管理进程间通信资源
    USER_CONTAINER,                 ///< 用户容器，管理用户和用户组ID映射
    TIME_CONTAINER,                 ///< 时间容器，管理系统时间相关资源
    TIME_CHILD_CONTAINER,           ///< 子时间容器，管理子进程的时间资源
    NET_CONTAINER,                  ///< 网络容器，管理网络命名空间和网络资源
    CONTAINER_MAX,                  ///< 容器类型总数，用于数组边界检查和容器类型有效性验证
} ContainerType;

/**
 * @brief 容器结构体定义
 * @details 系统容器的基础结构，包含各类具体容器的指针和引用计数
 */
typedef struct Container {
    Atomic   rc;                    ///< 原子引用计数，用于容器的生命周期管理
#ifdef LOSCFG_PID_CONTAINER
    struct PidContainer *pidContainer; ///< 指向进程ID容器的指针，仅在使能PID容器配置时有效
    struct PidContainer *pidForChildContainer; ///< 指向子进程ID容器的指针，用于管理子进程命名空间
#endif
#ifdef LOSCFG_UTS_CONTAINER
    struct UtsContainer *utsContainer; ///< 指向UTS容器的指针，仅在使能UTS容器配置时有效
#endif
#ifdef LOSCFG_MNT_CONTAINER
    struct MntContainer *mntContainer; ///< 指向挂载容器的指针，仅在使能挂载容器配置时有效
#endif
#ifdef LOSCFG_IPC_CONTAINER
    struct IpcContainer *ipcContainer; ///< 指向IPC容器的指针，仅在使能IPC容器配置时有效
#endif
#ifdef LOSCFG_TIME_CONTAINER
    struct TimeContainer *timeContainer; ///< 指向时间容器的指针，仅在使能时间容器配置时有效
    struct TimeContainer *timeForChildContainer; ///< 指向子进程时间容器的指针
#endif
#ifdef LOSCFG_NET_CONTAINER
    struct NetContainer *netContainer; ///< 指向网络容器的指针，仅在使能网络容器配置时有效
#endif
} Container;

/**
 * @brief 容器数量限制结构体
 * @details 定义各类容器的数量上限，用于资源控制和系统稳定性保障
 */
typedef struct TagContainerLimit {
#ifdef LOSCFG_PID_CONTAINER
    UINT32 pidLimit;                ///< PID容器的最大数量限制
#endif
#ifdef LOSCFG_UTS_CONTAINER
    UINT32 utsLimit;                ///< UTS容器的最大数量限制
#endif
#ifdef LOSCFG_MNT_CONTAINER
    UINT32 mntLimit;                ///< 挂载容器的最大数量限制
#endif
#ifdef LOSCFG_IPC_CONTAINER
    UINT32 ipcLimit;                ///< IPC容器的最大数量限制
#endif
#ifdef LOSCFG_TIME_CONTAINER
    UINT32 timeLimit;               ///< 时间容器的最大数量限制
#endif
#ifdef LOSCFG_USER_CONTAINER
    UINT32 userLimit;               ///< 用户容器的最大数量限制
#endif
#ifdef LOSCFG_NET_CONTAINER
    UINT32 netLimit;                ///< 网络容器的最大数量限制
#endif
} ContainerLimit;

VOID OsContainerInitSystemProcess(LosProcessCB *processCB);

VOID OsInitRootContainer(VOID);

UINT32 OsCopyContainers(UINTPTR flags, LosProcessCB *child, LosProcessCB *parent, UINT32 *processID);

VOID OsOsContainersDestroyEarly(LosProcessCB *processCB);

VOID OsContainersDestroy(LosProcessCB *processCB);

VOID OsContainerFree(LosProcessCB *processCB);

UINT32 OsAllocContainerID(VOID);

UINT32 OsGetContainerID(LosProcessCB *processCB, ContainerType type);

INT32 OsUnshare(UINT32 flags);

INT32 OsSetNs(INT32 fd, INT32 type);

UINT32 OsGetContainerLimit(ContainerType type);

UINT32 OsContainerLimitCheck(ContainerType type, UINT32 *containerCount);

UINT32 OsSetContainerLimit(ContainerType type, UINT32 value);

UINT32 OsGetContainerCount(ContainerType type);
#endif
#endif /* _LOS_CONTAINER_PRI_H */
