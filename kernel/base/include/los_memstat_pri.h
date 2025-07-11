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

#ifndef _LOS_MEMSTAT_PRI_H
#define _LOS_MEMSTAT_PRI_H

#include "los_typedef.h"
#include "los_memory.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
 * @brief 任务内存使用信息结构体，用于记录单个任务的内存使用情况
 * @note 该结构体通常用于内存统计和任务资源监控
 */
typedef struct {
    UINT32 memUsed;          /* 任务已使用内存大小，单位：字节 */
} TskMemUsedInfo;

/**
 * @brief 增加任务内存使用量
 * @param[in] usedSize 增加的内存大小，单位：字节
 * @param[in] taskID 任务ID
 * @note 该函数会原子地更新指定任务的内存使用统计信息
 */
extern VOID OsTaskMemUsedInc(UINT32 usedSize, UINT32 taskID);

/**
 * @brief 减少任务内存使用量
 * @param[in] usedSize 减少的内存大小，单位：字节
 * @param[in] taskID 任务ID
 * @note 该函数会原子地更新指定任务的内存使用统计信息
 */
extern VOID OsTaskMemUsedDec(UINT32 usedSize, UINT32 taskID);

/**
 * @brief 获取任务当前内存使用量
 * @param[in] taskID 任务ID
 * @return UINT32 当前内存使用量，单位：字节
 */
extern UINT32 OsTaskMemUsage(UINT32 taskID);

/**
 * @brief 清除任务内存使用统计信息
 * @param[in] taskID 任务ID
 * @note 通常在任务销毁或重置时调用，将内存使用量清零
 */
extern VOID OsTaskMemClear(UINT32 taskID);

/**
 * @brief 启用内存统计功能的宏定义
 * @note 当定义此宏时，系统会开启任务级别的内存使用统计功能
 */
#define OS_MEM_ENABLE_MEM_STATISTICS

#ifdef LOS_MEM_SLAB
/**
 * @brief 任务 slab 内存使用信息结构体，用于记录单个任务的 slab 内存使用情况
 * @note 仅在启用 slab 内存分配器（LOS_MEM_SLAB）时有效
 */
typedef struct {
    UINT32 slabUsed;         /* 任务已使用 slab 内存大小，单位：字节 */
} TskSlabUsedInfo;

extern VOID OsTaskSlabUsedInc(UINT32 usedSize, UINT32 taskID);
extern VOID OsTaskSlabUsedDec(UINT32 usedSize, UINT32 taskID);
extern UINT32 OsTaskSlabUsage(UINT32 taskID);
#endif

#ifdef OS_MEM_ENABLE_MEM_STATISTICS
#define OS_MEM_ADD_USED(usedSize, taskID)    OsTaskMemUsedInc(usedSize, taskID)
#define OS_MEM_REDUCE_USED(usedSize, taskID) OsTaskMemUsedDec(usedSize, taskID)
#define OS_MEM_CLEAR(taskID)                 OsTaskMemClear(taskID)
#ifdef LOS_MEM_SLAB
#define OS_SLAB_ADD_USED(usedSize, taskID)    OsTaskSlabUsedInc(usedSize, taskID)
#define OS_SLAB_REDUCE_USED(usedSize, taskID) OsTaskSlabUsedDec(usedSize, taskID)
#endif
#else
#define OS_MEM_ADD_USED(usedSize, taskID)
#define OS_MEM_REDUCE_USED(usedSize, taskID)
#define OS_MEM_CLEAR(taskID)
#ifdef LOS_MEM_SLAB
#define OS_SLAB_ADD_USED(usedSize, taskID)
#define OS_SLAB_REDUCE_USED(usedSize, taskID)
#endif
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_MEMSTAT_PRI_H */
