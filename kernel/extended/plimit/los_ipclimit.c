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

#ifdef LOSCFG_KERNEL_IPC_PLIMIT
#include "los_ipclimit.h"
#include "los_process_pri.h"

STATIC ProcIPCLimit *g_rootIPCLimit = NULL;
#define PLIMIT_IPC_SHM_LIMIT_MAX 0xFFFFFFFF

VOID OsIPCLimitInit(UINTPTR limite)
{
    ProcIPCLimit *plimite = (ProcIPCLimit *)limite;
    plimite->mqCountLimit = LOSCFG_BASE_IPC_QUEUE_LIMIT;
    plimite->shmSizeLimit = PLIMIT_IPC_SHM_LIMIT_MAX;
    g_rootIPCLimit = plimite;
}

VOID *OsIPCLimitAlloc(VOID)
{
    ProcIPCLimit *plimite = (ProcIPCLimit *)LOS_MemAlloc(m_aucSysMem1, sizeof(ProcIPCLimit));
    if (plimite == NULL) {
        return NULL;
    }
    (VOID)memset_s(plimite, sizeof(ProcIPCLimit), 0, sizeof(ProcIPCLimit));
    return (VOID *)plimite;
}

VOID OsIPCLimitFree(UINTPTR limite)
{
    ProcIPCLimit *plimite = (ProcIPCLimit *)limite;
    if (plimite == NULL) {
        return;
    }

    (VOID)LOS_MemFree(m_aucSysMem1, (VOID *)plimite);
}

VOID OsIPCLimitCopy(UINTPTR dest, UINTPTR src)
{
    ProcIPCLimit *plimiteDest = (ProcIPCLimit *)dest;
    ProcIPCLimit *plimiteSrc = (ProcIPCLimit *)src;
    plimiteDest->mqCountLimit = plimiteSrc->mqCountLimit;
    plimiteDest->shmSizeLimit = plimiteSrc->shmSizeLimit;
    return;
}

BOOL OsIPCLimiteMigrateCheck(UINTPTR curr, UINTPTR parent)
{
    ProcIPCLimit *currIpcLimit = (ProcIPCLimit *)curr;
    ProcIPCLimit *parentIpcLimit = (ProcIPCLimit *)parent;
    if ((currIpcLimit->mqCount + parentIpcLimit->mqCount) >= parentIpcLimit->mqCountLimit) {
        return FALSE;
    }

    if ((currIpcLimit->shmSize + parentIpcLimit->shmSize) >= parentIpcLimit->shmSizeLimit) {
        return FALSE;
    }
    return TRUE;
}

VOID OsIPCLimitMigrate(UINTPTR currLimit, UINTPTR parentLimit, UINTPTR process)
{
    ProcIPCLimit *currIpcLimit = (ProcIPCLimit *)currLimit;
    ProcIPCLimit *parentIpcLimit = (ProcIPCLimit *)parentLimit;
    LosProcessCB *pcb = (LosProcessCB *)process;

    if (pcb == NULL) {
        parentIpcLimit->mqCount += currIpcLimit->mqCount;
        parentIpcLimit->mqFailedCount += currIpcLimit->mqFailedCount;
        parentIpcLimit->shmSize += currIpcLimit->shmSize;
        parentIpcLimit->shmFailedCount += currIpcLimit->shmFailedCount;
        return;
    }

    parentIpcLimit->mqCount -= pcb->limitStat.mqCount;
    parentIpcLimit->shmSize -= pcb->limitStat.shmSize;
    currIpcLimit->mqCount += pcb->limitStat.mqCount;
    currIpcLimit->shmSize += pcb->limitStat.shmSize;
}

BOOL OsIPCLimitAddProcessCheck(UINTPTR limit, UINTPTR process)
{
    ProcIPCLimit *ipcLimit = (ProcIPCLimit *)limit;
    LosProcessCB *pcb = (LosProcessCB *)process;
    if ((ipcLimit->mqCount + pcb->limitStat.mqCount) >= ipcLimit->mqCountLimit) {
        return FALSE;
    }

    if ((ipcLimit->shmSize + pcb->limitStat.shmSize) >= ipcLimit->shmSizeLimit) {
        return FALSE;
    }

    return TRUE;
}

VOID OsIPCLimitAddProcess(UINTPTR limit, UINTPTR process)
{
    LosProcessCB *pcb = (LosProcessCB *)process;
    ProcIPCLimit *plimits = (ProcIPCLimit *)limit;
    plimits->mqCount += pcb->limitStat.mqCount;
    plimits->shmSize += pcb->limitStat.shmSize;
    return;
}

VOID OsIPCLimitDelProcess(UINTPTR limit, UINTPTR process)
{
    LosProcessCB *pcb = (LosProcessCB *)process;
    ProcIPCLimit *plimits = (ProcIPCLimit *)limit;

    plimits->mqCount -= pcb->limitStat.mqCount;
    plimits->shmSize -= pcb->limitStat.shmSize;
    return;
}

UINT32 OsIPCLimitSetMqLimit(ProcIPCLimit *ipcLimit, UINT32 value)
{
    UINT32 intSave;

    if ((ipcLimit == NULL) || (value == 0) || (value > LOSCFG_BASE_IPC_QUEUE_LIMIT)) {
        return EINVAL;
    }

    if (ipcLimit == g_rootIPCLimit) {
        return EPERM;
    }

    SCHEDULER_LOCK(intSave);
    if (value < ipcLimit->mqCount) {
        SCHEDULER_UNLOCK(intSave);
        return EINVAL;
    }

    ipcLimit->mqCountLimit = value;
    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;
}

UINT32 OsIPCLimitSetShmLimit(ProcIPCLimit *ipcLimit, UINT32 value)
{
    UINT32 intSave;

    if ((ipcLimit == NULL) || (value == 0) || (value > PLIMIT_IPC_SHM_LIMIT_MAX)) {
        return EINVAL;
    }

    if (ipcLimit == g_rootIPCLimit) {
        return EPERM;
    }

    SCHEDULER_LOCK(intSave);
    if (value < ipcLimit->shmSize) {
        SCHEDULER_UNLOCK(intSave);
        return EINVAL;
    }

    ipcLimit->shmSizeLimit = value;
    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;
}

UINT32 OsIPCLimitMqAlloc(VOID)
{
    UINT32 intSave;
    SCHEDULER_LOCK(intSave);
    LosProcessCB *run = OsCurrProcessGet();
    ProcIPCLimit *ipcLimit = (ProcIPCLimit *)run->plimits->limitsList[PROCESS_LIMITER_ID_IPC];
    if (ipcLimit->mqCount >= ipcLimit->mqCountLimit) {
        ipcLimit->mqFailedCount++;
        SCHEDULER_UNLOCK(intSave);
        return EINVAL;
    }

    run->limitStat.mqCount++;
    ipcLimit->mqCount++;
    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;
}

VOID OsIPCLimitMqFree(VOID)
{
    UINT32 intSave;
    SCHEDULER_LOCK(intSave);
    LosProcessCB *run = OsCurrProcessGet();
    ProcIPCLimit *ipcLimit = (ProcIPCLimit *)run->plimits->limitsList[PROCESS_LIMITER_ID_IPC];
    ipcLimit->mqCount--;
    run->limitStat.mqCount--;
    SCHEDULER_UNLOCK(intSave);
    return;
}

UINT32 OsIPCLimitShmAlloc(UINT32 size)
{
    UINT32 intSave;
    SCHEDULER_LOCK(intSave);
    LosProcessCB *run = OsCurrProcessGet();
    ProcIPCLimit *ipcLimit = (ProcIPCLimit *)run->plimits->limitsList[PROCESS_LIMITER_ID_IPC];
    if ((ipcLimit->shmSize + size) >= ipcLimit->shmSizeLimit) {
        ipcLimit->shmFailedCount++;
        SCHEDULER_UNLOCK(intSave);
        return EINVAL;
    }

    run->limitStat.shmSize += size;
    ipcLimit->shmSize += size;
    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;
}

VOID OsIPCLimitShmFree(UINT32 size)
{
    UINT32 intSave;
    SCHEDULER_LOCK(intSave);
    LosProcessCB *run = OsCurrProcessGet();
    ProcIPCLimit *ipcLimit = (ProcIPCLimit *)run->plimits->limitsList[PROCESS_LIMITER_ID_IPC];
    ipcLimit->shmSize -= size;
    run->limitStat.shmSize -= size;
    SCHEDULER_UNLOCK(intSave);
    return;
}

#endif
