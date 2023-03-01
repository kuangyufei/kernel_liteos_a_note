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

#include "los_info_pri.h"
#include "los_task_pri.h"
#include "los_vm_dump.h"

STATIC UINT32 GetCurrParentPid(UINT32 pid, const LosProcessCB *processCB)
{
    if (processCB->parentProcess == NULL) {
        return 0;
    }

#ifdef LOSCFG_PID_CONTAINER
    if (pid == OS_USER_ROOT_PROCESS_ID) {
        return 0;
    }

    if (OS_PROCESS_CONTAINER_CHECK(processCB->parentProcess, OsCurrProcessGet())) {
        return OsGetVpidFromCurrContainer(processCB->parentProcess);
    }
#endif
    return processCB->parentProcess->processID;
}

STATIC INLINE UINT32 GetCurrTid(const LosTaskCB *taskCB)
{
#ifdef LOSCFG_PID_CONTAINER
    if (taskCB->pidContainer != OsCurrTaskGet()->pidContainer) {
        return OsGetVtidFromCurrContainer(taskCB);
    }
#endif
    return taskCB->taskID;
}

STATIC UINT16 GetProcessStatus(LosProcessCB *processCB)
{
    UINT16 status;
    LosTaskCB *taskCB = NULL;

    if (LOS_ListEmpty(&processCB->threadSiblingList)) {
        return processCB->processStatus;
    }

    status = processCB->processStatus;
    LOS_DL_LIST_FOR_EACH_ENTRY(taskCB, &processCB->threadSiblingList, LosTaskCB, threadList) {
        status |= (taskCB->taskStatus & 0x00FF);
    }
    return status;
}

STATIC VOID GetProcessInfo(ProcessInfo *pcbInfo, const LosProcessCB *processCB)
{
    SchedParam param = {0};
    pcbInfo->pid = OsGetPid(processCB);
    pcbInfo->ppid = GetCurrParentPid(pcbInfo->pid, processCB);
    pcbInfo->status = GetProcessStatus((LosProcessCB *)processCB);
    pcbInfo->mode = processCB->processMode;
    if (processCB->pgroup != NULL) {
        pcbInfo->pgroupID = OsGetPid(OS_GET_PGROUP_LEADER(processCB->pgroup));
    } else {
        pcbInfo->pgroupID = -1;
    }
#ifdef LOSCFG_SECURITY_CAPABILITY
    if (processCB->user != NULL) {
        pcbInfo->userID = processCB->user->userID;
    } else {
        pcbInfo->userID = -1;
    }
#else
    pcbInfo->userID = 0;
#endif
    LosTaskCB *taskCB = processCB->threadGroup;
    pcbInfo->threadGroupID = taskCB->taskID;
    taskCB->ops->schedParamGet(taskCB, &param);
    pcbInfo->policy = LOS_SCHED_RR;
    pcbInfo->basePrio = param.basePrio;
    pcbInfo->threadNumber = processCB->threadNumber;
#ifdef LOSCFG_KERNEL_CPUP
    (VOID)OsGetProcessAllCpuUsageUnsafe(processCB->processCpup, pcbInfo);
#endif
    (VOID)memcpy_s(pcbInfo->name, OS_PCB_NAME_LEN, processCB->processName, OS_PCB_NAME_LEN);
}

#ifdef LOSCFG_KERNEL_VM
STATIC VOID GetProcessMemInfo(ProcessInfo *pcbInfo, const LosProcessCB *processCB, LosVmSpace *vmSpace)
{
    /* Process memory usage statistics, idle task defaults to 0 */
    if (processCB == &g_processCBArray[0]) {
        pcbInfo->virtualMem = 0;
        pcbInfo->shareMem = 0;
        pcbInfo->physicalMem = 0;
    } else if (vmSpace == LOS_GetKVmSpace()) {
        (VOID)OsShellCmdProcessPmUsage(vmSpace, &pcbInfo->shareMem, &pcbInfo->physicalMem);
        pcbInfo->virtualMem = pcbInfo->physicalMem;
    } else {
        pcbInfo->virtualMem = OsShellCmdProcessVmUsage(vmSpace);
        if (pcbInfo->virtualMem == 0) {
            pcbInfo->status = OS_PROCESS_FLAG_UNUSED;
            return;
        }
        if (OsShellCmdProcessPmUsage(vmSpace, &pcbInfo->shareMem, &pcbInfo->physicalMem) == 0) {
            pcbInfo->status = OS_PROCESS_FLAG_UNUSED;
        }
    }
}
#endif

STATIC VOID GetThreadInfo(ProcessThreadInfo *threadInfo, LosProcessCB *processCB)
{
    SchedParam param = {0};
    LosTaskCB *taskCB = NULL;
    if (LOS_ListEmpty(&processCB->threadSiblingList)) {
        threadInfo->threadCount = 0;
        return;
    }

    threadInfo->threadCount = 0;
    LOS_DL_LIST_FOR_EACH_ENTRY(taskCB, &processCB->threadSiblingList, LosTaskCB, threadList) {
        TaskInfo *taskInfo = &threadInfo->taskInfo[threadInfo->threadCount];
        taskInfo->tid = GetCurrTid(taskCB);
        taskInfo->pid = OsGetPid(processCB);
        taskInfo->status = taskCB->taskStatus;
        taskCB->ops->schedParamGet(taskCB, &param);
        taskInfo->policy = param.policy;
        taskInfo->priority = param.priority;
#ifdef LOSCFG_KERNEL_SMP
        taskInfo->currCpu = taskCB->currCpu;
        taskInfo->cpuAffiMask = taskCB->cpuAffiMask;
#endif
        taskInfo->stackPoint = (UINTPTR)taskCB->stackPointer;
        taskInfo->topOfStack = taskCB->topOfStack;
        taskInfo->stackSize = taskCB->stackSize;
        taskInfo->waitFlag = taskCB->waitFlag;
        taskInfo->waitID = taskCB->waitID;
        taskInfo->taskMux = taskCB->taskMux;
        (VOID)OsStackWaterLineGet((const UINTPTR *)(taskCB->topOfStack + taskCB->stackSize),
                                  (const UINTPTR *)taskCB->topOfStack, &taskInfo->waterLine);
#ifdef LOSCFG_KERNEL_CPUP
        (VOID)OsGetTaskAllCpuUsageUnsafe(&taskCB->taskCpup, taskInfo);
#endif
        (VOID)memcpy_s(taskInfo->name, OS_TCB_NAME_LEN, taskCB->taskName, OS_TCB_NAME_LEN);
        threadInfo->threadCount++;
    }
}

UINT32 OsGetProcessThreadInfo(UINT32 pid, ProcessThreadInfo *threadInfo)
{
    UINT32 intSave;

    if (OS_PID_CHECK_INVALID(pid) || (pid == 0) || (threadInfo == NULL)) {
        return LOS_NOK;
    }

    LosProcessCB *processCB = OS_PCB_FROM_PID(pid);
    if (OsProcessIsUnused(processCB)) {
        return LOS_NOK;
    }

#ifdef LOSCFG_KERNEL_VM
    GetProcessMemInfo(&threadInfo->processInfo, processCB, processCB->vmSpace);
#endif

    SCHEDULER_LOCK(intSave);
    GetProcessInfo(&threadInfo->processInfo, processCB);
    GetThreadInfo(threadInfo, processCB);
    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;
}

STATIC VOID ProcessMemUsageGet(ProcessInfo *pcbArray)
{
    UINT32 intSave;
#ifdef LOSCFG_PID_CONTAINER
    PidContainer *pidContainer = OsCurrTaskGet()->pidContainer;
    for (UINT32 pid = 0; pid < g_processMaxNum; ++pid) {
        ProcessVid *processVid = &pidContainer->pidArray[pid];
        const LosProcessCB *processCB = (LosProcessCB *)processVid->cb;
#else
    for (UINT32 pid = 0; pid < g_processMaxNum; ++pid) {
        const LosProcessCB *processCB = OS_PCB_FROM_RPID(pid);
#endif
        ProcessInfo *pcbInfo = pcbArray + pid;
        SCHEDULER_LOCK(intSave);
        if (OsProcessIsUnused(processCB)) {
            SCHEDULER_UNLOCK(intSave);
            pcbInfo->status = OS_PROCESS_FLAG_UNUSED;
            continue;
        }
#ifdef LOSCFG_KERNEL_VM
        LosVmSpace *vmSpace  = processCB->vmSpace;
#endif
        SCHEDULER_UNLOCK(intSave);

#ifdef LOSCFG_KERNEL_VM
        GetProcessMemInfo(pcbInfo, processCB, vmSpace);
#endif
    }
}

UINT32 OsGetAllProcessInfo(ProcessInfo *pcbArray)
{
    UINT32 intSave;
    if (pcbArray == NULL) {
        return LOS_NOK;
    }

    ProcessMemUsageGet(pcbArray);

    SCHEDULER_LOCK(intSave);
#ifdef LOSCFG_PID_CONTAINER
    PidContainer *pidContainer = OsCurrTaskGet()->pidContainer;
    for (UINT32 index = 0; index < LOSCFG_BASE_CORE_PROCESS_LIMIT; index++) {
        ProcessVid *processVid = &pidContainer->pidArray[index];
        LosProcessCB *processCB = (LosProcessCB *)processVid->cb;
#else
    for (UINT32 index = 0; index < LOSCFG_BASE_CORE_PROCESS_LIMIT; index++) {
        LosProcessCB *processCB = OS_PCB_FROM_RPID(index);
#endif
        ProcessInfo *pcbInfo = pcbArray + index;
        if (OsProcessIsUnused(processCB)) {
            pcbInfo->status = OS_PROCESS_FLAG_UNUSED;
            continue;
        }
        GetProcessInfo(pcbInfo, processCB);
    }
    SCHEDULER_UNLOCK(intSave);
    return LOS_OK;
}
