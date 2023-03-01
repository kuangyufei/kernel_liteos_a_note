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

STATIC Container g_rootContainer;
STATIC ContainerLimit g_containerLimit;
STATIC Atomic g_containerCount = 0xF0000000U;
#ifdef LOSCFG_USER_CONTAINER
STATIC Credentials *g_rootCredentials = NULL;
#endif

UINT32 OsAllocContainerID(VOID)
{
    return LOS_AtomicIncRet(&g_containerCount);
}

VOID OsContainerInitSystemProcess(LosProcessCB *processCB)
{
    processCB->container = &g_rootContainer;
#ifdef LOSCFG_USER_CONTAINER
    processCB->credentials = g_rootCredentials;
#endif
    LOS_AtomicInc(&processCB->container->rc);
#ifdef LOSCFG_PID_CONTAINER
    (VOID)OsAllocSpecifiedVpidUnsafe(processCB->processID, processCB->container->pidContainer, processCB, NULL);
#endif
    return;
}

UINT32 OsGetContainerLimit(ContainerType type)
{
    switch (type) {
#ifdef LOSCFG_PID_CONTAINER
        case PID_CONTAINER:
        case PID_CHILD_CONTAINER:
            return g_containerLimit.pidLimit;
#endif
#ifdef LOSCFG_USER_CONTAINER
        case USER_CONTAINER:
            return g_containerLimit.userLimit;
#endif
#ifdef LOSCFG_UTS_CONTAINER
        case UTS_CONTAINER:
            return g_containerLimit.utsLimit;
#endif
#ifdef LOSCFG_MNT_CONTAINER
        case MNT_CONTAINER:
            return g_containerLimit.mntLimit;
#endif
#ifdef LOSCFG_IPC_CONTAINER
        case IPC_CONTAINER:
            return g_containerLimit.ipcLimit;
#endif
#ifdef LOSCFG_TIME_CONTAINER
        case TIME_CONTAINER:
        case TIME_CHILD_CONTAINER:
            return g_containerLimit.timeLimit;
#endif
#ifdef LOSCFG_NET_CONTAINER
        case NET_CONTAINER:
            return g_containerLimit.netLimit;
#endif
        default:
            break;
    }
    return OS_INVALID_VALUE;
}

UINT32 OsContainerLimitCheck(ContainerType type, UINT32 *containerCount)
{
    UINT32 intSave;
    SCHEDULER_LOCK(intSave);
    if ((*containerCount) >= OsGetContainerLimit(type)) {
        SCHEDULER_UNLOCK(intSave);
        return EINVAL;
    }
    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;
}

UINT32 OsSetContainerLimit(ContainerType type, UINT32 value)
{
    UINT32 intSave;

    if (value > LOSCFG_KERNEL_CONTAINER_DEFAULT_LIMIT) {
        return EINVAL;
    }

    SCHEDULER_LOCK(intSave);
    switch (type) {
#ifdef LOSCFG_PID_CONTAINER
        case PID_CONTAINER:
        case PID_CHILD_CONTAINER:
            g_containerLimit.pidLimit = value;
            break;
#endif
#ifdef LOSCFG_USER_CONTAINER
        case USER_CONTAINER:
            g_containerLimit.userLimit = value;
            break;
#endif
#ifdef LOSCFG_UTS_CONTAINER
        case UTS_CONTAINER:
            g_containerLimit.utsLimit = value;
            break;
#endif
#ifdef LOSCFG_MNT_CONTAINER
        case MNT_CONTAINER:
            g_containerLimit.mntLimit = value;
            break;
#endif
#ifdef LOSCFG_IPC_CONTAINER
        case IPC_CONTAINER:
            g_containerLimit.ipcLimit = value;
            break;
#endif
#ifdef LOSCFG_TIME_CONTAINER
        case TIME_CONTAINER:
        case TIME_CHILD_CONTAINER:
            g_containerLimit.timeLimit = value;
            break;
#endif
#ifdef LOSCFG_NET_CONTAINER
        case NET_CONTAINER:
            g_containerLimit.netLimit = value;
            break;
#endif
        default:
            SCHEDULER_UNLOCK(intSave);
            return EINVAL;
    }
    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;
}

UINT32 OsGetContainerCount(ContainerType type)
{
    switch (type) {
#ifdef LOSCFG_PID_CONTAINER
        case PID_CONTAINER:
        case PID_CHILD_CONTAINER:
            return OsGetPidContainerCount();
#endif
#ifdef LOSCFG_USER_CONTAINER
        case USER_CONTAINER:
            return OsGetUserContainerCount();
#endif
#ifdef LOSCFG_UTS_CONTAINER
        case UTS_CONTAINER:
            return OsGetUtsContainerCount();
#endif
#ifdef LOSCFG_MNT_CONTAINER
        case MNT_CONTAINER:
            return OsGetMntContainerCount();
#endif
#ifdef LOSCFG_IPC_CONTAINER
        case IPC_CONTAINER:
            return OsGetIpcContainerCount();
#endif
#ifdef LOSCFG_TIME_CONTAINER
        case TIME_CONTAINER:
        case TIME_CHILD_CONTAINER:
            return OsGetTimeContainerCount();
#endif
#ifdef LOSCFG_NET_CONTAINER
        case NET_CONTAINER:
            return OsGetNetContainerCount();
#endif
        default:
            break;
    }
    return OS_INVALID_VALUE;
}

VOID OsInitRootContainer(VOID)
{
#ifdef LOSCFG_USER_CONTAINER
    g_containerLimit.userLimit = LOSCFG_KERNEL_CONTAINER_DEFAULT_LIMIT;
    OsInitRootUserCredentials(&g_rootCredentials);
#endif
#ifdef LOSCFG_PID_CONTAINER
    g_containerLimit.pidLimit = LOSCFG_KERNEL_CONTAINER_DEFAULT_LIMIT;
    (VOID)OsInitRootPidContainer(&g_rootContainer.pidContainer);
    g_rootContainer.pidForChildContainer = g_rootContainer.pidContainer;
#endif
#ifdef LOSCFG_UTS_CONTAINER
    g_containerLimit.utsLimit = LOSCFG_KERNEL_CONTAINER_DEFAULT_LIMIT;
    (VOID)OsInitRootUtsContainer(&g_rootContainer.utsContainer);
#endif
#ifdef LOSCFG_MNT_CONTAINER
    g_containerLimit.mntLimit = LOSCFG_KERNEL_CONTAINER_DEFAULT_LIMIT;
    (VOID)OsInitRootMntContainer(&g_rootContainer.mntContainer);
#endif
#ifdef LOSCFG_IPC_CONTAINER
    g_containerLimit.ipcLimit = LOSCFG_KERNEL_CONTAINER_DEFAULT_LIMIT;
    (VOID)OsInitRootIpcContainer(&g_rootContainer.ipcContainer);
#endif
#ifdef LOSCFG_TIME_CONTAINER
    g_containerLimit.timeLimit = LOSCFG_KERNEL_CONTAINER_DEFAULT_LIMIT;
    (VOID)OsInitRootTimeContainer(&g_rootContainer.timeContainer);
    g_rootContainer.timeForChildContainer = g_rootContainer.timeContainer;
#endif
#ifdef LOSCFG_NET_CONTAINER
    g_containerLimit.netLimit = LOSCFG_KERNEL_CONTAINER_DEFAULT_LIMIT;
    (VOID)OsInitRootNetContainer(&g_rootContainer.netContainer);
#endif
    return;
}

STATIC INLINE Container *CreateContainer(VOID)
{
    Container *container = (Container *)LOS_MemAlloc(m_aucSysMem1, sizeof(Container));
    if (container == NULL) {
        return NULL;
    }

    (VOID)memset_s(container, sizeof(Container), 0, sizeof(Container));

    LOS_AtomicInc(&container->rc);
    return container;
}

STATIC UINT32 CopyContainers(UINTPTR flags, LosProcessCB *child, LosProcessCB *parent, UINT32 *processID)
{
    UINT32 ret = LOS_OK;
    /* Pid container initialization must precede other container initialization. */
#ifdef LOSCFG_PID_CONTAINER
    ret = OsCopyPidContainer(flags, child, parent, processID);
    if (ret != LOS_OK) {
        return ret;
    }
#endif
#ifdef LOSCFG_USER_CONTAINER
    ret = OsCopyCredentials(flags,  child, parent);
    if (ret != LOS_OK) {
        return ret;
    }
#endif
#ifdef LOSCFG_UTS_CONTAINER
    ret = OsCopyUtsContainer(flags, child, parent);
    if (ret != LOS_OK) {
        return ret;
    }
#endif
#ifdef LOSCFG_MNT_CONTAINER
    ret = OsCopyMntContainer(flags, child, parent);
    if (ret != LOS_OK) {
        return ret;
    }
#endif
#ifdef LOSCFG_IPC_CONTAINER
    ret = OsCopyIpcContainer(flags, child, parent);
    if (ret != LOS_OK) {
        return ret;
    }
#endif
#ifdef LOSCFG_TIME_CONTAINER
    ret = OsCopyTimeContainer(flags, child, parent);
    if (ret != LOS_OK) {
        return ret;
    }
#endif
#ifdef LOSCFG_NET_CONTAINER
    ret = OsCopyNetContainer(flags, child, parent);
    if (ret != LOS_OK) {
        return ret;
    }
#endif
    return ret;
}

STATIC INLINE UINT32 GetContainerFlagsMask(VOID)
{
    UINT32 mask = 0;
#ifdef LOSCFG_PID_CONTAINER
    mask |= CLONE_NEWPID;
#endif
#ifdef LOSCFG_MNT_CONTAINER
    mask |= CLONE_NEWNS;
#endif
#ifdef LOSCFG_IPC_CONTAINER
    mask |= CLONE_NEWIPC;
#endif
#ifdef LOSCFG_USER_CONTAINER
    mask |= CLONE_NEWUSER;
#endif
#ifdef LOSCFG_TIME_CONTAINER
    mask |= CLONE_NEWTIME;
#endif
#ifdef LOSCFG_NET_CONTAINER
    mask |= CLONE_NEWNET;
#endif
#ifdef LOSCFG_UTS_CONTAINER
    mask |= CLONE_NEWUTS;
#endif
    return mask;
}

/*
 * called from clone.  This now handles copy for Container and all
 * namespaces therein.
 */
UINT32 OsCopyContainers(UINTPTR flags, LosProcessCB *child, LosProcessCB *parent, UINT32 *processID)
{
    UINT32 intSave;

#ifdef LOSCFG_TIME_CONTAINER
    flags &= ~CLONE_NEWTIME;
#endif

    if (!(flags & GetContainerFlagsMask())) {
#ifdef LOSCFG_PID_CONTAINER
        if (parent->container->pidContainer != parent->container->pidForChildContainer) {
            goto CREATE_CONTAINER;
        }
#endif
#ifdef LOSCFG_TIME_CONTAINER
        if (parent->container->timeContainer != parent->container->timeForChildContainer) {
            goto CREATE_CONTAINER;
        }
#endif
        SCHEDULER_LOCK(intSave);
        child->container = parent->container;
        LOS_AtomicInc(&child->container->rc);
        SCHEDULER_UNLOCK(intSave);
        goto COPY_CONTAINERS;
    }

#if defined(LOSCFG_PID_CONTAINER) || defined(LOSCFG_TIME_CONTAINER)
CREATE_CONTAINER:
#endif
    child->container = CreateContainer();
    if (child->container == NULL) {
        return ENOMEM;
    }

COPY_CONTAINERS:
    return CopyContainers(flags, child, parent, processID);
}

VOID OsContainerFree(LosProcessCB *processCB)
{
    LOS_AtomicDec(&processCB->container->rc);
    if (LOS_AtomicRead(&processCB->container->rc) == 0) {
        (VOID)LOS_MemFree(m_aucSysMem1, processCB->container);
        processCB->container = NULL;
    }
}

VOID OsOsContainersDestroyEarly(LosProcessCB *processCB)
{
   /* All processes in the container must be destroyed before the container is destroyed. */
#ifdef LOSCFG_PID_CONTAINER
    if (processCB->processID == OS_USER_ROOT_PROCESS_ID) {
        OsPidContainerDestroyAllProcess(processCB);
    }
#endif

#ifdef LOSCFG_MNT_CONTAINER
    OsMntContainerDestroy(processCB->container);
#endif
}

VOID OsContainersDestroy(LosProcessCB *processCB)
{
#ifdef LOSCFG_USER_CONTAINER
    OsUserContainerDestroy(processCB);
#endif

#ifdef LOSCFG_UTS_CONTAINER
    OsUtsContainerDestroy(processCB->container);
#endif

#ifdef LOSCFG_IPC_CONTAINER
    OsIpcContainerDestroy(processCB->container);
#endif

#ifdef LOSCFG_TIME_CONTAINER
    OsTimeContainerDestroy(processCB->container);
#endif

#ifdef LOSCFG_NET_CONTAINER
    OsNetContainerDestroy(processCB->container);
#endif

#ifndef LOSCFG_PID_CONTAINER
    UINT32 intSave;
    SCHEDULER_LOCK(intSave);
    OsContainerFree(processCB);
    SCHEDULER_UNLOCK(intSave);
#endif
}

STATIC VOID DeInitContainers(UINT32 flags, Container *container, LosProcessCB *processCB)
{
    UINT32 intSave;
    if (container == NULL) {
        return;
    }

#ifdef LOSCFG_PID_CONTAINER
    SCHEDULER_LOCK(intSave);
    if ((flags & CLONE_NEWPID) != 0) {
        OsPidContainerDestroy(container, processCB);
    } else {
        OsPidContainerDestroy(container, NULL);
    }
    SCHEDULER_UNLOCK(intSave);
#endif

#ifdef LOSCFG_UTS_CONTAINER
    OsUtsContainerDestroy(container);
#endif
#ifdef LOSCFG_MNT_CONTAINER
    OsMntContainerDestroy(container);
#endif
#ifdef LOSCFG_IPC_CONTAINER
    OsIpcContainerDestroy(container);
#endif

#ifdef LOSCFG_TIME_CONTAINER
    OsTimeContainerDestroy(container);
#endif

#ifdef LOSCFG_NET_CONTAINER
    OsNetContainerDestroy(container);
#endif

    SCHEDULER_LOCK(intSave);
    LOS_AtomicDec(&container->rc);
    if (LOS_AtomicRead(&container->rc) == 0) {
        (VOID)LOS_MemFree(m_aucSysMem1, container);
    }
    SCHEDULER_UNLOCK(intSave);
}

UINT32 OsGetContainerID(LosProcessCB *processCB, ContainerType type)
{
    Container *container = processCB->container;
    if (container == NULL) {
        return OS_INVALID_VALUE;
    }

    switch (type) {
#ifdef LOSCFG_PID_CONTAINER
        case PID_CONTAINER:
            return OsGetPidContainerID(container->pidContainer);
        case PID_CHILD_CONTAINER:
            return OsGetPidContainerID(container->pidForChildContainer);
#endif
#ifdef LOSCFG_USER_CONTAINER
        case USER_CONTAINER:
            return OsGetUserContainerID(processCB->credentials);
#endif
#ifdef LOSCFG_UTS_CONTAINER
        case UTS_CONTAINER:
            return OsGetUtsContainerID(container->utsContainer);
#endif
#ifdef LOSCFG_MNT_CONTAINER
        case MNT_CONTAINER:
            return OsGetMntContainerID(container->mntContainer);
#endif
#ifdef LOSCFG_IPC_CONTAINER
        case IPC_CONTAINER:
            return OsGetIpcContainerID(container->ipcContainer);
#endif
#ifdef LOSCFG_TIME_CONTAINER
        case TIME_CONTAINER:
            return OsGetTimeContainerID(container->timeContainer);
        case TIME_CHILD_CONTAINER:
            return OsGetTimeContainerID(container->timeForChildContainer);
#endif
#ifdef LOSCFG_NET_CONTAINER
        case NET_CONTAINER:
            return OsGetNetContainerID(container->netContainer);
#endif
        default:
            break;
    }
    return OS_INVALID_VALUE;
}

STATIC UINT32 UnshareCreateNewContainers(UINT32 flags, LosProcessCB *curr, Container *newContainer)
{
    UINT32 ret = LOS_OK;
#ifdef LOSCFG_PID_CONTAINER
    ret = OsUnsharePidContainer(flags, curr, newContainer);
    if (ret != LOS_OK) {
        return ret;
    }
#endif
#ifdef LOSCFG_UTS_CONTAINER
    ret = OsUnshareUtsContainer(flags, curr, newContainer);
    if (ret != LOS_OK) {
        return ret;
    }
#endif
#ifdef LOSCFG_MNT_CONTAINER
    ret = OsUnshareMntContainer(flags, curr, newContainer);
    if (ret != LOS_OK) {
        return ret;
    }
#endif
#ifdef LOSCFG_IPC_CONTAINER
    ret = OsUnshareIpcContainer(flags, curr, newContainer);
    if (ret != LOS_OK) {
        return ret;
    }
#endif
#ifdef LOSCFG_TIME_CONTAINER
    ret = OsUnshareTimeContainer(flags, curr, newContainer);
    if (ret != LOS_OK) {
        return ret;
    }
#endif
#ifdef LOSCFG_NET_CONTAINER
    ret = OsUnshareNetContainer(flags, curr, newContainer);
    if (ret != LOS_OK) {
        return ret;
    }
#endif
    return ret;
}

INT32 OsUnshare(UINT32 flags)
{
    UINT32 ret;
    UINT32 intSave;
    LosProcessCB *curr = OsCurrProcessGet();
    Container *oldContainer = curr->container;
    UINT32 unshareFlags = GetContainerFlagsMask();
    if (!(flags & unshareFlags) || ((flags & (~unshareFlags)) != 0)) {
        return -EINVAL;
    }

#ifdef LOSCFG_USER_CONTAINER
    ret = OsUnshareUserCredentials(flags, curr);
    if (ret != LOS_OK) {
        return ret;
    }
    if (flags == CLONE_NEWUSER) {
        return LOS_OK;
    }
#endif

    Container *newContainer = CreateContainer();
    if (newContainer == NULL) {
        return -ENOMEM;
    }

    ret = UnshareCreateNewContainers(flags, curr, newContainer);
    if (ret != LOS_OK) {
        goto EXIT;
    }

    SCHEDULER_LOCK(intSave);
    oldContainer = curr->container;
    curr->container = newContainer;
    SCHEDULER_UNLOCK(intSave);

    DeInitContainers(flags, oldContainer, NULL);
    return LOS_OK;

EXIT:
    DeInitContainers(flags, newContainer, NULL);
    return -ret;
}

STATIC UINT32 SetNsGetFlagByContainerType(UINT32 containerType)
{
    if (containerType >= (UINT32)CONTAINER_MAX) {
        return 0;
    }
    ContainerType type = (ContainerType)containerType;
    switch (type) {
        case PID_CONTAINER:
        case PID_CHILD_CONTAINER:
            return CLONE_NEWPID;
        case USER_CONTAINER:
            return CLONE_NEWUSER;
        case UTS_CONTAINER:
            return CLONE_NEWUTS;
        case MNT_CONTAINER:
            return CLONE_NEWNS;
        case IPC_CONTAINER:
            return CLONE_NEWIPC;
        case TIME_CONTAINER:
        case TIME_CHILD_CONTAINER:
            return CLONE_NEWTIME;
        case NET_CONTAINER:
            return CLONE_NEWNET;
        default:
            break;
    }
    return 0;
}

STATIC UINT32 SetNsCreateNewContainers(UINT32 flags, Container *newContainer, Container *container)
{
    UINT32 ret = LOS_OK;
#ifdef LOSCFG_PID_CONTAINER
    ret = OsSetNsPidContainer(flags, container, newContainer);
    if (ret != LOS_OK) {
        return ret;
    }
#endif
#ifdef LOSCFG_UTS_CONTAINER
    ret = OsSetNsUtsContainer(flags, container, newContainer);
    if (ret != LOS_OK) {
        return ret;
    }
#endif
#ifdef LOSCFG_MNT_CONTAINER
    ret = OsSetNsMntContainer(flags, container, newContainer);
    if (ret != LOS_OK) {
        return ret;
    }
#endif
#ifdef LOSCFG_IPC_CONTAINER
    ret = OsSetNsIpcContainer(flags, container, newContainer);
    if (ret != LOS_OK) {
        return ret;
    }
#endif
#ifdef LOSCFG_TIME_CONTAINER
    ret = OsSetNsTimeContainer(flags, container, newContainer);
    if (ret != LOS_OK) {
        return ret;
    }
#endif
#ifdef LOSCFG_NET_CONTAINER
    ret = OsSetNsNetContainer(flags, container, newContainer);
    if (ret != LOS_OK) {
        return ret;
    }
#endif
    return LOS_OK;
}

STATIC UINT32 SetNsParamCheck(INT32 fd, INT32 type, UINT32 *flag, LosProcessCB **target)
{
    UINT32 typeMask = GetContainerFlagsMask();
    *flag = (UINT32)(type & typeMask);
    UINT32 containerType = 0;

    if (((type & (~typeMask)) != 0)) {
        return EINVAL;
    }

    LosProcessCB *processCB = (LosProcessCB *)ProcfsContainerGet(fd, &containerType);
    if (processCB == NULL) {
        return EBADF;
    }

    if (*flag == 0) {
        *flag = SetNsGetFlagByContainerType(containerType);
    }

    if ((*flag == 0) || (*flag != SetNsGetFlagByContainerType(containerType))) {
        return EINVAL;
    }

    if (processCB == OsCurrProcessGet()) {
        return EINVAL;
    }
    *target = processCB;
    return LOS_OK;
}

INT32 OsSetNs(INT32 fd, INT32 type)
{
    UINT32 intSave, ret;
    UINT32 flag = 0;
    LosProcessCB *curr = OsCurrProcessGet();
    LosProcessCB *processCB = NULL;

    ret = SetNsParamCheck(fd, type, &flag, &processCB);
    if (ret != LOS_OK) {
        return -ret;
    }

#ifdef LOSCFG_USER_CONTAINER
    if (flag == CLONE_NEWUSER) {
        SCHEDULER_LOCK(intSave);
        if ((processCB->credentials == NULL) || (processCB->credentials->userContainer == NULL)) {
            SCHEDULER_UNLOCK(intSave);
            return -EBADF;
        }
        UserContainer *userContainer = processCB->credentials->userContainer;
        ret = OsSetNsUserContainer(userContainer, curr);
        SCHEDULER_UNLOCK(intSave);
        return ret;
    }
#endif

    Container *newContainer = CreateContainer();
    if (newContainer == NULL) {
        return -ENOMEM;
    }

    SCHEDULER_LOCK(intSave);
    Container *targetContainer = processCB->container;
    if (targetContainer == NULL) {
        SCHEDULER_UNLOCK(intSave);
        return -EBADF;
    }

    ret = SetNsCreateNewContainers(flag, newContainer, targetContainer);
    if (ret != LOS_OK) {
        SCHEDULER_UNLOCK(intSave);
        goto EXIT;
    }

    Container *oldContainer = curr->container;
    curr->container = newContainer;
    SCHEDULER_UNLOCK(intSave);
    DeInitContainers(flag, oldContainer, NULL);
    return LOS_OK;

EXIT:
    DeInitContainers(flag, newContainer, curr);
    return ret;
}
#endif
