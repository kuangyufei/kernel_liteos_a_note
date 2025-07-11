/*
 * Copyright (c) 2022-2022 Huawei Device Co., Ltd. All rights reserved.
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

#ifndef _LOS_USER_CONTAINER_PRI_H
#define _LOS_USER_CONTAINER_PRI_H

#include "los_atomic.h"
#include "los_credentials_pri.h"

#define UID_GID_MAP_MAX_EXTENTS 5

#ifdef LOSCFG_USER_CONTAINER
struct ProcFile;

typedef struct UidGidExtent {
    UINT32 first;
    UINT32 lowerFirst;
    UINT32 count;
} UidGidExtent;

typedef struct UidGidMap {
    UINT32 extentCount;
    union {
        UidGidExtent extent[UID_GID_MAP_MAX_EXTENTS];
    };
} UidGidMap;

typedef struct UserContainer {
    Atomic rc;
    INT32 level;
    UINT32 owner;
    UINT32 group;
    struct UserContainer *parent;
    UidGidMap uidMap;
    UidGidMap gidMap;
    UINT32 containerID;
} UserContainer;

UINT32 OsCreateUserContainer(Credentials *newCredentials, UserContainer *parentUserContainer);

VOID FreeUserContainer(UserContainer *userContainer);

UINT32 OsFromKuidMunged(UserContainer *userContainer, UINT32 kuid);

UINT32 OsFromKgidMunged(UserContainer *userContainer, UINT32 kgid);

UINT32 OsMakeKuid(UserContainer *userContainer, UINT32 uid);

UINT32 OsMakeKgid(UserContainer *userContainer, UINT32 gid);

INT32 OsUserContainerMapWrite(struct ProcFile *fp, CHAR *buf, size_t count,
                              INT32 capSetid, UidGidMap *map, UidGidMap *parentMap);

UINT32 OsGetUserContainerCount(VOID);
#endif
#endif
