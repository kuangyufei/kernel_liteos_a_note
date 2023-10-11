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

#ifdef LOSCFG_KERNEL_DEV_PLIMIT
#include "los_seq_buf.h"
#include "los_bitmap.h"
#include "los_process_pri.h"
#include "los_devicelimit.h"

#define TYPE_CHAR_LEN            (1)
#define DEVICE_NAME_PREFIX_SPACE (1)
#define DEVICE_ACCESS_MAXLEN     (3)
#define BUF_SEPARATOR            (5)

STATIC ProcDevLimit *g_procDevLimit = NULL;

VOID OsDevLimitInit(UINTPTR limit)
{
    ProcDevLimit *deviceLimit = (ProcDevLimit *)limit;
    deviceLimit->behavior = DEVLIMIT_DEFAULT_ALLOW;
    LOS_ListInit(&(deviceLimit->accessList));
    g_procDevLimit = deviceLimit;
}

VOID *OsDevLimitAlloc(VOID)
{
    ProcDevLimit *plimit = (ProcDevLimit *)LOS_MemAlloc(m_aucSysMem1, sizeof(ProcDevLimit));
    if (plimit == NULL) {
        return NULL;
    }
    (VOID)memset_s(plimit, sizeof(ProcDevLimit), 0, sizeof(ProcDevLimit));
    LOS_ListInit(&(plimit->accessList));
    plimit->behavior = DEVLIMIT_DEFAULT_NONE;
    return (VOID *)plimit;
}

STATIC VOID DevAccessListDelete(ProcDevLimit *devLimit)
{
    DevAccessItem *delItem = NULL;
    DevAccessItem *tmpItem = NULL;
    LOS_DL_LIST_FOR_EACH_ENTRY_SAFE(delItem, tmpItem, &devLimit->accessList, DevAccessItem, list) {
        LOS_ListDelete(&delItem->list);
        (VOID)LOS_MemFree(m_aucSysMem1, (VOID *)delItem);
    }
}

VOID OsDevLimitFree(UINTPTR limit)
{
    ProcDevLimit *devLimit = (ProcDevLimit *)limit;
    if (devLimit == NULL) {
        return;
    }

    DevAccessListDelete(devLimit);
    (VOID)LOS_MemFree(m_aucSysMem1, devLimit);
}

STATIC UINT32 DevLimitCopyAccess(ProcDevLimit *devLimitDest, ProcDevLimit *devLimitSrc)
{
    DevAccessItem *tmpItem = NULL;
    INT32 itemSize = sizeof(DevAccessItem);
    devLimitDest->behavior = devLimitSrc->behavior;
    LOS_DL_LIST_FOR_EACH_ENTRY(tmpItem, &devLimitSrc->accessList, DevAccessItem, list) {
        DevAccessItem *newItem = (DevAccessItem *)LOS_MemAlloc(m_aucSysMem1, itemSize);
        if (newItem == NULL) {
            return ENOMEM;
        }
        (VOID)memcpy_s(newItem, sizeof(DevAccessItem), tmpItem, sizeof(DevAccessItem));
        LOS_ListTailInsert(&devLimitDest->accessList, &newItem->list);
    }
    return LOS_OK;
}

VOID OsDevLimitCopy(UINTPTR dest, UINTPTR src)
{
    ProcDevLimit *devLimitDest = (ProcDevLimit *)dest;
    ProcDevLimit *devLimitSrc = (ProcDevLimit *)src;
    (VOID)DevLimitCopyAccess(devLimitDest, devLimitSrc);
    devLimitDest->parent = (ProcDevLimit *)src;
}

STATIC INLINE INT32 IsSpace(INT32 c)
{
    return (c == ' ' || (unsigned)c - '\t' < BUF_SEPARATOR);
}

STATIC UINT32 ParseItemAccess(const CHAR *buf, DevAccessItem *item)
{
    switch (*buf) {
        case 'a':
            item->type = DEVLIMIT_DEV_ALL;
            return LOS_OK;
        case 'b':
            item->type = DEVLIMIT_DEV_BLOCK;
            break;
        case 'c':
            item->type = DEVLIMIT_DEV_CHAR;
            break;
        default:
            return EINVAL;
    }
    buf += DEVICE_NAME_PREFIX_SPACE;
    if (!IsSpace(*buf)) {
        return EINVAL;
    }
    buf += DEVICE_NAME_PREFIX_SPACE;

    for (INT32 count = 0; count < sizeof(item->name) - 1; count++) {
        if (IsSpace(*buf)) {
            break;
        }
        item->name[count] = *buf;
        buf += TYPE_CHAR_LEN;
    }
    if (!IsSpace(*buf)) {
        return EINVAL;
    }

    buf += DEVICE_NAME_PREFIX_SPACE;
    for (INT32 i = 0; i < DEVICE_ACCESS_MAXLEN; i++) {
        switch (*buf) {
            case 'r':
                item->access |= DEVLIMIT_ACC_READ;
                break;
            case 'w':
                item->access |= DEVLIMIT_ACC_WRITE;
                break;
            case 'm':
                item->access |= DEVLIMIT_ACC_MKNOD;
                break;
            case '\n':
            case '\0':
                i = DEVICE_ACCESS_MAXLEN;
                break;
            default:
                return EINVAL;
        }
        buf += TYPE_CHAR_LEN;
    }
    return LOS_OK;
}

STATIC BOOL DevLimitMayAllowAll(ProcDevLimit *parent)
{
    if (parent == NULL) {
        return TRUE;
    }
    return (parent->behavior == DEVLIMIT_DEFAULT_ALLOW);
}

STATIC BOOL DevLimitHasChildren(ProcLimitSet *procLimitSet, ProcDevLimit *devLimit)
{
    ProcLimitSet *parent = procLimitSet;
    ProcLimitSet *childProcLimitSet = NULL;
    if (devLimit == NULL) {
        return FALSE;
    }

    LOS_DL_LIST_FOR_EACH_ENTRY(childProcLimitSet, &(procLimitSet->childList), ProcLimitSet, childList) {
        if (childProcLimitSet == NULL) {
            continue;
        }
        if (childProcLimitSet->parent != parent) {
            continue;
        }
        if (!((childProcLimitSet->mask) & BIT(PROCESS_LIMITER_ID_DEV))) {
            continue;
        }
        return TRUE;
    }
    return FALSE;
}

STATIC UINT32 DealItemAllAccess(ProcLimitSet *procLimitSet, ProcDevLimit *devLimit,
                                ProcDevLimit *devParentLimit, INT32 filetype)
{
    switch (filetype) {
        case DEVLIMIT_ALLOW: {
            if (DevLimitHasChildren(procLimitSet, devLimit)) {
                return EINVAL;
            }
            if (!DevLimitMayAllowAll(devParentLimit)) {
                return EPERM;
            }
            DevAccessListDelete(devLimit);
            devLimit->behavior = DEVLIMIT_DEFAULT_ALLOW;
            if (devParentLimit == NULL) {
                break;
            }
            DevLimitCopyAccess(devLimit, devParentLimit);
            break;
        }
        case DEVLIMIT_DENY: {
            if (DevLimitHasChildren(procLimitSet, devLimit)) {
                return EINVAL;
            }
            DevAccessListDelete(devLimit);
            devLimit->behavior = DEVLIMIT_DEFAULT_DENY;
            break;
        }
        default:
            return EINVAL;
    }
    return LOS_OK;
}

STATIC BOOL DevLimitMatchItemPartial(LOS_DL_LIST *list, DevAccessItem *item)
{
    if ((list == NULL) || (item == NULL)) {
        return FALSE;
    }
    if (LOS_ListEmpty(list)) {
        return FALSE;
    }
    DevAccessItem *walk = NULL;
    LOS_DL_LIST_FOR_EACH_ENTRY(walk, list, DevAccessItem, list) {
        if (item->type != walk->type) {
            continue;
        }
        if ((strcmp(walk->name, "*") != 0) && (strcmp(item->name, "*") != 0)
        && (strcmp(walk->name, item->name) != 0)) {
            continue;
        }
        if (!(item->access & ~(walk->access))) {
            return TRUE;
        }
    }
    return FALSE;
}

STATIC BOOL DevLimitParentAllowsRmItem(ProcDevLimit *devParentLimit, DevAccessItem *item)
{
    if (devParentLimit == NULL) {
        return TRUE;
    }
     /* Make sure you're not removing part or a whole item existing in the parent plimits */
    return !DevLimitMatchItemPartial(&devParentLimit->accessList, item);
}

STATIC BOOL DevLimitMatchItem(LOS_DL_LIST *list, DevAccessItem *item)
{
    if ((list == NULL) || (item == NULL)) {
        return FALSE;
    }
    if (LOS_ListEmpty(list)) {
        return FALSE;
    }
    DevAccessItem *walk = NULL;
    LOS_DL_LIST_FOR_EACH_ENTRY(walk, list, DevAccessItem, list) {
        if (item->type != walk->type) {
            continue;
        }
        if ((strcmp(walk->name, "*") != 0) && (strcmp(walk->name, item->name) != 0)) {
            continue;
        }
        if (!(item->access & ~(walk->access))) {
            return TRUE;
        }
    }
    return FALSE;
}

/**
 * This is used to make sure a child plimits won't have more privileges than its parent
 */
STATIC BOOL DevLimitVerifyNewItem(ProcDevLimit *parent, DevAccessItem *item, INT32 currBehavior)
{
    if (parent == NULL) {
        return TRUE;
    }

    if (parent->behavior == DEVLIMIT_DEFAULT_ALLOW) {
        if (currBehavior == DEVLIMIT_DEFAULT_ALLOW) {
            return TRUE;
        }
        return !DevLimitMatchItemPartial(&parent->accessList, item);
    }
    return DevLimitMatchItem(&parent->accessList, item);
}

STATIC BOOL DevLimitParentAllowsAddItem(ProcDevLimit *devParentLimit, DevAccessItem *item, INT32 currBehavior)
{
    return DevLimitVerifyNewItem(devParentLimit, item, currBehavior);
}

STATIC VOID DevLimitAccessListRm(ProcDevLimit *devLimit, DevAccessItem *item)
{
    if ((item == NULL) || (devLimit == NULL)) {
        return;
    }
    DevAccessItem *walk, *tmp = NULL;
    LOS_DL_LIST_FOR_EACH_ENTRY_SAFE(walk, tmp, &devLimit->accessList, DevAccessItem, list) {
        if (walk->type != item->type) {
            continue;
        }
        if (strcmp(walk->name, item->name) != 0) {
            continue;
        }
        walk->access &= ~item->access;
        if (!walk->access) {
            LOS_ListDelete(&walk->list);
            (VOID)LOS_MemFree(m_aucSysMem1, (VOID *)walk);
        }
    }
}

STATIC UINT32 DevLimitAccessListAdd(ProcDevLimit *devLimit, DevAccessItem *item)
{
    if ((item == NULL) || (devLimit == NULL)) {
        return ENOMEM;
    }

    DevAccessItem *walk = NULL;
    DevAccessItem *newItem = (DevAccessItem *)LOS_MemAlloc(m_aucSysMem1, sizeof(DevAccessItem));
    if (newItem == NULL) {
        return ENOMEM;
    }
    (VOID)memcpy_s(newItem, sizeof(DevAccessItem), item, sizeof(DevAccessItem));
    LOS_DL_LIST_FOR_EACH_ENTRY(walk, &devLimit->accessList, DevAccessItem, list) {
        if (walk->type != item->type) {
            continue;
        }
        if (strcmp(walk->name, item->name) != 0) {
            continue;
        }
        walk->access |= item->access;
        (VOID)LOS_MemFree(m_aucSysMem1, (VOID *)newItem);
        newItem = NULL;
    }

    if (newItem != NULL) {
        LOS_ListTailInsert(&devLimit->accessList, &newItem->list);
    }
    return LOS_OK;
}

/**
 * Revalidate permissions
 */
STATIC VOID DevLimitRevalidateActiveItems(ProcDevLimit *devLimit, ProcDevLimit *devParentLimit)
{
    if ((devLimit == NULL) || (devParentLimit == NULL)) {
        return;
    }
    DevAccessItem *walK = NULL;
    DevAccessItem *tmp = NULL;
    LOS_DL_LIST_FOR_EACH_ENTRY_SAFE(walK, tmp, &devLimit->accessList, DevAccessItem, list) {
        if (!DevLimitParentAllowsAddItem(devParentLimit, walK, devLimit->behavior)) {
            DevLimitAccessListRm(devLimit, walK);
        }
    }
}

/**
 * propagates a new item to the children
 */
STATIC UINT32 DevLimitPropagateItem(ProcLimitSet *procLimitSet, ProcDevLimit *devLimit, DevAccessItem *item)
{
    UINT32 ret = LOS_OK;
    ProcLimitSet *parent = procLimitSet;
    ProcLimitSet *childProcLimitSet = NULL;

    if ((procLimitSet == NULL) || (item == NULL)) {
        return ENOMEM;
    }

    if (devLimit == NULL) {
        return LOS_OK;
    }

    LOS_DL_LIST_FOR_EACH_ENTRY(childProcLimitSet, &procLimitSet->childList, ProcLimitSet, childList) {
        if (childProcLimitSet == NULL) {
            continue;
        }
        if (childProcLimitSet->parent != parent) {
            continue;
        }
        if (!((childProcLimitSet->mask) & BIT(PROCESS_LIMITER_ID_DEV))) {
            continue;
        }
        ProcDevLimit *devLimitChild = (ProcDevLimit *)childProcLimitSet->limitsList[PROCESS_LIMITER_ID_DEV];
        if (devLimit->behavior == DEVLIMIT_DEFAULT_ALLOW &&
            devLimitChild->behavior == DEVLIMIT_DEFAULT_ALLOW) {
            ret = DevLimitAccessListAdd(devLimitChild, item);
        } else {
            DevLimitAccessListRm(devLimitChild, item);
        }
        DevLimitRevalidateActiveItems(devLimitChild, (ProcDevLimit *)parent->limitsList[PROCESS_LIMITER_ID_DEV]);
    }
    return ret;
}

STATIC UINT32 DevLimitUpdateAccess(ProcLimitSet *procLimitSet, const CHAR *buf, INT32 filetype)
{
    UINT32 ret;
    UINT32 intSave;
    DevAccessItem item = {0};

    SCHEDULER_LOCK(intSave);
    ProcDevLimit *devLimit = (ProcDevLimit *)(procLimitSet->limitsList[PROCESS_LIMITER_ID_DEV]);
    ProcDevLimit *devParentLimit = devLimit->parent;

    ret = ParseItemAccess(buf, &item);
    if (ret != LOS_OK) {
        SCHEDULER_UNLOCK(intSave);
        return ret;
    }
    if (item.type == DEVLIMIT_DEV_ALL) {
        ret = DealItemAllAccess(procLimitSet, devLimit, devParentLimit, filetype);
        SCHEDULER_UNLOCK(intSave);
        return ret;
    }
    switch (filetype) {
        case DEVLIMIT_ALLOW: {
            if (devLimit->behavior == DEVLIMIT_DEFAULT_ALLOW) {
                if (!DevLimitParentAllowsRmItem(devParentLimit, &item)) {
                    SCHEDULER_UNLOCK(intSave);
                    return EPERM;
                }
                DevLimitAccessListRm(devLimit, &item);
                break;
            }
            if (!DevLimitParentAllowsAddItem(devParentLimit, &item, devLimit->behavior)) {
                SCHEDULER_UNLOCK(intSave);
                return EPERM;
            }
            ret = DevLimitAccessListAdd(devLimit, &item);
            break;
        }
        case DEVLIMIT_DENY: {
            if (devLimit->behavior == DEVLIMIT_DEFAULT_DENY) {
                DevLimitAccessListRm(devLimit, &item);
            } else {
                ret = DevLimitAccessListAdd(devLimit, &item);
            }
            // update child access list
            ret = DevLimitPropagateItem(procLimitSet, devLimit, &item);
            break;
        }
        default:
            ret = EINVAL;
            break;
    }
    SCHEDULER_UNLOCK(intSave);
    return ret;
}


UINT32 OsDevLimitWriteAllow(ProcLimitSet *plimit, const CHAR *buf, UINT32 size)
{
    (VOID)size;
    return DevLimitUpdateAccess(plimit, buf, DEVLIMIT_ALLOW);
}

UINT32 OsDevLimitWriteDeny(ProcLimitSet *plimit, const CHAR *buf, UINT32 size)
{
    (VOID)size;
    return DevLimitUpdateAccess(plimit, buf, DEVLIMIT_DENY);
}

STATIC VOID DevLimitItemSetAccess(CHAR *accArray, INT16 access)
{
    INT32 index = 0;
    (VOID)memset_s(acc, ACCLEN, 0, ACCLEN);
    if (access & DEVLIMIT_ACC_READ) {
        accArray[index] = 'r';
        index++;
    }
    if (access & DEVLIMIT_ACC_WRITE) {
        accArray[index] = 'w';
        index++;
    }
    if (access & DEVLIMIT_ACC_MKNOD) {
        accArray[index] = 'm';
        index++;
    }
}

STATIC CHAR DevLimitItemTypeToChar(INT16 type)
{
    switch (type) {
        case DEVLIMIT_DEV_ALL:
            return 'a';
        case DEVLIMIT_DEV_CHAR:
            return 'c';
        case DEVLIMIT_DEV_BLOCK:
            return 'b';
        default:
            break;
    }
    return 'X';
}

UINT32 OsDevLimitShow(ProcDevLimit *devLimit, struct SeqBuf *seqBuf)
{
    DevAccessItem *item = NULL;
    CHAR acc[ACCLEN];
    UINT32 intSave;

    if ((devLimit == NULL) || (seqBuf == NULL)) {
        return EINVAL;
    }

    SCHEDULER_LOCK(intSave);
    if (devLimit->behavior == DEVLIMIT_DEFAULT_ALLOW) {
        DevLimitItemSetAccess(acc, DEVLIMIT_ACC_MASK);
        SCHEDULER_UNLOCK(intSave);
        LosBufPrintf(seqBuf, "%c %s %s\n", DevLimitItemTypeToChar(DEVLIMIT_DEV_ALL), "*", acc);
        return LOS_OK;
    }
    LOS_DL_LIST_FOR_EACH_ENTRY(item, &devLimit->accessList, DevAccessItem, list) {
        DevLimitItemSetAccess(acc, item->access);
        LosBufPrintf(seqBuf, "%c %s %s\n", DevLimitItemTypeToChar(item->type), item->name, acc);
    }
    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;
}

STATIC INLINE INT16 ConversionDevType(INT32 vnodeType)
{
    INT16 type = 0;
    if (vnodeType == VNODE_TYPE_BLK) {
        type = DEVLIMIT_DEV_BLOCK;
    } else if (vnodeType == VNODE_TYPE_CHR) {
        type = DEVLIMIT_DEV_CHAR;
    }
    return type;
}

STATIC INLINE INT16 ConversionDevAccess(INT32 flags)
{
    INT16 access = 0;
    if ((flags & O_ACCMODE) == O_RDONLY) {
        access |= DEVLIMIT_ACC_READ;
    }
    if (flags & O_WRONLY) {
        access |= DEVLIMIT_ACC_WRITE;
    }
    if (flags & O_RDWR) {
        access |= DEVLIMIT_ACC_WRITE | DEVLIMIT_ACC_READ;
    }
    if (flags & O_CREAT) {
        access |= DEVLIMIT_ACC_MKNOD;
    }
    return access;
}

UINT32 OsDevLimitCheckPermission(INT32 vnodeType, const CHAR *pathName, INT32 flags)
{
    BOOL matched = FALSE;
    DevAccessItem item = {0};
    LosProcessCB *run = OsCurrProcessGet();
    if ((run == NULL) || (run->plimits == NULL)) {
        return LOS_OK;
    }

    if (pathName == NULL) {
        return EINVAL;
    }

    ProcDevLimit *devLimit = (ProcDevLimit *)run->plimits->limitsList[PROCESS_LIMITER_ID_DEV];

    item.type = ConversionDevType(vnodeType);
    item.access = ConversionDevAccess(flags);
    LOS_ListInit(&(item.list));
    (VOID)strncpy_s(item.name, PATH_MAX, pathName, PATH_MAX);

    if (devLimit->behavior == DEVLIMIT_DEFAULT_ALLOW) {
        matched = !DevLimitMatchItemPartial(&devLimit->accessList, &item);
    } else {
        matched = DevLimitMatchItem(&devLimit->accessList, &item);
    }
    if (!matched) {
        return EPERM;
    }
    return LOS_OK;
}
#endif
