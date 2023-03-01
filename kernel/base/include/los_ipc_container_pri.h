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

typedef struct IpcContainer {
    Atomic rc;
    LosQueueCB *allQueue;
    LOS_DL_LIST freeQueueList;
    fd_set queueFdSet;
    struct mqarray queueTable[LOSCFG_BASE_IPC_QUEUE_LIMIT];
    pthread_mutex_t mqueueMutex;
    struct mqpersonal *mqPrivBuf[MAX_MQ_FD];
    struct shminfo shmInfo;
    LosMux sysvShmMux;
    struct shmIDSource *shmSegs;
    UINT32 shmUsedPageCount;
    UINT32 containerID;
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
