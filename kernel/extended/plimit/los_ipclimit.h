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

#ifndef _LOS_IPCLIMIT_H
#define _LOS_IPCLIMIT_H

#include "los_typedef.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */
/**
 * @brief 进程IPC资源限制结构体
 * @details 用于跟踪和限制进程的IPC资源使用，包括消息队列和共享内存
 * @note 所有计数类字段单位为个，大小类字段单位为字节(B)
 */
typedef struct ProcIPCLimit {
    UINT32 mqCount;         // 当前消息队列数量
    UINT32 mqFailedCount;   // 消息队列创建失败次数
    UINT32 mqCountLimit;    // 消息队列数量限制阈值
    UINT32 shmSize;         // 当前共享内存大小
    UINT32 shmFailedCount;  // 共享内存分配失败次数
    UINT32 shmSizeLimit;    // 共享内存大小限制阈值
} ProcIPCLimit;

/**
 * @brief IPC统计类型枚举
 * @details 定义不同IPC资源的统计类型，用于索引统计数据
 */
enum IPCStatType {
    IPC_STAT_TYPE_MQ  = 0,  // 消息队列统计类型
    IPC_STAT_TYPE_SHM = 3,  // 共享内存统计类型（偏移3为预留其他IPC类型空间）
    IPC_STAT_TYPE_BUT       // 枚举结束标记
};

/**
 * @brief IPC统计偏移量枚举
 * @details 定义不同IPC资源在统计数组中的偏移位置
 */
enum IPCStatOffset {
    IPC_STAT_OFFSET_MQ  = 0,  // 消息队列统计偏移量
    IPC_STAT_OFFSET_SHM = 3,  // 共享内存统计偏移量（偏移3为预留其他IPC类型空间）
    IPC_STAT_OFFSET_BUT       // 枚举结束标记
};

/**
 * @brief 统计项类型枚举
 * @details 定义IPC资源统计项的类型，包括总量、失败量和限制值
 */
enum StatItem {
    STAT_ITEM_TOTAL,   // 总使用量统计项
    STAT_ITEM_FAILED,  // 失败次数统计项
    STAT_ITEM_LIMIT,   // 限制阈值统计项
    STAT_ITEM_BUT      // 枚举结束标记
};

VOID OsIPCLimitInit(UINTPTR limite);
VOID *OsIPCLimitAlloc(VOID);
VOID OsIPCLimitFree(UINTPTR limite);
VOID OsIPCLimitCopy(UINTPTR dest, UINTPTR src);
BOOL OsIPCLimiteMigrateCheck(UINTPTR curr, UINTPTR parent);
VOID OsIPCLimitMigrate(UINTPTR currLimit, UINTPTR parentLimit, UINTPTR process);
BOOL OsIPCLimitAddProcessCheck(UINTPTR limit, UINTPTR process);
VOID OsIPCLimitAddProcess(UINTPTR limit, UINTPTR process);
VOID OsIPCLimitDelProcess(UINTPTR limit, UINTPTR process);
UINT32 OsIPCLimitSetMqLimit(ProcIPCLimit *ipcLimit, UINT32 value);
UINT32 OsIPCLimitSetShmLimit(ProcIPCLimit *ipcLimit, UINT32 value);
UINT32 OsIPCLimitMqAlloc(VOID);
VOID OsIPCLimitMqFree(VOID);
UINT32 OsIPCLimitShmAlloc(UINT32 size);
VOID OsIPCLimitShmFree(UINT32 size);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_IPCIMIT_H */
