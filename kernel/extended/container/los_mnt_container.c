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

#ifdef LOSCFG_MNT_CONTAINER
#include <unistd.h>
#include "los_mnt_container_pri.h"
#include "los_container_pri.h"
#include "los_process_pri.h"
#include "sys/mount.h"
#include "vnode.h"
#include "internal.h"

STATIC UINT32 g_currentMntContainerNum;

LIST_HEAD *GetContainerMntList(VOID)
{
    return &OsCurrProcessGet()->container->mntContainer->mountList;
}

STATIC MntContainer *CreateNewMntContainer(MntContainer *parent)
{
    MntContainer *mntContainer = (MntContainer *)LOS_MemAlloc(m_aucSysMem1, sizeof(MntContainer));
    if (mntContainer == NULL) {
        return NULL;
    }
    mntContainer->containerID = OsAllocContainerID();
    LOS_ListInit(&mntContainer->mountList);

    if (parent != NULL) {
        LOS_AtomicSet(&mntContainer->rc, 1);
    } else {
        LOS_AtomicSet(&mntContainer->rc, 3); /* 3: Three system processes */
    }
    return mntContainer;
}

STATIC UINT32 CreateMntContainer(LosProcessCB *child, LosProcessCB *parent)
{
    UINT32 intSave;
    MntContainer *parentContainer = parent->container->mntContainer;
    MntContainer *newMntContainer = CreateNewMntContainer(parentContainer);
    if (newMntContainer == NULL) {
        return ENOMEM;
    }

    SCHEDULER_LOCK(intSave);
    g_currentMntContainerNum++;
    child->container->mntContainer = newMntContainer;
    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;
}

UINT32 OsInitRootMntContainer(MntContainer **mntContainer)
{
    UINT32 intSave;
    MntContainer *newMntContainer = CreateNewMntContainer(NULL);
    if (newMntContainer == NULL) {
        return ENOMEM;
    }

    SCHEDULER_LOCK(intSave);
    g_currentMntContainerNum++;
    *mntContainer = newMntContainer;
    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;
}

STATIC UINT32 CopyMountList(MntContainer *parentContainer, MntContainer *newContainer)
{
    struct Mount *mnt = NULL;
    VnodeHold();
    LOS_DL_LIST_FOR_EACH_ENTRY(mnt, &parentContainer->mountList, struct Mount, mountList) {
        struct Mount *newMnt = (struct Mount *)zalloc(sizeof(struct Mount));
        if (newMnt == NULL) {
            VnodeDrop();
            return ENOMEM;
        }
        *newMnt = *mnt;
        LOS_ListTailInsert(&newContainer->mountList, &newMnt->mountList);
        newMnt->vnodeCovered->mntCount++;
    }
    VnodeDrop();
    return LOS_OK;
}

UINT32 OsCopyMntContainer(UINTPTR flags, LosProcessCB *child, LosProcessCB *parent)
{
    UINT32 ret;
    UINT32 intSave;
    MntContainer *currMntContainer = parent->container->mntContainer;

    if (!(flags & CLONE_NEWNS)) {
        SCHEDULER_LOCK(intSave);
        LOS_AtomicInc(&currMntContainer->rc);
        child->container->mntContainer = currMntContainer;
        SCHEDULER_UNLOCK(intSave);
        return LOS_OK;
    }

    if (OsContainerLimitCheck(MNT_CONTAINER, &g_currentMntContainerNum) != LOS_OK) {
        return EPERM;
    }

    ret = CreateMntContainer(child, parent);
    if (ret != LOS_OK) {
        return ret;
    }

    return CopyMountList(currMntContainer, child->container->mntContainer);
}

UINT32 OsUnshareMntContainer(UINTPTR flags, LosProcessCB *curr, Container *newContainer)
{
    UINT32 intSave;
    UINT32 ret;
    MntContainer *parentContainer = curr->container->mntContainer;

    if (!(flags & CLONE_NEWNS)) {
        SCHEDULER_LOCK(intSave);
        newContainer->mntContainer = parentContainer;
        LOS_AtomicInc(&parentContainer->rc);
        SCHEDULER_UNLOCK(intSave);
        return LOS_OK;
    }

    if (OsContainerLimitCheck(MNT_CONTAINER, &g_currentMntContainerNum) != LOS_OK) {
        return EPERM;
    }

    MntContainer *mntContainer = CreateNewMntContainer(parentContainer);
    if (mntContainer == NULL) {
        return ENOMEM;
    }

    ret = CopyMountList(parentContainer, mntContainer);
    if (ret != LOS_OK) {
        (VOID)LOS_MemFree(m_aucSysMem1, mntContainer);
        return ret;
    }

    SCHEDULER_LOCK(intSave);
    newContainer->mntContainer = mntContainer;
    g_currentMntContainerNum++;
    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;
}

UINT32 OsSetNsMntContainer(UINT32 flags, Container *container, Container *newContainer)
{
    if (flags & CLONE_NEWNS) {
        newContainer->mntContainer = container->mntContainer;
        LOS_AtomicInc(&container->mntContainer->rc);
        return LOS_OK;
    }

    newContainer->mntContainer = OsCurrProcessGet()->container->mntContainer;
    LOS_AtomicInc(&newContainer->mntContainer->rc);
    return LOS_OK;
}

STATIC VOID FreeMountList(LIST_HEAD *mountList)
{
    struct Mount *mnt = NULL;
    struct Mount *nextMnt = NULL;

    VnodeHold();
    if (LOS_ListEmpty(mountList)) {
        VnodeDrop();
        return;
    }

    LOS_DL_LIST_FOR_EACH_ENTRY_SAFE(mnt, nextMnt, mountList, struct Mount, mountList) {
        if (mnt->vnodeCovered->mntCount > 0) {
            mnt->vnodeCovered->mntCount--;
            LOS_ListDelete(&mnt->mountList);
            free(mnt);
        } else {
            umount(mnt->pathName);
        }
    }
    VnodeDrop();
    return;
}

VOID OsMntContainerDestroy(Container *container)
{
    UINT32 intSave;
    if (container == NULL) {
        return;
    }

    SCHEDULER_LOCK(intSave);
    MntContainer *mntContainer = container->mntContainer;
    if (mntContainer == NULL) {
        SCHEDULER_UNLOCK(intSave);
        return;
    }

    LOS_AtomicDec(&mntContainer->rc);
    if (LOS_AtomicRead(&mntContainer->rc) > 0) {
        SCHEDULER_UNLOCK(intSave);
        return;
    }

    g_currentMntContainerNum--;
    SCHEDULER_UNLOCK(intSave);
    FreeMountList(&mntContainer->mountList);
    container->mntContainer = NULL;
    (VOID)LOS_MemFree(m_aucSysMem1, mntContainer);
    return;
}

UINT32 OsGetMntContainerID(MntContainer *mntContainer)
{
    if (mntContainer == NULL) {
        return OS_INVALID_VALUE;
    }

    return mntContainer->containerID;
}

UINT32 OsGetMntContainerCount(VOID)
{
    return g_currentMntContainerNum;
}
#endif
