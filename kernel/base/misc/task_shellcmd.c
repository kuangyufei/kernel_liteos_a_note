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

// 进程内存信息标志位，用于控制是否显示进程内存相关信息
#define OS_PROCESS_MEM_INFO 0x2U
#undef SHOW  // 取消之前可能的SHOW宏定义

#ifdef LOSCFG_FS_VFS  // 如果启用了虚拟文件系统
#if defined(LOSCFG_BLACKBOX) && defined(LOSCFG_SAVE_EXCINFO)  // 如果同时启用了黑盒和异常信息保存功能
#define SaveExcInfo(arg, ...) WriteExcInfoToBuf(arg, ##__VA_ARGS__)  // 定义保存异常信息的宏
#else
#define SaveExcInfo(arg, ...)  // 空宏定义，不执行任何操作
#endif
// 定义SHOW宏，根据seqBuf是否为空选择输出方式，并保存异常信息
#define SHOW(arg...) do {                                    \
    if (seqBuf != NULL) {                                    \
        (void)LosBufPrintf((struct SeqBuf *)seqBuf, ##arg);  \
    } else {                                                 \
        PRINTK(arg);                                         \
    }                                                        \
    SaveExcInfo(arg);                                        \
} while (0)                                                  \
#else
#define SHOW(arg...) PRINTK(arg)  // 如果未启用VFS，直接使用PRINTK输出
#endif

// CPU使用率计算的精度乘数，用于将百分比转换为整数处理
#define CPUP_MULT LOS_CPUP_PRECISION_MULT

/**
 * @brief 将进程模式转换为字符串表示
 * @param mode 进程模式，OS_KERNEL_MODE或OS_USER_MODE
 * @return 指向模式字符串的指针，未知模式返回"ERROR"
 */
STATIC UINT8 *ConvertProcessModeToString(UINT16 mode)
{
    if (mode == OS_KERNEL_MODE) {
        return (UINT8 *)"kernel";  // 内核模式
    } else if (mode == OS_USER_MODE) {
        return (UINT8 *)"user";    // 用户模式
    }

    return (UINT8 *)"ERROR";  // 未知模式
}

/**
 * @brief 将调度策略转换为字符串表示
 * @param policy 调度策略，如LOS_SCHED_RR、LOS_SCHED_FIFO等
 * @return 指向策略字符串的指针，未知策略返回"ERROR"
 */
STATIC UINT8 *ConvertSchedPolicyToString(UINT16 policy)
{
    if (policy == LOS_SCHED_RR) {
        return (UINT8 *)"RR";       // 时间片轮转调度
    } else if (policy == LOS_SCHED_FIFO) {
        return (UINT8 *)"FIFO";     // 先进先出调度
    } else if (policy == LOS_SCHED_DEADLINE) {
        return (UINT8 *)"EDF";      // 截止时间最早优先调度
    } else if (policy == LOS_SCHED_IDLE) {
        return (UINT8 *)"IDLE";     // 空闲任务调度
    }

    return (UINT8 *)"ERROR";  // 未知调度策略
}

/**
 * @brief 将进程状态转换为字符串表示
 * @param status 进程状态标志位
 * @return 指向状态字符串的指针
 */
STATIC UINT8 *ConvertProcessStatusToString(UINT16 status)
{
    if (status & OS_PROCESS_STATUS_ZOMBIES) {
        return (UINT8 *)"Zombies";  // 僵尸状态
    } else if (status & OS_PROCESS_STATUS_INIT) {
        return (UINT8 *)"Init";     // 初始化状态
    } else if (status & OS_PROCESS_STATUS_RUNNING) {
        return (UINT8 *)"Running";  // 运行状态
    } else if (status & OS_PROCESS_STATUS_READY) {
        return (UINT8 *)"Ready";    // 就绪状态
    }
    return (UINT8 *)"Pending";       // 挂起状态
}

/**
 * @brief 显示进程信息标题栏
 * @param seqBuf 序列缓冲区指针，用于输出重定向
 * @param flag 显示标志位，控制显示内容
 */
STATIC VOID ProcessInfoTitle(VOID *seqBuf, UINT16 flag)
{
    SHOW("\r\n  PID  PPID PGID   UID   Mode  Status Policy Priority MTID TTotal");
    if (flag & OS_PROCESS_INFO_ALL) {  // 如果需要显示全部信息
#ifdef LOSCFG_KERNEL_VM  // 如果启用了虚拟内存
        if (flag & OS_PROCESS_MEM_INFO) {  // 如果需要显示内存信息
            SHOW(" VirtualMem ShareMem PhysicalMem");  // 显示内存相关列标题
        }
#endif
#ifdef LOSCFG_KERNEL_CPUP  // 如果启用了CPU使用率统计
        SHOW(" CPUUSE CPUUSE10s CPUUSE1s");  // 显示CPU使用率相关列标题
#endif /* LOSCFG_KERNEL_CPUP */
    } else {
#ifdef LOSCFG_KERNEL_CPUP
        SHOW(" CPUUSE10s");  // 仅显示10秒CPU使用率
#endif /* LOSCFG_KERNEL_CPUP */
    }
    SHOW(" PName\n");  // 进程名称列标题
}

/**
 * @brief 显示单个进程的详细信息
 * @param processInfo 进程信息结构体指针
 * @param seqBuf 序列缓冲区指针，用于输出重定向
 * @param flag 显示标志位，控制显示内容
 */
STATIC VOID ProcessDataShow(const ProcessInfo *processInfo, VOID *seqBuf, UINT16 flag)
{
    // 显示进程基本信息：PID、PPID、PGID、UID、模式、状态、策略、优先级、线程组ID、线程总数
    SHOW("%5u%6u%5d%6d%7s%8s%7s%9u%5u%7u", processInfo->pid, processInfo->ppid, processInfo->pgroupID,
         processInfo->userID, ConvertProcessModeToString(processInfo->mode),
         ConvertProcessStatusToString(processInfo->status),
         ConvertSchedPolicyToString(processInfo->policy), processInfo->basePrio,
         processInfo->threadGroupID, processInfo->threadNumber);

    if (flag & OS_PROCESS_INFO_ALL) {  // 如果需要显示全部信息
#ifdef LOSCFG_KERNEL_VM  // 如果启用了虚拟内存
        if (flag & OS_PROCESS_MEM_INFO) {  // 如果需要显示内存信息
            // 显示虚拟内存、共享内存、物理内存（十六进制）
            SHOW("%#11x%#9x%#12x", processInfo->virtualMem, processInfo->shareMem, processInfo->physicalMem);
        }
#endif
#ifdef LOSCFG_KERNEL_CPUP  // 如果启用了CPU使用率统计
        // 显示CPU使用率（总使用率、10秒使用率、1秒使用率）
        SHOW("%4u.%-2u%7u.%-2u%6u.%-2u ",
             processInfo->cpupAllsUsage / CPUP_MULT, processInfo->cpupAllsUsage % CPUP_MULT,
             processInfo->cpup10sUsage / CPUP_MULT, processInfo->cpup10sUsage % CPUP_MULT,
             processInfo->cpup1sUsage / CPUP_MULT, processInfo->cpup1sUsage % CPUP_MULT);
#endif /* LOSCFG_KERNEL_CPUP */
    } else {
#ifdef LOSCFG_KERNEL_CPUP
        // 仅显示10秒CPU使用率
        SHOW("%7u.%-2u ", processInfo->cpup10sUsage / CPUP_MULT, processInfo->cpup10sUsage % CPUP_MULT);
#endif /* LOSCFG_KERNEL_CPUP */
    }
    SHOW("%-32s\n", processInfo->name);  // 显示进程名称
}

/**
 * @brief 显示所有进程的信息
 * @param pcbArray 进程信息数组指针
 * @param seqBuf 序列缓冲区指针，用于输出重定向
 * @param flag 显示标志位，控制显示内容
 */
STATIC VOID AllProcessDataShow(const ProcessInfo *pcbArray, VOID *seqBuf, UINT16 flag)
{
    // 遍历所有可能的PID
    for (UINT32 pid = 1; pid < g_processMaxNum; ++pid) {
        const ProcessInfo *processInfo = pcbArray + pid;  // 获取当前PID对应的进程信息
        if (processInfo->status & OS_PROCESS_FLAG_UNUSED) {  // 跳过未使用的进程槽位
            continue;
        }
        ProcessDataShow(processInfo, seqBuf, flag);  // 显示进程信息
    }
}

/**
 * @brief 显示系统中所有进程的汇总信息
 * @param pcbArray 进程信息数组指针
 * @param seqBuf 序列缓冲区指针，用于输出重定向
 * @param flag 显示标志位，控制显示内容
 */
STATIC VOID ProcessInfoShow(const ProcessInfo *pcbArray, VOID *seqBuf, UINT16 flag)
{
#ifdef LOSCFG_KERNEL_CPUP  // 如果启用了CPU使用率统计
    UINT32 pid = OS_KERNEL_IDLE_PROCESS_ID;  // 空闲进程ID
    UINT32 sysUsage = LOS_CPUP_PRECISION - pcbArray[pid].cpup10sUsage;  // 计算系统CPU使用率
    // 显示总CPU使用率：系统使用率和空闲使用率
    SHOW("\n  allCpu(%%): %4u.%02u sys, %4u.%02u idle\n", sysUsage / CPUP_MULT, sysUsage % CPUP_MULT,
         pcbArray[pid].cpup10sUsage / CPUP_MULT, pcbArray[pid].cpup10sUsage % CPUP_MULT);
#endif

    ProcessInfoTitle(seqBuf, flag);  // 显示进程信息标题栏
    AllProcessDataShow(pcbArray, seqBuf, flag);  // 显示所有进程数据
}

/**
 * @brief 将任务状态转换为字符串表示
 * @param taskStatus 任务状态标志位
 * @return 指向状态字符串的指针，未知状态返回"Invalid"
 */
STATIC UINT8 *ConvertTaskStatusToString(UINT16 taskStatus)
{
    if (taskStatus & OS_TASK_STATUS_INIT) {
        return (UINT8 *)"Init";       // 初始化状态
    } else if (taskStatus & OS_TASK_STATUS_RUNNING) {
        return (UINT8 *)"Running";    // 运行状态
    } else if (taskStatus & OS_TASK_STATUS_READY) {
        return (UINT8 *)"Ready";      // 就绪状态
    } else if (taskStatus & OS_TASK_STATUS_FROZEN) {
        return (UINT8 *)"Frozen";     // 冻结状态
    } else if (taskStatus & OS_TASK_STATUS_SUSPENDED) {
        return (UINT8 *)"Suspended";  // 挂起状态
    } else if (taskStatus & OS_TASK_STATUS_DELAY) {
        return (UINT8 *)"Delay";      // 延时状态
    } else if (taskStatus & OS_TASK_STATUS_PEND_TIME) {
        return (UINT8 *)"PendTime";   // 等待超时状态
    } else if (taskStatus & OS_TASK_STATUS_PENDING) {
        return (UINT8 *)"Pending";    // 等待状态
    } else if (taskStatus & OS_TASK_STATUS_EXIT) {
        return (UINT8 *)"Exit";       // 退出状态
    }

    return (UINT8 *)"Invalid";  // 无效状态
}

#ifdef LOSCFG_SHELL_CMD_DEBUG  // 如果启用了shell命令调试
#define OS_PEND_REASON_MAX_LEN 20  // 等待原因字符串最大长度

/**
 * @brief 检查任务等待标志并返回对应的等待原因
 * @param taskInfo 任务信息结构体指针
 * @param lockID 用于存储锁ID的指针
 * @return 指向等待原因字符串的指针，未知原因返回NULL
 */
STATIC CHAR *CheckTaskWaitFlag(const TaskInfo *taskInfo, UINTPTR *lockID)
{
    *lockID = taskInfo->waitID;  // 获取等待ID
    switch (taskInfo->waitFlag) {  // 根据等待标志判断等待原因
        case OS_TASK_WAIT_PROCESS:
            return "Child";       // 等待子进程
        case OS_TASK_WAIT_GID:
            return "PGroup";      // 等待进程组
        case OS_TASK_WAIT_ANYPROCESS:
            return "AnyChild";    // 等待任意子进程
        case OS_TASK_WAIT_SEM:
            return "Semaphore";   // 等待信号量
        case OS_TASK_WAIT_QUEUE:
            return "Queue";       // 等待队列
        case OS_TASK_WAIT_JOIN:
            return "Join";        // 等待任务连接
        case OS_TASK_WAIT_SIGNAL:
            return "Signal";      // 等待信号
        case OS_TASK_WAIT_LITEIPC:
            return "LiteIPC";     // 等待轻量级IPC
        case OS_TASK_WAIT_MUTEX:
            return "Mutex";       // 等待互斥锁
        case OS_TASK_WAIT_EVENT:
            return "Event";       // 等待事件
        case OS_TASK_WAIT_FUTEX:
            return "Futex";       // 等待Futex
        case OS_TASK_WAIT_COMPLETE:
            return "Complete";    // 等待完成量
        default:
            break;
    }

    return NULL;  // 未知等待原因
}

/**
 * @brief 获取任务等待原因信息
 * @param taskInfo 任务信息结构体指针
 * @param pendReason 用于存储等待原因的缓冲区
 * @param maxLen 缓冲区最大长度
 * @param lockID 用于存储锁ID的指针
 */
STATIC VOID TaskPendingReasonInfoGet(const TaskInfo *taskInfo, CHAR *pendReason, UINT32 maxLen, UINTPTR *lockID)
{
    CHAR *reason = NULL;

    if (!(taskInfo->status & OS_TASK_STATUS_PENDING)) {  // 如果任务不处于等待状态
        reason = (CHAR *)ConvertTaskStatusToString(taskInfo->status);  // 获取任务状态
        goto EXIT;
    }

    reason = CheckTaskWaitFlag(taskInfo, lockID);  // 检查等待标志
    if (reason == NULL) {
        reason = "Others";  // 其他等待原因
    }

    if (taskInfo->taskMux != NULL) {  // 如果任务正在等待互斥锁
        *lockID = (UINTPTR)taskInfo->taskMux;  // 获取互斥锁ID
        LosTaskCB *owner = ((LosMux *)taskInfo->taskMux)->owner;  // 获取互斥锁所有者
        if (owner != NULL) {
            // 格式化互斥锁等待信息，包含所有者任务ID
            if (snprintf_s(pendReason, maxLen, maxLen - 1, "Mutex-%u", owner->taskID) == EOK) {
                return;
            }
        }
    }

EXIT:
    if (strcpy_s(pendReason, maxLen, reason) != EOK) {  // 复制等待原因到缓冲区
        PRINT_ERR("Get pend reason copy failed !\n");  // 复制失败时输出错误信息
    }
}
#endif

/**
 * @brief 显示任务信息标题栏
 * @param seqBuf 序列缓冲区指针，用于输出重定向
 * @param flag 显示标志位，控制显示内容
 */
STATIC VOID TaskInfoTitle(VOID *seqBuf, UINT16 flag)
{
    SHOW("\r\n  TID  PID");
#ifdef LOSCFG_KERNEL_SMP  // 如果启用了SMP（对称多处理）
    SHOW(" Affi CPU");  // 显示CPU亲和性和当前CPU列标题
#endif
    // 显示状态、策略、优先级、堆栈大小、堆栈水位线列标题
    SHOW("    Status Policy Priority StackSize WaterLine");
    if (flag & OS_PROCESS_INFO_ALL) {  // 如果需要显示全部信息
#ifdef LOSCFG_KERNEL_CPUP  // 如果启用了CPU使用率统计
        SHOW(" CPUUSE CPUUSE10s CPUUSE1s");  // 显示CPU使用率相关列标题
#endif /* LOSCFG_KERNEL_CPUP */
#ifdef LOSCFG_SHELL_CMD_DEBUG  // 如果启用了shell命令调试
        // 显示堆栈指针、堆栈顶部、等待原因、锁ID列标题
        SHOW("   StackPoint  TopOfStack PendReason     LockID");
#endif
    } else {
#ifdef LOSCFG_KERNEL_CPUP
        SHOW("  CPUUSE10s ");  // 仅显示10秒CPU使用率
#endif /* LOSCFG_KERNEL_CPUP */
    }
    SHOW("  TaskName\n");  // 任务名称列标题
}

/**
 * @brief 显示单个任务的详细信息
 * @param taskInfo 任务信息结构体指针
 * @param seqBuf 序列缓冲区指针，用于输出重定向
 * @param flag 显示标志位，控制显示内容
 */
STATIC VOID TaskInfoDataShow(const TaskInfo *taskInfo, VOID *seqBuf, UINT16 flag)
{
#ifdef LOSCFG_SHELL_CMD_DEBUG
    UINTPTR lockID = 0;  // 锁ID
    CHAR pendReason[OS_PEND_REASON_MAX_LEN] = { 0 };  // 等待原因缓冲区
#endif
    SHOW(" %4u%5u", taskInfo->tid, taskInfo->pid);  // 显示TID和PID

#ifdef LOSCFG_KERNEL_SMP
    // 显示CPU亲和性掩码和当前CPU
    SHOW("%#5x%4d ", taskInfo->cpuAffiMask, (INT16)(taskInfo->currCpu));
#endif
    // 显示状态、策略、优先级、堆栈大小、堆栈水位线
    SHOW("%9s%7s%9u%#10x%#10x", ConvertTaskStatusToString(taskInfo->status),
         ConvertSchedPolicyToString(taskInfo->policy), taskInfo->priority, taskInfo->stackSize, taskInfo->waterLine);
    if (flag & OS_PROCESS_INFO_ALL) {  // 如果需要显示全部信息
#ifdef LOSCFG_KERNEL_CPUP
        // 显示CPU使用率（总使用率、10秒使用率、1秒使用率）
        SHOW("%4u.%-2u%7u.%-2u%6u.%-2u ", taskInfo->cpupAllsUsage / CPUP_MULT, taskInfo->cpupAllsUsage % CPUP_MULT,
             taskInfo->cpup10sUsage / CPUP_MULT, taskInfo->cpup10sUsage % CPUP_MULT,
             taskInfo->cpup1sUsage / CPUP_MULT, taskInfo->cpup1sUsage % CPUP_MULT);
#endif /* LOSCFG_KERNEL_CPUP */
#ifdef LOSCFG_SHELL_CMD_DEBUG
        // 获取等待原因信息
        TaskPendingReasonInfoGet(taskInfo, pendReason, OS_PEND_REASON_MAX_LEN, &lockID);
        // 显示堆栈指针、堆栈顶部、等待原因、锁ID
        SHOW("%#12x%#12x%11s%#11x", taskInfo->stackPoint, taskInfo->topOfStack, pendReason, lockID);
#endif
    } else {
#ifdef LOSCFG_KERNEL_CPUP
        // 仅显示10秒CPU使用率
        SHOW("%8u.%-2u ", taskInfo->cpup10sUsage / CPUP_MULT, taskInfo->cpup10sUsage % CPUP_MULT);
#endif /* LOSCFG_KERNEL_CPUP */
    }
    SHOW("  %-32s\n", taskInfo->name);  // 显示任务名称
}

/**
 * @brief 显示进程的所有任务信息
 * @param allTaskInfo 进程线程信息结构体指针
 * @param seqBuf 序列缓冲区指针，用于输出重定向
 * @param flag 显示标志位，控制显示内容
 */
STATIC VOID ProcessTaskInfoDataShow(const ProcessThreadInfo *allTaskInfo, VOID *seqBuf, UINT16 flag)
{
    // 遍历所有任务
    for (UINT32 index = 0; index < allTaskInfo->threadCount; index++) {
        const TaskInfo *taskInfo = &allTaskInfo->taskInfo[index];  // 获取当前任务信息
        TaskInfoDataShow(taskInfo, seqBuf, flag);  // 显示任务信息
    }
}

/**
 * @brief 显示进程及其任务的信息
 * @param allTaskInfo 进程线程信息结构体指针
 * @param seqBuf 序列缓冲区指针，用于输出重定向
 * @param flag 显示标志位，控制显示内容
 */
STATIC VOID TaskInfoData(const ProcessThreadInfo *allTaskInfo, VOID *seqBuf, UINT16 flag)
{
    ProcessInfoTitle(seqBuf, flag);  // 显示进程信息标题栏
    ProcessDataShow(&allTaskInfo->processInfo, seqBuf, flag);  // 显示进程信息
    TaskInfoTitle(seqBuf, flag);  // 显示任务信息标题栏
    ProcessTaskInfoDataShow(allTaskInfo, seqBuf, flag);  // 显示所有任务信息
}

/**
 * @brief 获取并显示任务信息
 * @param processID 进程ID，OS_ALL_TASK_MASK表示所有进程
 * @param seqBuf 序列缓冲区指针，用于输出重定向
 * @param flag 显示标志位，控制显示内容
 * @return 成功返回LOS_OK，失败返回LOS_NOK
 */
LITE_OS_SEC_TEXT_MINOR UINT32 OsShellCmdTskInfoGet(UINT32 processID, VOID *seqBuf, UINT16 flag)
{
    UINT32 size;

    if (processID == OS_ALL_TASK_MASK) {  // 如果需要显示所有进程
        size = sizeof(ProcessInfo) * g_processMaxNum;  // 计算进程信息数组大小
        // 分配内存存储所有进程信息
        ProcessInfo *pcbArray = (ProcessInfo *)LOS_MemAlloc(m_aucSysMem1, size);
        if (pcbArray == NULL) {  // 内存分配失败
            PRINT_ERR("Memory is not enough to save task info!\n");
            return LOS_NOK;
        }
        (VOID)memset_s(pcbArray, size, 0, size);  // 初始化内存
        OsGetAllProcessInfo(pcbArray);  // 获取所有进程信息
        ProcessInfoShow((const ProcessInfo *)pcbArray, seqBuf, flag);  // 显示进程信息
        (VOID)LOS_MemFree(m_aucSysMem1, pcbArray);  // 释放内存
        return LOS_OK;
    }

    // 分配内存存储进程线程信息
    ProcessThreadInfo *threadInfo = (ProcessThreadInfo *)LOS_MemAlloc(m_aucSysMem1, sizeof(ProcessThreadInfo));
    if (threadInfo == NULL) {  // 内存分配失败
        return LOS_NOK;
    }
    (VOID)memset_s(threadInfo, sizeof(ProcessThreadInfo), 0, sizeof(ProcessThreadInfo));  // 初始化内存

    if (OsGetProcessThreadInfo(processID, threadInfo) != LOS_OK) {  // 获取进程线程信息失败
        (VOID)LOS_MemFree(m_aucSysMem1, threadInfo);  // 释放内存
        return LOS_NOK;
    }

    TaskInfoData(threadInfo, seqBuf, flag);  // 显示进程及其任务信息
    (VOID)LOS_MemFree(m_aucSysMem1, threadInfo);  // 释放内存
    return LOS_OK;
}

/**
 * @brief 处理任务信息显示命令
 * @param argc 参数数量
 * @param argv 参数数组
 * @return 成功返回LOS_OK，失败返回LOS_NOK
 */
LITE_OS_SEC_TEXT_MINOR UINT32 OsShellCmdDumpTask(INT32 argc, const CHAR **argv)
{
    INT32 processID = OS_ALL_TASK_MASK;  // 默认显示所有进程
    UINT32 flag = 0;  // 显示标志位
#ifdef LOSCFG_KERNEL_VM
    flag |= OS_PROCESS_MEM_INFO;  // 如果启用虚拟内存，默认显示内存信息
#endif

    if (argc == 0) {  // 无参数，显示基本信息
        return OsShellCmdTskInfoGet((UINT32)processID, NULL, flag);
    }

    if (argc >= 3) {  // 参数数量过多
        goto TASK_HELP;
    }

    if ((argc == 1) && (strcmp("-a", argv[0]) == 0)) {  // 显示所有信息
        flag |= OS_PROCESS_INFO_ALL;
    } else if ((argc == 2) && (strcmp("-p", argv[0]) == 0)) {  // 显示指定进程信息
        flag |= OS_PROCESS_INFO_ALL;
        processID = atoi(argv[1]);  // 转换进程ID
#ifdef LOSCFG_SCHED_DEBUG
#ifdef LOSCFG_SCHED_TICK_DEBUG
    } else if (strcmp("-i", argv[0]) == 0) {  // 显示调度滴答响应信息
        if (!OsShellShowTickResponse()) {
            return LOS_OK;
        }
        goto TASK_HELP;
#endif
#ifdef LOSCFG_SCHED_HPF_DEBUG
    } else if (strcmp("-t", argv[0]) == 0) {  // 显示调度统计信息
        if (!OsShellShowSchedStatistics()) {
            return LOS_OK;
        }
        goto TASK_HELP;
#endif
#ifdef LOSCFG_SCHED_EDF_DEBUG
    } else if (strcmp("-e", argv[0]) == 0) {  // 显示EDF调度统计信息
        if (!OsShellShowEdfSchedStatistics()) {
            return LOS_OK;
        }
        goto TASK_HELP;
#endif
#endif
    } else {  // 未知参数
        goto TASK_HELP;
    }

    return OsShellCmdTskInfoGet((UINT32)processID, NULL, flag);  // 获取并显示任务信息

TASK_HELP:  // 显示命令帮助信息
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

