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
/**
 * @brief 进程限制的最小周期（微秒）
 * @note 1000000微秒 = 1秒，表示进程限制的最小时间周期
 */
#define PLIMITS_MIN_PERIOD_IN_US 1000000
/**
 * @brief 进程限制的最小配额（微秒）
 * @note 1微秒，表示进程可分配的最小时间配额单位
 */
#define PLIMITS_MIN_QUOTA_IN_US        1
/**
 * @brief 进程限制删除等待时间（毫秒）
 * @note 1000毫秒 = 1秒，表示删除限制集前的等待时间
 */
#define PLIMITS_DELETE_WAIT_TIME    1000

/**
 * @brief 进程控制块(Process Control Block)前向声明
 * @note 实际定义位于进程管理模块，此处仅作引用声明
 */
typedef struct ProcessCB LosProcessCB;

/**
 * @brief 进程限制器ID枚举
 * @details 定义不同类型的进程限制器标识符，用于索引限制器数组
 */
enum ProcLimiterID {
    PROCESS_LIMITER_ID_PIDS = 0,                     /* PID数量限制器ID，基础限制类型 */
#ifdef LOSCFG_KERNEL_MEM_PLIMIT
    PROCESS_LIMITER_ID_MEM,                          /* 内存限制器ID，使能内存限制时生效 */
#endif
#ifdef LOSCFG_KERNEL_SCHED_PLIMIT
    PROCESS_LIMITER_ID_SCHED,                        /* 调度限制器ID，使能调度限制时生效 */
#endif
#ifdef LOSCFG_KERNEL_DEV_PLIMIT
    PROCESS_LIMITER_ID_DEV,                          /* 设备限制器ID，使能设备限制时生效 */
#endif
#ifdef LOSCFG_KERNEL_IPC_PLIMIT
    PROCESS_LIMITER_ID_IPC,                          /* IPC限制器ID，使能IPC限制时生效 */
#endif
    PROCESS_LIMITER_COUNT,                           /* 限制器总数，用于数组边界检查 */
};

/**
 * @brief 进程限制数据结构体
 * @details 记录进程各类资源的使用情况，根据使能的限制类型动态包含不同字段
 */
typedef struct {
#ifdef LOSCFG_KERNEL_MEM_PLIMIT
    UINT64 memUsed;                                  /* 已使用内存大小，单位：字节 */
#endif
#ifdef LOSCFG_KERNEL_IPC_PLIMIT
    UINT32 mqCount;                                  /* 消息队列数量 */
    UINT32 shmSize;                                  /* 共享内存大小，单位：字节 */
#endif
#ifdef LOSCFG_KERNEL_SCHED_PLIMIT
    UINT64  allRuntime;                              /* 总运行时间，单位：微秒 */
#endif
} PLimitsData;

/**
 * @brief 进程限制器集合结构体
 * @details 实现进程限制的层级管理，支持父子关系和多类型限制器组合
 */
typedef struct TagPLimiterSet {
    struct TagPLimiterSet *parent;                   /* 父限制器集合指针，构成层级关系 */
    LOS_DL_LIST childList;                           /* 子限制器集合链表头，管理下级限制集 */
    LOS_DL_LIST processList;                         /* 受此限制集管控的进程链表 */
    UINT32      pidCount;                            /* 进程数量计数器，记录管控的进程总数 */
    UINTPTR limitsList[PROCESS_LIMITER_COUNT];       /* 限制器实例数组，按ProcLimiterID索引 */
    UINT32 mask;                                     /* 限制器使能掩码，每一位表示对应ID的限制器是否存在 */
    UINT8 level;                                     /* 限制集层级，0表示根节点，数值越大层级越深 */
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
