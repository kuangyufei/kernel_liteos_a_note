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

#ifdef LOSCFG_KERNEL_MEM_PLIMIT
#include <stdlib.h>
#include <securec.h>
#include "los_config.h"
#include "los_hook.h"
#include "los_process_pri.h"
#include "los_plimits.h"

STATIC ProcMemLimiter *g_procMemLimiter = NULL;

VOID OsMemLimiterInit(UINTPTR limite)
{
    ProcMemLimiter *procMemLimiter = (ProcMemLimiter *)limite;
    procMemLimiter->limit = OS_NULL_INT;
    g_procMemLimiter = procMemLimiter;
}

VOID OsMemLimitSetLimit(UINT32 limit)
{
    g_procMemLimiter->limit = limit;
}

VOID *OsMemLimiterAlloc(VOID)
{
    ProcMemLimiter *plimite = (ProcMemLimiter *)LOS_MemAlloc(m_aucSysMem1, sizeof(ProcMemLimiter));
    if (plimite == NULL) {
        return NULL;
    }
    (VOID)memset_s(plimite, sizeof(ProcMemLimiter), 0, sizeof(ProcMemLimiter));
    return (VOID *)plimite;
}

VOID OsMemLimiterFree(UINTPTR limite)
{
    ProcMemLimiter *plimite = (ProcMemLimiter *)limite;
    if (plimite == NULL) {
        return;
    }

    (VOID)LOS_MemFree(m_aucSysMem1, (VOID *)limite);
}

VOID OsMemLimiterCopy(UINTPTR dest, UINTPTR src)
{
    ProcMemLimiter *plimiteDest = (ProcMemLimiter *)dest;
    ProcMemLimiter *plimiteSrc = (ProcMemLimiter *)src;
    plimiteDest->limit = plimiteSrc->limit;
    return;
}

BOOL MemLimiteMigrateCheck(UINTPTR curr, UINTPTR parent)
{
    ProcMemLimiter *currMemLimit = (ProcMemLimiter *)curr;
    ProcMemLimiter *parentMemLimit = (ProcMemLimiter *)parent;
    if ((currMemLimit->usage + parentMemLimit->usage) >= parentMemLimit->limit) {
        return FALSE;
    }
    return TRUE;
}

VOID OsMemLimiterMigrate(UINTPTR currLimit, UINTPTR parentLimit, UINTPTR process)
{
    ProcMemLimiter *currMemLimit = (ProcMemLimiter *)currLimit;
    ProcMemLimiter *parentMemLimit = (ProcMemLimiter *)parentLimit;
    LosProcessCB *pcb = (LosProcessCB *)process;

    if (pcb == NULL) {
        parentMemLimit->usage += currMemLimit->usage;
        parentMemLimit->failcnt += currMemLimit->failcnt;
        if (parentMemLimit->peak < parentMemLimit->usage) {
            parentMemLimit->peak = parentMemLimit->usage;
        }
        return;
    }

    parentMemLimit->usage -= pcb->limitStat.memUsed;
    currMemLimit->usage += pcb->limitStat.memUsed;
}

BOOL OsMemLimitAddProcessCheck(UINTPTR limit, UINTPTR process)
{
    ProcMemLimiter *memLimit = (ProcMemLimiter *)limit;
    LosProcessCB *pcb = (LosProcessCB *)process;
    if ((memLimit->usage + pcb->limitStat.memUsed) > memLimit->limit) {
        return FALSE;
    }
    return TRUE;
}

VOID OsMemLimitAddProcess(UINTPTR limit, UINTPTR process)
{
    LosProcessCB *pcb = (LosProcessCB *)process;
    ProcMemLimiter *plimits = (ProcMemLimiter *)limit;
    plimits->usage += pcb->limitStat.memUsed;
    if (plimits->peak < plimits->usage) {
        plimits->peak = plimits->usage;
    }
    return;
}

VOID OsMemLimitDelProcess(UINTPTR limit, UINTPTR process)
{
    LosProcessCB *pcb = (LosProcessCB *)process;
    ProcMemLimiter *plimits = (ProcMemLimiter *)limit;

    plimits->usage -= pcb->limitStat.memUsed;
    return;
}

UINT32 OsMemLimitSetMemLimit(ProcMemLimiter *memLimit, UINT64 value)
{
    UINT32 intSave;
    if ((memLimit == NULL) || (value == 0)) {
        return EINVAL;
    }

    if (memLimit == g_procMemLimiter) {
        return EPERM;
    }

    SCHEDULER_LOCK(intSave);
    if (value < memLimit->usage) {
        SCHEDULER_UNLOCK(intSave);
        return EINVAL;
    }

    memLimit->limit = value;
    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;
}

#define MEM_LIMIT_LOCK(state, locked) do { \
    if (SCHEDULER_HELD()) {                \
        locked = TRUE;                     \
    } else {                               \
        SCHEDULER_LOCK(state);             \
    }                                      \
} while (0)

#define MEM_LIMIT_UNLOCK(state, locked) do { \
    if (!locked) {                           \
        SCHEDULER_UNLOCK(state);             \
    }                                        \
} while (0)

UINT32 OsMemLimitCheckAndMemAdd(UINT32 size)
{
    UINT32 intSave;
    BOOL locked = FALSE;
    MEM_LIMIT_LOCK(intSave, locked);
    LosProcessCB *run = OsCurrProcessGet();
    UINT32 currProcessID = run->processID;
    if ((run == NULL) || (run->plimits == NULL)) {
        MEM_LIMIT_UNLOCK(intSave, locked);
        return LOS_OK;
    }

    ProcMemLimiter *memLimit = (ProcMemLimiter *)run->plimits->limitsList[PROCESS_LIMITER_ID_MEM];
    if ((memLimit->usage + size) > memLimit->limit) {
        memLimit->failcnt++;
        MEM_LIMIT_UNLOCK(intSave, locked);
        PRINT_ERR("plimits: process %u adjust the memory limit of Plimits group\n", currProcessID);
        return ENOMEM;
    }

    memLimit->usage += size;
    run->limitStat.memUsed += size;
    if (memLimit->peak < memLimit->usage) {
        memLimit->peak = memLimit->usage;
    }
    MEM_LIMIT_UNLOCK(intSave, locked);
    return LOS_OK;
}

VOID OsMemLimitMemFree(UINT32 size)
{
    UINT32 intSave;
    BOOL locked = FALSE;
    MEM_LIMIT_LOCK(intSave, locked);
    LosProcessCB *run = OsCurrProcessGet();
    if ((run == NULL) || (run->plimits == NULL)) {
        MEM_LIMIT_UNLOCK(intSave, locked);
        return;
    }

    ProcMemLimiter *memLimit = (ProcMemLimiter *)run->plimits->limitsList[PROCESS_LIMITER_ID_MEM];
    if (run->limitStat.memUsed > size) {
        run->limitStat.memUsed -= size;
        memLimit->usage -= size;
    }
    MEM_LIMIT_UNLOCK(intSave, locked);
}
#endif
