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
#include "los_config.h"
#include "los_process_pri.h"
#include "los_process.h"
#include "los_plimits.h"
#include "los_task_pri.h"
#include "los_processlimit.h"

STATIC PidLimit *g_rootPidLimit = NULL;

VOID PidLimiterInit(UINTPTR limit)
{
    PidLimit *pidLimit = (PidLimit *)limit;
    if (pidLimit == NULL) {
        return;
    }

    pidLimit->pidLimit = LOS_GetSystemProcessMaximum();
    pidLimit->priorityLimit = OS_USER_PROCESS_PRIORITY_HIGHEST;
    g_rootPidLimit = pidLimit;
}

VOID *PidLimiterAlloc(VOID)
{
    PidLimit *plimite = (PidLimit *)LOS_MemAlloc(m_aucSysMem1, sizeof(PidLimit));
    if (plimite == NULL) {
        return NULL;
    }
    (VOID)memset_s(plimite, sizeof(PidLimit), 0, sizeof(PidLimit));
    return (VOID *)plimite;
}

VOID PidLimterFree(UINTPTR limit)
{
    PidLimit *pidLimit = (PidLimit *)limit;
    if (pidLimit == NULL) {
        return;
    }

    (VOID)LOS_MemFree(m_aucSysMem1, (VOID *)limit);
}

BOOL PidLimitMigrateCheck(UINTPTR curr, UINTPTR parent)
{
    PidLimit *currPidLimit = (PidLimit *)curr;
    PidLimit *parentPidLimit = (PidLimit *)parent;
    if ((currPidLimit == NULL) || (parentPidLimit == NULL)) {
        return FALSE;
    }

    if ((currPidLimit->pidCount + parentPidLimit->pidCount) >= parentPidLimit->pidLimit) {
        return FALSE;
    }
    return TRUE;
}

BOOL OsPidLimitAddProcessCheck(UINTPTR limit, UINTPTR process)
{
    (VOID)process;
    PidLimit *pidLimit = (PidLimit *)limit;
    if (pidLimit->pidCount >= pidLimit->pidLimit) {
        return FALSE;
    }
    return TRUE;
}

VOID OsPidLimitAddProcess(UINTPTR limit, UINTPTR process)
{
    (VOID)process;
    PidLimit *plimits = (PidLimit *)limit;

    plimits->pidCount++;
    return;
}

VOID OsPidLimitDelProcess(UINTPTR limit, UINTPTR process)
{
    (VOID)process;
    PidLimit *plimits = (PidLimit *)limit;

    plimits->pidCount--;
    return;
}

VOID PidLimiterCopy(UINTPTR curr, UINTPTR parent)
{
    PidLimit *currPidLimit = (PidLimit *)curr;
    PidLimit *parentPidLimit = (PidLimit *)parent;
    if ((currPidLimit == NULL) || (parentPidLimit == NULL)) {
        return;
    }

    currPidLimit->pidLimit = parentPidLimit->pidLimit;
    currPidLimit->priorityLimit = parentPidLimit->priorityLimit;
    currPidLimit->pidCount = 0;
    return;
}

UINT32 PidLimitSetPidLimit(PidLimit *pidLimit, UINT32 pidMax)
{
    UINT32 intSave;

    if ((pidLimit == NULL) || (pidMax > LOS_GetSystemProcessMaximum())) {
        return EINVAL;
    }

    if (pidLimit == g_rootPidLimit) {
        return EPERM;
    }

    SCHEDULER_LOCK(intSave);
    if (pidLimit->pidCount >= pidMax) {
        SCHEDULER_UNLOCK(intSave);
        return EPERM;
    }

    pidLimit->pidLimit = pidMax;
    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;
}

UINT32 PidLimitSetPriorityLimit(PidLimit *pidLimit, UINT32 priority)
{
    UINT32 intSave;

    if ((pidLimit == NULL) ||
        (priority < OS_USER_PROCESS_PRIORITY_HIGHEST) ||
        (priority > OS_PROCESS_PRIORITY_LOWEST)) {
        return EINVAL;
    }

    if (pidLimit == g_rootPidLimit) {
        return EPERM;
    }

    SCHEDULER_LOCK(intSave);
    pidLimit->priorityLimit = priority;
    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;
}

UINT16 OsPidLimitGetPriorityLimit(VOID)
{
    UINT32 intSave;
    SCHEDULER_LOCK(intSave);
    LosProcessCB *run = OsCurrProcessGet();
    PidLimit *pidLimit = (PidLimit *)run->plimits->limitsList[PROCESS_LIMITER_ID_PIDS];
    UINT16 priority = pidLimit->priorityLimit;
    SCHEDULER_UNLOCK(intSave);
    return priority;
}
#endif
