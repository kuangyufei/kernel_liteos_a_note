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

#ifndef __LOS_STAT_PRI_H
#define __LOS_STAT_PRI_H

#include "los_typedef.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
 * @brief 调度统计信息结构体
 * @core 记录任务调度相关的运行时统计数据，用于性能分析和调度优化
 * @attention 所有时间相关字段单位为系统时钟周期，需根据系统主频换算为实际时间
 */
typedef struct {
    UINT64      allRuntime;           /* 总运行时间，任务从创建到当前的累计运行时钟周期数 */
    UINT64      runTime;              /* 本轮运行时间，任务从上次调度到当前的运行时钟周期数 */
    UINT64      switchCount;          /* 调度切换次数，记录任务调度器切换上下文的总次数 */
    UINT64      timeSliceRealTime;    /* 时间片实际使用时间，每个时间片内任务实际运行的时钟周期数 */
    UINT64      timeSliceTime;        /* 时间片配置时间，系统为任务分配的单个时间片时钟周期数 */
    UINT64      timeSliceCount;       /* 时间片分配次数，任务累计获得的时间片总数 */
    UINT64      pendTime;             /* 等待时间，任务处于阻塞状态的累计时钟周期数 */
    UINT64      pendCount;            /* 等待次数，任务进入阻塞状态的累计次数 */
    UINT64      waitSchedTime;        /* 调度等待时间，任务从就绪到运行状态的累计等待时钟周期数 */
    UINT64      waitSchedCount;       /* 调度等待次数，任务从就绪到运行状态的转换次数 */
} SchedStat;

#ifdef LOSCFG_SCHED_DEBUG
#ifdef LOSCFG_SCHED_TICK_DEBUG
VOID OsSchedDebugRecordData(VOID);
UINT32 OsShellShowTickResponse(VOID);
UINT32 OsSchedDebugInit(VOID);
#endif
UINT32 OsShellShowSchedStatistics(VOID);
UINT32 OsShellShowEdfSchedStatistics(VOID);
VOID EDFDebugRecord(UINTPTR *taskCB, UINT64 oldFinish);
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* __LOS_STAT_PRI_H */
