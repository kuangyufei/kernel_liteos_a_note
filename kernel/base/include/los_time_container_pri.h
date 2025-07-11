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

#ifndef _LOS_TIME_CONTAINER_PRI_H
#define _LOS_TIME_CONTAINER_PRI_H
#include "time.h"
#include "los_atomic.h"

#ifdef LOSCFG_TIME_CONTAINER
typedef struct ProcessCB LosProcessCB;
struct Container;

/**
 * @brief 时间容器结构体，用于管理系统时间相关信息
 * @core 存储单调时间、容器标识及状态控制，支持多线程安全访问
 * @attention rc字段需通过原子操作函数访问，禁止直接读写
 */
typedef struct TimeContainer {
    Atomic            rc;                  ///< 原子引用计数，记录当前容器被引用次数，初始值为0
    BOOL              frozenOffsets;       ///< 时间偏移冻结标志：TRUE表示冻结偏移，FALSE表示允许动态调整
    struct timespec64 monotonic;           ///< 单调时间结构体，记录自系统启动以来的时间（不受系统时间调整影响）
    UINT32            containerID;         ///< 容器唯一标识符，用于区分不同的时间容器实例
} TimeContainer;

UINT32 OsInitRootTimeContainer(TimeContainer **timeContainer);

UINT32 OsCopyTimeContainer(UINTPTR flags, LosProcessCB *child, LosProcessCB *parent);

UINT32 OsUnshareTimeContainer(UINTPTR flags, LosProcessCB *curr, struct Container *newContainer);

UINT32 OsSetNsTimeContainer(UINT32 flags, struct Container *container, struct Container *newContainer);

VOID OsTimeContainerDestroy(struct Container *container);

UINT32 OsGetTimeContainerID(TimeContainer *timeContainer);

TimeContainer *OsGetCurrTimeContainer(VOID);

UINT32 OsGetTimeContainerMonotonic(LosProcessCB *processCB, struct timespec64 *offsets);

UINT32 OsSetTimeContainerMonotonic(LosProcessCB *processCB, struct timespec64 *offsets);

UINT32 OsGetTimeContainerCount(VOID);

#define CLOCK_MONOTONIC_TIME_BASE (OsGetCurrTimeContainer()->monotonic)

#endif
#endif /* _LOS_TIME_CONTAINER_PRI_H */
