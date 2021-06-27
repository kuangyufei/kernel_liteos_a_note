/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd. All rights reserved.
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

#include "los_pm.h"
#include "securec.h"
#include "los_sched_pri.h"
#include "los_init.h"
#include "los_memory.h"
#include "los_spinlock.h"
#include "los_mp.h"

#ifdef LOSCFG_KERNEL_PM
#define PM_INFO_SHOW(seqBuf, arg...) do {                   \
    if (seqBuf != NULL) {                                   \
        (void)LosBufPrintf((struct SeqBuf *)seqBuf, ##arg); \
    } else {                                                \
        PRINTK(arg);                                        \
    }                                                       \
} while (0)

#define OS_PM_LOCK_MAX      0xFFFFU
#define OS_PM_LOCK_NAME_MAX 28

typedef UINT32 (*Suspend)(VOID);

typedef struct {
    CHAR         name[OS_PM_LOCK_NAME_MAX];
    UINT32       count;
    LOS_DL_LIST  list;
} OsPmLockCB;

typedef struct {
    LOS_SysSleepEnum  mode;
    UINT16            lock;
    LOS_DL_LIST       lockList;
} LosPmCB;

STATIC LosPmCB g_pmCB;
STATIC SPIN_LOCK_INIT(g_pmSpin);

LOS_SysSleepEnum LOS_PmModeGet(VOID)
{
    LOS_SysSleepEnum mode;
    LosPmCB *pm = &g_pmCB;

    LOS_SpinLock(&g_pmSpin);
    mode = pm->mode;
    LOS_SpinUnlock(&g_pmSpin);

    return mode;
}

UINT32 LOS_PmLockCountGet(VOID)
{
    UINT16 count;
    LosPmCB *pm = &g_pmCB;

    LOS_SpinLock(&g_pmSpin);
    count = pm->lock;
    LOS_SpinUnlock(&g_pmSpin);

    return (UINT32)count;
}

VOID LOS_PmLockInfoShow(struct SeqBuf *m)
{
    UINT32 intSave;
    LosPmCB *pm = &g_pmCB;
    OsPmLockCB *lock = NULL;
    LOS_DL_LIST *head = &pm->lockList;
    LOS_DL_LIST *list = head->pstNext;

    intSave = LOS_IntLock();
    while (list != head) {
        lock = LOS_DL_LIST_ENTRY(list, OsPmLockCB, list);
        PM_INFO_SHOW(m, "%-30s%5u\n\r", lock->name, lock->count);
        list = list->pstNext;
    }
    LOS_IntRestore(intSave);

    return;
}

UINT32 LOS_PmLockRequest(const CHAR *name)
{
    INT32 len;
    errno_t err;
    LosPmCB *pm = &g_pmCB;
    OsPmLockCB *listNode = NULL;
    OsPmLockCB *lock = NULL;
    LOS_DL_LIST *head = &pm->lockList;
    LOS_DL_LIST *list = head->pstNext;

    if (name == NULL) {
        return LOS_EINVAL;
    }

    if (OS_INT_ACTIVE) {
        return LOS_EINTR;
    }

    len = strlen(name);
    if (len == 0) {
        return LOS_EINVAL;
    }

    LOS_SpinLock(&g_pmSpin);
    if (pm->lock >= OS_PM_LOCK_MAX) {
        LOS_SpinUnlock(&g_pmSpin);
        return LOS_EINVAL;
    }

    while (list != head) {
        listNode = LOS_DL_LIST_ENTRY(list, OsPmLockCB, list);
        if (strcmp(name, listNode->name) == 0) {
            lock = listNode;
            break;
        }

        list = list->pstNext;
    }

    if (lock == NULL) {
        lock = LOS_MemAlloc((VOID *)OS_SYS_MEM_ADDR, sizeof(OsPmLockCB));
        if (lock == NULL) {
            LOS_SpinUnlock(&g_pmSpin);
            return LOS_ENOMEM;
        }

        err = memcpy_s(lock->name, OS_PM_LOCK_NAME_MAX, name, len + 1);
        if (err != EOK) {
            LOS_SpinUnlock(&g_pmSpin);
            (VOID)LOS_MemFree((VOID *)OS_SYS_MEM_ADDR, lock);
            return err;
        }

        lock->count = 1;
        LOS_ListTailInsert(head, &lock->list);
    } else if (lock->count < OS_PM_LOCK_MAX) {
        lock->count++;
    }

    pm->lock++;
    LOS_SpinUnlock(&g_pmSpin);
    return LOS_OK;
}

UINT32 LOS_PmLockRelease(const CHAR *name)
{
    LosPmCB *pm = &g_pmCB;
    OsPmLockCB *lock = NULL;
    OsPmLockCB *listNode = NULL;
    LOS_DL_LIST *head = &pm->lockList;
    LOS_DL_LIST *list = head->pstNext;
    VOID *lockFree = NULL;

    if (name == NULL) {
        return LOS_EINVAL;
    }

    if (OS_INT_ACTIVE) {
        return LOS_EINTR;
    }

    LOS_SpinLock(&g_pmSpin);
    if (pm->lock == 0) {
        LOS_SpinUnlock(&g_pmSpin);
        return LOS_EINVAL;
    }

    while (list != head) {
        listNode = LOS_DL_LIST_ENTRY(list, OsPmLockCB, list);
        if (strcmp(name, listNode->name) == 0) {
            lock = listNode;
            break;
        }

        list = list->pstNext;
    }

    if (lock == NULL) {
        LOS_SpinUnlock(&g_pmSpin);
        return LOS_EACCES;
    } else if (lock->count > 0) {
        lock->count--;
        if (lock->count == 0) {
            LOS_ListDelete(&lock->list);
            lockFree = lock;
        }
    }
    pm->lock--;
    LOS_SpinUnlock(&g_pmSpin);

    (VOID)LOS_MemFree((VOID *)OS_SYS_MEM_ADDR, lockFree);
    return LOS_OK;
}

UINT32 OsPmInit(VOID)
{
    LosPmCB *pm = &g_pmCB;

    (VOID)memset_s(pm, sizeof(LosPmCB), 0, sizeof(LosPmCB));

    pm->mode = LOS_SYS_NORMAL_SLEEP;
    LOS_ListInit(&pm->lockList);
    return LOS_OK;
}

LOS_MODULE_INIT(OsPmInit, LOS_INIT_LEVEL_KMOD_EXTENDED);
#endif
