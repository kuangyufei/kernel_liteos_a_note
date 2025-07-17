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

#include <sched.h>
#include "los_pid_container_pri.h"
#include "los_config.h"
#include "los_process_pri.h"
#include "los_container_pri.h"

#ifdef LOSCFG_PID_CONTAINER

STATIC UINT32 g_currentPidContainerNum;
STATIC LosProcessCB *g_defaultProcessCB = NULL;
STATIC LosTaskCB *g_defaultTaskCB = NULL;

STATIC VOID FreeVpid(LosProcessCB *processCB)
{
    PidContainer *pidContainer = processCB->container->pidContainer;
    UINT32 vpid = processCB->processID;

    while ((pidContainer != NULL) && !OS_PID_CHECK_INVALID(vpid)) {
        ProcessVid *processVid = &pidContainer->pidArray[vpid];
        processVid->cb = (UINTPTR)g_defaultProcessCB;
        vpid = processVid->vpid;
        processVid->vpid = OS_INVALID_VALUE;
        LOS_ListTailInsert(&pidContainer->pidFreeList, &processVid->node);
        LOS_AtomicDec(&pidContainer->rc);
        PidContainer *parentPidContainer = pidContainer->parent;
        if (LOS_AtomicRead(&pidContainer->rc) > 0) {
            pidContainer = parentPidContainer;
            continue;
        }
        g_currentPidContainerNum--;
        (VOID)LOS_MemFree(m_aucSysMem1, pidContainer->rootPGroup);
        (VOID)LOS_MemFree(m_aucSysMem1, pidContainer);
        if (pidContainer == processCB->container->pidContainer) {
            processCB->container->pidContainer = NULL;
        }
        pidContainer = parentPidContainer;
    }
}

STATIC ProcessVid *OsGetFreeVpid(PidContainer *pidContainer)
{
    if (LOS_ListEmpty(&pidContainer->pidFreeList)) {
        return NULL;
    }

    ProcessVid *vpid = LOS_DL_LIST_ENTRY(LOS_DL_LIST_FIRST(&pidContainer->pidFreeList), ProcessVid, node);
    LOS_ListDelete(&vpid->node);
    return vpid;
}

UINT32 OsAllocSpecifiedVpidUnsafe(UINT32 vpid, PidContainer *pidContainer,
                                  LosProcessCB *processCB, LosProcessCB *parent)
{
    if ((pidContainer == NULL) || OS_PID_CHECK_INVALID(vpid)) {
        return OS_INVALID_VALUE;
    }

    if (LOS_AtomicRead(&pidContainer->lock) > 0) {
        return OS_INVALID_VALUE;
    }

    ProcessVid *processVid = &pidContainer->pidArray[vpid];
    if (processVid->cb != (UINTPTR)g_defaultProcessCB) {
        return OS_INVALID_VALUE;
    }

    processVid->cb = (UINTPTR)processCB;
    processVid->realParent = parent;
    processCB->processID = vpid;
    LOS_ListDelete(&processVid->node);
    LOS_AtomicInc(&pidContainer->rc);

    if (vpid == OS_USER_ROOT_PROCESS_ID) {
        if (parent != NULL) {
            ProcessVid *vppidItem = &pidContainer->pidArray[0];
            LOS_ListDelete(&vppidItem->node);
            vppidItem->cb = (UINTPTR)parent;
        }

        if (OsCreateProcessGroup(processCB) == NULL) {
            return OS_INVALID_VALUE;
        }
    }

    pidContainer = pidContainer->parent;
    while (pidContainer != NULL) {
        ProcessVid *item = OsGetFreeVpid(pidContainer);
        if (item == NULL) {
            break;
        }

        item->cb = (UINTPTR)processCB;
        processVid->vpid = item->vid;
        LOS_AtomicInc(&pidContainer->rc);
        processVid = item;
        pidContainer = pidContainer->parent;
    }
    return processCB->processID;
}

STATIC UINT32 OsAllocVpid(LosProcessCB *processCB, LosProcessCB *parent)
{
    ProcessVid *oldProcessVid = NULL;
    PidContainer *pidContainer = processCB->container->pidContainer;
    if ((pidContainer == NULL) || (LOS_AtomicRead(&pidContainer->lock) > 0)) {
        return OS_INVALID_VALUE;
    }

    processCB->processID = OS_INVALID_VALUE;
    do {
        ProcessVid *vpid = OsGetFreeVpid(pidContainer);
        if (vpid == NULL) {
            break;
        }
        vpid->cb = (UINTPTR)processCB;
        if (processCB->processID == OS_INVALID_VALUE) {
            processCB->processID = vpid->vid;
            vpid->realParent = parent;
        } else {
            oldProcessVid->vpid = vpid->vid;
        }
        LOS_AtomicInc(&pidContainer->rc);
        oldProcessVid = vpid;
        pidContainer = pidContainer->parent;
    } while (pidContainer != NULL);
    return processCB->processID;
}

STATIC ProcessVid *OsGetFreeVtid(PidContainer *pidContainer)
{
    if (LOS_ListEmpty(&pidContainer->tidFreeList)) {
        return NULL;
    }

    ProcessVid *vtid = LOS_DL_LIST_ENTRY(LOS_DL_LIST_FIRST(&pidContainer->tidFreeList), ProcessVid, node);
    LOS_ListDelete(&vtid->node);
    return vtid;
}

VOID OsFreeVtid(LosTaskCB *taskCB)
{
    PidContainer *pidContainer = taskCB->pidContainer;
    UINT32 vtid = taskCB->taskID;

    while ((pidContainer != NULL) && !OS_TID_CHECK_INVALID(vtid)) {
        ProcessVid *item = &pidContainer->tidArray[vtid];
        item->cb = (UINTPTR)g_defaultTaskCB;
        vtid = item->vpid;
        item->vpid = OS_INVALID_VALUE;
        item->realParent = NULL;
        LOS_ListTailInsert(&pidContainer->tidFreeList, &item->node);
        pidContainer = pidContainer->parent;
    }
    taskCB->pidContainer = NULL;
}

UINT32 OsAllocVtid(LosTaskCB *taskCB, const LosProcessCB *processCB)
{
    PidContainer *pidContainer = processCB->container->pidContainer;
    ProcessVid *oldTaskVid = NULL;

    do {
        ProcessVid *item = OsGetFreeVtid(pidContainer);
        if (item == NULL) {
            return OS_INVALID_VALUE;
        }

        if ((pidContainer->parent != NULL) && (item->vid == 0)) {
            item->cb = (UINTPTR)OsCurrTaskGet();
            item->vpid = OsCurrTaskGet()->taskID;
            continue;
        }

        item->cb = (UINTPTR)taskCB;
        if (taskCB->pidContainer == NULL) {
            taskCB->pidContainer = pidContainer;
            taskCB->taskID = item->vid;
        } else {
            oldTaskVid->vpid = item->vid;
        }
        oldTaskVid = item;
        pidContainer = pidContainer->parent;
    } while (pidContainer != NULL);

    return taskCB->taskID;
}

VOID OsPidContainerDestroyAllProcess(LosProcessCB *curr)
{
    INT32 ret;
    UINT32 intSave;
    PidContainer *pidContainer = curr->container->pidContainer;
    LOS_AtomicInc(&pidContainer->lock);

    for (UINT32 index = 2; index < LOSCFG_BASE_CORE_PROCESS_LIMIT; index++) { /* 2: ordinary process */
        SCHEDULER_LOCK(intSave);
        LosProcessCB *processCB = OS_PCB_FROM_PID(index);
        if (OsProcessIsUnused(processCB) || (processCB->parentProcess == NULL)) {
            SCHEDULER_UNLOCK(intSave);
            continue;
        }

        if (curr != processCB->parentProcess) {
            LOS_ListDelete(&processCB->siblingList);
            if (OsProcessIsInactive(processCB)) {
                LOS_ListTailInsert(&curr->exitChildList, &processCB->siblingList);
            } else {
                LOS_ListTailInsert(&curr->childrenList, &processCB->siblingList);
            }
            processCB->parentProcess = curr;
        }
        SCHEDULER_UNLOCK(intSave);

        ret = OsKillLock(index, SIGKILL);
        if (ret < 0) {
            PRINT_ERR("Pid container kill all process failed, pid %u, errno=%d\n", index, -ret);
        }

        ret = LOS_Wait(index, NULL, 0, NULL);
        if (ret != index) {
            PRINT_ERR("Pid container wait pid %d failed, errno=%d\n", index, -ret);
        }
    }
}

STATIC PidContainer *CreateNewPidContainer(PidContainer *parent)
{
    UINT32 index;
    PidContainer *newPidContainer = (PidContainer *)LOS_MemAlloc(m_aucSysMem1, sizeof(PidContainer));
    if (newPidContainer == NULL) {
        return NULL;
    }
    (VOID)memset_s(newPidContainer, sizeof(PidContainer), 0, sizeof(PidContainer));

    newPidContainer->containerID = OsAllocContainerID();
    LOS_ListInit(&newPidContainer->pidFreeList);
    for (index = 0; index < LOSCFG_BASE_CORE_PROCESS_LIMIT; index++) {
        ProcessVid *vpid = &newPidContainer->pidArray[index];
        vpid->vid = index;
        vpid->vpid = OS_INVALID_VALUE;
        vpid->cb = (UINTPTR)g_defaultProcessCB;
        LOS_ListTailInsert(&newPidContainer->pidFreeList, &vpid->node);
    }

    LOS_ListInit(&newPidContainer->tidFreeList);
    for (index = 0; index < LOSCFG_BASE_CORE_TSK_LIMIT; index++) {
        ProcessVid *vtid = &newPidContainer->tidArray[index];
        vtid->vid = index;
        vtid->vpid = OS_INVALID_VALUE;
        vtid->cb = (UINTPTR)g_defaultTaskCB;
        LOS_ListTailInsert(&newPidContainer->tidFreeList, &vtid->node);
    }

    newPidContainer->parent = parent;
    if (parent != NULL) {
        LOS_AtomicSet(&newPidContainer->level, parent->level + 1);
        newPidContainer->referenced = FALSE;
    } else {
        LOS_AtomicSet(&newPidContainer->level, 0);
        newPidContainer->referenced = TRUE;
    }
    return newPidContainer;
}

VOID OsPidContainerDestroy(Container *container, LosProcessCB *processCB)
{
    if (container == NULL) {
        return;
    }

    PidContainer *pidContainer = container->pidContainer;
    if (pidContainer == NULL) {
        return;
    }

    if (processCB != NULL) {
        FreeVpid(processCB);
    }

    if ((container->pidForChildContainer != NULL) && (pidContainer != container->pidForChildContainer)) {
        LOS_AtomicDec(&container->pidForChildContainer->rc);
        if (LOS_AtomicRead(&container->pidForChildContainer->rc) <= 0) {
            g_currentPidContainerNum--;
            (VOID)LOS_MemFree(m_aucSysMem1, container->pidForChildContainer);
            container->pidForChildContainer = NULL;
        }
    }

    if ((container->pidContainer != NULL) && (LOS_AtomicRead(&pidContainer->rc) <= 0)) {
        g_currentPidContainerNum--;
        container->pidContainer = NULL;
        container->pidForChildContainer = NULL;
        (VOID)LOS_MemFree(m_aucSysMem1, pidContainer->rootPGroup);
        (VOID)LOS_MemFree(m_aucSysMem1, pidContainer);
    }

    if (processCB != NULL) {
        OsContainerFree(processCB);
    }
}

STATIC UINT32 CreatePidContainer(LosProcessCB *child, LosProcessCB *parent)
{
    UINT32 ret;
    UINT32 intSave;

    PidContainer *parentContainer = parent->container->pidContainer;
    PidContainer *newPidContainer = CreateNewPidContainer(parentContainer);
    if (newPidContainer == NULL) {
        return ENOMEM;
    }

    SCHEDULER_LOCK(intSave);
    if ((parentContainer->level + 1) >= PID_CONTAINER_LEVEL_LIMIT) {
        (VOID)LOS_MemFree(m_aucSysMem1, newPidContainer);
        SCHEDULER_UNLOCK(intSave);
        return EINVAL;
    }

    g_currentPidContainerNum++;
    newPidContainer->referenced = TRUE;
    child->container->pidContainer = newPidContainer;
    child->container->pidForChildContainer = newPidContainer;
    ret = OsAllocSpecifiedVpidUnsafe(OS_USER_ROOT_PROCESS_ID, newPidContainer, child, parent);
    if (ret == OS_INVALID_VALUE) {
        g_currentPidContainerNum--;
        FreeVpid(child);
        child->container->pidContainer = NULL;
        child->container->pidForChildContainer = NULL;
        SCHEDULER_UNLOCK(intSave);
        (VOID)LOS_MemFree(m_aucSysMem1, newPidContainer);
        return ENOSPC;
    }
    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;
}

STATIC UINT32 AllocVpidFormPidForChildContainer(LosProcessCB *child, LosProcessCB *parent)
{
    UINT32 ret;
    PidContainer *pidContainer = parent->container->pidForChildContainer;
    child->container->pidContainer = pidContainer;
    child->container->pidForChildContainer = pidContainer;

    if (!pidContainer->referenced) {
        pidContainer->referenced = TRUE;
        ret = OsAllocSpecifiedVpidUnsafe(OS_USER_ROOT_PROCESS_ID, pidContainer, child, parent);
    } else {
        ret = OsAllocVpid(child, parent);
        if (ret != OS_INVALID_VALUE) {
            LosProcessCB *parentProcessCB = (LosProcessCB *)pidContainer->pidArray[OS_USER_ROOT_PROCESS_ID].cb;
            child->parentProcess = parentProcessCB;
            LOS_ListTailInsert(&parentProcessCB->childrenList, &child->siblingList);
            child->pgroup = parentProcessCB->pgroup;
            LOS_ListTailInsert(&parentProcessCB->pgroup->processList, &child->subordinateGroupList);
        }
    }

    if (ret == OS_INVALID_VALUE) {
        FreeVpid(child);
        child->container->pidContainer = NULL;
        child->container->pidForChildContainer = NULL;
        return ENOSPC;
    }

    return LOS_OK;
}

UINT32 OsCopyPidContainer(UINTPTR flags, LosProcessCB *child, LosProcessCB *parent, UINT32 *processID)
{
    UINT32 ret;
    UINT32 intSave;

    SCHEDULER_LOCK(intSave);
    PidContainer *parentPidContainer = parent->container->pidContainer;
    PidContainer *parentPidContainerForChild = parent->container->pidForChildContainer;
    if (parentPidContainer == parentPidContainerForChild) {
        /* The current process is not executing unshare */
        if (!(flags & CLONE_NEWPID)) {
            child->container->pidContainer = parentPidContainer;
            child->container->pidForChildContainer = parentPidContainer;
            ret = OsAllocVpid(child, parent);
            SCHEDULER_UNLOCK(intSave);
            if (ret == OS_INVALID_VALUE) {
                PRINT_ERR("[%s] alloc vpid failed\n", __FUNCTION__);
                return ENOSPC;
            }
            *processID = child->processID;
            return LOS_OK;
        }
        SCHEDULER_UNLOCK(intSave);

        if (OsContainerLimitCheck(PID_CONTAINER, &g_currentPidContainerNum) != LOS_OK) {
            return EPERM;
        }

        ret = CreatePidContainer(child, parent);
        if (ret != LOS_OK) {
            return ret;
        }
    } else {
        /* Create the first process after unshare */
        ret = AllocVpidFormPidForChildContainer(child, parent);
        SCHEDULER_UNLOCK(intSave);
        if (ret != LOS_OK) {
            return ret;
        }
    }

    PidContainer *pidContainer = child->container->pidContainer;
    if (pidContainer->pidArray[child->processID].vpid == OS_INVALID_VALUE) {
        *processID = child->processID;
    } else {
        *processID = pidContainer->pidArray[child->processID].vpid;
    }
    return LOS_OK;
}

UINT32 OsUnsharePidContainer(UINTPTR flags, LosProcessCB *curr, Container *newContainer)
{
    UINT32 intSave;
    if (!(flags & CLONE_NEWPID)) {
        SCHEDULER_LOCK(intSave);
        newContainer->pidContainer = curr->container->pidContainer;
        newContainer->pidForChildContainer = curr->container->pidForChildContainer;
        if (newContainer->pidContainer != newContainer->pidForChildContainer) {
            LOS_AtomicInc(&newContainer->pidForChildContainer->rc);
        }
        SCHEDULER_UNLOCK(intSave);
        return LOS_OK;
    }

    if (OsContainerLimitCheck(PID_CONTAINER, &g_currentPidContainerNum) != LOS_OK) {
        return EPERM;
    }

    PidContainer *pidForChild = CreateNewPidContainer(curr->container->pidContainer);
    if (pidForChild == NULL) {
        return ENOMEM;
    }

    SCHEDULER_LOCK(intSave);
    if (curr->container->pidContainer != curr->container->pidForChildContainer) {
        SCHEDULER_UNLOCK(intSave);
        (VOID)LOS_MemFree(m_aucSysMem1, pidForChild);
        return EINVAL;
    }

    if (pidForChild->level >= PID_CONTAINER_LEVEL_LIMIT) {
        SCHEDULER_UNLOCK(intSave);
        (VOID)LOS_MemFree(m_aucSysMem1, pidForChild);
        return EINVAL;
    }

    g_currentPidContainerNum++;
    newContainer->pidContainer = curr->container->pidContainer;
    newContainer->pidForChildContainer = pidForChild;
    LOS_AtomicSet(&pidForChild->rc, 1);
    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;
}

UINT32 OsSetNsPidContainer(UINT32 flags, Container *container, Container *newContainer)
{
    UINT32 ret = LOS_OK;
    LosProcessCB *curr = OsCurrProcessGet();
    newContainer->pidContainer = curr->container->pidContainer;

    if (flags & CLONE_NEWPID) {
        newContainer->pidForChildContainer = container->pidContainer;
        LOS_AtomicInc(&container->pidContainer->rc);
        return ret;
    }

    newContainer->pidForChildContainer = curr->container->pidForChildContainer;
    if (newContainer->pidContainer != newContainer->pidForChildContainer) {
        LOS_AtomicInc(&newContainer->pidForChildContainer->rc);
    }
    return LOS_OK;
}

UINT32 OsInitRootPidContainer(PidContainer **pidContainer)
{
    UINT32 intSave;
    g_defaultTaskCB = OsGetDefaultTaskCB();
    g_defaultProcessCB = OsGetDefaultProcessCB();

    PidContainer *newPidContainer = CreateNewPidContainer(NULL);
    if (newPidContainer == NULL) {
        return ENOMEM;
    }

    SCHEDULER_LOCK(intSave);
    g_currentPidContainerNum++;
    *pidContainer = newPidContainer;
    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;
}

UINT32 OsGetVpidFromCurrContainer(const LosProcessCB *processCB)
{
    UINT32 vpid = processCB->processID;
    PidContainer *pidContainer = processCB->container->pidContainer;
    PidContainer *currPidContainer = OsCurrTaskGet()->pidContainer;
    while (pidContainer != NULL) {
        ProcessVid *vid = &pidContainer->pidArray[vpid];
        if (currPidContainer != pidContainer) {
            vpid = vid->vpid;
            pidContainer = pidContainer->parent;
            continue;
        }

        return vid->vid;
    }
    return OS_INVALID_VALUE;
}

UINT32 OsGetVpidFromRootContainer(const LosProcessCB *processCB)
{
    UINT32 vpid = processCB->processID;
    PidContainer *pidContainer = processCB->container->pidContainer;
    while (pidContainer != NULL) {
        ProcessVid *vid = &pidContainer->pidArray[vpid];
        if (pidContainer->parent != NULL) {
            vpid = vid->vpid;
            pidContainer = pidContainer->parent;
            continue;
        }

        return vid->vid;
    }
    return OS_INVALID_VALUE;
}

UINT32 OsGetVtidFromCurrContainer(const LosTaskCB *taskCB)
{
    UINT32 vtid = taskCB->taskID;
    PidContainer *pidContainer = taskCB->pidContainer;
    PidContainer *currPidContainer = OsCurrTaskGet()->pidContainer;
    while (pidContainer != NULL) {
        ProcessVid *vid = &pidContainer->tidArray[vtid];
        if (currPidContainer != pidContainer) {
            vtid = vid->vpid;
            pidContainer = pidContainer->parent;
            continue;
        }
        return vid->vid;
    }
    return OS_INVALID_VALUE;
}

LosProcessCB *OsGetPCBFromVpid(UINT32 vpid)
{
    PidContainer *pidContainer = OsCurrTaskGet()->pidContainer;
    ProcessVid *processVid = &pidContainer->pidArray[vpid];
    return (LosProcessCB *)processVid->cb;
}

LosTaskCB *OsGetTCBFromVtid(UINT32 vtid)
{
    PidContainer *pidContainer = OsCurrTaskGet()->pidContainer;
    ProcessVid *taskVid = &pidContainer->tidArray[vtid];
    return (LosTaskCB *)taskVid->cb;
}

BOOL OsPidContainerProcessParentIsRealParent(const LosProcessCB *processCB, const LosProcessCB *curr)
{
    if (curr == processCB->parentProcess) {
        return FALSE;
    }

    PidContainer *pidContainer = processCB->container->pidContainer;
    ProcessVid *processVid = &pidContainer->pidArray[processCB->processID];
    if (processVid->realParent == curr) {
        return TRUE;
    }
    return FALSE;
}

UINT32 OsGetPidContainerID(PidContainer *pidContainer)
{
    if (pidContainer == NULL) {
        return OS_INVALID_VALUE;
    }

    return pidContainer->containerID;
}

UINT32 OsGetPidContainerCount(VOID)
{
    return g_currentPidContainerNum;
}
#endif
