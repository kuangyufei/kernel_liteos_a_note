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

#ifdef LOSCFG_USER_CONTAINER
#include "los_user_container_pri.h"
#include "errno.h"
#include "ctype.h"
#include "los_config.h"
#include "los_memory.h"
#include "proc_fs.h"
#include "user_copy.h"
#include "los_seq_buf.h"
#include "capability_type.h"
#include "capability_api.h"
#include "internal.h"

#define DEFAULT_OVERFLOWUID    65534
#define DEFAULT_OVERFLOWGID    65534
#define LEVEL_MAX 3
#define HEX 16
#define OCT 8
#define DEC 10

UINT32 g_currentUserContainerNum = 0;

UINT32 OsCreateUserContainer(Credentials *newCredentials, UserContainer *parentUserContainer)
{
    if (g_currentUserContainerNum >= OsGetContainerLimit(USER_CONTAINER)) {
        return EPERM;
    }

    if ((parentUserContainer != NULL) && (parentUserContainer->level >= LEVEL_MAX)) {
        return EINVAL;
    }

    if ((newCredentials->euid < 0) || (newCredentials->egid < 0)) {
        return EINVAL;
    }

    UserContainer *userContainer = LOS_MemAlloc(m_aucSysMem1, sizeof(UserContainer));
    if (userContainer == NULL) {
        return ENOMEM;
    }
    (VOID)memset_s(userContainer, sizeof(UserContainer), 0, sizeof(UserContainer));

    g_currentUserContainerNum++;
    userContainer->containerID = OsAllocContainerID();
    userContainer->parent = parentUserContainer;
    newCredentials->userContainer = userContainer;
    if (parentUserContainer != NULL) {
        LOS_AtomicInc(&parentUserContainer->rc);
        LOS_AtomicSet(&userContainer->rc, 1);
        userContainer->level = parentUserContainer->level + 1;
        userContainer->owner = newCredentials->euid;
        userContainer->group = newCredentials->egid;
    } else {
        LOS_AtomicSet(&userContainer->rc, 3); /* 3: Three system processes */
        userContainer->uidMap.extentCount = 1;
        userContainer->uidMap.extent[0].count = 4294967295U;
        userContainer->gidMap.extentCount = 1;
        userContainer->gidMap.extent[0].count = 4294967295U;
    }
    return LOS_OK;
}

VOID FreeUserContainer(UserContainer *userContainer)
{
    UserContainer *parent = NULL;
    do {
        parent = userContainer->parent;
        (VOID)LOS_MemFree(m_aucSysMem1, userContainer);
        userContainer->parent = NULL;
        userContainer = parent;
        g_currentUserContainerNum--;
        if (userContainer == NULL) {
            break;
        }
        LOS_AtomicDec(&userContainer->rc);
    } while (LOS_AtomicRead(&userContainer->rc) <= 0);
}

STATIC UidGidExtent *MapIdUpBase(UINT32 extents, UidGidMap *map, UINT32 id)
{
    UINT32 idx;
    UINT32 first;
    UINT32 last;

    for (idx = 0; idx < extents; idx++) {
        first = map->extent[idx].lowerFirst;
        last = first + map->extent[idx].count - 1;
        if ((id >= first) && (id <= last)) {
            return &map->extent[idx];
        }
    }
    return NULL;
}

STATIC UINT32 MapIdUp(UidGidMap *map, UINT32 id)
{
    UINT32 extents = map->extentCount;
    if (extents > UID_GID_MAP_MAX_EXTENTS) {
        return (UINT32)-1;
    }

    UidGidExtent *extent = MapIdUpBase(extents, map, id);
    if (extent != NULL) {
        return ((id - extent->lowerFirst) + extent->first);
    }

    return (UINT32)-1;
}

UINT32 FromKuid(UserContainer *userContainer, UINT32 kuid)
{
    return MapIdUp(&userContainer->uidMap, kuid);
}

UINT32 FromKgid(UserContainer *userContainer, UINT32 kgid)
{
    return MapIdUp(&userContainer->gidMap, kgid);
}

UINT32 OsFromKuidMunged(UserContainer *userContainer, UINT32 kuid)
{
    UINT32 uid = FromKuid(userContainer, kuid);
    if (uid == (UINT32)-1) {
        uid = DEFAULT_OVERFLOWUID;
    }
    return uid;
}

UINT32 OsFromKgidMunged(UserContainer *userContainer, UINT32 kgid)
{
    UINT32 gid = FromKgid(userContainer, kgid);
    if (gid == (UINT32)-1) {
        gid = DEFAULT_OVERFLOWGID;
    }
    return gid;
}

STATIC UidGidExtent *MapIdRangeDownBase(UINT32 extents, UidGidMap *map, UINT32 id, UINT32 count)
{
    UINT32 idx;
    UINT32 first;
    UINT32 last;
    UINT32 id2;

    id2 = id + count - 1;
    for (idx = 0; idx < extents; idx++) {
        first = map->extent[idx].first;
        last = first + map->extent[idx].count - 1;
        if ((id >= first && id <= last) && (id2 >= first && id2 <= last)) {
            return &map->extent[idx];
        }
    }
    return NULL;
}

STATIC UINT32 MapIdRangeDown(UidGidMap *map, UINT32 id, UINT32 count)
{
    UINT32 extents = map->extentCount;
    if (extents > UID_GID_MAP_MAX_EXTENTS) {
        return (UINT32)-1;
    }

    UidGidExtent *extent = MapIdRangeDownBase(extents, map, id, count);
    if (extent != NULL) {
        return ((id - extent->first) + extent->lowerFirst);
    }

    return (UINT32)-1;
}

STATIC UINT32 MapIdDown(UidGidMap *map, UINT32 id)
{
    return MapIdRangeDown(map, id, 1);
}

UINT32 OsMakeKuid(UserContainer *userContainer, UINT32 uid)
{
    return MapIdDown(&userContainer->uidMap, uid);
}

UINT32 OsMakeKgid(UserContainer *userContainer, UINT32 gid)
{
    return MapIdDown(&userContainer->gidMap, gid);
}

STATIC INT32 InsertExtent(UidGidMap *idMap, UidGidExtent *extent)
{
    if (idMap->extentCount > UID_GID_MAP_MAX_EXTENTS) {
        return -EINVAL;
    }

    UidGidExtent *dest = &idMap->extent[idMap->extentCount];
    *dest = *extent;
    idMap->extentCount++;

    return 0;
}

STATIC BOOL MappingsOverlap(UidGidMap *idMap, UidGidExtent *extent)
{
    UINT32 upperFirst = extent->first;
    UINT32 lowerFirst = extent->lowerFirst;
    UINT32 upperLast = upperFirst + extent->count - 1;
    UINT32 lowerLast = lowerFirst + extent->count - 1;
    INT32 idx;

    for (idx = 0; idx < idMap->extentCount; idx++) {
        if (idMap->extentCount > UID_GID_MAP_MAX_EXTENTS) {
            return TRUE;
        }
        UidGidExtent *prev = &idMap->extent[idx];
        UINT32 prevUpperFirst = prev->first;
        UINT32 prevLowerFirst = prev->lowerFirst;
        UINT32 prevUpperLast = prevUpperFirst + prev->count - 1;
        UINT32 prevLowerLast = prevLowerFirst + prev->count - 1;

        if ((prevUpperFirst <= upperLast) && (prevUpperLast >= upperFirst)) {
            return TRUE;
        }

        if ((prevLowerFirst <= lowerLast) && (prevLowerLast >= lowerFirst)) {
            return TRUE;
        }
    }
    return FALSE;
}

STATIC CHAR *SkipSpaces(const CHAR *str)
{
    while (isspace(*str)) {
        ++str;
    }

    return (CHAR *)str;
}

STATIC UINTPTR StrToUInt(const CHAR *str, CHAR **endp, UINT32 base)
{
    unsigned long result = 0;
    unsigned long value;

    if (*str == '0') {
        str++;
        if ((*str == 'x') && isxdigit(str[1])) {
            base = HEX;
            str++;
        }
        if (!base) {
            base = OCT;
        }
    }
    if (!base) {
        base = DEC;
    }
    while (isxdigit(*str) && (value = isdigit(*str) ? *str - '0' : (islower(*str) ?
        toupper(*str) : *str) - 'A' + DEC) < base) {
        result = result * base + value;
        str++;
    }
    if (endp != NULL) {
        *endp = (CHAR *)str;
    }
    return result;
}

STATIC INT32 ParsePosData(CHAR *pos, UidGidExtent *extent)
{
    INT32 ret = -EINVAL;
    pos = SkipSpaces(pos);
    extent->first = StrToUInt(pos, &pos, DEC);
    if (!isspace(*pos)) {
        return ret;
    }

    pos = SkipSpaces(pos);
    extent->lowerFirst = StrToUInt(pos, &pos, DEC);
    if (!isspace(*pos)) {
        return ret;
    }

    pos = SkipSpaces(pos);
    extent->count = StrToUInt(pos, &pos, DEC);
    if (*pos && !isspace(*pos)) {
        return ret;
    }

    pos = SkipSpaces(pos);
    if (*pos != '\0') {
        return ret;
    }
    return LOS_OK;
}

STATIC INT32 ParseUserData(CHAR *kbuf, UidGidExtent *extent, UidGidMap *newMap)
{
    INT32 ret = -EINVAL;
    CHAR *pos = NULL;
    CHAR *nextLine = NULL;

    for (pos = kbuf; pos != NULL; pos = nextLine) {
        nextLine = strchr(pos, '\n');
        if (nextLine != NULL) {
            *nextLine = '\0';
            nextLine++;
            if (*nextLine == '\0') {
                nextLine = NULL;
            }
        }

        if (ParsePosData(pos, extent) != LOS_OK) {
            return ret;
        }

        if ((extent->first == (UINT32)-1) || (extent->lowerFirst == (UINT32)-1)) {
            return ret;
        }

        if ((extent->first + extent->count) <= extent->first) {
            return ret;
        }

        if ((extent->lowerFirst + extent->count) <= extent->lowerFirst) {
            return ret;
        }

        if (MappingsOverlap(newMap, extent)) {
            return ret;
        }

        if ((newMap->extentCount + 1) == UID_GID_MAP_MAX_EXTENTS && (nextLine != NULL)) {
            return ret;
        }

        ret = InsertExtent(newMap, extent);
        if (ret < 0) {
            return ret;
        }
        ret = 0;
    }

    if (newMap->extentCount == 0) {
        return -EINVAL;
    }

    return ret;
}

STATIC INT32 ParentMapIdRange(UidGidMap *newMap, UidGidMap *parentMap)
{
    UINT32 idx;
    INT32 ret = -EPERM;
    for (idx = 0; idx < newMap->extentCount; idx++) {
        if (newMap->extentCount > UID_GID_MAP_MAX_EXTENTS) {
            return ret;
        }

        UidGidExtent *extent = &newMap->extent[idx];
        UINT32 lowerFirst = MapIdRangeDown(parentMap, extent->lowerFirst, extent->count);
        if (lowerFirst == (UINT32) -1) {
            return ret;
        }

        extent->lowerFirst = lowerFirst;
    }
    return 0;
}

INT32 OsUserContainerMapWrite(struct ProcFile *fp, CHAR *kbuf, size_t count,
                              INT32 capSetid, UidGidMap *map, UidGidMap *parentMap)
{
    UidGidMap newMap = {0};
    UidGidExtent extent;
    INT32 ret;

    if (map->extentCount != 0) {
        return -EPERM;
    }

    if (!IsCapPermit(capSetid)) {
        return -EPERM;
    }

    ret = ParseUserData(kbuf, &extent, &newMap);
    if (ret < 0) {
        return -EPERM;
    }

    ret = ParentMapIdRange(&newMap, parentMap);
    if (ret < 0) {
        return -EPERM;
    }

    if (newMap.extentCount <= UID_GID_MAP_MAX_EXTENTS) {
        size_t mapSize = newMap.extentCount * sizeof(newMap.extent[0]);
        ret = memcpy_s(map->extent, sizeof(map->extent), newMap.extent, mapSize);
        if (ret != EOK) {
            return -EPERM;
        }
    }

    map->extentCount = newMap.extentCount;
    return count;
}

UINT32 OsGetUserContainerCount(VOID)
{
    return g_currentUserContainerNum;
}
#endif
