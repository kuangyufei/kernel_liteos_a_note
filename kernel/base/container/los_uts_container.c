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

STATIC UINT32 CreateUtsContainer(UtsContainer **newUtsContainer)
{
    UINT32 intSave;
    UINT32 size = sizeof(UtsContainer);
    UtsContainer *utsContainer = LOS_MemAlloc(m_aucSysMem1, size);
    if (utsContainer == NULL) {
        return ENOMEM;
    }
    (VOID)memset_s(utsContainer, sizeof(UtsContainer), 0, sizeof(UtsContainer));

    LOS_AtomicSet(&utsContainer->rc, 1);

    SCHEDULER_LOCK(intSave);
    g_currentUtsContainerNum += 1;
    *newUtsContainer = utsContainer;
    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;
}

UINT32 OsCopyUtsContainer(UINTPTR flags, LosProcessCB *child, LosProcessCB *parent)
{
    UINT32 intSave;
    UINT32 ret;
    UtsContainer *newUtsContainer = NULL;
    UtsContainer *currUtsContainer = parent->container->utsContainer;

    if (!(flags & CLONE_NEWUTS)) {
        SCHEDULER_LOCK(intSave);
        LOS_AtomicInc(&currUtsContainer->rc);
        child->container->utsContainer = currUtsContainer;
        SCHEDULER_UNLOCK(intSave);
        return LOS_OK;
    }

    ret = CreateUtsContainer(&newUtsContainer);
    if (ret != LOS_OK) {
        return ret;
    }

    SCHEDULER_LOCK(intSave);
    (VOID)memcpy_s(&newUtsContainer->utsName, sizeof(newUtsContainer->utsName),
                   &currUtsContainer->utsName, sizeof(currUtsContainer->utsName));
    child->container->utsContainer = newUtsContainer;
    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;
}

VOID OsUtsContainersDestroy(LosProcessCB *curr)
{
    UINT32 intSave;
    if (curr->container == NULL) {
        return;
    }

    SCHEDULER_LOCK(intSave);
    UtsContainer *utsContainer = curr->container->utsContainer;
    if (utsContainer != NULL) {
        if (LOS_AtomicRead(&utsContainer->rc) == 0) {
            g_currentUtsContainerNum--;
            curr->container->utsContainer = NULL;
            SCHEDULER_UNLOCK(intSave);
            (VOID)LOS_MemFree(m_aucSysMem1, utsContainer);
            return;
        }
    }
    SCHEDULER_UNLOCK(intSave);
    return;
}

STATIC UINT32 InitUtsContainer(struct utsname *name)
{
    UINT32 ret;

    ret = sprintf_s(name->sysname, sizeof(name->sysname), "%s", KERNEL_NAME);
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

UINT32 OsInitRootUtsContainer(UtsContainer **utsContainer)
{
    UINT32 ret = CreateUtsContainer(utsContainer);
    if (ret != LOS_OK) {
        return ret;
    }

    return InitUtsContainer(&(*utsContainer)->utsName);
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
#endif
