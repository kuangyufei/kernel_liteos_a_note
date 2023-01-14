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

typedef struct {
    UINT32            vid;  /* Virtual ID */
    UINT32            vpid; /* Virtual parent ID */
    UINTPTR           cb;   /* Control block */
    LOS_DL_LIST       node;
} ProcessVid;

#define PID_CONTAINER_LEVEL_LIMIT 3

typedef struct PidContainer {
    Atomic              rc;
    Atomic              level;
    Atomic              lock;
    struct PidContainer *parent;
    struct ProcessGroup *rootPGroup;
    LOS_DL_LIST         tidFreeList;
    ProcessVid          tidArray[LOSCFG_BASE_CORE_TSK_LIMIT];
    LOS_DL_LIST         pidFreeList;
    ProcessVid          pidArray[LOSCFG_BASE_CORE_PROCESS_LIMIT];
} PidContainer;

#define OS_PID_CONTAINER_FROM_PCB(processCB) ((processCB)->container->pidContainer)

#define OS_ROOT_PGRP(processCB) (OS_PID_CONTAINER_FROM_PCB(processCB)->rootPGroup)

#define OS_PROCESS_CONTAINER_CHECK(processCB, currProcessCB) \
    ((processCB)->container->pidContainer != (currProcessCB)->container->pidContainer)

UINT32 OsAllocSpecifiedVpidUnsafe(UINT32 vpid, LosProcessCB *processCB, LosProcessCB *parent);

VOID OsPidContainersDestroyAllProcess(LosProcessCB *processCB);

VOID OsPidContainersDestroy(LosProcessCB *curr);

UINT32 OsCopyPidContainer(UINTPTR flags, LosProcessCB *child, LosProcessCB *parent, UINT32 *processID);

UINT32 OsInitRootPidContainer(PidContainer **pidContainer);

LosProcessCB *OsGetPCBFromVpid(UINT32 vpid);

LosTaskCB *OsGetTCBFromVtid(UINT32 vtid);

UINT32 OsGetVpidFromCurrContainer(const LosProcessCB *processCB);

UINT32 OsGetVtidFromCurrContainer(const LosTaskCB *taskCB);

VOID OsFreeVtid(LosTaskCB *taskCB);

UINT32 OsAllocVtid(LosTaskCB *taskCB, const LosProcessCB *processCB);

#endif /* _LOS_PID_CONTAINER_PRI_H */
