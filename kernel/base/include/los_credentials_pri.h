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

#ifndef _LOS_CREDENTIALS_PRI_H
#define _LOS_CREDENTIALS_PRI_H

#include "los_atomic.h"
#include "los_list.h"

#ifdef LOSCFG_USER_CONTAINER
struct Container;
struct UserContainer;
typedef struct ProcessCB LosProcessCB;

/**
 * @brief 进程凭证结构体
 * @details 存储进程的用户身份凭证信息，包括用户ID、组ID及相关容器引用
 */
typedef struct Credentials {
    Atomic rc;                      ///< 原子引用计数，用于凭证对象的生命周期管理
    UINT32 uid;                     ///< 实际用户ID，标识进程的创建者身份
    UINT32 gid;                     ///< 实际组ID，标识进程所属的主组
    UINT32 euid;                    ///< 有效用户ID，用于权限检查的主要依据
    UINT32 egid;                    ///< 有效组ID，用于组权限检查
    struct UserContainer *userContainer; ///< 指向用户容器的指针，关联进程所属的用户命名空间
} Credentials;

UINT32 OsCopyCredentials(unsigned long flags,  LosProcessCB *child, LosProcessCB *parent);

UINT32 OsInitRootUserCredentials(Credentials **credentials);

UINT32 OsUnshareUserCredentials(UINTPTR flags, LosProcessCB *curr);

UINT32 OsSetNsUserContainer(struct UserContainer *targetContainer, LosProcessCB *runProcess);

VOID FreeCredential(Credentials *credentials);

VOID OsUserContainerDestroy(LosProcessCB *curr);

UINT32 OsGetUserContainerID(Credentials *credentials);

Credentials *PrepareCredential(LosProcessCB *runProcessCB);

INT32 CommitCredentials(Credentials *newCredentials);

Credentials *CurrentCredentials(VOID);

struct UserContainer *OsCurrentUserContainer(VOID);
#endif
#endif /* _LOS_CREDENTIALS_PRI_H */
