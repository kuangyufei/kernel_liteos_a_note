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

#include "pthread.h"

/*!
* @file pthread_mutex.c
* @brief 对鸿蒙轻内核互斥锁的封装
* @verbatim 
    当 pthread_mutex_lock() 返回时，该互斥锁已被锁定。调用线程是该互斥锁的属主。
    如果该互斥锁已被另一个线程锁定和拥有，则调用线程将阻塞，直到该互斥锁变为可用为止。

    如果互斥锁类型为 PTHREAD_MUTEX_NORMAL，则不提供死锁检测。尝试重新锁定互斥锁会导致死锁。
    如果某个线程尝试解除锁定的互斥锁不是由该线程锁定或未锁定，则将产生不确定的行为。

    如果互斥锁类型为 PTHREAD_MUTEX_ERRORCHECK，则会提供错误检查。如果某个线程尝试重新锁定的互斥锁已经由该线程锁定，
    则将返回错误。如果某个线程尝试解除锁定的互斥锁不是由该线程锁定或者未锁定，则将返回错误。

    如果互斥锁类型为 PTHREAD_MUTEX_RECURSIVE，则该互斥锁会保留锁定计数这一概念。线程首次成功获取互斥锁时，
    锁定计数会设置为 1。线程每重新锁定该互斥锁一次，锁定计数就增加 1。线程每解除锁定该互斥锁一次，锁定计数就减小 1。 
    锁定计数达到 0 时，该互斥锁即可供其他线程获取。如果某个线程尝试解除锁定的互斥锁不是由该线程锁定或者未锁定，则将返回错误。

    如果互斥锁类型是 PTHREAD_MUTEX_DEFAULT，则尝试以递归方式锁定该互斥锁将产生不确定的行为。
    对于不是由调用线程锁定的互斥锁，如果尝试解除对它的锁定，则会产生不确定的行为。如果尝试解除锁定尚未锁定的互斥锁，
    则会产生不确定的行为。
    参考链接: https://docs.oracle.com/cd/E19253-01/819-7051/sync-12/index.html
* @endverbatim 
*/
/**
 * @brief 初始化互斥锁属性对象
 * @param[in,out] attr 互斥锁属性对象指针
 * @return 成功返回0，失败返回错误码
 */
int pthread_mutexattr_init(pthread_mutexattr_t *attr)
{
    unsigned int ret = LOS_MuxAttrInit(attr);  // 调用内核接口初始化互斥锁属性
    if (ret != LOS_OK) {                       // 检查初始化是否成功
        return (int)ret;                       // 初始化失败，返回错误码
    }

#if defined POSIX_MUTEX_DEFAULT_INHERIT
    attr->protocol    = PTHREAD_PRIO_INHERIT;  // 设置默认协议为优先级继承
#elif defined POSIX_MUTEX_DEFAULT_PROTECT
    attr->protocol    = PTHREAD_PRIO_PROTECT;  // 设置默认协议为优先级保护
#else
    attr->protocol    = PTHREAD_PRIO_NONE;     // 设置默认协议为无优先级策略
#endif
    attr->type = PTHREAD_MUTEX_NORMAL;         // 设置默认互斥锁类型为普通类型
    return LOS_OK;                             // 初始化成功，返回0
}

/**
 * @brief 销毁互斥锁属性对象
 * @param[in] attr 互斥锁属性对象指针
 * @return 成功返回0，失败返回错误码
 */
int pthread_mutexattr_destroy(pthread_mutexattr_t *attr)
{
    return LOS_MuxAttrDestroy(attr);  // 调用内核接口销毁互斥锁属性
}

/**
 * @brief 设置互斥锁的优先级协议
 * @param[in,out] attr 互斥锁属性对象指针
 * @param[in] protocol 优先级协议(PTHREAD_PRIO_NONE/PTHREAD_PRIO_INHERIT/PTHREAD_PRIO_PROTECT)
 * @return 成功返回0，失败返回错误码
 */
int pthread_mutexattr_setprotocol(pthread_mutexattr_t *attr, int protocol)
{
    return LOS_MuxAttrSetProtocol(attr, protocol);  // 调用内核接口设置优先级协议
}

/**
 * @brief 获取互斥锁的优先级协议
 * @param[in] attr 互斥锁属性对象指针
 * @param[out] protocol 优先级协议输出指针
 * @return 成功返回0，失败返回错误码
 */
int pthread_mutexattr_getprotocol(const pthread_mutexattr_t *attr, int *protocol)
{
    return LOS_MuxAttrGetProtocol(attr, protocol);  // 调用内核接口获取优先级协议
}

/**
 * @brief 设置互斥锁的优先级上限
 * @param[in,out] attr 互斥锁属性对象指针
 * @param[in] prioceiling 优先级上限值
 * @return 成功返回0，失败返回错误码
 */
int pthread_mutexattr_setprioceiling(pthread_mutexattr_t *attr, int prioceiling)
{
    return LOS_MuxAttrSetPrioceiling(attr, prioceiling);  // 调用内核接口设置优先级上限
}

/**
 * @brief 获取互斥锁的优先级上限
 * @param[in] attr 互斥锁属性对象指针
 * @param[out] prioceiling 优先级上限输出指针
 * @return 成功返回0，失败返回错误码
 */
int pthread_mutexattr_getprioceiling(const pthread_mutexattr_t *attr, int *prioceiling)
{
    return LOS_MuxAttrGetPrioceiling(attr, prioceiling);  // 调用内核接口获取优先级上限
}

/**
 * @brief 设置互斥锁的当前优先级上限
 * @param[in,out] mutex 互斥锁对象指针
 * @param[in] prioceiling 新的优先级上限值
 * @param[out] oldPrioceiling 旧的优先级上限输出指针
 * @return 成功返回0，失败返回错误码
 */
int pthread_mutex_setprioceiling(pthread_mutex_t *mutex, int prioceiling, int *oldPrioceiling)
{
    return LOS_MuxSetPrioceiling(mutex, prioceiling, oldPrioceiling);  // 调用内核接口设置当前优先级上限
}

/**
 * @brief 获取互斥锁的当前优先级上限
 * @param[in] mutex 互斥锁对象指针
 * @param[out] prioceiling 优先级上限输出指针
 * @return 成功返回0，失败返回错误码
 */
int pthread_mutex_getprioceiling(const pthread_mutex_t *mutex, int *prioceiling)
{
    return LOS_MuxGetPrioceiling(mutex, prioceiling);  // 调用内核接口获取当前优先级上限
}

/**
 * @brief 获取互斥锁的类型
 * @param[in] attr 互斥锁属性对象指针
 * @param[out] outType 互斥锁类型输出指针
 * @return 成功返回0，失败返回错误码
 */
int pthread_mutexattr_gettype(const pthread_mutexattr_t *attr, int *outType)
{
    return LOS_MuxAttrGetType(attr, outType);  // 调用内核接口获取互斥锁类型
}

/**
 * @brief 设置互斥锁的类型
 * @param[in,out] attr 互斥锁属性对象指针
 * @param[in] type 互斥锁类型(PTHREAD_MUTEX_NORMAL/PTHREAD_MUTEX_ERRORCHECK等)
 * @return 成功返回0，失败返回错误码
 */
int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type)
{
    return LOS_MuxAttrSetType(attr, type);  // 调用内核接口设置互斥锁类型
}

/**
 * @brief 初始化互斥锁对象
 * @param[in,out] mutex 互斥锁对象指针
 * @param[in] mutexAttr 互斥锁属性对象指针，为NULL时使用默认属性
 * @return 成功返回0，失败返回错误码
 */
int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *mutexAttr)
{
    unsigned int ret = LOS_MuxInit(mutex, mutexAttr);  // 调用内核接口初始化互斥锁
    if ((ret == LOS_OK) && (mutexAttr == NULL)) {      // 初始化成功且使用默认属性时
#if defined POSIX_MUTEX_DEFAULT_INHERIT
        mutex->attr.protocol    = PTHREAD_PRIO_INHERIT;  // 设置默认协议为优先级继承
#elif defined POSIX_MUTEX_DEFAULT_PROTECT
        mutex->attr.protocol    = PTHREAD_PRIO_PROTECT;  // 设置默认协议为优先级保护
#else
        mutex->attr.protocol    = PTHREAD_PRIO_NONE;     // 设置默认协议为无优先级策略
#endif
        mutex->attr.type = PTHREAD_MUTEX_NORMAL;         // 设置默认互斥锁类型为普通类型
    }

    return (int)ret;  // 返回初始化结果
}

/**
 * @brief 销毁互斥锁对象
 * @param[in] mutex 互斥锁对象指针
 * @return 成功返回0，失败返回错误码
 */
int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
    return LOS_MuxDestroy(mutex);  // 调用内核接口销毁互斥锁
}

/**
 * @brief 加锁互斥锁，必要时等待
 * @param[in,out] mutex 互斥锁对象指针
 * @return 成功返回0，失败返回错误码
 */
int pthread_mutex_lock(pthread_mutex_t *mutex)
{
    return LOS_MuxLock(mutex, LOS_WAIT_FOREVER);  // 调用内核接口加锁，永久等待
}

/**
 * @brief 尝试加锁互斥锁，不阻塞
 * @param[in,out] mutex 互斥锁对象指针
 * @return 成功返回0，失败返回错误码(EBUSY表示已被锁定)
 */
int pthread_mutex_trylock(pthread_mutex_t *mutex)
{
    return LOS_MuxTrylock(mutex);  // 调用内核接口尝试加锁
}

/**
 * @brief 解锁互斥锁
 * @param[in,out] mutex 互斥锁对象指针
 * @return 成功返回0，失败返回错误码
 */
int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    return LOS_MuxUnlock(mutex);  // 调用内核接口解锁
}