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
 * @defgroup los_rwlock Rwlock
 * @ingroup kernel
 */

#ifndef _LOS_RWLOCK_H
#define _LOS_RWLOCK_H

#include "los_base.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
 * @ingroup los_rwlock
 * Rwlock object.
 */
typedef struct OsRwlock {
    INT32 magic:24;        /**< Magic number */
    INT32 rwCount:8;       /**< Times of locking the rwlock, rwCount > 0 when rwkick is read mode, rwCount < 0
                                when the rwlock is write mode, rwCount = 0 when the lock is free. */
    VOID *writeOwner;      /**< The current write thread that is locking the rwlock */
    LOS_DL_LIST readList;  /**< Read waiting list */
    LOS_DL_LIST writeList; /**< Write waiting list */
} LosRwlock;

extern BOOL LOS_RwlockIsValid(const LosRwlock *rwlock);

/**
 * @ingroup los_rwlock
 * @brief Init a rwlock.
 *
 * @par Description:
 * This API is used to Init a rwlock. A rwlock handle is assigned to rwlockHandle when the rwlock is init successfully.
 * Return LOS_OK on creating successful, return specific error code otherwise.
 * @param rwlock            [IN] Handle pointer of the successfully init rwlock.
 *
 * @retval #LOS_EINVAL      The rwlock pointer is NULL.
 * @retval #LOS_EPERM       Multiply initialization.
 * @retval #LOS_OK          The rwlock is successfully created.
 * @par Dependency:
 * <ul><li>los_rwlock.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_RwlockDestroy
 */
extern UINT32 LOS_RwlockInit(LosRwlock *rwlock);

/**
 * @ingroup los_rwlock
 * @brief Destroy a rwlock.
 *
 * @par Description:
 * This API is used to delete a specified rwlock. Return LOS_OK on deleting successfully, return specific error code
 * otherwise.
 * @attention
 * <ul>
 * <li>The specific rwlock should be created firstly.</li>
 * <li>The rwlock can be deleted successfully only if no other tasks pend on it.</li>
 * </ul>
 *
 * @param rwlock        [IN] Handle of the rwlock to be deleted.
 *
 * @retval #LOS_EINVAL  The rwlock pointer is NULL.
 * @retval #LOS_EBUSY   Tasks pended on this rwlock.
 * @retval #LOS_EBADF   The lock has been destroyed or broken.
 * @retval #LOS_OK      The rwlock is successfully deleted.
 * @par Dependency:
 * <ul><li>los_rwlock.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_RwlockInit
 */
extern UINT32 LOS_RwlockDestroy(LosRwlock *rwlock);

/**
 * @ingroup los_rwlock
 * @brief Wait to lock a read lock.
 *
 * @par Description:
 * This API is used to wait for a specified period of time to lock a read lock.
 * @attention
 * <ul>
 * <li>The specific rwlock should be created firstly.</li>
 * <li>The function fails if the rwlock that is waited on is already locked by another thread when the task scheduling
 * is disabled.</li>
 * <li>Do not wait on a rwlock during an interrupt.</li>
 * <li>The function fails when other tasks have the write lock or there are some task pending on the write list with
 * the higher priority.</li>
 * <li>A recursive rwlock can be locked more than once by the same thread.</li>
 * <li>Do not call this API in software timer callback. </li>
 * </ul>
 *
 * @param rwlock          [IN] Handle of the rwlock to be waited on.
 * @param timeout         [IN] Waiting time. The value range is [0, LOS_WAIT_FOREVER](unit: Tick).
 *
 * @retval #LOS_EINVAL    The rwlock pointer is NULL, The timeout is zero or Lock status error.
 * @retval #LOS_EINTR     The rwlock is being locked during an interrupt.
 * @retval #LOS_EBADF     The lock has been destroyed or broken.
 * @retval #LOS_EDEADLK   Rwlock error check failed or System locked task scheduling.
 * @retval #LOS_ETIMEDOUT The rwlock waiting times out.
 * @retval #LOS_EPERM     The rwlock is used in system tasks.
 * @retval #LOS_OK        The rwlock is successfully locked.
 * @par Dependency:
 * <ul><li>los_rwlock.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_RwlockInit | LOS_RwlockUnLock
 */
extern UINT32 LOS_RwlockRdLock(LosRwlock *rwlock, UINT32 timeout);

/**
 * @ingroup los_rwlock
 * @brief Try wait to lock a read lock.
 *
 * @par Description:
 * This API is used to wait for a specified period of time to lock a read lock.
 * @attention
 * <ul>
 * <li>The specific rwlock should be created firstly.</li>
 * <li>The function fails if the rwlock that is waited on is already locked by another thread when the task scheduling
 * is disabled.</li>
 * <li>Do not wait on a rwlock during an interrupt.</li>
 * <li>The function fails when other tasks have the write lock or there are some task pending on the write list with
 * the higher priority.</li>
 * <li>A recursive rwlock can be locked more than once by the same thread.</li>
 * <li>Do not call this API in software timer callback. </li>
 * </ul>
 *
 * @param rwlock          [IN] Handle of the rwlock to be waited on.
 *
 * @retval #LOS_EINVAL    The rwlock pointer is NULL or Lock status error.
 * @retval #LOS_EINTR     The rwlock is being locked during an interrupt.
 * @retval #LOS_EBUSY     Fail to get the rwlock, the rwlock has been used.
 * @retval #LOS_EBADF     The lock has been destroyed or broken.
 * @retval #LOS_EDEADLK   rwlock error check failed or System locked task scheduling.
 * @retval #LOS_ETIMEDOUT The rwlock waiting times out.
 * @retval #LOS_EPERM     The rwlock is used in system tasks.
 * @retval #LOS_OK        The rwlock is successfully locked.
 * @par Dependency:
 * <ul><li>los_rwlock.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_RwlockInit | LOS_RwlockUnLock
 */
extern UINT32 LOS_RwlockTryRdLock(LosRwlock *rwlock);

/**
 * @ingroup los_rwlock
 * @brief Wait to lock a write lock.
 *
 * @par Description:
 * This API is used to wait for a specified period of time to lock a write lock.
 * @attention
 * <ul>
 * <li>The specific rwlock should be created firstly.</li>
 * <li>The function fails if the rwlock that is waited on is already locked by another thread when
 * the task scheduling.</li>
 * is disabled.</li>
 * <li>Do not wait on a rwlock during an interrupt.</li>
 * <li>The funtion fails when other tasks have the read or write lock.</li>
 * <li>A recursive rwlock can be locked more than once by the same thread.</li>
 * <li>Do not call this API in software timer callback. </li>
 * </ul>
 *
 * @param rwlock          [IN] Handle of the rwlock to be waited on.
 * @param timeout         [IN] Waiting time. The value range is [0, LOS_WAIT_FOREVER](unit: Tick).
 *
 * @retval #LOS_EINVAL    The rwlock pointer is NULL, The timeout is zero or Lock status error.
 * @retval #LOS_EINTR     The rwlock is being locked during an interrupt.
 * @retval #LOS_EBADF     The lock has been destroyed or broken.
 * @retval #LOS_EDEADLK   Rwlock error check failed or System locked task scheduling.
 * @retval #LOS_ETIMEDOUT The rwlock waiting times out.
 * @retval #LOS_EPERM     The rwlock is used in system tasks.
 * @retval #LOS_OK        The rwlock is successfully locked.
 * @par Dependency:
 * <ul><li>los_rwlock.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_MuxInit | LOS_MuxUnlock
 */
extern UINT32 LOS_RwlockWrLock(LosRwlock *rwlock, UINT32 timeout);

/**
 * @ingroup los_rwlock
 * @brief Try wait to lock a write lock.
 *
 * @par Description:
 * This API is used to wait for a specified period of time to lock a write lock.
 * @attention
 * <ul>
 * <li>The specific rwlock should be created firstly.</li>
 * <li>The function fails if the rwlock that is waited on is already locked by another thread
 * when the task scheduling.</li>
 * is disabled.</li>
 * <li>Do not wait on a rwlock during an interrupt.</li>
 * <li>The funtion fails when other tasks have the read or write lock.</li>
 * <li>A recursive rwlock can be locked more than once by the same thread.</li>
 * <li>Do not call this API in software timer callback. </li>
 * </ul>
 *
 * @param rwlock          [IN] Handle of the rwlock to be waited on.
 *
 * @retval #LOS_EINVAL    The rwlock pointer is NULL or Lock status error.
 * @retval #LOS_EINTR     The rwlock is being locked during an interrupt.
 * @retval #LOS_EBUSY     Fail to get the rwlock, the rwlock has been used.
 * @retval #LOS_EBADF     The lock has been destroyed or broken.
 * @retval #LOS_EDEADLK   rwlock error check failed or System locked task scheduling.
 * @retval #LOS_ETIMEDOUT The rwlock waiting times out.
 * @retval #LOS_EPERM     The rwlock is used in system tasks.
 * @retval #LOS_OK        The rwlock is successfully locked.
 * @par Dependency:
 * <ul><li>los_rwlock.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_RwlockInit | LOS_RwlockUnLock
 */
extern UINT32 LOS_RwlockTryWrLock(LosRwlock *rwlock);

/**
 * @ingroup los_rwlock
 * @brief Release a rwlock.
 *
 * @par Description:
 * This API is used to release a specified rwlock.
 * @attention
 * <ul>
 * <li>The specific rwlock should be created firstly.</li>
 * <li>Do not release a rwlock during an interrupt.</li>
 * <li>When the write list is null and the read list is not null, all the task pending on the read list
 * will be waken.<li>
 * <li>When the write list is not null and the read list is null, the task pending on the read list will be
 * waken by the priority.<li>
 * <li>When the write list and the read list are not null, all the task pending on the both list will be waken
 * by the priority.<li>
 * If the task on the write list has the same priority as the task on the read list, the forth will
 * be waken.<li>
 * <li>If a recursive rwlock is locked for many times, it must be unlocked for the same times to be
 * released.</li>
 * </ul>
 *
 * @param rwlock          [IN] Handle of the rwlock to be released.
 *
 * @retval #LOS_EINVAL    The rwlock pointer is NULL, The timeout is zero or Lock status error.
 * @retval #LOS_EINTR     The rwlock is being locked during an interrupt.
 * @retval #LOS_EPERM     The rwlock is not locked or has been used.
 * @retval #LOS_EBADF     The lock has been destroyed or broken.
 * @retval #LOS_EDEADLK   rwlock error check failed or System locked task scheduling.
 * @retval #LOS_ETIMEDOUT The rwlock waiting times out.
 * @retval #LOS_EPERM     The rwlock is used in system tasks.
 * @retval #LOS_OK        The rwlock is successfully locked.
 * @par Dependency:
 * <ul><li>los_mux.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_RwlockInit | LOS_ReadLock | LOS_WriteLock
 */
extern UINT32 LOS_RwlockUnLock(LosRwlock *rwlock);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

#endif /* _LOS_MUX_H */
