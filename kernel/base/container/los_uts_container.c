/*
 * Copyright (c) 2022-2022 Huawei Device Co., Ltd. All rights reserved.
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

#include "internal.h"
#include "los_uts_container_pri.h"
#include "los_process_pri.h"

#ifdef LOSCFG_UTS_CONTAINER

STATIC UINT32 g_currentUtsContainerNum;

STATIC UINT32 InitUtsContainer(struct utsname *name)
{
    INT32 ret = sprintf_s(name->sysname, sizeof(name->sysname), "%s", KERNEL_NAME);
    if (ret < 0) {
        return LOS_NOK;
    }
    ret = sprintf_s(name->nodename, sizeof(name->nodename), "%s", KERNEL_NODE_NAME);
    if (ret < 0) {
        return LOS_NOK;
    }
    ret = sprintf_s(name->version, sizeof(name->version), "%s %u.%u.%u.%u %s %s",
        KERNEL_NAME, KERNEL_MAJOR, KERNEL_MINOR, KERNEL_PATCH, KERNEL_ITRE, __DATE__, __TIME__);
    if (ret < 0) {
        return LOS_NOK;
    }
    const char *cpuInfo = LOS_CpuInfo();
    ret = sprintf_s(name->machine, sizeof(name->machine), "%s", cpuInfo);
    if (ret < 0) {
        return LOS_NOK;
    }
    ret = sprintf_s(name->release, sizeof(name->release), "%u.%u.%u.%u",
        KERNEL_MAJOR, KERNEL_MINOR, KERNEL_PATCH, KERNEL_ITRE);
    if (ret < 0) {
        return LOS_NOK;
    }
    name->domainname[0] = '\0';
    return LOS_OK;
}

STATIC UtsContainer *CreateNewUtsContainer(UtsContainer *parent)
{
    UINT32 ret;
    UINT32 size = sizeof(UtsContainer);
    UtsContainer *utsContainer = (UtsContainer *)LOS_MemAlloc(m_aucSysMem1, size);
    if (utsContainer == NULL) {
        return NULL;
    }
    (VOID)memset_s(utsContainer, sizeof(UtsContainer), 0, sizeof(UtsContainer));

    utsContainer->containerID = OsAllocContainerID();
    if (parent != NULL) {
        LOS_AtomicSet(&utsContainer->rc, 1);
        return utsContainer;
    }
    LOS_AtomicSet(&utsContainer->rc, 3); /* 3: Three system processes */
    ret = InitUtsContainer(&utsContainer->utsName);
    if (ret != LOS_OK) {
        (VOID)LOS_MemFree(m_aucSysMem1, utsContainer);
        return NULL;
    }
    return utsContainer;
}

STATIC UINT32 CreateUtsContainer(LosProcessCB *child, LosProcessCB *parent)
{
    UINT32 intSave;
    UtsContainer *parentContainer = parent->container->utsContainer;
    UtsContainer *newUtsContainer = CreateNewUtsContainer(parentContainer);
    if (newUtsContainer == NULL) {
        return ENOMEM;
    }

    SCHEDULER_LOCK(intSave);
    g_currentUtsContainerNum++;
    (VOID)memcpy_s(&newUtsContainer->utsName, sizeof(newUtsContainer->utsName),
                   &parentContainer->utsName, sizeof(parentContainer->utsName));
    child->container->utsContainer = newUtsContainer;
    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;
}

UINT32 OsInitRootUtsContainer(UtsContainer **utsContainer)
{
    UINT32 intSave;
    UtsContainer *newUtsContainer = CreateNewUtsContainer(NULL);
    if (newUtsContainer == NULL) {
        return ENOMEM;
    }

    SCHEDULER_LOCK(intSave);
    g_currentUtsContainerNum++;
    *utsContainer = newUtsContainer;
    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;
}

UINT32 OsCopyUtsContainer(UINTPTR flags, LosProcessCB *child, LosProcessCB *parent)
{
    UINT32 intSave;
    UtsContainer *currUtsContainer = parent->container->utsContainer;

    if (!(flags & CLONE_NEWUTS)) {
        SCHEDULER_LOCK(intSave);
        LOS_AtomicInc(&currUtsContainer->rc);
        child->container->utsContainer = currUtsContainer;
        SCHEDULER_UNLOCK(intSave);
        return LOS_OK;
    }

    return CreateUtsContainer(child, parent);
}

UINT32 OsUnshareUtsContainer(UINTPTR flags, LosProcessCB *curr, Container *newContainer)
{
    UINT32 intSave;
    UtsContainer *parentContainer = curr->container->utsContainer;

    if (!(flags & CLONE_NEWUTS)) {
        SCHEDULER_LOCK(intSave);
        newContainer->utsContainer = parentContainer;
        LOS_AtomicInc(&parentContainer->rc);
        SCHEDULER_UNLOCK(intSave);
        return LOS_OK;
    }

    UtsContainer *utsContainer = CreateNewUtsContainer(parentContainer);
    if (utsContainer == NULL) {
        return ENOMEM;
    }

    SCHEDULER_LOCK(intSave);
    newContainer->utsContainer = utsContainer;
    g_currentUtsContainerNum++;
    (VOID)memcpy_s(&utsContainer->utsName, sizeof(utsContainer->utsName),
                   &parentContainer->utsName, sizeof(parentContainer->utsName));
    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;
}

UINT32 OsSetNsUtsContainer(UINT32 flags, Container *container, Container *newContainer)
{
    if (flags & CLONE_NEWUTS) {
        newContainer->utsContainer = container->utsContainer;
        LOS_AtomicInc(&container->utsContainer->rc);
        return LOS_OK;
    }

    newContainer->utsContainer = OsCurrProcessGet()->container->utsContainer;
    LOS_AtomicInc(&newContainer->utsContainer->rc);
    return LOS_OK;
}

VOID OsUtsContainerDestroy(Container *container)
{
    UINT32 intSave;
    if (container == NULL) {
        return;
    }

    SCHEDULER_LOCK(intSave);
    UtsContainer *utsContainer = container->utsContainer;
    if (utsContainer == NULL) {
        SCHEDULER_UNLOCK(intSave);
        return;
    }

    LOS_AtomicDec(&utsContainer->rc);
    if (LOS_AtomicRead(&utsContainer->rc) > 0) {
        SCHEDULER_UNLOCK(intSave);
        return;
    }
    g_currentUtsContainerNum--;
    container->utsContainer = NULL;
    SCHEDULER_UNLOCK(intSave);
    (VOID)LOS_MemFree(m_aucSysMem1, utsContainer);
    return;
}

struct utsname *OsGetCurrUtsName(VOID)
{
    Container *container = OsCurrProcessGet()->container;
    if (container == NULL) {
        return NULL;
    }
    UtsContainer *utsContainer = container->utsContainer;
    if (utsContainer == NULL) {
        return NULL;
    }
    return &utsContainer->utsName;
}

UINT32 OsGetUtsContainerID(UtsContainer *utsContainer)
{
    if (utsContainer == NULL) {
        return OS_INVALID_VALUE;
    }

    return utsContainer->containerID;
}

#endif
