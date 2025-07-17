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

#ifndef _LOS_PROCESSLIMIT_H
#define _LOS_PROCESSLIMIT_H

#include "los_list.h"
#include "los_typedef.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */
/**
 * @brief   进程ID与优先级限制结构体
 * @details 用于限制进程创建数量和优先级范围，实现系统资源的精细化管控
 */
typedef struct PidLimit {
    UINT32 pidLimit;        // 最大进程ID限制值
    UINT32 priorityLimit;   // 进程优先级上限值
    UINT32 pidCount;        // 当前进程数量计数器
} PidLimit;

VOID PidLimiterInit(UINTPTR limit);
VOID *PidLimiterAlloc(VOID);
VOID PidLimterFree(UINTPTR limit);
VOID PidLimiterCopy(UINTPTR curr, UINTPTR parent);
BOOL PidLimitMigrateCheck(UINTPTR curr, UINTPTR parent);
BOOL OsPidLimitAddProcessCheck(UINTPTR limit, UINTPTR process);
VOID OsPidLimitAddProcess(UINTPTR limit, UINTPTR process);
VOID OsPidLimitDelProcess(UINTPTR limit, UINTPTR process);
UINT32 PidLimitSetPidLimit(PidLimit *pidLimit, UINT32 pidMax);
UINT32 PidLimitSetPriorityLimit(PidLimit *pidLimit, UINT32 priority);
UINT16 OsPidLimitGetPriorityLimit(VOID);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif
