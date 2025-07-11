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
#ifndef _LOS_IPC_CONTAINER_PRI_H
#define _LOS_IPC_CONTAINER_PRI_H

#include "los_atomic.h"
#include "los_list.h"
#include "mqueue.h"
#include "fs/file.h"

#ifdef LOSCFG_IPC_CONTAINER
struct shmIDSource;
struct Container;
typedef struct TagQueueCB LosQueueCB;
typedef struct OsMux LosMux;
typedef LosMux pthread_mutex_t;
typedef struct ProcessCB LosProcessCB;

/**
 * @brief IPC容器结构体，用于管理进程间通信资源（消息队列和共享内存）
 * @note 每个进程容器对应一个IPC容器，负责维护该容器内的所有IPC对象生命周期
 */
typedef struct IpcContainer {
    Atomic rc;                          /* 引用计数，原子操作确保并发安全 */
    LosQueueCB *allQueue;               /* 指向所有队列控制块的指针 */
    LOS_DL_LIST freeQueueList;          /* 空闲队列链表，管理可分配的消息队列 */
    fd_set queueFdSet;                  /* 文件描述符集合，跟踪已使用的队列FD */
    struct mqarray queueTable[LOSCFG_BASE_IPC_QUEUE_LIMIT]; /* 队列表，大小由最大队列数配置项限制 */
    pthread_mutex_t mqueueMutex;        /* 消息队列互斥锁，保护队列操作的线程安全 */
    struct mqpersonal *mqPrivBuf[MAX_MQ_FD]; /* 消息队列私有数据缓冲区，大小由最大FD数限制 */
    struct shminfo shmInfo;             /* 共享内存信息结构体，记录共享内存使用情况 */
    LosMux sysvShmMux;                  /* System V共享内存互斥锁，保护共享内存操作 */
    struct shmIDSource *shmSegs;        /* 共享内存段数组，管理所有活动的共享内存段 */
    UINT32 shmUsedPageCount;            /* 共享内存已使用页面数，单位：页 */
    UINT32 containerID;                 /* 容器ID，唯一标识一个IPC容器 */
} IpcContainer;

UINT32 OsInitRootIpcContainer(IpcContainer **ipcContainer);

UINT32 OsCopyIpcContainer(UINTPTR flags, LosProcessCB *child, LosProcessCB *parent);

UINT32 OsUnshareIpcContainer(UINTPTR flags, LosProcessCB *curr, struct Container *newContainer);

UINT32 OsSetNsIpcContainer(UINT32 flags, struct Container *container, struct Container *newContainer);

VOID OsIpcContainerDestroy(struct Container *container);

UINT32 OsGetIpcContainerID(IpcContainer *ipcContainer);

IpcContainer *OsGetCurrIpcContainer(VOID);

UINT32 OsGetIpcContainerCount(VOID);

#define IPC_ALL_QUEUE (OsGetCurrIpcContainer()->allQueue)

#define FREE_QUEUE_LIST (OsGetCurrIpcContainer()->freeQueueList)

#define IPC_QUEUE_FD_SET (OsGetCurrIpcContainer()->queueFdSet)

#define IPC_QUEUE_TABLE (OsGetCurrIpcContainer()->queueTable)

#define IPC_QUEUE_MUTEX (OsGetCurrIpcContainer()->mqueueMutex)

#define IPC_QUEUE_MQ_PRIV_BUF (OsGetCurrIpcContainer()->mqPrivBuf)

#define IPC_SHM_INFO (OsGetCurrIpcContainer()->shmInfo)

#define IPC_SHM_SYS_VSHM_MUTEX (OsGetCurrIpcContainer()->sysvShmMux)

#define IPC_SHM_SEGS (OsGetCurrIpcContainer()->shmSegs)

#define IPC_SHM_USED_PAGE_COUNT (OsGetCurrIpcContainer()->shmUsedPageCount)

#endif
#endif /* _LOS_IPC_CONTAINER_PRI_H */
