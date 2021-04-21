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

#include "stdlib.h"
#include "los_config.h"
#include "los_exc.h"
#include "los_memstat_pri.h"
#include "los_sem_pri.h"
#include "los_seq_buf.h"
#include "los_task_pri.h"
#ifdef LOSCFG_SHELL
#include "shcmd.h"
#include "shell.h"
#endif
#ifdef LOSCFG_KERNEL_CPUP
#include "los_cpup_pri.h"
#endif
#ifdef LOSCFG_SHELL_EXCINFO
#include "los_excinfo_pri.h"
#endif
#include "los_process_pri.h"
#include "los_vm_dump.h"
#ifdef LOSCFG_FS_VFS
#include "fs/fs.h"
#endif
#include "los_sched_pri.h"


#define OS_PROCESS_MEM_INFO 0x2U
#define OS_PROCESS_INFO_LEN          (g_processMaxNum * (sizeof(LosProcessCB)))
#define OS_PROCESS_GROUP_INFO_LEN    (g_processMaxNum * sizeof(UINT32))
#define OS_PROCESS_UID_INFO_LEN      (g_processMaxNum * sizeof(UINT32))
#define OS_PROCESS_MEM_ALL_INFO_LEN  (g_processMaxNum * PROCESS_MEMINFO_LEN)
#ifdef LOSCFG_KERNEL_CPUP
#define OS_PROCESS_CPUP_LEN           (g_processMaxNum * sizeof(CPUP_INFO_S))
#define OS_PROCESS_AND_TASK_CPUP_LEN  ((g_processMaxNum + g_taskMaxNum) * sizeof(CPUP_INFO_S))
#define OS_PROCESS_CPUP_ALLINFO_LEN   (OS_PROCESS_AND_TASK_CPUP_LEN * 3)
#else
#define OS_PROCESS_CPUP_ALLINFO_LEN 0
#endif
#define OS_PROCESS_ALL_INFO_LEN (g_processMaxNum * (sizeof(LosProcessCB) + sizeof(UINT32)) + \
    OS_PROCESS_CPUP_ALLINFO_LEN + OS_PROCESS_UID_INFO_LEN)

#ifdef LOSCFG_KERNEL_CPUP
LITE_OS_SEC_BSS STATIC CPUP_INFO_S *processCpupAll = NULL;
LITE_OS_SEC_BSS STATIC CPUP_INFO_S *processCpup10s = NULL;
LITE_OS_SEC_BSS STATIC CPUP_INFO_S *processCpup1s = NULL;
LITE_OS_SEC_BSS STATIC CPUP_INFO_S *taskCpupAll = NULL;
LITE_OS_SEC_BSS STATIC CPUP_INFO_S *taskCpup10s = NULL;
LITE_OS_SEC_BSS STATIC CPUP_INFO_S *taskCpup1s = NULL;
#endif

STATIC UINT32 *taskWaterLine = NULL;
#define OS_INVALID_SEM_ID         0xFFFFFFFF
#define OS_TASK_WATER_LINE_SIZE   (g_taskMaxNum * sizeof(UINT32))
#define OS_TASK_INFO_LEN          (g_taskMaxNum * sizeof(LosTaskCB))
#define OS_TASK_ALL_INFO_LEN      (g_taskMaxNum * (sizeof(LosTaskCB) + sizeof(UINT32)))

#ifdef LOSCFG_FS_VFS
#define PROCESS_INFO_SHOW(seqBuf, arg...) do {              \
    if (seqBuf != NULL) {                                   \
        (void)LosBufPrintf((struct SeqBuf *)seqBuf, ##arg); \
    } else {                                                 \
        PRINTK(arg);                                         \
    }                                                        \
} while (0)
#else
#define PROCESS_INFO_SHOW(seqBuf, arg...) PRINTK(arg)
#endif

LITE_OS_SEC_TEXT_MINOR UINT8 *OsShellCmdProcessMode(UINT16 mode)
{
    if (mode == OS_KERNEL_MODE) {
        return (UINT8 *)"kernel";
    } else if (mode == OS_USER_MODE) {
        return (UINT8 *)"user";
    }

    return (UINT8 *)"ERROR";
}

LITE_OS_SEC_TEXT_MINOR UINT8 *OsShellCmdSchedPolicy(UINT16 policy)
{
    if (policy == LOS_SCHED_RR) {
        return (UINT8 *)"RR";
    } else if (policy == LOS_SCHED_FIFO) {
        return (UINT8 *)"FIFO";
    } else if (policy == LOS_SCHED_IDLE) {
        return (UINT8 *)"IDLE";
    }

    return (UINT8 *)"ERROR";
}

LITE_OS_SEC_TEXT_MINOR UINT8 *OsShellProcessStatus(UINT16 status)
{
    status = status & OS_PROCESS_STATUS_MASK;
    if (status & OS_PROCESS_STATUS_ZOMBIES) {
        return (UINT8 *)"Zombies";
    } else if (status & OS_PROCESS_STATUS_INIT) {
        return (UINT8 *)"Init";
    } else if (status & OS_PROCESS_STATUS_RUNNING) {
        return (UINT8 *)"Running";
    } else if (status & OS_PROCESS_STATUS_READY) {
        return (UINT8 *)"Ready";
    } else if (status & OS_PROCESS_STATUS_PENDING) {
        return (UINT8 *)"Pending";
    }

    return (UINT8 *)"Invalid";
}

STATIC VOID OsShellCmdProcessTitle(VOID *seqBuf, UINT16 flag)
{
    PROCESS_INFO_SHOW(seqBuf, "\r\n  PID  PPID PGID       UID  Status ");
    if (flag & OS_PROCESS_MEM_INFO) {
        PROCESS_INFO_SHOW(seqBuf, "VirtualMem ShareMem PhysicalMem ");
    }

    if (flag & OS_PROCESS_INFO_ALL) {
#ifdef LOSCFG_KERNEL_CPUP
        PROCESS_INFO_SHOW(seqBuf, "CPUUSE CPUUSE10s CPUUSE1s ");
#endif /* LOSCFG_KERNEL_CPUP */
        PROCESS_INFO_SHOW(seqBuf, "Policy Priority MTID TaskTotal   Mode ");
    } else {
#ifdef LOSCFG_KERNEL_CPUP
        PROCESS_INFO_SHOW(seqBuf, "CPUUSE10s ");
#endif /* LOSCFG_KERNEL_CPUP */
    }
    PROCESS_INFO_SHOW(seqBuf, " PName\n");
}

STATIC VOID OsShellCmdProcessInfoShow(const LosProcessCB *processCB, const INT32 *group,
                                      const UINT32 *memArray, VOID *seqBuf, UINT16 flag)
{
    const UINT32 *procMemUsage = NULL;
    const INT32 *user = (const INT32 *)((UINTPTR)group + OS_PROCESS_GROUP_INFO_LEN);
    UINT32 pid = processCB->processID;
    PROCESS_INFO_SHOW(seqBuf, "%5u%6d%5d%10d%8s", pid, (INT32)processCB->parentProcessID, group[pid], user[pid],
                      OsShellProcessStatus(processCB->processStatus));

    if (flag & OS_PROCESS_MEM_INFO) {
        procMemUsage = &memArray[pid * PROCESS_VM_INDEX_MAX];
        PROCESS_INFO_SHOW(seqBuf, "%#11x%#9x%#12x", procMemUsage[PROCESS_VM_INDEX], procMemUsage[PROCESS_SM_INDEX],
                          procMemUsage[PROCESS_PM_INDEX]);
    }
    if (flag & OS_PROCESS_INFO_ALL) {
#ifdef LOSCFG_KERNEL_CPUP
        PROCESS_INFO_SHOW(seqBuf, "%4u.%-2u%7u.%-2u%6u.%-2u ",
                          processCpupAll[pid].usage / LOS_CPUP_PRECISION_MULT,
                          processCpupAll[pid].usage % LOS_CPUP_PRECISION_MULT,
                          processCpup10s[pid].usage / LOS_CPUP_PRECISION_MULT,
                          processCpup10s[pid].usage % LOS_CPUP_PRECISION_MULT,
                          processCpup1s[pid].usage / LOS_CPUP_PRECISION_MULT,
                          processCpup1s[pid].usage % LOS_CPUP_PRECISION_MULT);
#endif /* LOSCFG_KERNEL_CPUP */
        PROCESS_INFO_SHOW(seqBuf, "%6s%9u%5d%10u%7s ",
                          OsShellCmdSchedPolicy(LOS_SCHED_RR), processCB->priority,
                          (INT32)processCB->threadGroupID, processCB->threadNumber,
                          OsShellCmdProcessMode(processCB->processMode));
    } else {
#ifdef LOSCFG_KERNEL_CPUP
        PROCESS_INFO_SHOW(seqBuf, "%7u.%-2u ",
                          processCpup10s[pid].usage / LOS_CPUP_PRECISION_MULT,
                          processCpup10s[pid].usage % LOS_CPUP_PRECISION_MULT);
#endif /* LOSCFG_KERNEL_CPUP */
    }
    PROCESS_INFO_SHOW(seqBuf, " %-32s\n", processCB->processName);
}

STATIC VOID OsShellCmdAllProcessInfoShow(const LosProcessCB *pcbArray, const INT32 *group,
                                         const UINT32 *memArray, VOID *seqBuf, UINT16 flag)
{
    const LosProcessCB *processCB = NULL;
    UINT32 pid;

    for (pid = 1; pid < g_processMaxNum; ++pid) {
        processCB = pcbArray + pid;
        if (OsProcessIsUnused(processCB)) {
            continue;
        }

        OsShellCmdProcessInfoShow(processCB, group, memArray, seqBuf, flag);
    }
}

#ifdef LOSCFG_KERNEL_VM
STATIC VOID OsProcessMemUsageGet(UINT32 *memArray)
{
    UINT32 pid;
    LosProcessCB *processCB = NULL;
    UINT32 *proMemUsage = NULL;

    for (pid = 0; pid < g_processMaxNum; ++pid) {
        processCB = g_processCBArray + pid;
        if (OsProcessIsUnused(processCB)) {
            continue;
        }
        proMemUsage = &memArray[pid * PROCESS_VM_INDEX_MAX];

        /* Process memory usage statistics, idle task defaults to 0 */
        if (pid == OsGetIdleProcessID()) {
            proMemUsage[PROCESS_VM_INDEX] = 0;
            proMemUsage[PROCESS_SM_INDEX] = 0;
            proMemUsage[PROCESS_PM_INDEX] = 0;
        } else {
            proMemUsage[PROCESS_VM_INDEX] = OsShellCmdProcessVmUsage(processCB->vmSpace);
            OsShellCmdProcessPmUsage(processCB->vmSpace, &proMemUsage[PROCESS_SM_INDEX],
                                     &proMemUsage[PROCESS_PM_INDEX]);
        }
    }
}
#endif

STATIC UINT32 OsProcessInfoGet(LosProcessCB **pcbArray, INT32 **group, UINT32 **memArray, UINT16 flag)
{
    UINT32 len = OS_PROCESS_ALL_INFO_LEN;
    LosProcessCB *processCB = NULL;
    INT32 *user = NULL;

    (VOID)memcpy_s(*pcbArray, OS_PROCESS_INFO_LEN, g_processCBArray, OS_PROCESS_INFO_LEN);
    *group = (INT32 *)((UINTPTR)*pcbArray + OS_PROCESS_INFO_LEN);
    user = (INT32 *)((UINTPTR)*group + OS_PROCESS_GROUP_INFO_LEN);
    for (UINT32 pid = 0; pid < g_processMaxNum; ++pid) {
        processCB = *pcbArray + pid;
        if (OsProcessIsUnused(processCB)) {
            continue;
        }

        if (processCB->group != NULL) {
            (*group)[processCB->processID] = processCB->group->groupID;
        } else {
            (*group)[processCB->processID] = -1;
        }

#ifdef LOSCFG_SECURITY_CAPABILITY
        if (processCB->user != NULL) {
            user[processCB->processID] = processCB->user->userID;
        } else {
            user[processCB->processID] = -1;
        }
#else
        user[processCB->processID] = 0;
#endif
    }

#ifdef LOSCFG_KERNEL_CPUP
    processCpupAll = (CPUP_INFO_S *)((UINTPTR)user + OS_PROCESS_UID_INFO_LEN);
    taskCpupAll = (CPUP_INFO_S *)((UINTPTR)processCpupAll + OS_PROCESS_CPUP_LEN);
    processCpup10s = (CPUP_INFO_S *)((UINTPTR)processCpupAll + OS_PROCESS_AND_TASK_CPUP_LEN);
    taskCpup10s = (CPUP_INFO_S *)((UINTPTR)processCpup10s + OS_PROCESS_CPUP_LEN);
    processCpup1s = (CPUP_INFO_S *)((UINTPTR)processCpup10s + OS_PROCESS_AND_TASK_CPUP_LEN);
    taskCpup1s = (CPUP_INFO_S *)((UINTPTR)processCpup1s + OS_PROCESS_CPUP_LEN);

    (VOID)OsGetAllProcessAndTaskCpuUsageUnsafe(CPUP_ALL_TIME, processCpupAll, OS_PROCESS_AND_TASK_CPUP_LEN);
    (VOID)OsGetAllProcessAndTaskCpuUsageUnsafe(CPUP_LAST_TEN_SECONDS, processCpup10s, OS_PROCESS_AND_TASK_CPUP_LEN);
    (VOID)OsGetAllProcessAndTaskCpuUsageUnsafe(CPUP_LAST_ONE_SECONDS, processCpup1s, OS_PROCESS_AND_TASK_CPUP_LEN);
#endif

#ifdef LOSCFG_KERNEL_VM
    if (flag & OS_PROCESS_MEM_INFO) {
        *memArray = (UINT32 *)((UINTPTR)*pcbArray + OS_PROCESS_ALL_INFO_LEN);
        OsProcessMemUsageGet(*memArray);
        len += OS_PROCESS_MEM_ALL_INFO_LEN;
    }
#endif

    return len;
}

STATIC VOID OsShellCmdProcessInfoData(const LosProcessCB *pcbArray, const INT32 *group,
                                      const UINT32 *memArray, VOID *seqBuf, UINT16 flag)
{
#ifdef LOSCFG_KERNEL_CPUP
    UINT32 pid = OsGetIdleProcessID();
    UINT32 sysUsage = LOS_CPUP_PRECISION - processCpupAll[pid].usage;

    PROCESS_INFO_SHOW(seqBuf, "\n  allCpu(%%): %4u.%02u sys, %4u.%02u idle\n",
                      sysUsage / LOS_CPUP_PRECISION_MULT,
                      sysUsage % LOS_CPUP_PRECISION_MULT,
                      processCpupAll[pid].usage / LOS_CPUP_PRECISION_MULT,
                      processCpupAll[pid].usage % LOS_CPUP_PRECISION_MULT);
#endif

    OsShellCmdProcessTitle(seqBuf, flag);
    OsShellCmdAllProcessInfoShow(pcbArray, group, memArray, seqBuf, flag);
}

LITE_OS_SEC_TEXT_MINOR UINT8 *OsShellCmdConvertTskStatus(UINT16 taskStatus)
{
    if (taskStatus & OS_TASK_STATUS_INIT) {
        return (UINT8 *)"Init";
    } else if (taskStatus & OS_TASK_STATUS_RUNNING) {
        return (UINT8 *)"Running";
    } else if (taskStatus & OS_TASK_STATUS_READY) {
        return (UINT8 *)"Ready";
    } else if (taskStatus & OS_TASK_STATUS_SUSPENDED) {
        return (UINT8 *)"Suspended";
    } else if (taskStatus & OS_TASK_STATUS_DELAY) {
        return (UINT8 *)"Delay";
    } else if (taskStatus & OS_TASK_STATUS_PENDING) {
        if (taskStatus & OS_TASK_STATUS_PEND_TIME) {
            return (UINT8 *)"PendTime";
        } else {
            return (UINT8 *)"Pending";
        }
    } else if (taskStatus & OS_TASK_STATUS_EXIT) {
        return (UINT8 *)"Exit";
    }

    return (UINT8 *)"Invalid";
}

STATIC VOID OsShellCmdTaskWaterLineGet(const LosTaskCB *allTaskArray)
{
    const LosTaskCB *taskCB = NULL;
    UINT32 loop;

    for (loop = 0; loop < g_taskMaxNum; ++loop) {
        taskCB = allTaskArray + loop;
        if (OsTaskIsUnused(taskCB)) {
            continue;
        }

        (VOID)OsStackWaterLineGet((const UINTPTR *)((UINTPTR)taskCB->topOfStack + taskCB->stackSize),
                                  (const UINTPTR *)taskCB->topOfStack, &taskWaterLine[taskCB->taskID]);
    }
}

#ifdef LOSCFG_SHELL_CMD_DEBUG
#define OS_PEND_REASON_MAX_LEN 20

STATIC CHAR *OsShellCheckTaskWaitFlag(const LosTaskCB *taskCB, UINTPTR *lockID)
{
    *lockID = taskCB->waitID;
    switch (taskCB->waitFlag) {
        case OS_TASK_WAIT_PROCESS:
            return "Child";
        case OS_TASK_WAIT_GID:
            return "PGroup";
        case OS_TASK_WAIT_ANYPROCESS:
            return "AnyChild";
        case OS_TASK_WAIT_SEM:
            return "Semaphore";
        case OS_TASK_WAIT_QUEUE:
            return "Queue";
        case OS_TASK_WAIT_JOIN:
            return "Join";
        case OS_TASK_WAIT_SIGNAL:
            return "Signal";
        case OS_TASK_WAIT_LITEIPC:
            return "LiteIPC";
        case OS_TASK_WAIT_MUTEX:
            return "Mutex";
        case OS_TASK_WAIT_EVENT:
            return "Event";
        case OS_TASK_WAIT_FUTEX:
            return "Futex";
        case OS_TASK_WAIT_COMPLETE:
            return "Complete";
        default:
            break;
    }

    return NULL;
}

STATIC VOID OsShellCmdCheckTaskPend(const LosTaskCB *taskCB, CHAR *pendReason, UINT32 maxLen, UINTPTR *lockID)
{
    CHAR *reason = NULL;
    LosTaskCB *owner = NULL;

    if (!(taskCB->taskStatus & OS_TASK_STATUS_PENDING)) {
        reason = (CHAR *)OsShellCmdConvertTskStatus(taskCB->taskStatus);
        goto EXIT;
    }

    reason = OsShellCheckTaskWaitFlag(taskCB, lockID);
    if (reason == NULL) {
        reason = "Others";
    }

    if (taskCB->taskMux != NULL) {
        *lockID = (UINTPTR)taskCB->taskMux;
        owner = ((LosMux *)taskCB->taskMux)->owner;
        if (owner != NULL) {
            if (snprintf_s(pendReason, maxLen, maxLen - 1, "Mutex-%u", owner->taskID) == EOK) {
                return;
            }
        }
    }

EXIT:
    if (strcpy_s(pendReason, maxLen, reason) != EOK) {
        PRINT_ERR("Get pend reason copy failed !\n");
    }
}
#endif

STATIC VOID OsShellCmdTskInfoTitle(VOID *seqBuf, UINT16 flag)
{
    PROCESS_INFO_SHOW(seqBuf, "\r\n  TID  PID ");
#if (LOSCFG_KERNEL_SMP == YES)
    PROCESS_INFO_SHOW(seqBuf, "Affi CPU  ");
#endif
    PROCESS_INFO_SHOW(seqBuf, "     Status StackSize WaterLine ");
    if (flag & OS_PROCESS_INFO_ALL) {
#ifdef LOSCFG_KERNEL_CPUP
        PROCESS_INFO_SHOW(seqBuf, "CPUUSE CPUUSE10s CPUUSE1s ");
#endif /* LOSCFG_KERNEL_CPUP */
#ifdef LOSCFG_SHELL_CMD_DEBUG
        PROCESS_INFO_SHOW(seqBuf, "  StackPoint  TopOfStack PendReason     LockID ");
#endif
        PROCESS_INFO_SHOW(seqBuf, "Policy Priority ");
    } else {
#ifdef LOSCFG_KERNEL_CPUP
        PROCESS_INFO_SHOW(seqBuf, "CPUUSE10s ");
#endif /* LOSCFG_KERNEL_CPUP */
    }
    PROCESS_INFO_SHOW(seqBuf, "   MEMUSE  TaskName\n");
}

STATIC INLINE VOID OsShellTskInfoData(const LosTaskCB *taskCB, VOID *seqBuf, UINT16 flag)
{
#ifdef LOSCFG_SHELL_CMD_DEBUG
    UINTPTR lockID = 0;
    CHAR pendReason[OS_PEND_REASON_MAX_LEN] = { 0 };
#endif
    PROCESS_INFO_SHOW(seqBuf, " %4u%5u", taskCB->taskID, taskCB->processID);

#if (LOSCFG_KERNEL_SMP == YES)
    PROCESS_INFO_SHOW(seqBuf, "%#5x%4d ", taskCB->cpuAffiMask, (INT16)(taskCB->currCpu));
#endif

    PROCESS_INFO_SHOW(seqBuf, "%12s%#10x%#10x", OsShellCmdConvertTskStatus(taskCB->taskStatus),
                      taskCB->stackSize, taskWaterLine[taskCB->taskID]);
    if (flag & OS_PROCESS_INFO_ALL) {
#ifdef LOSCFG_KERNEL_CPUP
        PROCESS_INFO_SHOW(seqBuf, "%4u.%-2u%7u.%-2u%6u.%-2u ",
                          taskCpupAll[taskCB->taskID].usage / LOS_CPUP_PRECISION_MULT,
                          taskCpupAll[taskCB->taskID].usage % LOS_CPUP_PRECISION_MULT,
                          taskCpup10s[taskCB->taskID].usage / LOS_CPUP_PRECISION_MULT,
                          taskCpup10s[taskCB->taskID].usage % LOS_CPUP_PRECISION_MULT,
                          taskCpup1s[taskCB->taskID].usage / LOS_CPUP_PRECISION_MULT,
                          taskCpup1s[taskCB->taskID].usage % LOS_CPUP_PRECISION_MULT);
#endif /* LOSCFG_KERNEL_CPUP */
#ifdef LOSCFG_SHELL_CMD_DEBUG
        OsShellCmdCheckTaskPend(taskCB, pendReason, OS_PEND_REASON_MAX_LEN, &lockID);
        PROCESS_INFO_SHOW(seqBuf, "%#12x%#12x%11s%#11x", taskCB->stackPointer, taskCB->topOfStack, pendReason, lockID);
#endif
        PROCESS_INFO_SHOW(seqBuf, "%7s%9u", OsShellCmdSchedPolicy(taskCB->policy), taskCB->priority);
    } else {
#ifdef LOSCFG_KERNEL_CPUP
        PROCESS_INFO_SHOW(seqBuf, "%7u.%-2u ",
                          taskCpup10s[taskCB->taskID].usage / LOS_CPUP_PRECISION_MULT,
                          taskCpup10s[taskCB->taskID].usage % LOS_CPUP_PRECISION_MULT);
#endif /* LOSCFG_KERNEL_CPUP */
    }
    PROCESS_INFO_SHOW(seqBuf, "%#10x  %-32s\n", OsTaskMemUsage(taskCB->taskID), taskCB->taskName);
}

STATIC VOID OsShellCmdAllTaskInfoData(const LosTaskCB *allTaskArray, VOID *seqBuf, UINT16 flag)
{
    const LosTaskCB *taskCB = NULL;
    UINT32 pid;
    UINT32 loop;

    for (pid = 1; pid < g_processMaxNum; ++pid) {
        for (loop = 0; loop < g_taskMaxNum; ++loop) {
            taskCB = allTaskArray + loop;
            if (OsTaskIsUnused(taskCB) || (taskCB->processID != pid)) {
                continue;
            }

            OsShellTskInfoData(taskCB, seqBuf, flag);
        }
    }
}

STATIC VOID OsShellCmdTskInfoData(const LosTaskCB *allTaskArray, VOID *seqBuf, UINT16 flag)
{
    OsShellCmdTskInfoTitle(seqBuf, flag);
    OsShellCmdAllTaskInfoData(allTaskArray, seqBuf, flag);
}

STATIC VOID OsProcessAndTaskInfoGet(LosProcessCB **pcbArray, INT32 **group, LosTaskCB **tcbArray,
                                    UINT32 **memArray, UINT16 flag)
{
    BOOL lockFlag = FALSE;
    UINT32 intSave = 0;
    UINT32 processInfoLen;

    if (LOS_SpinHeld(&g_taskSpin) == FALSE) {
        SCHEDULER_LOCK(intSave);
        lockFlag = TRUE;
    }

    processInfoLen = OsProcessInfoGet(pcbArray, group, memArray, flag);
    *tcbArray = (LosTaskCB *)((UINTPTR)*pcbArray + processInfoLen);
    (VOID)memcpy_s(*tcbArray, OS_TASK_INFO_LEN, g_taskCBArray, OS_TASK_INFO_LEN);
    taskWaterLine = (UINT32 *)((UINTPTR)*tcbArray + OS_TASK_INFO_LEN);
    OsShellCmdTaskWaterLineGet(*tcbArray);
    if (lockFlag == TRUE) {
        SCHEDULER_UNLOCK(intSave);
    }
}

LITE_OS_SEC_TEXT_MINOR UINT32 OsShellCmdTskInfoGet(UINT32 taskID, VOID *seqBuf, UINT16 flag)
{
    UINT32 size;
    LosProcessCB *pcbArray = NULL;
    INT32 *group = NULL;
    LosTaskCB *tcbArray = NULL;
    UINT32 *memArray = NULL;

    if (taskID == OS_ALL_TASK_MASK) {
        if (flag & OS_PROCESS_MEM_INFO) {
            size = OS_PROCESS_ALL_INFO_LEN + OS_PROCESS_MEM_ALL_INFO_LEN + OS_TASK_ALL_INFO_LEN;
        } else {
            size = OS_PROCESS_ALL_INFO_LEN + OS_TASK_ALL_INFO_LEN;
        }
        pcbArray = (LosProcessCB *)LOS_MemAlloc(m_aucSysMem1, size);
        if (pcbArray == NULL) {
            PRINT_ERR("Memory is not enough to save task info!\n");
            return LOS_NOK;
        }
        (VOID)memset_s(pcbArray, size, 0, size);
        OsProcessAndTaskInfoGet(&pcbArray, &group, &tcbArray, &memArray, flag);
        OsShellCmdProcessInfoData(pcbArray, group, memArray, seqBuf, flag);
        OsShellCmdTskInfoData(tcbArray, seqBuf, flag);

        (VOID)LOS_MemFree(m_aucSysMem1, pcbArray);
    }

    return LOS_OK;
}

LITE_OS_SEC_TEXT_MINOR UINT32 OsShellCmdDumpTask(INT32 argc, const CHAR **argv)
{
    UINT32 flag = 0;
#ifdef LOSCFG_KERNEL_VM
    flag |= OS_PROCESS_MEM_INFO;
#endif

    if (argc >= 2) { /* 2: The task shell name restricts the parameters */
        goto TASK_HELP;
    }

    if (argc == 1) {
        if (strcmp("-a", argv[0]) == 0) {
            flag |= OS_PROCESS_INFO_ALL;
        } else if (strcmp("-i", argv[0]) == 0) {
            if (!OsShellShowTickRespo()) {
                return LOS_OK;
            }
            goto TASK_HELP;
        } else if (strcmp("-t", argv[0]) == 0) {
            if (!OsShellShowSchedParam()) {
                return LOS_OK;
            }
            goto TASK_HELP;
        } else {
            goto TASK_HELP;
        }
    }

    return OsShellCmdTskInfoGet(OS_ALL_TASK_MASK, NULL, flag);

TASK_HELP:
    PRINTK("Unknown option: %s\n", argv[0]);
    PRINTK("usage: task or task -a\n");
    return LOS_NOK;
}

#ifdef LOSCFG_SHELL
SHELLCMD_ENTRY(task_shellcmd, CMD_TYPE_EX, "task", 1, (CmdCallBackFunc)OsShellCmdDumpTask);
#endif

