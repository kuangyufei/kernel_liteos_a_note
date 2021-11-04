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
 * @defgroup los_mux Mutex
 * @ingroup kernel
 */

#ifndef _LOS_MUX_H
#define _LOS_MUX_H

#include "los_base.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */
/*! \enum */
enum {
    LOS_MUX_PRIO_NONE = 0,		///< 线程的优先级和调度不会受到互斥锁影响,先来后到,普通排队.
    LOS_MUX_PRIO_INHERIT = 1,	///< 当高优先级的等待低优先级的线程释放锁时，低优先级的线程以高优先级线程的优先级运行。
    							///< 当线程解锁互斥量时，线程的优先级自动被将到它原来的优先级
    LOS_MUX_PRIO_PROTECT = 2	///< 详见: OsMuxPendOp 中的注解,详细说明了LOS_MUX_PRIO_PROTECT的含义
};
/*! \enum */
enum {
    LOS_MUX_NORMAL = 0,		///< 非递归锁 只有[0.1]两个状态,不做任何特殊的错误检,不进行deadlock detection(死锁检测)
    LOS_MUX_RECURSIVE = 1,	///< 递归锁 允许同一线程在互斥量解锁前对该互斥量进行多次加锁。递归互斥量维护锁的计数，在解锁次数和加锁次数不相同的情况下，不会释放锁，别的线程就无法加锁此互斥量。
    LOS_MUX_ERRORCHECK = 2,	///< 进行错误检查,如果一个线程企图对一个已经锁住的mutex进行relock或对未加锁的unlock，将返回一个错误。
    LOS_MUX_DEFAULT = LOS_MUX_RECURSIVE //鸿蒙系统默认使用递归锁
};
/*! \struct LosMuxAttr */
typedef struct { //互斥锁的属性
    UINT8 protocol;		///< 协议
    UINT8 prioceiling;	///< 优先级上限
    UINT8 type;			///< 类型属性
    UINT8 reserved;		///< 保留字段
} LosMuxAttr;

/**
 * @ingroup los_mux
 * Mutex object.
 */
typedef struct OsMux { //互斥锁结构体
    UINT32 magic;        /**< magic number | 魔法数字*/
    LosMuxAttr attr;     /**< Mutex attribute | 互斥锁属性*/
    LOS_DL_LIST holdList; /**< The task holding the lock change | 当有任务拿到本锁时,通过holdList节点把锁挂到该任务的锁链表上 */
    LOS_DL_LIST muxList; /**< Mutex linked list | 等这个锁的任务链表,上面挂的都是任务,注意和holdList的区别. */
    VOID *owner;         /**< The current thread that is locking a mutex | 当前拥有这把锁的任务 */
    UINT16 muxCount;     /**< Times of locking a mutex | 锁定互斥体的次数,递归锁允许多次 */
} LosMux;

extern UINT32 LOS_MuxAttrInit(LosMuxAttr *attr);
extern UINT32 LOS_MuxAttrDestroy(LosMuxAttr *attr);
extern UINT32 LOS_MuxAttrGetType(const LosMuxAttr *attr, INT32 *outType);
extern UINT32 LOS_MuxAttrSetType(LosMuxAttr *attr, INT32 type);
extern UINT32 LOS_MuxAttrGetProtocol(const LosMuxAttr *attr, INT32 *protocol);
extern UINT32 LOS_MuxAttrSetProtocol(LosMuxAttr *attr, INT32 protocol);
extern UINT32 LOS_MuxAttrGetPrioceiling(const LosMuxAttr *attr, INT32 *prioceiling);
extern UINT32 LOS_MuxAttrSetPrioceiling(LosMuxAttr *attr, INT32 prioceiling);
extern UINT32 LOS_MuxSetPrioceiling(LosMux *mutex, INT32 prioceiling, INT32 *oldPrioceiling);
extern UINT32 LOS_MuxGetPrioceiling(const LosMux *mutex, INT32 *prioceiling);
extern BOOL LOS_MuxIsValid(const LosMux *mutex);

/**
 * @ingroup los_mux
 * @brief Init a mutex.
 *
 * @par Description:
 * This API is used to Init a mutex. A mutex handle is assigned to muxHandle when the mutex is init successfully.
 * Return LOS_OK on creating successful, return specific error code otherwise.
 * @attention
 * <ul>
 * <li>The total number of mutexes is pre-configured. If there are no available mutexes, the mutex creation fails.</li>
 * </ul>
 *
 * @param mutex             [IN] Handle pointer of the successfully init mutex.
 * @param attr              [IN] The mutex attribute.
 *
 * @retval #LOS_EINVAL      The mutex pointer is NULL.
 * @retval #LOS_OK          The mutex is successfully created.
 * @par Dependency:
 * <ul><li>los_mux.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_MuxDestroy
 */
extern UINT32 LOS_MuxInit(LosMux *mutex, const LosMuxAttr *attr);

/**
 * @ingroup los_mux
 * @brief Destroy a mutex.
 *
 * @par Description:
 * This API is used to delete a specified mutex. Return LOS_OK on deleting successfully, return specific error code
 * otherwise.
 * @attention
 * <ul>
 * <li>The specific mutex should be created firstly.</li>
 * <li>The mutex can be deleted successfully only if no other tasks pend on it.</li>
 * </ul>
 *
 * @param mutex         [IN] Handle of the mutex to be deleted.
 *
 * @retval #LOS_EINVAL  The mutex pointer is NULL.
 * @retval #LOS_EBUSY   Tasks pended on this mutex.
 * @retval #LOS_EBADF   The lock has been destroyed or broken.
 * @retval #LOS_OK      The mutex is successfully deleted.
 * @par Dependency:
 * <ul><li>los_mux.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_MuxInit
 */
extern UINT32 LOS_MuxDestroy(LosMux *mutex);

/**
 * @ingroup los_mux
 * @brief Wait to lock a mutex.
 *
 * @par Description:
 * This API is used to wait for a specified period of time to lock a mutex.
 * @attention
 * <ul>
 * <li>The specific mutex should be created firstly.</li>
 * <li>The function fails if the mutex that is waited on is already locked by another thread when the task scheduling
 * is disabled.</li>
 * <li>Do not wait on a mutex during an interrupt.</li>
 * <li>The priority inheritance protocol is supported. If a higher-priority thread is waiting on a mutex, it changes
 * the priority of the thread that owns the mutex to avoid priority inversion.</li>
 * <li>A recursive mutex can be locked more than once by the same thread.</li>
 * <li>Do not call this API in software timer callback. </li>
 * </ul>
 *
 * @param mutex           [IN] Handle of the mutex to be waited on.
 * @param timeout         [IN] Waiting time. The value range is [0, LOS_WAIT_FOREVER](unit: Tick).
 *
 * @retval #LOS_EINVAL    The mutex pointer is NULL, The timeout is zero or Lock status error.
 * @retval #LOS_EINTR     The mutex is being locked during an interrupt.
 * @retval #LOS_EBUSY     Tasks pended on this mutex.
 * @retval #LOS_EBADF     The lock has been destroyed or broken.
 * @retval #LOS_EDEADLK   Mutex error check failed or System locked task scheduling.
 * @retval #LOS_ETIMEDOUT The mutex waiting times out.
 * @retval #LOS_OK                           The mutex is successfully locked.
 * @par Dependency:
 * <ul><li>los_mux.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_MuxInit | LOS_MuxUnlock
 */
extern UINT32 LOS_MuxLock(LosMux *mutex, UINT32 timeout);

/**
 * @ingroup los_mux
 * @brief Try wait to lock a mutex.
 *
 * @par Description:
 * This API is used to wait for a specified period of time to lock a mutex.
 * @attention
 * <ul>
 * <li>The specific mutex should be created firstly.</li>
 * <li>The function fails if the mutex that is waited on is already locked by another thread when the task scheduling
 * is disabled.</li>
 * <li>Do not wait on a mutex during an interrupt.</li>
 * <li>The priority inheritance protocol is supported. If a higher-priority thread is waiting on a mutex, it changes
 * the priority of the thread that owns the mutex to avoid priority inversion.</li>
 * <li>A recursive mutex can be locked more than once by the same thread.</li>
 * <li>Do not call this API in software timer callback. </li>
 * </ul>
 *
 * @param mutex           [IN] Handle of the mutex to be waited on.
 *
 * @retval #LOS_EINVAL    The mutex pointer is NULL or Lock status error.
 * @retval #LOS_EINTR     The mutex is being locked during an interrupt.
 * @retval #LOS_EBUSY     Tasks pended on this mutex.
 * @retval #LOS_EBADF     The lock has been destroyed or broken.
 * @retval #LOS_EDEADLK   Mutex error check failed or System locked task scheduling.
 * @retval #LOS_ETIMEDOUT The mutex waiting times out.
 * @retval #LOS_OK                           The mutex is successfully locked.
 * @par Dependency:
 * <ul><li>los_mux.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_MuxInit | LOS_MuxUnlock
 */
extern UINT32 LOS_MuxTrylock(LosMux *mutex);

/**
 * @ingroup los_mux
 * @brief Release a mutex.
 *
 * @par Description:
 * This API is used to release a specified mutex.
 * @attention
 * <ul>
 * <li>The specific mutex should be created firstly.</li>
 * <li>Do not release a mutex during an interrupt.</li>
 * <li>If a recursive mutex is locked for many times, it must be unlocked for the same times to be released.</li>
 * </ul>
 *
 * @param mutex        [IN] Handle of the mutex to be released.
 *
 * @retval #LOS_EINVAL    The mutex pointer is NULL, The timeout is zero or Lock status error.
 * @retval #LOS_EINTR     The mutex is being locked during an interrupt.
 * @retval #LOS_EBUSY     Tasks pended on this mutex.
 * @retval #LOS_EBADF     The lock has been destroyed or broken.
 * @retval #LOS_EDEADLK   Mutex error check failed or System locked task scheduling.
 * @retval #LOS_ETIMEDOUT The mutex waiting times out.
 * @retval #LOS_OK                           The mutex is successfully locked.
 * @par Dependency:
 * <ul><li>los_mux.h: the header file that contains the API declaration.</li></ul>
 * @see LOS_MuxInit | LOS_MuxLock
 */
extern UINT32 LOS_MuxUnlock(LosMux *mutex);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

#endif /* _LOS_MUX_H */
