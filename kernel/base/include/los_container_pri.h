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

typedef enum {
    CONTAINER = 0,
    PID_CONTAINER,
    PID_CHILD_CONTAINER,
    UTS_CONTAINER,
    MNT_CONTAINER,
    IPC_CONTAINER,
    USER_CONTAINER,
    TIME_CONTAINER,
    TIME_CHILD_CONTAINER,
    NET_CONTAINER,
    CONTAINER_MAX,
} ContainerType;

typedef struct Container {
    Atomic   rc;
#ifdef LOSCFG_PID_CONTAINER
    struct PidContainer *pidContainer;
    struct PidContainer *pidForChildContainer;
#endif
#ifdef LOSCFG_UTS_CONTAINER
    struct UtsContainer *utsContainer;
#endif
#ifdef LOSCFG_MNT_CONTAINER
    struct MntContainer *mntContainer;
#endif
#ifdef LOSCFG_IPC_CONTAINER
    struct IpcContainer *ipcContainer;
#endif
#ifdef LOSCFG_TIME_CONTAINER
    struct TimeContainer *timeContainer;
    struct TimeContainer *timeForChildContainer;
#endif
#ifdef LOSCFG_NET_CONTAINER
    struct NetContainer *netContainer;
#endif
} Container;

typedef struct TagContainerLimit {
#ifdef LOSCFG_PID_CONTAINER
    UINT32 pidLimit;
#endif
#ifdef LOSCFG_UTS_CONTAINER
    UINT32 utsLimit;
#endif
#ifdef LOSCFG_MNT_CONTAINER
    UINT32 mntLimit;
#endif
#ifdef LOSCFG_IPC_CONTAINER
    UINT32 ipcLimit;
#endif
#ifdef LOSCFG_TIME_CONTAINER
    UINT32 timeLimit;
#endif
#ifdef LOSCFG_USER_CONTAINER
    UINT32 userLimit;
#endif
#ifdef LOSCFG_NET_CONTAINER
    UINT32 netLimit;
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
