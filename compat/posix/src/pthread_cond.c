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

#include "pprivate.h"
#include "pthread.h"
#include "stdlib.h"
#include "time_posix.h"
#include "los_atomic.h"
#include "los_event_pri.h"

/**
 * @file 
 * @brief 
 * @verbatim
条件变量属性

使用条件变量可以以原子方式阻塞线程，直到某个特定条件为真为止。条件变量始终与互斥锁一起使用。
使用条件变量，线程可以以原子方式阻塞，直到满足某个条件为止。对条件的测试是在互斥锁（互斥）的保护下进行的。
如果条件为假，线程通常会基于条件变量阻塞，并以原子方式释放等待条件变化的互斥锁。如果另一个线程更改了条件，
该线程可能会向相关的条件变量发出信号，从而使一个或多个等待的线程执行以下操作：
	唤醒
	再次获取互斥锁
	重新评估条件
在以下情况下，条件变量可用于在进程之间同步线程：
	线程是在可以写入的内存中分配的
	内存由协作进程共享
调度策略可确定唤醒阻塞线程的方式。对于缺省值 SCHED_OTHER，将按优先级顺序唤醒线程。

https://docs.oracle.com/cd/E19253-01/819-7051/sync-13528/index.html
https://docs.oracle.com/cd/E19253-01/819-7051/6n919hpai/index.html#sync-59145
 * @endverbatim
 */

#define BROADCAST_EVENT     1
#define COND_COUNTER_STEP   0x0004U
#define COND_FLAGS_MASK     0x0003U
#define COND_COUNTER_MASK   (~COND_FLAGS_MASK)

STATIC INLINE INT32 CondInitCheck(const pthread_cond_t *cond)
{
    if ((cond->event.stEventList.pstPrev == NULL) &&
        (cond->event.stEventList.pstNext == NULL)) {
        return 1;
    }
    return 0;
}
///获取条件变量的范围  
int pthread_condattr_getpshared(const pthread_condattr_t *attr, int *shared)
{
    if ((attr == NULL) || (shared == NULL)) {
        return EINVAL;
    }

    *shared = PTHREAD_PROCESS_PRIVATE;// PTHREAD_PROCESS_PRIVATE，则仅有那些由同一个进程创建的线程才能够处理该互斥锁。

    return 0;
}
///设置条件变量的范围  
int pthread_condattr_setpshared(pthread_condattr_t *attr, int shared)
{
    (VOID)attr;
    if ((shared != PTHREAD_PROCESS_PRIVATE) && (shared != PTHREAD_PROCESS_SHARED)) {
        return EINVAL;
    }
///如果 pshared 属性在共享内存中设置为 PTHREAD_PROCESS_SHARED，则其所创建的条件变量可以在多个进程中的线程之间共享。
    if (shared != PTHREAD_PROCESS_PRIVATE) {
        return ENOSYS;
    }

    return 0;
}
///销毁条件变量属性对象
int pthread_condattr_destroy(pthread_condattr_t *attr)
{
    if (attr == NULL) {
        return EINVAL;
    }

    return 0;
}
///初始化条件变量属性对象
int pthread_condattr_init(pthread_condattr_t *attr)
{
    if (attr == NULL) {
        return EINVAL;
    }

    return 0;
}
///销毁条件变量
int pthread_cond_destroy(pthread_cond_t *cond)
{
    if (cond == NULL) {
        return EINVAL;
    }

    if (CondInitCheck(cond)) {
        return ENOERR;
    }

    if (LOS_EventDestroy(&cond->event) != LOS_OK) {
        return EBUSY;
    }
    if (pthread_mutex_destroy(cond->mutex) != ENOERR) {
        PRINT_ERR("%s mutex destroy fail!\n", __FUNCTION__);
        return EINVAL;
    }
    free(cond->mutex);
    cond->mutex = NULL;
    return ENOERR;
}
///初始化条件变量
int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr)
{
    int ret = ENOERR;

    if (cond == NULL) {
        return EINVAL;
    }
    (VOID)attr;
    (VOID)LOS_EventInit(&(cond->event));

    cond->mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    if (cond->mutex == NULL) {
        return ENOMEM;
    }

    (VOID)pthread_mutex_init(cond->mutex, NULL);

    cond->value = 0;
    (VOID)pthread_mutex_lock(cond->mutex);
    cond->count = 0;
    (VOID)pthread_mutex_unlock(cond->mutex);

    return ret;
}

STATIC VOID PthreadCondValueModify(pthread_cond_t *cond)
{
    UINT32 flags = ((UINT32)cond->value & COND_FLAGS_MASK);
    INT32 oldVal, newVal;

    while (true) {
        oldVal = cond->value;
        newVal = (INT32)(((UINT32)(oldVal - COND_COUNTER_STEP) & COND_COUNTER_MASK) | flags);
        if (LOS_AtomicCmpXchg32bits(&cond->value, newVal, oldVal) == 0) {
            break;
        }
    }
}
///解除若干已被等待条件阻塞的线程 
int pthread_cond_broadcast(pthread_cond_t *cond)
{
    int ret = ENOERR;

    if (cond == NULL) {
        return EINVAL;
    }

    (VOID)pthread_mutex_lock(cond->mutex);
    if (cond->count > 0) {
        cond->count = 0;
        (VOID)pthread_mutex_unlock(cond->mutex);

        PthreadCondValueModify(cond);

        (VOID)LOS_EventWrite(&(cond->event), BROADCAST_EVENT);//写事件
        return ret;
    }
    (VOID)pthread_mutex_unlock(cond->mutex);

    return ret;
}
///解除被阻塞的线程
int pthread_cond_signal(pthread_cond_t *cond)
{
    int ret = ENOERR;

    if (cond == NULL) {
        return EINVAL;
    }

    (VOID)pthread_mutex_lock(cond->mutex);
    if (cond->count > 0) {
        cond->count--;
        (VOID)pthread_mutex_unlock(cond->mutex);
        PthreadCondValueModify(cond);
        (VOID)OsEventWriteOnce(&(cond->event), 0x01);//只写一次,也就是解除一个线程的阻塞

        return ret;
    }
    (VOID)pthread_mutex_unlock(cond->mutex);

    return ret;
}

STATIC INT32 PthreadCondWaitSub(pthread_cond_t *cond, INT32 value, UINT32 ticks)
{
    EventCond eventCond = { &cond->value, value, ~0x01U };
    /*
     * When the scheduling lock is held:
     * (1) value is not equal to cond->value, clear the event message and
     * do not block the current thread, because other threads is calling pthread_cond_broadcast or
     * pthread_cond_signal to modify cond->value and wake up the current thread,
     * and others threads will block on the scheduling lock until the current thread releases
     * the scheduling lock.
     * (2) value is equal to cond->value, block the current thread
     * and wait to be awakened by other threads.
     */
    return (int)OsEventReadWithCond(&eventCond, &(cond->event), 0x0fU,
                                    LOS_WAITMODE_OR | LOS_WAITMODE_CLR, ticks);
}
STATIC VOID PthreadCountSub(pthread_cond_t *cond)
{
    (VOID)pthread_mutex_lock(cond->mutex);
    if (cond->count > 0) {
        cond->count--;
    }
    (VOID)pthread_mutex_unlock(cond->mutex);
}

STATIC INT32 ProcessReturnVal(pthread_cond_t *cond, INT32 val)
{
    INT32 ret;
    switch (val) {
        /* 0: event does not occur */
        case 0:
        case BROADCAST_EVENT:
            ret = ENOERR;
            break;
        case LOS_ERRNO_EVENT_READ_TIMEOUT:
            PthreadCountSub(cond);
            ret = ETIMEDOUT;
            break;
        default:
            PthreadCountSub(cond);
            ret = EINVAL;
            break;
    }
    return ret;
}
///等待条件 在指定的时间之前阻塞,函数会一直阻塞，直到该条件获得信号，或者最后一个参数所指定的时间已过为止。
int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
                           const struct timespec *absTime)
{
    UINT32 absTicks;
    INT32 ret;
    INT32 oldValue;

    pthread_testcancel();
    if ((cond == NULL) || (mutex == NULL) || (absTime == NULL)) {
        return EINVAL;
    }

    if (CondInitCheck(cond)) {
        ret = pthread_cond_init(cond, NULL);
        if (ret != ENOERR) {
            return ret;
        }
    }
    oldValue = cond->value;

    (VOID)pthread_mutex_lock(cond->mutex);
    cond->count++;
    (VOID)pthread_mutex_unlock(cond->mutex);

    if ((absTime->tv_sec == 0) && (absTime->tv_nsec == 0)) {
        return ETIMEDOUT;
    }

    if (!ValidTimeSpec(absTime)) {
        return EINVAL;
    }

    absTicks = OsTimeSpec2Tick(absTime);
    if (pthread_mutex_unlock(mutex) != ENOERR) {
        PRINT_ERR("%s: %d failed\n", __FUNCTION__, __LINE__);
    }

#ifndef LOSCFG_ARCH_CORTEX_M7
    ret = PthreadCondWaitSub(cond, oldValue, absTicks);
#else
    ret = (INT32)LOS_EventRead(&(cond->event), 0x0f, LOS_WAITMODE_OR | LOS_WAITMODE_CLR, absTicks);
#endif
    if (pthread_mutex_lock(mutex) != ENOERR) {
        PRINT_ERR("%s: %d failed\n", __FUNCTION__, __LINE__);
    }

    ret = ProcessReturnVal(cond, ret);
    pthread_testcancel();
    return ret;
}
///基于条件变量阻塞,阻塞的线程可以通过 pthread_cond_signal() 或 pthread_cond_broadcast() 唤醒
int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
    int ret;
    int oldValue;

    if ((cond == NULL) || (mutex == NULL)) {
        return EINVAL;
    }

    if (CondInitCheck(cond)) {
        ret = pthread_cond_init(cond, NULL);
        if (ret != ENOERR) {
            return ret;
        }
    }
    oldValue = cond->value;

    (VOID)pthread_mutex_lock(cond->mutex);
    cond->count++;
    (VOID)pthread_mutex_unlock(cond->mutex);

    if (pthread_mutex_unlock(mutex) != ENOERR) {
        PRINT_ERR("%s: %d failed\n", __FUNCTION__, __LINE__);
    }

#ifndef LOSCFG_ARCH_CORTEX_M7
    ret = PthreadCondWaitSub(cond, oldValue, LOS_WAIT_FOREVER);
#else
    ret = (INT32)LOS_EventRead(&(cond->event), 0x0f, LOS_WAITMODE_OR | LOS_WAITMODE_CLR, LOS_WAIT_FOREVER);
#endif
    if (pthread_mutex_lock(mutex) != ENOERR) {
        PRINT_ERR("%s: %d failed\n", __FUNCTION__, __LINE__);
    }

    switch (ret) {
        /* 0: event does not occur */
        case 0:
        case BROADCAST_EVENT:
            ret = ENOERR;
            break;
        default:
            PthreadCountSub(cond);
            ret = EINVAL;
            break;
    }

    return ret;
}

