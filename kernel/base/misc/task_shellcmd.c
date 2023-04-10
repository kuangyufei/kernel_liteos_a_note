/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2023 Huawei Device Co., Ltd. All rights reserved.
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
#ifdef LOSCFG_SAVE_EXCINFO
#include "los_excinfo_pri.h"
#endif
#include "los_process_pri.h"
#ifdef LOSCFG_FS_VFS
#include "fs/file.h"
#endif
#include "los_sched_pri.h"
#include "los_swtmr_pri.h"
#include "los_info_pri.h"
#ifdef LOSCFG_SCHED_DEBUG
#include "los_statistics_pri.h"
#endif

#define OS_PROCESS_MEM_INFO 0x2U
#undef SHOW
#ifdef LOSCFG_FS_VFS
#if defined(LOSCFG_BLACKBOX) && defined(LOSCFG_SAVE_EXCINFO)
#define SaveExcInfo(arg, ...) WriteExcInfoToBuf(arg, ##__VA_ARGS__)
#else
#define SaveExcInfo(arg, ...)
#endif
#define SHOW(arg...) do {                                    \
    if (seqBuf != NULL) {                                    \
        (void)LosBufPrintf((struct SeqBuf *)seqBuf, ##arg);  \
    } else {                                                 \
        PRINTK(arg);                                         \
    }                                                        \
    SaveExcInfo(arg);                                        \
} while (0)
#else
#define SHOW(arg...) PRINTK(arg)
#endif

#define CPUP_MULT LOS_CPUP_PRECISION_MULT

STATIC UINT8 *ConvertProcessModeToString(UINT16 mode)
{
    if (mode == OS_KERNEL_MODE) {
        return (UINT8 *)"kernel";
    } else if (mode == OS_USER_MODE) {
        return (UINT8 *)"user";
    }

    return (UINT8 *)"ERROR";
}

STATIC UINT8 *ConvertSchedPolicyToString(UINT16 policy)
{
    if (policy == LOS_SCHED_RR) {
        return (UINT8 *)"RR";
    } else if (policy == LOS_SCHED_FIFO) {
        return (UINT8 *)"FIFO";
    } else if (policy == LOS_SCHED_DEADLINE) {
        return (UINT8 *)"EDF";
    } else if (policy == LOS_SCHED_IDLE) {
        return (UINT8 *)"IDLE";
    }

    return (UINT8 *)"ERROR";
}

STATIC UINT8 *ConvertProcessStatusToString(UINT16 status)
{
    if (status & OS_PROCESS_STATUS_ZOMBIES) {
        return (UINT8 *)"Zombies";
    } else if (status & OS_PROCESS_STATUS_INIT) {
        return (UINT8 *)"Init";
    } else if (status & OS_PROCESS_STATUS_RUNNING) {
        return (UINT8 *)"Running";
    } else if (status & OS_PROCESS_STATUS_READY) {
        return (UINT8 *)"Ready";
    }
    return (UINT8 *)"Pending";
}

STATIC VOID ProcessInfoTitle(VOID *seqBuf, UINT16 flag)
{
    SHOW("\r\n  PID  PPID PGID   UID   Mode  Status Policy Priority MTID TTotal");
    if (flag & OS_PROCESS_INFO_ALL) {
#ifdef LOSCFG_KERNEL_VM
        if (flag & OS_PROCESS_MEM_INFO) {
            SHOW(" VirtualMem ShareMem PhysicalMem");
        }
#endif
#ifdef LOSCFG_KERNEL_CPUP
        SHOW(" CPUUSE CPUUSE10s CPUUSE1s");
#endif /* LOSCFG_KERNEL_CPUP */
    } else {
#ifdef LOSCFG_KERNEL_CPUP
        SHOW(" CPUUSE10s");
#endif /* LOSCFG_KERNEL_CPUP */
    }
    SHOW(" PName\n");
}

STATIC VOID ProcessDataShow(const ProcessInfo *processInfo, VOID *seqBuf, UINT16 flag)
{
    SHOW("%5u%6u%5d%6d%7s%8s%7s%9u%5u%7u", processInfo->pid, processInfo->ppid, processInfo->pgroupID,
         processInfo->userID, ConvertProcessModeToString(processInfo->mode),
         ConvertProcessStatusToString(processInfo->status),
         ConvertSchedPolicyToString(processInfo->policy), processInfo->basePrio,
         processInfo->threadGroupID, processInfo->threadNumber);

    if (flag & OS_PROCESS_INFO_ALL) {
#ifdef LOSCFG_KERNEL_VM
        if (flag & OS_PROCESS_MEM_INFO) {
            SHOW("%#11x%#9x%#12x", processInfo->virtualMem, processInfo->shareMem, processInfo->physicalMem);
        }
#endif
#ifdef LOSCFG_KERNEL_CPUP
        SHOW("%4u.%-2u%7u.%-2u%6u.%-2u ",
             processInfo->cpupAllsUsage / CPUP_MULT, processInfo->cpupAllsUsage % CPUP_MULT,
             processInfo->cpup10sUsage / CPUP_MULT, processInfo->cpup10sUsage % CPUP_MULT,
             processInfo->cpup1sUsage / CPUP_MULT, processInfo->cpup1sUsage % CPUP_MULT);
#endif /* LOSCFG_KERNEL_CPUP */
    } else {
#ifdef LOSCFG_KERNEL_CPUP
        SHOW("%7u.%-2u ", processInfo->cpup10sUsage / CPUP_MULT, processInfo->cpup10sUsage % CPUP_MULT);
#endif /* LOSCFG_KERNEL_CPUP */
    }
    SHOW("%-32s\n", processInfo->name);
}

STATIC VOID AllProcessDataShow(const ProcessInfo *pcbArray, VOID *seqBuf, UINT16 flag)
{
    for (UINT32 pid = 1; pid < g_processMaxNum; ++pid) {
        const ProcessInfo *processInfo = pcbArray + pid;
        if (processInfo->status & OS_PROCESS_FLAG_UNUSED) {
            continue;
        }
        ProcessDataShow(processInfo, seqBuf, flag);
    }
}

STATIC VOID ProcessInfoShow(const ProcessInfo *pcbArray, VOID *seqBuf, UINT16 flag)
{
#ifdef LOSCFG_KERNEL_CPUP
    UINT32 pid = OS_KERNEL_IDLE_PROCESS_ID;
    UINT32 sysUsage = LOS_CPUP_PRECISION - pcbArray[pid].cpup10sUsage;
    SHOW("\n  allCpu(%%): %4u.%02u sys, %4u.%02u idle\n", sysUsage / CPUP_MULT, sysUsage % CPUP_MULT,
         pcbArray[pid].cpup10sUsage / CPUP_MULT, pcbArray[pid].cpup10sUsage % CPUP_MULT);
#endif

    ProcessInfoTitle(seqBuf, flag);
    AllProcessDataShow(pcbArray, seqBuf, flag);
}

STATIC UINT8 *ConvertTaskStatusToString(UINT16 taskStatus)
{
    if (taskStatus & OS_TASK_STATUS_INIT) {
        return (UINT8 *)"Init";
    } else if (taskStatus & OS_TASK_STATUS_RUNNING) {
        return (UINT8 *)"Running";
    } else if (taskStatus & OS_TASK_STATUS_READY) {
        return (UINT8 *)"Ready";
    } else if (taskStatus & OS_TASK_STATUS_FROZEN) {
        return (UINT8 *)"Frozen";
    } else if (taskStatus & OS_TASK_STATUS_SUSPENDED) {
        return (UINT8 *)"Suspended";
    } else if (taskStatus & OS_TASK_STATUS_DELAY) {
        return (UINT8 *)"Delay";
    } else if (taskStatus & OS_TASK_STATUS_PEND_TIME) {
        return (UINT8 *)"PendTime";
    } else if (taskStatus & OS_TASK_STATUS_PENDING) {
        return (UINT8 *)"Pending";
    } else if (taskStatus & OS_TASK_STATUS_EXIT) {
        return (UINT8 *)"Exit";
    }

    return (UINT8 *)"Invalid";
}

#ifdef LOSCFG_SHELL_CMD_DEBUG
#define OS_PEND_REASON_MAX_LEN 20

STATIC CHAR *CheckTaskWaitFlag(const TaskInfo *taskInfo, UINTPTR *lockID)
{
    *lockID = taskInfo->waitID;
    switch (taskInfo->waitFlag) {
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

STATIC VOID TaskPendingReasonInfoGet(const TaskInfo *taskInfo, CHAR *pendReason, UINT32 maxLen, UINTPTR *lockID)
{
    CHAR *reason = NULL;

    if (!(taskInfo->status & OS_TASK_STATUS_PENDING)) {
        reason = (CHAR *)ConvertTaskStatusToString(taskInfo->status);
        goto EXIT;
    }

    reason = CheckTaskWaitFlag(taskInfo, lockID);
    if (reason == NULL) {
        reason = "Others";
    }

    if (taskInfo->taskMux != NULL) {
        *lockID = (UINTPTR)taskInfo->taskMux;
        LosTaskCB *owner = ((LosMux *)taskInfo->taskMux)->owner;
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

STATIC VOID TaskInfoTitle(VOID *seqBuf, UINT16 flag)
{
    SHOW("\r\n  TID  PID");
#ifdef LOSCFG_KERNEL_SMP
    SHOW(" Affi CPU");
#endif
    SHOW("    Status Policy Priority StackSize WaterLine");
    if (flag & OS_PROCESS_INFO_ALL) {
#ifdef LOSCFG_KERNEL_CPUP
        SHOW(" CPUUSE CPUUSE10s CPUUSE1s");
#endif /* LOSCFG_KERNEL_CPUP */
#ifdef LOSCFG_SHELL_CMD_DEBUG
        SHOW("   StackPoint  TopOfStack PendReason     LockID");
#endif
    } else {
#ifdef LOSCFG_KERNEL_CPUP
        SHOW("  CPUUSE10s ");
#endif /* LOSCFG_KERNEL_CPUP */
    }
    SHOW("  TaskName\n");
}

STATIC VOID TaskInfoDataShow(const TaskInfo *taskInfo, VOID *seqBuf, UINT16 flag)
{
#ifdef LOSCFG_SHELL_CMD_DEBUG
    UINTPTR lockID = 0;
    CHAR pendReason[OS_PEND_REASON_MAX_LEN] = { 0 };
#endif
    SHOW(" %4u%5u", taskInfo->tid, taskInfo->pid);

#ifdef LOSCFG_KERNEL_SMP
    SHOW("%#5x%4d ", taskInfo->cpuAffiMask, (INT16)(taskInfo->currCpu));
#endif
    SHOW("%9s%7s%9u%#10x%#10x", ConvertTaskStatusToString(taskInfo->status),
         ConvertSchedPolicyToString(taskInfo->policy), taskInfo->priority, taskInfo->stackSize, taskInfo->waterLine);
    if (flag & OS_PROCESS_INFO_ALL) {
#ifdef LOSCFG_KERNEL_CPUP
        SHOW("%4u.%-2u%7u.%-2u%6u.%-2u ", taskInfo->cpupAllsUsage / CPUP_MULT, taskInfo->cpupAllsUsage % CPUP_MULT,
             taskInfo->cpup10sUsage / CPUP_MULT, taskInfo->cpup10sUsage % CPUP_MULT,
             taskInfo->cpup1sUsage / CPUP_MULT, taskInfo->cpup1sUsage % CPUP_MULT);
#endif /* LOSCFG_KERNEL_CPUP */
#ifdef LOSCFG_SHELL_CMD_DEBUG
        TaskPendingReasonInfoGet(taskInfo, pendReason, OS_PEND_REASON_MAX_LEN, &lockID);
        SHOW("%#12x%#12x%11s%#11x", taskInfo->stackPoint, taskInfo->topOfStack, pendReason, lockID);
#endif
    } else {
#ifdef LOSCFG_KERNEL_CPUP
        SHOW("%8u.%-2u ", taskInfo->cpup10sUsage / CPUP_MULT, taskInfo->cpup10sUsage % CPUP_MULT);
#endif /* LOSCFG_KERNEL_CPUP */
    }
    SHOW("  %-32s\n", taskInfo->name);
}

STATIC VOID ProcessTaskInfoDataShow(const ProcessThreadInfo *allTaskInfo, VOID *seqBuf, UINT16 flag)
{
    for (UINT32 index = 0; index < allTaskInfo->threadCount; index++) {
        const TaskInfo *taskInfo = &allTaskInfo->taskInfo[index];
        TaskInfoDataShow(taskInfo, seqBuf, flag);
    }
}

STATIC VOID TaskInfoData(const ProcessThreadInfo *allTaskInfo, VOID *seqBuf, UINT16 flag)
{
    ProcessInfoTitle(seqBuf, flag);
    ProcessDataShow(&allTaskInfo->processInfo, seqBuf, flag);
    TaskInfoTitle(seqBuf, flag);
    ProcessTaskInfoDataShow(allTaskInfo, seqBuf, flag);
}

LITE_OS_SEC_TEXT_MINOR UINT32 OsShellCmdTskInfoGet(UINT32 processID, VOID *seqBuf, UINT16 flag)
{
    UINT32 size;

    if (processID == OS_ALL_TASK_MASK) {
        size = sizeof(ProcessInfo) * g_processMaxNum;
        ProcessInfo *pcbArray = (ProcessInfo *)LOS_MemAlloc(m_aucSysMem1, size);
        if (pcbArray == NULL) {
            PRINT_ERR("Memory is not enough to save task info!\n");
            return LOS_NOK;
        }
        (VOID)memset_s(pcbArray, size, 0, size);
        OsGetAllProcessInfo(pcbArray);
        ProcessInfoShow((const ProcessInfo *)pcbArray, seqBuf, flag);
        (VOID)LOS_MemFree(m_aucSysMem1, pcbArray);
        return LOS_OK;
    }

    ProcessThreadInfo *threadInfo = (ProcessThreadInfo *)LOS_MemAlloc(m_aucSysMem1, sizeof(ProcessThreadInfo));
    if (threadInfo == NULL) {
        return LOS_NOK;
    }
    (VOID)memset_s(threadInfo, sizeof(ProcessThreadInfo), 0, sizeof(ProcessThreadInfo));

    if (OsGetProcessThreadInfo(processID, threadInfo) != LOS_OK) {
        return LOS_NOK;
    }

    TaskInfoData(threadInfo, seqBuf, flag);
    (VOID)LOS_MemFree(m_aucSysMem1, threadInfo);
    return LOS_OK;
}

LITE_OS_SEC_TEXT_MINOR UINT32 OsShellCmdDumpTask(INT32 argc, const CHAR **argv)
{
    INT32 processID = OS_ALL_TASK_MASK;
    UINT32 flag = 0;
#ifdef LOSCFG_KERNEL_VM
    flag |= OS_PROCESS_MEM_INFO;
#endif

    if (argc == 0) {
        return OsShellCmdTskInfoGet((UINT32)processID, NULL, flag);
    }

    if (argc >= 3) { /* 3: The task shell name restricts the parameters */
        goto TASK_HELP;
    }

    if ((argc == 1) && (strcmp("-a", argv[0]) == 0)) {
        flag |= OS_PROCESS_INFO_ALL;
    } else if ((argc == 2) && (strcmp("-p", argv[0]) == 0)) { /* 2: Two parameters */
        flag |= OS_PROCESS_INFO_ALL;
        processID = atoi(argv[1]);
#ifdef LOSCFG_SCHED_DEBUG
#ifdef LOSCFG_SCHED_TICK_DEBUG
    } else if (strcmp("-i", argv[0]) == 0) {
        if (!OsShellShowTickResponse()) {
            return LOS_OK;
        }
        goto TASK_HELP;
#endif
#ifdef LOSCFG_SCHED_HPF_DEBUG
    } else if (strcmp("-t", argv[0]) == 0) {
        if (!OsShellShowSchedStatistics()) {
            return LOS_OK;
        }
        goto TASK_HELP;
#endif
#ifdef LOSCFG_SCHED_EDF_DEBUG
    } else if (strcmp("-e", argv[0]) == 0) {
        if (!OsShellShowEdfSchedStatistics()) {
            return LOS_OK;
        }
        goto TASK_HELP;
#endif
#endif
    } else {
        goto TASK_HELP;
    }

    return OsShellCmdTskInfoGet((UINT32)processID, NULL, flag);

TASK_HELP:
    PRINTK("Unknown option: %s\n", argv[0]);
    PRINTK("Usage:\n");
    PRINTK(" task          --- Basic information about all created processes.\n");
    PRINTK(" task -a       --- Complete information about all created processes.\n");
    PRINTK(" task -p [pid] --- Complete information about specifies processes and its task.\n");
    return LOS_NOK;
}

#ifdef LOSCFG_SHELL
SHELLCMD_ENTRY(task_shellcmd, CMD_TYPE_EX, "task", 1, (CmdCallBackFunc)OsShellCmdDumpTask);
#endif

