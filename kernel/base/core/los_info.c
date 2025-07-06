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

/**
 * @brief 获取当前进程的父进程ID
 * @details 根据进程控制块获取父进程ID，支持容器环境下的虚拟PID转换
 * @param pid 当前进程ID
 * @param processCB 当前进程控制块指针
 * @return 父进程ID，若不存在父进程则返回0
 */
STATIC UINT32 GetCurrParentPid(UINT32 pid, const LosProcessCB *processCB)
{
    if (processCB->parentProcess == NULL) {  // 检查是否存在父进程
        return 0;                            // 无父进程返回0
    }

#ifdef LOSCFG_PID_CONTAINER                  // 若启用PID容器功能
    if (pid == OS_USER_ROOT_PROCESS_ID) {    // 根进程的父进程ID为0
        return 0;
    }

    if (OS_PROCESS_CONTAINER_CHECK(processCB->parentProcess, OsCurrProcessGet())) {  // 检查父进程是否在当前容器
        return OsGetVpidFromCurrContainer(processCB->parentProcess);  // 返回容器内的虚拟PID
    }
#endif
    return processCB->parentProcess->processID;  // 返回实际父进程ID
}

/**
 * @brief 获取当前线程的线程ID
 * @details 支持容器环境下的虚拟TID转换，非容器环境直接返回线程ID
 * @param taskCB 线程控制块指针
 * @return 线程ID（容器环境下为虚拟TID）
 */
STATIC INLINE UINT32 GetCurrTid(const LosTaskCB *taskCB)
{
#ifdef LOSCFG_PID_CONTAINER                  // 若启用PID容器功能
    if (taskCB->pidContainer != OsCurrTaskGet()->pidContainer) {  // 检查线程是否在当前容器
        return OsGetVtidFromCurrContainer(taskCB);  // 返回容器内的虚拟TID
    }
#endif
    return taskCB->taskID;  // 返回实际线程ID
}

/**
 * @brief 获取进程状态
 * @details 综合进程本身状态和所有线程状态，返回合并后的进程状态
 * @param processCB 进程控制块指针
 * @return 合并后的进程状态值
 */
STATIC UINT16 GetProcessStatus(LosProcessCB *processCB)
{
    UINT16 status;                           // 进程状态变量
    LosTaskCB *taskCB = NULL;                // 线程控制块指针

    if (LOS_ListEmpty(&processCB->threadSiblingList)) {  // 检查进程是否有线程
        return processCB->processStatus;     // 无线程时直接返回进程状态
    }

    status = processCB->processStatus;       // 初始化状态为进程本身状态
    LOS_DL_LIST_FOR_EACH_ENTRY(taskCB, &processCB->threadSiblingList, LosTaskCB, threadList) {  // 遍历所有线程
        status |= (taskCB->taskStatus & 0x00FF);  // 合并线程状态的低8位到进程状态
    }
    return status;                           // 返回合并后的状态
}

/**
 * @brief 填充进程基本信息
 * @details 将进程控制块中的信息提取并填充到ProcessInfo结构体中
 * @param pcbInfo 进程信息结构体指针，用于存储输出结果
 * @param processCB 进程控制块指针，作为信息来源
 */
STATIC VOID GetProcessInfo(ProcessInfo *pcbInfo, const LosProcessCB *processCB)
{
    SchedParam param = {0};                  // 调度参数结构体
    pcbInfo->pid = OsGetPid(processCB);      // 获取进程ID
    pcbInfo->ppid = GetCurrParentPid(pcbInfo->pid, processCB);  // 获取父进程ID
    pcbInfo->status = GetProcessStatus((LosProcessCB *)processCB);  // 获取进程状态
    pcbInfo->mode = processCB->processMode;  // 获取进程运行模式（内核/用户）
    if (processCB->pgroup != NULL) {         // 检查是否属于进程组
        pcbInfo->pgroupID = OsGetPid(OS_GET_PGROUP_LEADER(processCB->pgroup));  // 获取进程组ID
    } else {
        pcbInfo->pgroupID = -1;              // 不属于任何进程组时返回-1
    }
#ifdef LOSCFG_SECURITY_CAPABILITY           // 若启用安全能力功能
    if (processCB->user != NULL) {           // 检查用户信息是否存在
        pcbInfo->userID = processCB->user->userID;  // 获取用户ID
    } else {
        pcbInfo->userID = -1;                // 无用户信息时返回-1
    }
#else
    pcbInfo->userID = 0;                     // 未启用安全能力时默认用户ID为0
#endif
    LosTaskCB *taskCB = processCB->threadGroup;  // 获取线程组首线程
    pcbInfo->threadGroupID = taskCB->taskID;     // 获取线程组ID
    taskCB->ops->schedParamGet(taskCB, &param);  // 获取调度参数
    pcbInfo->policy = LOS_SCHED_RR;          // 设置调度策略为RR（时间片轮转）
    pcbInfo->basePrio = param.basePrio;      // 获取基础优先级
    pcbInfo->threadNumber = processCB->threadNumber;  // 获取线程数量
#ifdef LOSCFG_KERNEL_CPUP                    // 若启用CPU占用率统计
    (VOID)OsGetProcessAllCpuUsageUnsafe(processCB->processCpup, pcbInfo);  // 获取CPU使用率
#endif
    (VOID)memcpy_s(pcbInfo->name, OS_PCB_NAME_LEN, processCB->processName, OS_PCB_NAME_LEN);  // 复制进程名称
}

#ifdef LOSCFG_KERNEL_VM                      // 若启用内核虚拟内存
/**
 * @brief 获取进程内存使用信息
 * @details 统计进程的虚拟内存、共享内存和物理内存使用情况，特殊处理空闲任务
 * @param pcbInfo 进程信息结构体指针，用于存储输出结果
 * @param processCB 进程控制块指针
 * @param vmSpace 进程虚拟内存空间指针
 */
STATIC VOID GetProcessMemInfo(ProcessInfo *pcbInfo, const LosProcessCB *processCB, LosVmSpace *vmSpace)
{
    /* 进程内存使用统计，空闲任务默认值为0 */
    if (processCB == &g_processCBArray[0]) {  // 检查是否为空闲任务（IDLE进程）
        pcbInfo->virtualMem = 0;             // 虚拟内存置0
        pcbInfo->shareMem = 0;               // 共享内存置0
        pcbInfo->physicalMem = 0;            // 物理内存置0
    } else if (vmSpace == LOS_GetKVmSpace()) {  // 检查是否为内核虚拟内存空间
        (VOID)OsShellCmdProcessPmUsage(vmSpace, &pcbInfo->shareMem, &pcbInfo->physicalMem);  // 获取物理内存使用
        pcbInfo->virtualMem = pcbInfo->physicalMem;  // 内核进程虚拟内存等于物理内存
    } else {
        pcbInfo->virtualMem = OsShellCmdProcessVmUsage(vmSpace);  // 获取用户进程虚拟内存使用
        if (pcbInfo->virtualMem == 0) {      // 虚拟内存为0表示进程无效
            pcbInfo->status = OS_PROCESS_FLAG_UNUSED;  // 标记进程为未使用
            return;
        }
        if (OsShellCmdProcessPmUsage(vmSpace, &pcbInfo->shareMem, &pcbInfo->physicalMem) == 0) {  // 获取物理内存失败
            pcbInfo->status = OS_PROCESS_FLAG_UNUSED;  // 标记进程为未使用
        }
    }
}
#endif

/**
 * @brief 填充进程线程信息
 * @details 遍历进程所有线程，提取线程信息并填充到ProcessThreadInfo结构体
 * @param threadInfo 进程线程信息结构体指针，用于存储输出结果
 * @param processCB 进程控制块指针
 */
STATIC VOID GetThreadInfo(ProcessThreadInfo *threadInfo, LosProcessCB *processCB)
{
    SchedParam param = {0};                  // 调度参数结构体
    LosTaskCB *taskCB = NULL;                // 线程控制块指针
    if (LOS_ListEmpty(&processCB->threadSiblingList)) {  // 检查是否有线程
        threadInfo->threadCount = 0;         // 线程数量置0
        return;
    }

    threadInfo->threadCount = 0;             // 初始化线程计数器
    LOS_DL_LIST_FOR_EACH_ENTRY(taskCB, &processCB->threadSiblingList, LosTaskCB, threadList) {  // 遍历所有线程
        TaskInfo *taskInfo = &threadInfo->taskInfo[threadInfo->threadCount];  // 获取当前线程信息结构体
        taskInfo->tid = GetCurrTid(taskCB);  // 获取线程ID
        taskInfo->pid = OsGetPid(processCB);  // 获取所属进程ID
        taskInfo->status = taskCB->taskStatus;  // 获取线程状态
        taskCB->ops->schedParamGet(taskCB, &param);  // 获取调度参数
        taskInfo->policy = param.policy;     // 获取调度策略
        taskInfo->priority = param.priority;  // 获取优先级
#ifdef LOSCFG_KERNEL_SMP                     // 若启用SMP（对称多处理）
        taskInfo->currCpu = taskCB->currCpu;  // 获取当前运行CPU
        taskInfo->cpuAffiMask = taskCB->cpuAffiMask;  // 获取CPU亲和性掩码
#endif
        taskInfo->stackPoint = (UINTPTR)taskCB->stackPointer;  // 获取栈指针
        taskInfo->topOfStack = taskCB->topOfStack;  // 获取栈顶地址
        taskInfo->stackSize = taskCB->stackSize;  // 获取栈大小
        taskInfo->waitFlag = taskCB->waitFlag;  // 获取等待标志
        taskInfo->waitID = taskCB->waitID;      // 获取等待对象ID
        taskInfo->taskMux = taskCB->taskMux;    // 获取任务互斥锁
        (VOID)OsStackWaterLineGet((const UINTPTR *)(taskCB->topOfStack + taskCB->stackSize),  // 获取栈水位线
                                  (const UINTPTR *)taskCB->topOfStack, &taskInfo->waterLine);
#ifdef LOSCFG_KERNEL_CPUP                    // 若启用CPU占用率统计
        (VOID)OsGetTaskAllCpuUsageUnsafe(&taskCB->taskCpup, taskInfo);  // 获取线程CPU使用率
#endif
        (VOID)memcpy_s(taskInfo->name, OS_TCB_NAME_LEN, taskCB->taskName, OS_TCB_NAME_LEN);  // 复制线程名称
        threadInfo->threadCount++;            // 线程计数器递增
    }
}

/**
 * @brief 获取指定进程及其线程信息
 * @details 外部接口，通过进程ID获取进程基本信息和所有线程详细信息
 * @param pid 目标进程ID
 * @param threadInfo 进程线程信息结构体指针，用于存储输出结果
 * @return 操作结果，LOS_OK表示成功，LOS_NOK表示失败
 */
UINT32 OsGetProcessThreadInfo(UINT32 pid, ProcessThreadInfo *threadInfo)
{
    UINT32 intSave;                          // 中断保存变量

    if (OS_PID_CHECK_INVALID(pid) || (pid == 0) || (threadInfo == NULL)) {  // 参数合法性检查
        return LOS_NOK;                       // 参数无效返回失败
    }

    LosProcessCB *processCB = OS_PCB_FROM_PID(pid);  // 通过PID获取进程控制块
    if (OsProcessIsUnused(processCB)) {       // 检查进程是否未使用
        return LOS_NOK;                       // 进程无效返回失败
    }

#ifdef LOSCFG_KERNEL_VM                      // 若启用内核虚拟内存
    GetProcessMemInfo(&threadInfo->processInfo, processCB, processCB->vmSpace);  // 获取内存信息
#endif

    SCHEDULER_LOCK(intSave);                 // 加锁保护进程信息读取
    GetProcessInfo(&threadInfo->processInfo, processCB);  // 获取进程基本信息
    GetThreadInfo(threadInfo, processCB);    // 获取线程信息
    SCHEDULER_UNLOCK(intSave);               // 解锁
    return LOS_OK;                           // 成功返回
}

/**
 * @brief 批量获取进程内存使用信息
 * @details 遍历所有进程，获取每个进程的内存使用情况并填充到进程信息数组
 * @param pcbArray 进程信息数组指针，用于存储输出结果
 */
STATIC VOID ProcessMemUsageGet(ProcessInfo *pcbArray)
{
    UINT32 intSave;                          // 中断保存变量
#ifdef LOSCFG_PID_CONTAINER                  // 若启用PID容器功能
    PidContainer *pidContainer = OsCurrTaskGet()->pidContainer;  // 获取当前容器
    for (UINT32 pid = 0; pid < g_processMaxNum; ++pid) {  // 遍历容器内所有PID
        ProcessVid *processVid = &pidContainer->pidArray[pid];  // 获取进程虚拟ID信息
        const LosProcessCB *processCB = (LosProcessCB *)processVid->cb;  // 获取进程控制块
#else
    for (UINT32 pid = 0; pid < g_processMaxNum; ++pid) {  // 遍历所有物理PID
        const LosProcessCB *processCB = OS_PCB_FROM_RPID(pid);  // 通过物理PID获取进程控制块
#endif
        ProcessInfo *pcbInfo = pcbArray + pid;  // 获取当前进程信息结构体
        SCHEDULER_LOCK(intSave);              // 加锁保护
        if (OsProcessIsUnused(processCB)) {   // 检查进程是否未使用
            SCHEDULER_UNLOCK(intSave);        // 解锁
            pcbInfo->status = OS_PROCESS_FLAG_UNUSED;  // 标记为未使用
            continue;
        }
#ifdef LOSCFG_KERNEL_VM                      // 若启用内核虚拟内存
        LosVmSpace *vmSpace  = processCB->vmSpace;  // 获取虚拟内存空间
#endif
        SCHEDULER_UNLOCK(intSave);            // 解锁

#ifdef LOSCFG_KERNEL_VM                      // 若启用内核虚拟内存
        GetProcessMemInfo(pcbInfo, processCB, vmSpace);  // 获取内存使用信息
#endif
    }
}

/**
 * @brief 获取所有进程信息
 * @details 外部接口，批量获取系统中所有进程的基本信息和内存使用情况
 * @param pcbArray 进程信息数组指针，用于存储输出结果
 * @return 操作结果，LOS_OK表示成功，LOS_NOK表示失败
 */
UINT32 OsGetAllProcessInfo(ProcessInfo *pcbArray)
{
    UINT32 intSave;                          // 中断保存变量
    if (pcbArray == NULL) {                  // 检查输出缓冲区是否为空
        return LOS_NOK;                       // 缓冲区无效返回失败
    }

    ProcessMemUsageGet(pcbArray);            // 批量获取内存使用信息

    SCHEDULER_LOCK(intSave);                 // 加锁保护进程信息读取
#ifdef LOSCFG_PID_CONTAINER                  // 若启用PID容器功能
    PidContainer *pidContainer = OsCurrTaskGet()->pidContainer;  // 获取当前容器
    for (UINT32 index = 0; index < LOSCFG_BASE_CORE_PROCESS_LIMIT; index++) {  // 遍历容器内进程
        ProcessVid *processVid = &pidContainer->pidArray[index];  // 获取进程虚拟ID信息
        LosProcessCB *processCB = (LosProcessCB *)processVid->cb;  // 获取进程控制块
#else
    for (UINT32 index = 0; index < LOSCFG_BASE_CORE_PROCESS_LIMIT; index++) {  // 遍历所有物理进程
        LosProcessCB *processCB = OS_PCB_FROM_RPID(index);  // 通过物理索引获取进程控制块
#endif
        ProcessInfo *pcbInfo = pcbArray + index;  // 获取当前进程信息结构体
        if (OsProcessIsUnused(processCB)) {   // 检查进程是否未使用
            pcbInfo->status = OS_PROCESS_FLAG_UNUSED;  // 标记为未使用
            continue;
        }
        GetProcessInfo(pcbInfo, processCB);  // 填充进程基本信息
    }
    SCHEDULER_UNLOCK(intSave);               // 解锁
    return LOS_OK;                           // 成功返回
}
