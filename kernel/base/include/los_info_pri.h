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

/**
 * @brief 任务信息结构体，用于描述单个任务的详细状态和属性
 * @note 该结构体通常用于任务查询、调试和性能分析场景
 */
typedef struct TagTaskInfo {
    UINT32  tid;                /* 任务ID，系统内唯一标识一个任务 */
    UINT32  pid;                /* 进程ID，标识该任务所属的进程 */
    UINT16  status;             /* 任务状态：0-就绪，1-运行，2-阻塞，3-挂起等 */
    UINT16  policy;             /* 调度策略：0-抢占式，1-时间片轮转，2-FIFO等 */
    UINT16  priority;           /* 任务优先级，数值越小优先级越高（0-31） */
#ifdef LOSCFG_KERNEL_SMP
    UINT16  currCpu;            /* 当前运行CPU核心ID（多核心配置下有效） */
    UINT16  cpuAffiMask;        /* CPU亲和性掩码，bit位表示可运行的CPU核心 */
#endif
    UINT32  stackSize;          /* 栈大小，单位：字节 */
    UINTPTR stackPoint;         /* 当前栈指针 */
    UINTPTR topOfStack;         /* 栈顶地址 */
    UINT32  waitFlag;           /* 等待标志位，指示任务等待的事件类型 */
    UINT32  waitID;             /* 等待对象ID，如信号量ID、互斥锁ID等 */
    VOID    *taskMux;           /* 任务互斥量指针，用于任务同步 */
    UINT32  waterLine;          /* 栈水位线，记录栈的最大使用深度 */
#ifdef LOSCFG_KERNEL_CPUP
    UINT32  cpup1sUsage;        /* 最近1秒CPU使用率（0-1000，表示0.0%-100.0%） */
    UINT32  cpup10sUsage;       /* 最近10秒CPU使用率（0-1000，表示0.0%-100.0%） */
    UINT32  cpupAllsUsage;      /* 总CPU使用率（0-1000，表示0.0%-100.0%） */
#endif
    CHAR    name[OS_TCB_NAME_LEN]; /* 任务名称，最大长度由OS_TCB_NAME_LEN定义 */
} TaskInfo;

/**
 * @brief 进程信息结构体，用于描述单个进程的详细状态和属性
 * @note 该结构体通常用于进程管理、资源统计和系统监控
 */
typedef struct TagProcessInfo {
    UINT32 pid;                 /* 进程ID，系统内唯一标识一个进程 */
    UINT32 ppid;                /* 父进程ID，标识创建该进程的父进程 */
    UINT16 status;              /* 进程状态：0-就绪，1-运行，2-阻塞，3-退出等 */
    UINT16 mode;                /* 进程模式：0-用户态，1-内核态 */
    UINT32 pgroupID;            /* 进程组ID，标识进程所属的进程组 */
    UINT32 userID;              /* 用户ID，标识进程的所有者 */
    UINT16 policy;              /* 调度策略：0-默认，1-实时，2-批处理等 */
    UINT32 basePrio;            /* 基础优先级，进程初始优先级 */
    UINT32 threadGroupID;       /* 线程组ID，与进程ID相同，标识进程内所有线程 */
    UINT32 threadNumber;        /* 线程数量，进程内当前活跃的线程总数 */
#ifdef LOSCFG_KERNEL_VM
    UINT32 virtualMem;          /* 虚拟内存大小，单位：字节（启用虚拟内存时有效） */
    UINT32 shareMem;            /* 共享内存大小，单位：字节（启用虚拟内存时有效） */
    UINT32 physicalMem;         /* 物理内存大小，单位：字节（启用虚拟内存时有效） */
#endif
#ifdef LOSCFG_KERNEL_CPUP
    UINT32 cpup1sUsage;         /* 最近1秒CPU使用率（0-1000，表示0.0%-100.0%） */
    UINT32 cpup10sUsage;        /* 最近10秒CPU使用率（0-1000，表示0.0%-100.0%） */
    UINT32 cpupAllsUsage;       /* 总CPU使用率（0-1000，表示0.0%-100.0%） */
#endif
    CHAR   name[OS_PCB_NAME_LEN]; /* 进程名称，最大长度由OS_PCB_NAME_LEN定义 */
} ProcessInfo;

/**
 * @brief 进程线程信息结构体，用于同时描述进程及其包含的所有线程信息
 * @note 该结构体通常用于进程及其线程的批量查询场景
 */
typedef struct TagProcessThreadInfo {
    ProcessInfo processInfo;                /* 进程基本信息 */
    UINT32      threadCount;                /* 线程数量，实际有效的线程信息数量 */
    TaskInfo    taskInfo[LOSCFG_BASE_CORE_TSK_LIMIT]; /* 线程信息数组，大小由系统最大任务数限制定义 */
} ProcessThreadInfo;

UINT32 OsGetAllProcessInfo(ProcessInfo *pcbArray);

UINT32 OsGetProcessThreadInfo(UINT32 pid, ProcessThreadInfo *threadInfo);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_INFO_PRI_H */
