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

#ifndef _LOS_PID_CONTAINER_PRI_H
#define _LOS_PID_CONTAINER_PRI_H
#include "los_config.h"
#include "los_atomic.h"
#include "los_list.h"

typedef struct TagTaskCB LosTaskCB;
typedef struct ProcessCB LosProcessCB;
struct ProcessGroup;
struct Container;

/**
 * @brief 进程虚拟ID信息结构体
 * @details 用于存储进程在容器内的虚拟ID映射关系及控制块信息
 */
typedef struct {
    UINT32            vid;  /* 虚拟ID，容器内唯一标识进程/线程的ID */
    UINT32            vpid; /* 虚拟父ID，标识容器内父进程的虚拟ID */
    UINTPTR           cb;   /* 控制块指针，指向实际进程/线程控制块 */
    LosProcessCB      *realParent; /* 实际父进程控制块指针，指向真实系统中的父进程 */
    LOS_DL_LIST       node; /* 双向链表节点，用于将ProcessVid结构体链接到容器管理链表 */
} ProcessVid;

/**
 * @brief PID容器层级限制宏
 * @details 定义PID容器的最大嵌套层级为3级
 */
#define PID_CONTAINER_LEVEL_LIMIT 3

/**
 * @brief PID容器结构体
 * @details 管理进程ID命名空间，实现进程ID的隔离与虚拟化
 */
typedef struct PidContainer {
    Atomic              rc;                  /* 原子操作的引用计数器，用于容器实例的生命周期管理 */
    Atomic              level;               /* 容器层级计数器，记录当前容器在嵌套结构中的层级 */
    Atomic              lock;                /* 原子锁，用于容器操作的线程安全保护 */
    BOOL                referenced;          /* 容器引用标记，标识当前容器是否被引用 */
    UINT32              containerID;         /* 容器唯一标识符，用于区分不同的PID容器实例 */
    struct PidContainer *parent;             /* 父容器指针，指向当前容器的父PID容器 */
    struct ProcessGroup *rootPGroup;         /* 根进程组指针，指向容器内的根进程组 */
    LOS_DL_LIST         tidFreeList;         /* 线程ID空闲链表，管理可用的虚拟线程ID */
    ProcessVid          tidArray[LOSCFG_BASE_CORE_TSK_LIMIT]; /* 线程ID数组，存储所有线程的虚拟ID信息，大小由最大任务数配置决定 */
    LOS_DL_LIST         pidFreeList;         /* 进程ID空闲链表，管理可用的虚拟进程ID */
    ProcessVid          pidArray[LOSCFG_BASE_CORE_PROCESS_LIMIT]; /* 进程ID数组，存储所有进程的虚拟ID信息，大小由最大进程数配置决定 */
} PidContainer;

/**
 * @brief 从进程控制块获取PID容器指针
 * @param processCB 进程控制块指针
 * @return PidContainer* 返回进程所属的PID容器指针
 */
#define OS_PID_CONTAINER_FROM_PCB(processCB) ((processCB)->container->pidContainer)

/**
 * @brief 获取进程所在容器的根进程组
 * @param processCB 进程控制块指针
 * @return struct ProcessGroup* 返回容器的根进程组指针
 */
#define OS_ROOT_PGRP(processCB) (OS_PID_CONTAINER_FROM_PCB(processCB)->rootPGroup)

/**
 * @brief 检查两个进程是否属于不同的PID容器
 * @param processCB 待检查的进程控制块指针
 * @param currProcessCB 当前进程控制块指针
 * @return BOOL 不同容器返回TRUE，同一容器返回FALSE
 */
#define OS_PROCESS_CONTAINER_CHECK(processCB, currProcessCB) \
    ((processCB)->container->pidContainer != (currProcessCB)->container->pidContainer)

/**
 * @brief 检查进程是否需要为子容器分配PID
 * @param processCB 进程控制块指针
 * @return BOOL 需要分配返回TRUE，否则返回FALSE
 * @details 当进程所属容器与子容器PID不同且子容器未被引用时返回TRUE
 */
#define OS_PROCESS_PID_FOR_CONTAINER_CHECK(processCB) \
    (((processCB)->container->pidContainer != (processCB)->container->pidForChildContainer) && \
     ((processCB)->container->pidForChildContainer->referenced == FALSE))

UINT32 OsAllocSpecifiedVpidUnsafe(UINT32 vpid, PidContainer *pidContainer,
                                  LosProcessCB *processCB, LosProcessCB *parent);

VOID OsPidContainerDestroyAllProcess(LosProcessCB *processCB);

VOID OsPidContainerDestroy(struct Container *container, LosProcessCB *processCB);

UINT32 OsCopyPidContainer(UINTPTR flags, LosProcessCB *child, LosProcessCB *parent, UINT32 *processID);

UINT32 OsUnsharePidContainer(UINTPTR flags, LosProcessCB *curr, struct Container *newContainer);

UINT32 OsSetNsPidContainer(UINT32 flags, struct Container *container, struct Container *newContainer);

UINT32 OsInitRootPidContainer(PidContainer **pidContainer);

LosProcessCB *OsGetPCBFromVpid(UINT32 vpid);

LosTaskCB *OsGetTCBFromVtid(UINT32 vtid);

UINT32 OsGetVpidFromCurrContainer(const LosProcessCB *processCB);

UINT32 OsGetVpidFromRootContainer(const LosProcessCB *processCB);

UINT32 OsGetVtidFromCurrContainer(const LosTaskCB *taskCB);

VOID OsFreeVtid(LosTaskCB *taskCB);

UINT32 OsAllocVtid(LosTaskCB *taskCB, const LosProcessCB *processCB);

UINT32 OsGetPidContainerID(PidContainer *pidContainer);

BOOL OsPidContainerProcessParentIsRealParent(const LosProcessCB *processCB, const LosProcessCB *curr);

UINT32 OsGetPidContainerCount(VOID);
#endif /* _LOS_PID_CONTAINER_PRI_H */
