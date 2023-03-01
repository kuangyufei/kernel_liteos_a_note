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

#ifdef LOSCFG_KERNEL_PLIMITS
#include "los_base.h"
#include "los_process_pri.h"
#include "hal_timer.h"
#include "los_plimits.h"

typedef struct PlimiteOperations {
    VOID (*LimiterInit)(UINTPTR);
    VOID *(*LimiterAlloc)(VOID);
    VOID (*LimiterFree)(UINTPTR);
    VOID (*LimiterCopy)(UINTPTR, UINTPTR);
    BOOL (*LimiterAddProcessCheck)(UINTPTR, UINTPTR);
    VOID (*LimiterAddProcess)(UINTPTR, UINTPTR);
    VOID (*LimiterDelProcess)(UINTPTR, UINTPTR);
    BOOL (*LimiterMigrateCheck)(UINTPTR, UINTPTR);
    VOID (*LimiterMigrate)(UINTPTR, UINTPTR, UINTPTR);
} PlimiteOperations;

static PlimiteOperations g_limiteOps[PROCESS_LIMITER_COUNT] = {
    [PROCESS_LIMITER_ID_PIDS] = {
        .LimiterInit = PidLimiterInit,
        .LimiterAlloc = PidLimiterAlloc,
        .LimiterFree = PidLimterFree,
        .LimiterCopy = PidLimiterCopy,
        .LimiterAddProcessCheck = OsPidLimitAddProcessCheck,
        .LimiterAddProcess = OsPidLimitAddProcess,
        .LimiterDelProcess = OsPidLimitDelProcess,
        .LimiterMigrateCheck = PidLimitMigrateCheck,
        .LimiterMigrate = NULL,
    },
#ifdef LOSCFG_KERNEL_MEM_PLIMIT
    [PROCESS_LIMITER_ID_MEM] = {
        .LimiterInit = OsMemLimiterInit,
        .LimiterAlloc = OsMemLimiterAlloc,
        .LimiterFree = OsMemLimiterFree,
        .LimiterCopy = OsMemLimiterCopy,
        .LimiterAddProcessCheck = OsMemLimitAddProcessCheck,
        .LimiterAddProcess = OsMemLimitAddProcess,
        .LimiterDelProcess = OsMemLimitDelProcess,
        .LimiterMigrateCheck = MemLimiteMigrateCheck,
        .LimiterMigrate = OsMemLimiterMigrate,
    },
#endif
#ifdef LOSCFG_KERNEL_SCHED_PLIMIT
    [PROCESS_LIMITER_ID_SCHED] = {
        .LimiterInit = OsSchedLimitInit,
        .LimiterAlloc = OsSchedLimitAlloc,
        .LimiterFree = OsSchedLimitFree,
        .LimiterCopy = OsSchedLimitCopy,
        .LimiterAddProcessCheck = NULL,
        .LimiterAddProcess = NULL,
        .LimiterDelProcess = NULL,
        .LimiterMigrateCheck = NULL,
        .LimiterMigrate = NULL,
    },
#endif
#ifdef LOSCFG_KERNEL_DEV_PLIMIT
    [PROCESS_LIMITER_ID_DEV] = {
        .LimiterInit = OsDevLimitInit,
        .LimiterAlloc = OsDevLimitAlloc,
        .LimiterFree = OsDevLimitFree,
        .LimiterCopy = OsDevLimitCopy,
        .LimiterAddProcessCheck = NULL,
        .LimiterAddProcess = NULL,
        .LimiterDelProcess = NULL,
        .LimiterMigrateCheck = NULL,
        .LimiterMigrate = NULL,
    },
#endif
#ifdef LOSCFG_KERNEL_IPC_PLIMIT
    [PROCESS_LIMITER_ID_IPC] = {
        .LimiterInit = OsIPCLimitInit,
        .LimiterAlloc = OsIPCLimitAlloc,
        .LimiterFree = OsIPCLimitFree,
        .LimiterCopy = OsIPCLimitCopy,
        .LimiterAddProcessCheck = OsIPCLimitAddProcessCheck,
        .LimiterAddProcess = OsIPCLimitAddProcess,
        .LimiterDelProcess = OsIPCLimitDelProcess,
        .LimiterMigrateCheck = OsIPCLimiteMigrateCheck,
        .LimiterMigrate = OsIPCLimitMigrate,
    },
#endif
};

STATIC ProcLimiterSet *g_rootPLimite = NULL;

ProcLimiterSet *OsRootPLimitsGet(VOID)
{
    return g_rootPLimite;
}

UINT32 OsProcLimiterSetInit(VOID)
{
    g_rootPLimite = (ProcLimiterSet *)LOS_MemAlloc(m_aucSysMem1, sizeof(ProcLimiterSet));
    if (g_rootPLimite == NULL) {
        return ENOMEM;
    }
    (VOID)memset_s(g_rootPLimite, sizeof(ProcLimiterSet), 0, sizeof(ProcLimiterSet));

    LOS_ListInit(&g_rootPLimite->childList);
    LOS_ListInit(&g_rootPLimite->processList);

    ProcLimiterSet *procLimiterSet = g_rootPLimite;
    for (INT32 plimiteID = 0; plimiteID < PROCESS_LIMITER_COUNT; ++plimiteID) {
        procLimiterSet->limitsList[plimiteID] = (UINTPTR)g_limiteOps[plimiteID].LimiterAlloc();
        if (procLimiterSet->limitsList[plimiteID] == (UINTPTR)NULL) {
            OsPLimitsFree(procLimiterSet);
            return ENOMEM;
        }

        if (g_limiteOps[plimiteID].LimiterInit != NULL) {
            g_limiteOps[plimiteID].LimiterInit(procLimiterSet->limitsList[plimiteID]);
        }
    }
    return LOS_OK;
}

STATIC VOID PLimitsDeleteProcess(LosProcessCB *processCB)
{
    if ((processCB == NULL) || (processCB->plimits == NULL)) {
        return;
    }

    ProcLimiterSet *plimits = processCB->plimits;
    for (UINT32 limitsID = 0; limitsID < PROCESS_LIMITER_COUNT; limitsID++) {
        if (g_limiteOps[limitsID].LimiterDelProcess == NULL) {
            continue;
        }
        g_limiteOps[limitsID].LimiterDelProcess(plimits->limitsList[limitsID], (UINTPTR)processCB);
    }
    plimits->pidCount--;
    LOS_ListDelete(&processCB->plimitsList);
    processCB->plimits = NULL;
    return;
}

STATIC UINT32 PLimitsAddProcess(ProcLimiterSet *plimits, LosProcessCB *processCB)
{
    UINT32 limitsID;
    if (plimits == NULL) {
        plimits = g_rootPLimite;
    }

    if (processCB->plimits == g_rootPLimite) {
        return EPERM;
    }

    if (processCB->plimits == plimits) {
        return LOS_OK;
    }

    for (limitsID = 0; limitsID < PROCESS_LIMITER_COUNT; limitsID++) {
        if (g_limiteOps[limitsID].LimiterAddProcessCheck == NULL) {
            continue;
        }

        if (!g_limiteOps[limitsID].LimiterAddProcessCheck(plimits->limitsList[limitsID], (UINTPTR)processCB)) {
            return EACCES;
        }
    }

    PLimitsDeleteProcess(processCB);

    for (limitsID = 0; limitsID < PROCESS_LIMITER_COUNT; limitsID++) {
        if (g_limiteOps[limitsID].LimiterAddProcess == NULL) {
            continue;
        }
        g_limiteOps[limitsID].LimiterAddProcess(plimits->limitsList[limitsID], (UINTPTR)processCB);
    }

    LOS_ListTailInsert(&plimits->processList, &processCB->plimitsList);
    plimits->pidCount++;
    processCB->plimits = plimits;
    return LOS_OK;
}

UINT32 OsPLimitsAddProcess(ProcLimiterSet *plimits, LosProcessCB *processCB)
{
    UINT32 intSave;
    SCHEDULER_LOCK(intSave);
    UINT32 ret = PLimitsAddProcess(plimits, processCB);
    SCHEDULER_UNLOCK(intSave);
    return ret;
}

UINT32 OsPLimitsAddPid(ProcLimiterSet *plimits, UINT32 pid)
{
    UINT32 intSave, ret;
    if ((plimits == NULL) || OS_PID_CHECK_INVALID(pid) || (pid == 0)) {
        return EINVAL;
    }

    if (plimits == g_rootPLimite) {
        return EPERM;
    }

    SCHEDULER_LOCK(intSave);
    LosProcessCB *processCB = OS_PCB_FROM_PID((unsigned int)pid);
    if (OsProcessIsInactive(processCB)) {
        SCHEDULER_UNLOCK(intSave);
        return EINVAL;
    }

    ret = PLimitsAddProcess(plimits, processCB);
    SCHEDULER_UNLOCK(intSave);
    return ret;
}

VOID OsPLimitsDeleteProcess(LosProcessCB *processCB)
{
    UINT32 intSave;
    SCHEDULER_LOCK(intSave);
    PLimitsDeleteProcess(processCB);
    SCHEDULER_UNLOCK(intSave);
}

UINT32 OsPLimitsPidsGet(const ProcLimiterSet *plimits, UINT32 *pids, UINT32 size)
{
    UINT32 intSave;
    LosProcessCB *processCB = NULL;
    UINT32 minSize = LOS_GetSystemProcessMaximum() * sizeof(UINT32);
    if ((plimits == NULL) || (pids == NULL) || (size < minSize)) {
        return EINVAL;
    }

    SCHEDULER_LOCK(intSave);
    LOS_DL_LIST *listHead = (LOS_DL_LIST *)&plimits->processList;
    if (LOS_ListEmpty(listHead)) {
        SCHEDULER_UNLOCK(intSave);
        return EINVAL;
    }

    LOS_DL_LIST_FOR_EACH_ENTRY(processCB, listHead, LosProcessCB, plimitsList) {
        pids[OsGetPid(processCB)] = 1;
    }
    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;
}

STATIC VOID PLimitsProcessMerge(ProcLimiterSet *currPLimits, ProcLimiterSet *parentPLimits)
{
    LOS_DL_LIST *head = &currPLimits->processList;
    while (!LOS_ListEmpty(head)) {
        LosProcessCB *processCB = LOS_DL_LIST_ENTRY(head->pstNext, LosProcessCB, plimitsList);
        PLimitsDeleteProcess(processCB);
        PLimitsAddProcess(parentPLimits, processCB);
    }
    LOS_ListDelete(&currPLimits->childList);
    currPLimits->parent = NULL;
    return;
}

STATIC UINT32 PLimitsMigrateCheck(ProcLimiterSet *currPLimits, ProcLimiterSet *parentPLimits)
{
    for (UINT32 limiteID = 0; limiteID < PROCESS_LIMITER_COUNT; limiteID++) {
        UINTPTR currLimit = currPLimits->limitsList[limiteID];
        UINTPTR parentLimit = parentPLimits->limitsList[limiteID];
        if (g_limiteOps[limiteID].LimiterMigrateCheck == NULL) {
            continue;
        }

        if (!g_limiteOps[limiteID].LimiterMigrateCheck(currLimit, parentLimit)) {
            return EPERM;
        }
    }
    return LOS_OK;
}

UINT32 OsPLimitsFree(ProcLimiterSet *currPLimits)
{
    UINT32 intSave, ret;
    if (currPLimits == NULL) {
        return EINVAL;
    }

    SCHEDULER_LOCK(intSave);
    ProcLimiterSet *parentPLimits = currPLimits->parent;
    ret = PLimitsMigrateCheck(currPLimits, parentPLimits);
    if (ret != LOS_OK) {
        SCHEDULER_UNLOCK(intSave);
        return ret;
    }

    PLimitsProcessMerge(currPLimits, parentPLimits);
    SCHEDULER_UNLOCK(intSave);

    for (INT32 limiteID = 0; limiteID < PROCESS_LIMITER_COUNT; ++limiteID) {
        UINTPTR procLimiter = currPLimits->limitsList[limiteID];
        if (g_limiteOps[limiteID].LimiterFree != NULL) {
            g_limiteOps[limiteID].LimiterFree(procLimiter);
        }
    }
    (VOID)LOS_MemFree(m_aucSysMem1, currPLimits);
    return LOS_OK;
}

ProcLimiterSet *OsPLimitsCreate(ProcLimiterSet *parentPLimits)
{
    UINT32 intSave;

    ProcLimiterSet *newPLimits = (ProcLimiterSet *)LOS_MemAlloc(m_aucSysMem1, sizeof(ProcLimiterSet));
    if (newPLimits == NULL) {
        return NULL;
    }
    (VOID)memset_s(newPLimits, sizeof(ProcLimiterSet), 0, sizeof(ProcLimiterSet));
    LOS_ListInit(&newPLimits->childList);
    LOS_ListInit(&newPLimits->processList);

    SCHEDULER_LOCK(intSave);
    newPLimits->parent = parentPLimits;
    newPLimits->level = parentPLimits->level + 1;
    newPLimits->mask = parentPLimits->mask;

    for (INT32 plimiteID = 0; plimiteID < PROCESS_LIMITER_COUNT; ++plimiteID) {
        newPLimits->limitsList[plimiteID] = (UINTPTR)g_limiteOps[plimiteID].LimiterAlloc();
        if (newPLimits->limitsList[plimiteID] == (UINTPTR)NULL) {
            SCHEDULER_UNLOCK(intSave);
            OsPLimitsFree(newPLimits);
            return NULL;
        }

        if (g_limiteOps[plimiteID].LimiterCopy != NULL) {
            g_limiteOps[plimiteID].LimiterCopy(newPLimits->limitsList[plimiteID],
                                               parentPLimits->limitsList[plimiteID]);
        }
    }
    LOS_ListTailInsert(&g_rootPLimite->childList, &newPLimits->childList);

    (VOID)PLimitsDeleteProcess(OsCurrProcessGet());
    (VOID)PLimitsAddProcess(newPLimits, OsCurrProcessGet());
    SCHEDULER_UNLOCK(intSave);
    return newPLimits;
}

#ifdef LOSCFG_KERNEL_MEM_PLIMIT
UINT32 OsPLimitsMemUsageGet(ProcLimiterSet *plimits, UINT64 *usage, UINT32 size)
{
    UINT32 intSave;
    LosProcessCB *processCB = NULL;
    UINT32 minSize = LOS_GetSystemProcessMaximum() * sizeof(UINT64) + sizeof(ProcMemLimiter);
    ProcMemLimiter *memLimit = (ProcMemLimiter *)usage;
    UINT64 *memSuage = (UINT64 *)((UINTPTR)usage + sizeof(ProcMemLimiter));
    if ((plimits == NULL) || (usage == NULL) || (size < minSize)) {
        return EINVAL;
    }

    SCHEDULER_LOCK(intSave);
    (VOID)memcpy_s(memLimit, sizeof(ProcMemLimiter),
                   (VOID *)plimits->limitsList[PROCESS_LIMITER_ID_MEM], sizeof(ProcMemLimiter));
    LOS_DL_LIST *listHead = (LOS_DL_LIST *)&plimits->processList;
    if (LOS_ListEmpty(listHead)) {
        SCHEDULER_UNLOCK(intSave);
        return EINVAL;
    }

    LOS_DL_LIST_FOR_EACH_ENTRY(processCB, listHead, LosProcessCB, plimitsList) {
        memSuage[OsGetPid(processCB)] = processCB->limitStat.memUsed;
    }
    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;
}
#endif

#ifdef LOSCFG_KERNEL_IPC_PLIMIT
UINT32 OsPLimitsIPCStatGet(ProcLimiterSet *plimits, ProcIPCLimit *ipc, UINT32 size)
{
    UINT32 intSave;
    if ((plimits == NULL) || (ipc == NULL) || (size != sizeof(ProcIPCLimit))) {
        return EINVAL;
    }

    SCHEDULER_LOCK(intSave);
    (VOID)memcpy_s(ipc, sizeof(ProcIPCLimit),
                   (VOID *)plimits->limitsList[PROCESS_LIMITER_ID_IPC], sizeof(ProcIPCLimit));
    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;
}
#endif

#ifdef LOSCFG_KERNEL_SCHED_PLIMIT
UINT32 OsPLimitsSchedUsageGet(ProcLimiterSet *plimits, UINT64 *usage, UINT32 size)
{
    UINT32 intSave;
    LosProcessCB *processCB = NULL;
    UINT32 minSize = LOS_GetSystemProcessMaximum() * sizeof(UINT64);
    if ((plimits == NULL) || (usage == NULL) || (size < minSize)) {
        return EINVAL;
    }

    SCHEDULER_LOCK(intSave);
    LOS_DL_LIST *listHead = (LOS_DL_LIST *)&plimits->processList;
    if (LOS_ListEmpty(listHead)) {
        SCHEDULER_UNLOCK(intSave);
        return EINVAL;
    }

    LOS_DL_LIST_FOR_EACH_ENTRY(processCB, listHead, LosProcessCB, plimitsList) {
        usage[OsGetPid(processCB)] = processCB->limitStat.allRuntime;
    }
    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;
}
#endif
#endif
