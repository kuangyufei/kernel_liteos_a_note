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
//获取当前进程的父进程ID
STATIC UINT32 GetCurrParentPid(UINT32 pid, const LosProcessCB *processCB)
{
    if (processCB->parentProcess == NULL) {
        return 0;
    }

#ifdef LOSCFG_PID_CONTAINER //从容器中获取
    if (pid == OS_USER_ROOT_PROCESS_ID) {//从这里可以看出 0号进程（kidle）是，1,2号进程的父进程
        return 0;  // 返回0，表示没有父进程
    }

    // 检查父进程是否在当前容器中
    if (OS_PROCESS_CONTAINER_CHECK(processCB->parentProcess, OsCurrProcessGet())) {
        return OsGetVpidFromCurrContainer(processCB->parentProcess); // 从当前容器中获取虚拟PID
    }
#endif
    return processCB->parentProcess->processID; // 返回父进程的实际PID
}
//获取当前任务ID
STATIC INLINE UINT32 GetCurrTid(const LosTaskCB *taskCB)
{
#ifdef LOSCFG_PID_CONTAINER
    // 如果任务不在当前容器中
    if (taskCB->pidContainer != OsCurrTaskGet()->pidContainer) {
        return OsGetVtidFromCurrContainer(taskCB); // 从当前容器中获取虚拟TID
    }
#endif
    return taskCB->taskID; // 返回任务的实际ID
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

/*
该函数用于获取指定进程的所有线程信息
遍历进程的线程列表，收集每个线程的详细信息
包括线程ID、状态、调度信息、栈信息、CPU使用率等
使用了条件编译来处理SMP和CPUP相关的功能
*/
STATIC VOID GetThreadInfo(ProcessThreadInfo *threadInfo, LosProcessCB *processCB)
{
    SchedParam param = {0};
    LosTaskCB *taskCB = NULL;
    
    // 如果线程列表为空，设置线程计数为0并返回
    if (LOS_ListEmpty(&processCB->threadSiblingList)) {
        threadInfo->threadCount = 0;
        return;
    }

    threadInfo->threadCount = 0;  // 初始化线程计数
    // 遍历进程的线程列表
    LOS_DL_LIST_FOR_EACH_ENTRY(taskCB, &processCB->threadSiblingList, LosTaskCB, threadList) {
        TaskInfo *taskInfo = &threadInfo->taskInfo[threadInfo->threadCount];  // 获取当前线程信息结构体
        taskInfo->tid = GetCurrTid(taskCB);  // 获取线程ID
        taskInfo->pid = OsGetPid(processCB);  // 获取进程ID
        taskInfo->status = taskCB->taskStatus;  // 获取线程状态
        taskCB->ops->schedParamGet(taskCB, &param);  // 获取调度参数
        taskInfo->policy = param.policy;  // 获取调度策略
        taskInfo->priority = param.priority;  // 获取线程优先级
#ifdef LOSCFG_KERNEL_SMP
        taskInfo->currCpu = taskCB->currCpu;  // 获取当前运行的CPU
        taskInfo->cpuAffiMask = taskCB->cpuAffiMask;  // 获取CPU亲和性掩码
#endif
        taskInfo->stackPoint = (UINTPTR)taskCB->stackPointer;  // 获取栈指针
        taskInfo->topOfStack = taskCB->topOfStack;  // 获取栈顶
        taskInfo->stackSize = taskCB->stackSize;  // 获取栈大小
        taskInfo->waitFlag = taskCB->waitFlag;  // 获取等待标志
        taskInfo->waitID = taskCB->waitID;  // 获取等待ID
        taskInfo->taskMux = taskCB->taskMux;  // 获取任务互斥量
        // 获取栈水位线
        (VOID)OsStackWaterLineGet((const UINTPTR *)(taskCB->topOfStack + taskCB->stackSize),
                                  (const UINTPTR *)taskCB->topOfStack, &taskInfo->waterLine);
#ifdef LOSCFG_KERNEL_CPUP
        // 获取CPU使用率
        (VOID)OsGetTaskAllCpuUsageUnsafe(&taskCB->taskCpup, taskInfo);
#endif
        // 复制任务名称
        (VOID)memcpy_s(taskInfo->name, OS_TCB_NAME_LEN, taskCB->taskName, OS_TCB_NAME_LEN);
        threadInfo->threadCount++;  // 增加线程计数
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

// 获取所有进程的信息
UINT32 OsGetAllProcessInfo(ProcessInfo *pcbArray)
{
    UINT32 intSave;
    if (pcbArray == NULL) {
        return LOS_NOK;  // 如果传入的数组为空，返回错误
    }

    ProcessMemUsageGet(pcbArray);  // 获取所有进程的内存使用信息

    SCHEDULER_LOCK(intSave);  // 加锁，保证线程安全
#ifdef LOSCFG_PID_CONTAINER
    // 如果启用了PID容器功能
    PidContainer *pidContainer = OsCurrTaskGet()->pidContainer;  // 获取当前任务的PID容器
    for (UINT32 index = 0; index < LOSCFG_BASE_CORE_PROCESS_LIMIT; index++) {
        ProcessVid *processVid = &pidContainer->pidArray[index];  // 获取进程虚拟ID
        LosProcessCB *processCB = (LosProcessCB *)processVid->cb;  // 获取进程控制块
#else
    // 未启用PID容器功能
    for (UINT32 index = 0; index < LOSCFG_BASE_CORE_PROCESS_LIMIT; index++) {
        LosProcessCB *processCB = OS_PCB_FROM_RPID(index);  // 直接通过索引获取进程控制块
#endif
        ProcessInfo *pcbInfo = pcbArray + index;  // 获取当前进程的信息结构体
        if (OsProcessIsUnused(processCB)) {
            pcbInfo->status = OS_PROCESS_FLAG_UNUSED;  // 如果进程未使用，标记状态为未使用
            continue;
        }
        GetProcessInfo(pcbInfo, processCB);  // 获取进程的详细信息
    }
    SCHEDULER_UNLOCK(intSave);  // 解锁
    return LOS_OK;  // 返回成功
}
