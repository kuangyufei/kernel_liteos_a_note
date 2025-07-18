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

#ifndef _LOS_MEMLIMIT_H
#define _LOS_MEMLIMIT_H

#include "los_typedef.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */
/**
 * @brief 进程内存限制器结构体
 * @details 用于跟踪和限制单个进程的内存使用情况，包括当前用量、限制阈值、历史峰值和分配失败计数
 * @note 所有内存相关字段单位为字节(B)
 */
typedef struct ProcMemLimiter {
    UINT64 usage;   // 当前内存使用量
    UINT64 limit;   // 内存使用限制阈值
    UINT64 peak;    // 内存使用峰值
    UINT32 failcnt; // 内存分配失败次数
} ProcMemLimiter;

VOID OsMemLimiterInit(UINTPTR limite);
VOID *OsMemLimiterAlloc(VOID);
VOID OsMemLimiterFree(UINTPTR limite);
VOID OsMemLimiterCopy(UINTPTR dest, UINTPTR src);
BOOL MemLimiteMigrateCheck(UINTPTR curr, UINTPTR parent);
VOID OsMemLimiterMigrate(UINTPTR currLimit, UINTPTR parentLimit, UINTPTR process);
BOOL OsMemLimitAddProcessCheck(UINTPTR limit, UINTPTR process);
VOID OsMemLimitAddProcess(UINTPTR limit, UINTPTR process);
VOID OsMemLimitDelProcess(UINTPTR limit, UINTPTR process);
UINT32 OsMemLimitSetMemLimit(ProcMemLimiter *memLimit, UINT64 value);
VOID OsMemLimitSetLimit(UINTPTR limit);
UINT32 OsMemLimitCheckAndMemAdd(UINT32 size);
VOID OsMemLimitMemFree(UINT32 size);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_MEMLIMIT_H */
