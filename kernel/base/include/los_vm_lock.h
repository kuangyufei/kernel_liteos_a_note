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

/**
 * @defgroup los_vm_map vm lock operation
 * @ingroup kernel
 */

#ifndef __LOS_VM_LOCK_H__
#define __LOS_VM_LOCK_H__

#include "los_mux.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
 * @brief 获取互斥锁，永久等待直到成功
 * @param m [IN] 指向互斥锁结构体LosMux的指针
 * @return STATUS_T 操作结果，成功返回LOS_OK，失败返回错误码
 * @note 此函数调用LOS_MuxLock并传入LOS_WAIT_FOREVER参数，表示无限期等待锁
 * @see LOS_MuxLock
 */
STATIC INLINE STATUS_T LOS_MuxAcquire(LosMux *m)
{
    return LOS_MuxLock(m, LOS_WAIT_FOREVER); // 调用带永久等待参数的互斥锁锁定函数
}

/**
 * @brief 释放互斥锁
 * @param m [IN] 指向互斥锁结构体LosMux的指针
 * @return STATUS_T 操作结果，成功返回LOS_OK，失败返回错误码
 * @note 此函数直接调用LOS_MuxUnlock完成锁释放
 * @see LOS_MuxUnlock
 */
STATIC INLINE STATUS_T LOS_MuxRelease(LosMux *m)
{
    return LOS_MuxUnlock(m); // 调用互斥锁解锁函数
}
#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* __LOS_VM_LOCK_H__ */
