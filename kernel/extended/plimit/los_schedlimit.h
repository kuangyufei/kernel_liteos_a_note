/*
 * Copyright (c) 2022-2022 Huawei Device Co., Ltd. All rights reserved.
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

#ifndef _LOS_SCHEDLIMIT_H
#define _LOS_SCHEDLIMIT_H

#include "los_typedef.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */
/**
 * @brief   核心时间转换宏定义
 * @note    1秒 = 1000毫秒 = 1000000微秒，用于CPU时间配额计算
 * @value   (1000 * 1000) = 1000000微秒/秒
 */
#define CORE_US_PER_SECOND (1000 * 1000)  /* 1000 * 1000 us per second */

/**
 * @brief   任务控制块结构体前向声明
 * @note    用于避免头文件循环包含，实际定义在任务管理模块
 */
typedef struct TagTaskCB LosTaskCB;

/**
 * @brief   进程调度限制器结构体
 * @details 用于实现进程级CPU时间配额管理，控制进程在指定周期内的运行时长
 */
typedef struct ProcSchedLimiter {
    UINT64 startTime;       // 调度周期开始时间戳(微秒)
    UINT64 endTime;         // 调度周期结束时间戳(微秒)
    UINT64 period;          // 调度周期长度(微秒)
    UINT64 quota;           // 周期内允许的CPU时间配额(微秒)
    UINT64 allRuntime;      // 累计运行时间(微秒)
} ProcSchedLimiter;

VOID OsSchedLimitInit(UINTPTR limit);
VOID *OsSchedLimitAlloc(VOID);
VOID OsSchedLimitFree(UINTPTR limit);
VOID OsSchedLimitCopy(UINTPTR dest, UINTPTR src);
VOID OsSchedLimitUpdateRuntime(LosTaskCB *runTask, UINT64 currTime, INT32 incTime);
UINT32 OsSchedLimitSetPeriod(ProcSchedLimiter *schedLimit, UINT64 value);
UINT32 OsSchedLimitSetQuota(ProcSchedLimiter *schedLimit, UINT64 value);
BOOL OsSchedLimitCheckTime(LosTaskCB *task);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_SCHEDLIMIT_H */
