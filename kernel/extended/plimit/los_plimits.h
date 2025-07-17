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

#ifndef _LOS_PLIMITS_H
#define _LOS_PLIMITS_H

#include "los_list.h"
#include "los_typedef.h"
#include "los_processlimit.h"
#include "los_memlimit.h"
#include "los_ipclimit.h"
#include "los_devicelimit.h"
#include "los_schedlimit.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/* proc limit set lock */
#define PLIMITS_MIN_PERIOD_IN_US 1000000
#define PLIMITS_MIN_QUOTA_IN_US        1
#define PLIMITS_DELETE_WAIT_TIME    1000
typedef struct ProcessCB LosProcessCB;

enum ProcLimiterID {
    PROCESS_LIMITER_ID_PIDS = 0,
#ifdef LOSCFG_KERNEL_MEM_PLIMIT
    PROCESS_LIMITER_ID_MEM,
#endif
#ifdef LOSCFG_KERNEL_SCHED_PLIMIT
    PROCESS_LIMITER_ID_SCHED,
#endif
#ifdef LOSCFG_KERNEL_DEV_PLIMIT
    PROCESS_LIMITER_ID_DEV,
#endif
#ifdef LOSCFG_KERNEL_IPC_PLIMIT
    PROCESS_LIMITER_ID_IPC,
#endif
    PROCESS_LIMITER_COUNT,
};

typedef struct {
#ifdef LOSCFG_KERNEL_MEM_PLIMIT
    UINT64 memUsed;
#endif
#ifdef LOSCFG_KERNEL_IPC_PLIMIT
    UINT32 mqCount;
    UINT32 shmSize;
#endif
#ifdef LOSCFG_KERNEL_SCHED_PLIMIT
    UINT64  allRuntime;
#endif
} PLimitsData;

typedef struct TagPLimiterSet {
    struct TagPLimiterSet *parent;                       /* plimite parent */
    LOS_DL_LIST childList;                               /* plimite childList */
    LOS_DL_LIST processList;
    UINT32      pidCount;
    UINTPTR limitsList[PROCESS_LIMITER_COUNT];           /* plimite list */
    UINT32 mask;                                         /* each bit indicates that a limite exists */
    UINT8 level;                                         /* plimits hierarchy level */
} ProcLimiterSet;

typedef struct ProcessCB LosProcessCB;
ProcLimiterSet *OsRootPLimitsGet(VOID);
UINT32 OsPLimitsAddProcess(ProcLimiterSet *plimits, LosProcessCB *processCB);
UINT32 OsPLimitsAddPid(ProcLimiterSet *plimits, UINT32 pid);
VOID OsPLimitsDeleteProcess(LosProcessCB *processCB);
UINT32 OsPLimitsPidsGet(const ProcLimiterSet *plimits, UINT32 *pids, UINT32 size);
UINT32 OsPLimitsFree(ProcLimiterSet *currPLimits);
ProcLimiterSet *OsPLimitsCreate(ProcLimiterSet *parentProcLimiterSet);
UINT32 OsProcLimiterSetInit(VOID);

UINT32 OsPLimitsMemUsageGet(ProcLimiterSet *plimits, UINT64 *usage, UINT32 size);
UINT32 OsPLimitsIPCStatGet(ProcLimiterSet *plimits, ProcIPCLimit *ipc, UINT32 size);
UINT32 OsPLimitsSchedUsageGet(ProcLimiterSet *plimits, UINT64 *usage, UINT32 size);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_PLIMITS_H */
