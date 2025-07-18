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

#ifndef _LOS_CPUP_PRI_H
#define _LOS_CPUP_PRI_H

#include "los_cpup.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */
/**
 * @ingroup los_cpup
 * @brief 历史运行时间记录数量
 * @details 定义CPU性能统计模块中用于存储历史运行时间的记录条数，包含额外一条用于存储零值
 */
#define OS_CPUP_HISTORY_RECORD_NUM   11  /* 历史运行时间记录数量 */

/**
 * @ingroup los_cpup
 * @brief CPU性能统计基础信息结构体
 * @details 包含任务/进程/IRQ的CPU运行时间统计基础数据，用于计算CPU使用率
 */
typedef struct {
    UINT64 allTime;    /**< 总运行时间（单位：CPU周期） */
    UINT64 startTime;  /**< 任务调度前的起始时间（单位：CPU周期） */
    /**
     * 历史运行时间数组
     * @note 数组大小为OS_CPUP_HISTORY_RECORD_NUM + 1，最后一个元素固定存储0值
     * @note 用于计算不同时间粒度（1秒/10秒/总时间）的CPU使用率
     */
    UINT64 historyTime[OS_CPUP_HISTORY_RECORD_NUM + 1]; /**< 历史运行时间数组，最后一个元素存储零值 */
} OsCpupBase;  /* CPU性能统计基础信息结构体 */

/**
 * @ingroup los_cpup
 * @brief IRQ CPU使用率统计结构体
 * @details 记录中断(IRQ)的CPU使用情况，包括总时间、最大时间和采样计数等信息
 */
typedef struct {
    UINT32 id;         /**< IRQ中断号 */
    UINT16 status;     /**< IRQ状态（OS_CPUP_USED/OS_CPUP_UNUSED） */
    UINT64 allTime;    /**< IRQ总运行时间（单位：CPU周期） */
    UINT64 timeMax;    /**< IRQ最大运行时间（单位：CPU周期） */
    UINT64 count;      /**< IRQ采样计数（最大100次） */
    OsCpupBase cpup;   /**< IRQ的CPU性能统计基础信息 */
} OsIrqCpupCB;  /* IRQ CPU使用率统计结构体 */

/**
 * @brief 任务控制块结构体前向声明
 * @note 用于避免头文件循环包含，完整定义在任务管理模块中
 */
typedef struct TagTaskCB LosTaskCB;  /* 任务控制块结构体前向声明 */

/**
 * @brief 任务信息结构体前向声明
 * @note 用于存储任务CPU使用率等信息，完整定义在任务管理模块中
 */
typedef struct TagTaskInfo TaskInfo;  /* 任务信息结构体前向声明 */

/**
 * @brief 进程信息结构体前向声明
 * @note 用于存储进程CPU使用率等信息，完整定义在进程管理模块中
 */
typedef struct TagProcessInfo ProcessInfo;  /* 进程信息结构体前向声明 */

extern UINT32 OsCpupInit(VOID);
extern UINT32 OsCpupGuardCreator(VOID);
extern VOID OsCpupCycleEndStart(LosTaskCB *runTask, LosTaskCB *newTask);
extern UINT32 OsGetProcessAllCpuUsageUnsafe(OsCpupBase *processCpup, ProcessInfo *processInfo);
extern UINT32 OsGetTaskAllCpuUsageUnsafe(OsCpupBase *taskCpup, TaskInfo *taskInfo);
#ifdef LOSCFG_CPUP_INCLUDE_IRQ
extern UINT32 OsGetAllIrqCpuUsageUnsafe(UINT16 mode, CPUP_INFO_S *cpupInfo, UINT32 len);
extern VOID OsCpupIrqStart(UINT16);
extern VOID OsCpupIrqEnd(UINT16, UINT32);
extern OsIrqCpupCB *OsGetIrqCpupArrayBase(VOID);
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_CPUP_PRI_H */
