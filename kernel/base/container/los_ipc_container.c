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
#ifdef LOSCFG_IPC_CONTAINER
#include "los_ipc_container_pri.h"
#include "los_config.h"
#include "los_queue_pri.h"
#include "los_vm_shm_pri.h"
#include "los_process_pri.h"
#include "vnode.h"
#include "proc_fs.h"
#include "pthread.h"

#define IPC_INIT_NUM 3

STATIC UINT32 g_currentIpcContainerNum = 0;

STATIC IpcContainer *CreateNewIpcContainer(IpcContainer *parent)
{
    pthread_mutexattr_t attr;
    UINT32 size = sizeof(IpcContainer);
    IpcContainer *ipcContainer = (IpcContainer *)LOS_MemAlloc(m_aucSysMem1, size);
    if (ipcContainer == NULL) {
        return NULL;
    }
    (VOID)memset_s(ipcContainer, size, 0, size);

    ipcContainer->allQueue = OsAllQueueCBInit(&ipcContainer->freeQueueList);
    if (ipcContainer->allQueue == NULL) {
        (VOID)LOS_MemFree(m_aucSysMem1, ipcContainer);
        return NULL;
    }
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE_NP);
    pthread_mutex_init(&ipcContainer->mqueueMutex, &attr);

    ipcContainer->shmSegs = OsShmCBInit(&ipcContainer->sysvShmMux, &ipcContainer->shmInfo,
                                        &ipcContainer->shmUsedPageCount);
    if (ipcContainer->shmSegs == NULL) {
        (VOID)LOS_MemFree(m_aucSysMem1, ipcContainer->allQueue);
        (VOID)LOS_MemFree(m_aucSysMem1, ipcContainer);
        return NULL;
    }
    ipcContainer->containerID = OsAllocContainerID();

    if (parent != NULL) {
        LOS_AtomicSet(&ipcContainer->rc, 1);
    } else {
        LOS_AtomicSet(&ipcContainer->rc, 3); /* 3: Three system processes */
    }
    return ipcContainer;
}

STATIC UINT32 CreateIpcContainer(LosProcessCB *child, LosProcessCB *parent)
{
    UINT32 intSave;
    IpcContainer *parentContainer = parent->container->ipcContainer;
    IpcContainer *newIpcContainer = CreateNewIpcContainer(parentContainer);
    if (newIpcContainer == NULL) {
        return ENOMEM;
    }

    SCHEDULER_LOCK(intSave);
    g_currentIpcContainerNum++;
    child->container->ipcContainer = newIpcContainer;
    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;
}

UINT32 OsInitRootIpcContainer(IpcContainer **ipcContainer)
{
    UINT32 intSave;
    IpcContainer *newIpcContainer = CreateNewIpcContainer(NULL);
    if (newIpcContainer == NULL) {
        return ENOMEM;
    }

    SCHEDULER_LOCK(intSave);
    g_currentIpcContainerNum++;
    *ipcContainer = newIpcContainer;
    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;
}

UINT32 OsCopyIpcContainer(UINTPTR flags, LosProcessCB *child, LosProcessCB *parent)
{
    UINT32 intSave;
    IpcContainer *currIpcContainer = parent->container->ipcContainer;

    if (!(flags & CLONE_NEWIPC)) {
        SCHEDULER_LOCK(intSave);
        LOS_AtomicInc(&currIpcContainer->rc);
        child->container->ipcContainer = currIpcContainer;
        SCHEDULER_UNLOCK(intSave);
        return LOS_OK;
    }

    if (OsContainerLimitCheck(IPC_CONTAINER, &g_currentIpcContainerNum) != LOS_OK) {
        return EPERM;
    }

    return CreateIpcContainer(child, parent);
}

UINT32 OsUnshareIpcContainer(UINTPTR flags, LosProcessCB *curr, Container *newContainer)
{
    UINT32 intSave;
    IpcContainer *parentContainer = curr->container->ipcContainer;

    if (!(flags & CLONE_NEWIPC)) {
        SCHEDULER_LOCK(intSave);
        newContainer->ipcContainer = parentContainer;
        LOS_AtomicInc(&parentContainer->rc);
        SCHEDULER_UNLOCK(intSave);
        return LOS_OK;
    }

    if (OsContainerLimitCheck(IPC_CONTAINER, &g_currentIpcContainerNum) != LOS_OK) {
        return EPERM;
    }

    IpcContainer *ipcContainer = CreateNewIpcContainer(parentContainer);
    if (ipcContainer == NULL) {
        return ENOMEM;
    }

    SCHEDULER_LOCK(intSave);
    newContainer->ipcContainer = ipcContainer;
    g_currentIpcContainerNum++;
    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;
}

UINT32 OsSetNsIpcContainer(UINT32 flags, Container *container, Container *newContainer)
{
    if (flags & CLONE_NEWIPC) {
        newContainer->ipcContainer = container->ipcContainer;
        LOS_AtomicInc(&container->ipcContainer->rc);
        return LOS_OK;
    }

    newContainer->ipcContainer = OsCurrProcessGet()->container->ipcContainer;
    LOS_AtomicInc(&newContainer->ipcContainer->rc);
    return LOS_OK;
}

VOID OsIpcContainerDestroy(Container *container)
{
    UINT32 intSave;
    if (container == NULL) {
        return;
    }

    SCHEDULER_LOCK(intSave);
    IpcContainer *ipcContainer = container->ipcContainer;
    if (ipcContainer == NULL) {
        SCHEDULER_UNLOCK(intSave);
        return;
    }

    LOS_AtomicDec(&ipcContainer->rc);
    if (LOS_AtomicRead(&ipcContainer->rc) > 0) {
        SCHEDULER_UNLOCK(intSave);
        return;
    }

    g_currentIpcContainerNum--;
    container->ipcContainer = NULL;
    SCHEDULER_UNLOCK(intSave);
    OsShmCBDestroy(ipcContainer->shmSegs, &ipcContainer->shmInfo, &ipcContainer->sysvShmMux);
    ipcContainer->shmSegs = NULL;
    OsMqueueCBDestroy(ipcContainer->queueTable);
    (VOID)LOS_MemFree(m_aucSysMem1, ipcContainer->allQueue);
    (VOID)LOS_MemFree(m_aucSysMem1, ipcContainer);
    return;
}

UINT32 OsGetIpcContainerID(IpcContainer *ipcContainer)
{
    if (ipcContainer == NULL) {
        return OS_INVALID_VALUE;
    }

    return ipcContainer->containerID;
}

IpcContainer *OsGetCurrIpcContainer(VOID)
{
    return OsCurrProcessGet()->container->ipcContainer;
}

UINT32 OsGetIpcContainerCount(VOID)
{
    return g_currentIpcContainerNum;
}
#endif
