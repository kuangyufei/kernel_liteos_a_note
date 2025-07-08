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
// 广播事件标志
#define BROADCAST_EVENT     1
// 条件变量计数器步长
#define COND_COUNTER_STEP   0x0004U
// 条件变量标志掩码（用于提取标志位）
#define COND_FLAGS_MASK     0x0003U
// 条件变量计数器掩码（用于提取计数器部分）
#define COND_COUNTER_MASK   (~COND_FLAGS_MASK)

/**
 * @brief 检查条件变量是否已初始化
 * @param cond 条件变量指针
 * @return 未初始化返回1，已初始化返回0
 */
STATIC INLINE INT32 CondInitCheck(const pthread_cond_t *cond)
{
    // 检查事件链表是否为空（未初始化状态）
    if ((cond->event.stEventList.pstPrev == NULL) &&
        (cond->event.stEventList.pstNext == NULL)) {
        return 1;  // 未初始化
    }
    return 0;  // 已初始化
}

/**
 * @brief 获取条件变量属性的进程共享属性
 * @param attr 条件变量属性指针
 * @param shared 输出参数，返回共享属性（仅支持PTHREAD_PROCESS_PRIVATE）
 * @return 成功返回0，失败返回EINVAL
 */
int pthread_condattr_getpshared(const pthread_condattr_t *attr, int *shared)
{
    if ((attr == NULL) || (shared == NULL)) {  // 参数校验
        return EINVAL;  // 返回参数无效错误
    }

    *shared = PTHREAD_PROCESS_PRIVATE;  // 仅支持进程内私有

    return 0;  // 成功返回
}

/**
 * @brief 设置条件变量属性的进程共享属性
 * @param attr 条件变量属性指针
 * @param shared 共享属性（仅支持PTHREAD_PROCESS_PRIVATE）
 * @return 成功返回0，不支持返回ENOSYS，参数无效返回EINVAL
 */
int pthread_condattr_setpshared(pthread_condattr_t *attr, int shared)
{
    (VOID)attr;  // 未使用参数
    // 校验共享属性值
    if ((shared != PTHREAD_PROCESS_PRIVATE) && (shared != PTHREAD_PROCESS_SHARED)) {
        return EINVAL;  // 参数无效
    }

    if (shared != PTHREAD_PROCESS_PRIVATE) {  // 仅支持私有属性
        return ENOSYS;  // 不支持进程间共享
    }

    return 0;  // 成功返回
}

/**
 * @brief 销毁条件变量属性对象
 * @param attr 条件变量属性指针
 * @return 成功返回0，参数无效返回EINVAL
 */
int pthread_condattr_destroy(pthread_condattr_t *attr)
{
    if (attr == NULL) {  // 参数校验
        return EINVAL;  // 返回参数无效错误
    }

    return 0;  // 成功返回（无实际资源需要释放）
}

/**
 * @brief 初始化条件变量属性对象
 * @param attr 条件变量属性指针
 * @return 成功返回0，参数无效返回EINVAL
 */
int pthread_condattr_init(pthread_condattr_t *attr)
{
    if (attr == NULL) {  // 参数校验
        return EINVAL;  // 返回参数无效错误
    }

    return 0;  // 成功返回（无特殊属性需要初始化）
}

/**
 * @brief 销毁条件变量
 * @param cond 条件变量指针
 * @return 成功返回0，参数无效返回EINVAL，资源忙返回EBUSY
 */
int pthread_cond_destroy(pthread_cond_t *cond)
{
    if (cond == NULL) {  // 参数校验
        return EINVAL;  // 返回参数无效错误
    }

    if (CondInitCheck(cond)) {  // 检查是否未初始化
        return ENOERR;  // 未初始化状态直接返回成功
    }

    if (LOS_EventDestroy(&cond->event) != LOS_OK) {  // 销毁事件
        return EBUSY;  // 事件资源忙
    }
    if (pthread_mutex_destroy(cond->mutex) != ENOERR) {  // 销毁内部互斥锁
        PRINT_ERR("%s mutex destroy fail!\n", __FUNCTION__);  // 打印错误信息
        return EINVAL;  // 返回互斥锁销毁失败
    }
    free(cond->mutex);  // 释放互斥锁内存
    cond->mutex = NULL;  // 置空指针
    return ENOERR;  // 成功返回
}

/**
 * @brief 初始化条件变量
 * @param cond 条件变量指针
 * @param attr 条件变量属性（未使用）
 * @return 成功返回0，参数无效返回EINVAL，内存不足返回ENOMEM
 */
int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr)
{
    int ret = ENOERR;  // 返回值

    if (cond == NULL) {  // 参数校验
        return EINVAL;  // 返回参数无效错误
    }
    (VOID)attr;  // 未使用属性参数
    (VOID)LOS_EventInit(&(cond->event));  // 初始化事件

    // 分配内部互斥锁内存
    cond->mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    if (cond->mutex == NULL) {  // 内存分配失败
        return ENOMEM;  // 返回内存不足错误
    }

    (VOID)pthread_mutex_init(cond->mutex, NULL);  // 初始化互斥锁

    cond->value = 0;  // 初始化条件变量值
    (VOID)pthread_mutex_lock(cond->mutex);  // 加互斥锁
    cond->count = 0;  // 初始化等待计数
    (VOID)pthread_mutex_unlock(cond->mutex);  // 解互斥锁

    return ret;  // 成功返回
}

/**
 * @brief 修改条件变量值（原子操作）
 * @param cond 条件变量指针
 */
STATIC VOID PthreadCondValueModify(pthread_cond_t *cond)
{
    UINT32 flags = ((UINT32)cond->value & COND_FLAGS_MASK);  // 提取标志位
    INT32 oldVal, newVal;  // 旧值和新值

    while (true) {  // 循环直到原子操作成功
        oldVal = cond->value;  // 获取当前值
        // 计算新值：计数器减步长，保留标志位
        newVal = (INT32)(((UINT32)(oldVal - COND_COUNTER_STEP) & COND_COUNTER_MASK) | flags);
        // 原子比较并交换值
        if (LOS_AtomicCmpXchg32bits(&cond->value, newVal, oldVal) == 0) {
            break;  // 交换成功，退出循环
        }
    }
}

/**
 * @brief 广播唤醒所有等待在条件变量上的线程
 * @param cond 条件变量指针
 * @return 成功返回0，参数无效返回EINVAL
 */
int pthread_cond_broadcast(pthread_cond_t *cond)
{
    int ret = ENOERR;  // 返回值

    if (cond == NULL) {  // 参数校验
        return EINVAL;  // 返回参数无效错误
    }

    (VOID)pthread_mutex_lock(cond->mutex);  // 加互斥锁
    if (cond->count > 0) {  // 有等待线程
        cond->count = 0;  // 重置等待计数
        (VOID)pthread_mutex_unlock(cond->mutex);  // 解互斥锁

        PthreadCondValueModify(cond);  // 修改条件变量值

        (VOID)LOS_EventWrite(&(cond->event), BROADCAST_EVENT);  // 写入广播事件
        return ret;  // 返回成功
    }
    (VOID)pthread_mutex_unlock(cond->mutex);  // 解互斥锁

    return ret;  // 无等待线程，直接返回
}

/**
 * @brief 唤醒一个等待在条件变量上的线程
 * @param cond 条件变量指针
 * @return 成功返回0，参数无效返回EINVAL
 */
int pthread_cond_signal(pthread_cond_t *cond)
{
    int ret = ENOERR;  // 返回值

    if (cond == NULL) {  // 参数校验
        return EINVAL;  // 返回参数无效错误
    }

    (VOID)pthread_mutex_lock(cond->mutex);  // 加互斥锁
    if (cond->count > 0) {  // 有等待线程
        cond->count--;  // 等待计数减1
        (VOID)pthread_mutex_unlock(cond->mutex);  // 解互斥锁
        PthreadCondValueModify(cond);  // 修改条件变量值
        (VOID)OsEventWriteOnce(&(cond->event), 0x01);  // 写入信号事件（确保只唤醒一个线程）

        return ret;  // 返回成功
    }
    (VOID)pthread_mutex_unlock(cond->mutex);  // 解互斥锁

    return ret;  // 无等待线程，直接返回
}

/**
 * @brief 条件变量等待子函数（内部实现）
 * @param cond 条件变量指针
 * @param value 条件变量期望值
 * @param ticks 等待超时时间（ticks）
 * @return 事件值或错误码
 */
STATIC INT32 PthreadCondWaitSub(pthread_cond_t *cond, INT32 value, UINT32 ticks)
{
    // 事件条件结构体：检查条件变量值是否变化
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
    // 带条件的事件读取：若条件变量值变化则不阻塞
    return (int)OsEventReadWithCond(&eventCond, &(cond->event), 0x0fU,
                                    LOS_WAITMODE_OR | LOS_WAITMODE_CLR, ticks);
}

/**
 * @brief 减少条件变量等待计数
 * @param cond 条件变量指针
 */
STATIC VOID PthreadCountSub(pthread_cond_t *cond)
{
    (VOID)pthread_mutex_lock(cond->mutex);  // 加互斥锁
    if (cond->count > 0) {  // 等待计数大于0
        cond->count--;  // 计数减1
    }
    (VOID)pthread_mutex_unlock(cond->mutex);  // 解互斥锁
}

/**
 * @brief 处理等待返回值
 * @param cond 条件变量指针
 * @param val 原始返回值
 * @return 转换后的错误码
 */
STATIC INT32 ProcessReturnVal(pthread_cond_t *cond, INT32 val)
{
    INT32 ret;  // 处理后的返回值
    switch (val) {
        /* 0: event does not occur */
        case 0:               // 事件未发生
        case BROADCAST_EVENT: // 广播事件
            ret = ENOERR;     // 成功返回
            break;
        case LOS_ERRNO_EVENT_READ_TIMEOUT:  // 超时错误
            PthreadCountSub(cond);          // 减少等待计数
            ret = ETIMEDOUT;                // 返回超时错误
            break;
        default:  // 其他错误
            PthreadCountSub(cond);  // 减少等待计数
            ret = EINVAL;           // 返回参数无效错误
            break;
    }
    return ret;  // 返回处理后的结果
}

/**
 * @brief 带超时的条件变量等待
 * @param cond 条件变量指针
 * @param mutex 关联的互斥锁
 * @param absTime 绝对超时时间
 * @return 成功返回0，错误返回对应的错误码
 */
int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
                           const struct timespec *absTime)
{
    UINT32 absTicks;  // 绝对超时时间（ticks）
    INT32 ret;        // 返回值
    INT32 oldValue;   // 条件变量旧值

    pthread_testcancel();  // 检查取消请求
    if ((cond == NULL) || (mutex == NULL) || (absTime == NULL)) {  // 参数校验
        return EINVAL;  // 返回参数无效错误
    }

    if (CondInitCheck(cond)) {  // 检查条件变量是否未初始化
        ret = pthread_cond_init(cond, NULL);  // 自动初始化
        if (ret != ENOERR) {  // 初始化失败
            return ret;       // 返回错误
        }
    }
    oldValue = cond->value;  // 保存条件变量当前值

    (VOID)pthread_mutex_lock(cond->mutex);  // 加内部互斥锁
    cond->count++;  // 等待计数加1
    (VOID)pthread_mutex_unlock(cond->mutex);  // 解内部互斥锁

    if ((absTime->tv_sec == 0) && (absTime->tv_nsec == 0)) {  // 超时时间为0
        return ETIMEDOUT;  // 直接返回超时
    }

    if (!ValidTimeSpec(absTime)) {  // 校验时间格式
        return EINVAL;  // 返回参数无效错误
    }

    absTicks = OsTimeSpec2Tick(absTime);  // 将timespec转换为ticks
    if (pthread_mutex_unlock(mutex) != ENOERR) {  // 释放用户提供的互斥锁
        PRINT_ERR("%s: %d failed\n", __FUNCTION__, __LINE__);  // 打印错误信息
    }

#ifndef LOSCFG_ARCH_CORTEX_M7  // 非Cortex-M7架构
    ret = PthreadCondWaitSub(cond, oldValue, absTicks);  // 带条件等待事件
#else  // Cortex-M7架构特殊处理
    ret = (INT32)LOS_EventRead(&(cond->event), 0x0f, LOS_WAITMODE_OR | LOS_WAITMODE_CLR, absTicks);
#endif
    if (pthread_mutex_lock(mutex) != ENOERR) {  // 重新获取用户提供的互斥锁
        PRINT_ERR("%s: %d failed\n", __FUNCTION__, __LINE__);  // 打印错误信息
    }

    ret = ProcessReturnVal(cond, ret);  // 处理返回值
    pthread_testcancel();  // 检查取消请求
    return ret;  // 返回结果
}

/**
 * @brief 无条件等待条件变量
 * @param cond 条件变量指针
 * @param mutex 关联的互斥锁
 * @return 成功返回0，错误返回对应的错误码
 */
int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
    int ret;         // 返回值
    int oldValue;    // 条件变量旧值

    if ((cond == NULL) || (mutex == NULL)) {  // 参数校验
        return EINVAL;  // 返回参数无效错误
    }

    if (CondInitCheck(cond)) {  // 检查条件变量是否未初始化
        ret = pthread_cond_init(cond, NULL);  // 自动初始化
        if (ret != ENOERR) {  // 初始化失败
            return ret;       // 返回错误
        }
    }
    oldValue = cond->value;  // 保存条件变量当前值

    (VOID)pthread_mutex_lock(cond->mutex);  // 加内部互斥锁
    cond->count++;  // 等待计数加1
    (VOID)pthread_mutex_unlock(cond->mutex);  // 解内部互斥锁

    if (pthread_mutex_unlock(mutex) != ENOERR) {  // 释放用户提供的互斥锁
        PRINT_ERR("%s: %d failed\n", __FUNCTION__, __LINE__);  // 打印错误信息
    }

#ifndef LOSCFG_ARCH_CORTEX_M7  // 非Cortex-M7架构
    ret = PthreadCondWaitSub(cond, oldValue, LOS_WAIT_FOREVER);  // 永久等待
#else  // Cortex-M7架构特殊处理
    ret = (INT32)LOS_EventRead(&(cond->event), 0x0f, LOS_WAITMODE_OR | LOS_WAITMODE_CLR, LOS_WAIT_FOREVER);
#endif
    if (pthread_mutex_lock(mutex) != ENOERR) {  // 重新获取用户提供的互斥锁
        PRINT_ERR("%s: %d failed\n", __FUNCTION__, __LINE__);  // 打印错误信息
    }

    switch (ret) {  // 处理返回值
        /* 0: event does not occur */
        case 0:               // 事件未发生
        case BROADCAST_EVENT: // 广播事件
            ret = ENOERR;     // 成功返回
            break;
        default:  // 其他错误
            PthreadCountSub(cond);  // 减少等待计数
            ret = EINVAL;           // 返回参数无效错误
            break;
    }

    return ret;  // 返回结果
}
