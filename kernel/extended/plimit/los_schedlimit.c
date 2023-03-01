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

#ifdef LOSCFG_KERNEL_SCHED_PLIMIT
#include "los_schedlimit.h"
#include "los_process_pri.h"

STATIC ProcSchedLimiter *g_procSchedLimit = NULL;

VOID OsSchedLimitInit(UINTPTR limit)
{
    ProcSchedLimiter *schedLimit = (ProcSchedLimiter *)limit;
    schedLimit->period = 0;
    schedLimit->quota = 0;
    g_procSchedLimit = schedLimit;
}

VOID *OsSchedLimitAlloc(VOID)
{
    ProcSchedLimiter *plimit = (ProcSchedLimiter *)LOS_MemAlloc(m_aucSysMem1, sizeof(ProcSchedLimiter));
    if (plimit == NULL) {
        return NULL;
    }
    (VOID)memset_s(plimit, sizeof(ProcSchedLimiter), 0, sizeof(ProcSchedLimiter));
    return (VOID *)plimit;
}

VOID OsSchedLimitFree(UINTPTR limit)
{
    ProcSchedLimiter *schedLimit = (ProcSchedLimiter *)limit;
    if (schedLimit == NULL) {
        return;
    }

    (VOID)LOS_MemFree(m_aucSysMem1, (VOID *)limit);
}

VOID OsSchedLimitCopy(UINTPTR dest, UINTPTR src)
{
    ProcSchedLimiter *plimitDest = (ProcSchedLimiter *)dest;
    ProcSchedLimiter *plimitSrc = (ProcSchedLimiter *)src;
    plimitDest->period = plimitSrc->period;
    plimitDest->quota = plimitSrc->quota;
    return;
}

VOID OsSchedLimitUpdateRuntime(LosTaskCB *runTask, UINT64 currTime, INT32 incTime)
{
    LosProcessCB *run = (LosProcessCB *)runTask->processCB;
    if ((run == NULL) || (run->plimits == NULL)) {
        return;
    }

    ProcSchedLimiter *schedLimit = (ProcSchedLimiter *)run->plimits->limitsList[PROCESS_LIMITER_ID_SCHED];

    run->limitStat.allRuntime += incTime;
    if ((schedLimit->period <= 0) || (schedLimit->quota <= 0)) {
        return;
    }

    if ((schedLimit->startTime <= currTime) && (schedLimit->allRuntime < schedLimit->quota)) {
        schedLimit->allRuntime += incTime;
        return;
    }

    if (schedLimit->startTime <= currTime) {
        schedLimit->startTime = currTime + schedLimit->period;
        schedLimit->allRuntime = 0;
    }
}

BOOL OsSchedLimitCheckTime(LosTaskCB *task)
{
    UINT64 currTime = OsGetCurrSchedTimeCycle();
    LosProcessCB *processCB = (LosProcessCB *)task->processCB;
    if ((processCB == NULL) || (processCB->plimits == NULL)) {
        return TRUE;
    }
    ProcSchedLimiter *schedLimit = (ProcSchedLimiter *)processCB->plimits->limitsList[PROCESS_LIMITER_ID_SCHED];
    if (schedLimit->startTime >= currTime) {
        return FALSE;
    }
    return TRUE;
}

UINT32 OsSchedLimitSetPeriod(ProcSchedLimiter *schedLimit, UINT64 value)
{
    UINT32 intSave;
    if (schedLimit == NULL) {
        return EINVAL;
    }

    if (schedLimit == g_procSchedLimit) {
        return EPERM;
    }

    SCHEDULER_LOCK(intSave);
    schedLimit->period = value;
    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;
}

UINT32 OsSchedLimitSetQuota(ProcSchedLimiter *schedLimit, UINT64 value)
{
    UINT32 intSave;
    if (schedLimit == NULL) {
        return EINVAL;
    }

    if (schedLimit == g_procSchedLimit) {
        return EPERM;
    }

    SCHEDULER_LOCK(intSave);
    if (value > (UINT64)schedLimit->period) {
        SCHEDULER_UNLOCK(intSave);
        return EINVAL;
    }

    schedLimit->quota = value;
    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;
}
#endif
