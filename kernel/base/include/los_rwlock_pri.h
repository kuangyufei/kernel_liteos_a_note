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

#ifndef _LOS_RWLOCK_PRI_H
#define _LOS_RWLOCK_PRI_H

#include "los_rwlock.h"
#include "los_task_pri.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
 * @ingroup los_rwlock
 * @brief 读写锁魔术字
 * @details 用于标识读写锁控制块的有效性，防止非法访问或内存 corruption
 * @note 十六进制值0xEFDCAU对应十进制973258
 */
#define OS_RWLOCK_MAGIC 0xEFDCAU

/**
 * @ingroup los_rwlock
 * @brief 读写锁工作模式枚举
 * @details 定义读写锁的不同调度策略，决定读/写操作的优先级和抢占行为
 */
enum RwlockMode {
    RWLOCK_NONE_MODE,        /**< 无特定模式，默认调度策略 */
    RWLOCK_READ_MODE,        /**< 读模式，允许多个读操作同时持有锁 */
    RWLOCK_WRITE_MODE,       /**< 写模式，同一时间仅允许一个写操作持有锁 */
    RWLOCK_READFIRST_MODE,   /**< 读优先模式，已有读操作时新写操作等待 */
    RWLOCK_WRITEFIRST_MODE   /**< 写优先模式，写操作请求时阻塞后续读操作 */
};

extern UINT32 OsRwlockRdUnsafe(LosRwlock *rwlock, UINT32 timeout);
extern UINT32 OsRwlockTryRdUnsafe(LosRwlock *rwlock, UINT32 timeout);
extern UINT32 OsRwlockWrUnsafe(LosRwlock *rwlock, UINT32 timeout);
extern UINT32 OsRwlockTryWrUnsafe(LosRwlock *rwlock, UINT32 timeout);
extern UINT32 OsRwlockUnlockUnsafe(LosRwlock *rwlock, BOOL *needSched);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_RWLOCK_PRI_H */
