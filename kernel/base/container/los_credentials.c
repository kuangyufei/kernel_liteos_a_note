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

#include <errno.h>
#include "los_credentials_pri.h"
#include "los_user_container_pri.h"
#include "los_config.h"
#include "los_memory.h"
#include "los_process_pri.h"

#ifdef LOSCFG_USER_CONTAINER
STATIC Credentials *CreateNewCredential(LosProcessCB *parent)
{
    UINT32 size = sizeof(Credentials);
    Credentials *newCredentials = LOS_MemAlloc(m_aucSysMem1, size);
    if (newCredentials == NULL) {
        return NULL;
    }

    if (parent != NULL) {
        const Credentials *oldCredentials = parent->credentials;
        (VOID)memcpy_s(newCredentials, sizeof(Credentials), oldCredentials, sizeof(Credentials));
        LOS_AtomicSet(&newCredentials->rc, 1);
    } else {
        (VOID)memset_s(newCredentials, sizeof(Credentials), 0, sizeof(Credentials));
        LOS_AtomicSet(&newCredentials->rc, 3); /* 3: Three system processes */
    }
    newCredentials->userContainer = NULL;
    return newCredentials;
}

Credentials *PrepareCredential(LosProcessCB *runProcessCB)
{
    Credentials *newCredentials = CreateNewCredential(runProcessCB);
    if (newCredentials == NULL) {
        return NULL;
    }

    newCredentials->userContainer = runProcessCB->credentials->userContainer;
    LOS_AtomicInc(&newCredentials->userContainer->rc);
    return newCredentials;
}

VOID FreeCredential(Credentials *credentials)
{
    if (credentials == NULL) {
        return;
    }

    if (credentials->userContainer != NULL) {
        LOS_AtomicDec(&credentials->userContainer->rc);
        if (LOS_AtomicRead(&credentials->userContainer->rc) <= 0) {
            FreeUserContainer(credentials->userContainer);
            credentials->userContainer = NULL;
        }
    }

    LOS_AtomicDec(&credentials->rc);
    if (LOS_AtomicRead(&credentials->rc) <= 0) {
        (VOID)LOS_MemFree(m_aucSysMem1, credentials);
    }
}

VOID OsUserContainerDestroy(LosProcessCB *curr)
{
    UINT32 intSave;
    SCHEDULER_LOCK(intSave);
    FreeCredential(curr->credentials);
    curr->credentials = NULL;
    SCHEDULER_UNLOCK(intSave);
    return;
}

STATIC Credentials *CreateCredentials(unsigned long flags, LosProcessCB *parent)
{
    UINT32 ret;
    Credentials *newCredentials = CreateNewCredential(parent);
    if (newCredentials == NULL) {
        return NULL;
    }

    if (!(flags & CLONE_NEWUSER)) {
        newCredentials->userContainer = parent->credentials->userContainer;
        LOS_AtomicInc(&newCredentials->userContainer->rc);
        return newCredentials;
    }

    if (parent != NULL) {
        ret = OsCreateUserContainer(newCredentials, parent->credentials->userContainer);
    } else {
        ret = OsCreateUserContainer(newCredentials, NULL);
    }
    if (ret != LOS_OK) {
        FreeCredential(newCredentials);
        return NULL;
    }
    return newCredentials;
}

UINT32 OsCopyCredentials(unsigned long flags,  LosProcessCB *child, LosProcessCB *parent)
{
    UINT32 intSave;

    SCHEDULER_LOCK(intSave);
    child->credentials = CreateCredentials(flags, parent);
    SCHEDULER_UNLOCK(intSave);
    if (child->credentials == NULL) {
        return ENOMEM;
    }
    return LOS_OK;
}

UINT32 OsInitRootUserCredentials(Credentials **credentials)
{
    *credentials = CreateCredentials(CLONE_NEWUSER, NULL);
    if (*credentials == NULL) {
        return ENOMEM;
    }
    return LOS_OK;
}

UINT32 OsUnshareUserCredentials(UINTPTR flags, LosProcessCB *curr)
{
    UINT32 intSave;
    if (!(flags & CLONE_NEWUSER)) {
        return LOS_OK;
    }

    SCHEDULER_LOCK(intSave);
    UINT32 ret = OsCreateUserContainer(curr->credentials, curr->credentials->userContainer);
    SCHEDULER_UNLOCK(intSave);
    return ret;
}

UINT32 OsSetNsUserContainer(struct UserContainer *targetContainer, LosProcessCB *runProcess)
{
    Credentials *oldCredentials = runProcess->credentials;
    Credentials *newCredentials = CreateNewCredential(runProcess);
    if (newCredentials == NULL) {
        return ENOMEM;
    }

    runProcess->credentials = newCredentials;
    newCredentials->userContainer = targetContainer;
    LOS_AtomicInc(&targetContainer->rc);
    FreeCredential(oldCredentials);
    return LOS_OK;
}

UINT32 OsGetUserContainerID(Credentials *credentials)
{
    if ((credentials == NULL) || (credentials->userContainer == NULL)) {
        return OS_INVALID_VALUE;
    }

    return credentials->userContainer->containerID;
}

INT32 CommitCredentials(Credentials *newCredentials)
{
    Credentials *oldCredentials = OsCurrProcessGet()->credentials;

    if (LOS_AtomicRead(&newCredentials->rc) < 1) {
        return -EINVAL;
    }

    OsCurrProcessGet()->credentials = newCredentials;
    FreeCredential(oldCredentials);
    return 0;
}

Credentials *CurrentCredentials(VOID)
{
    return OsCurrProcessGet()->credentials;
}

UserContainer *OsCurrentUserContainer(VOID)
{
    UserContainer *userContainer = OsCurrProcessGet()->credentials->userContainer;
    return userContainer;
}
#endif
