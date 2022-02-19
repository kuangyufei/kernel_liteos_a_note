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

#define OS_RWLOCK_MAGIC 0xBEFDCAU ///< 读写锁魔法数字

enum RwlockMode {
    RWLOCK_NONE_MODE,	///< 自由模式: 读写链表都没有内容 
    RWLOCK_READ_MODE,	///< 读模式: 读链表有数据,写链表没有数据
    RWLOCK_WRITE_MODE,  ///< 写模式: 写链表有数据,读链表没有数据
    RWLOCK_READFIRST_MODE,	///< 读优先模式: 读链表中的任务最高优先级高于写链表中任务最高优先级
    RWLOCK_WRITEFIRST_MODE	///< 写优先模式: 写链表中的任务最高优先级高于读链表中任务最高优先级
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
