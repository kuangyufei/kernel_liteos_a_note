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

#ifndef _LOS_INFO_PRI_H
#define _LOS_INFO_PRI_H

#include "los_process_pri.h"
#include "los_sched_pri.h"

#ifdef __cplusplus
#if __cplusplus
    extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

typedef struct TagTaskInfo {
    UINT32  tid;
    UINT32  pid;
    UINT16  status;
    UINT16  policy;
    UINT16  priority;
#ifdef LOSCFG_KERNEL_SMP
    UINT16  currCpu;
    UINT16  cpuAffiMask;
#endif
    UINT32  stackSize;
    UINTPTR stackPoint;
    UINTPTR topOfStack;
    UINT32  waitFlag;
    UINT32  waitID;
    VOID    *taskMux;
    UINT32  waterLine;
#ifdef LOSCFG_KERNEL_CPUP
    UINT32  cpup1sUsage;
    UINT32  cpup10sUsage;
    UINT32  cpupAllsUsage;
#endif
    CHAR    name[OS_TCB_NAME_LEN];
} TaskInfo;

typedef struct TagProcessInfo {
    UINT32 pid;
    UINT32 ppid;
    UINT16 status;
    UINT16 mode;
    UINT32 pgroupID;
    UINT32 userID;
    UINT16 policy;
    UINT32 basePrio;
    UINT32 threadGroupID;
    UINT32 threadNumber;
#ifdef LOSCFG_KERNEL_VM
    UINT32 virtualMem;
    UINT32 shareMem;
    UINT32 physicalMem;
#endif
#ifdef LOSCFG_KERNEL_CPUP
    UINT32 cpup1sUsage;
    UINT32 cpup10sUsage;
    UINT32 cpupAllsUsage;
#endif
    CHAR   name[OS_PCB_NAME_LEN];
} ProcessInfo;

typedef struct TagProcessThreadInfo {
    ProcessInfo processInfo;
    UINT32      threadCount;
    TaskInfo    taskInfo[LOSCFG_BASE_CORE_TSK_LIMIT];
} ProcessThreadInfo;

UINT32 OsGetAllProcessInfo(ProcessInfo *pcbArray);

UINT32 OsGetProcessThreadInfo(UINT32 pid, ProcessThreadInfo *threadInfo);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_INFO_PRI_H */
