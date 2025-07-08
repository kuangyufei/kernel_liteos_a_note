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

#include "semaphore.h"
#include "sys/types.h"
#include "map_error.h"
#include "time_posix.h"


/* Initialize semaphore to value, shared is not supported in Huawei LiteOS. */
/**
 * @brief 初始化无名信号量
 * @param[in,out] sem 信号量对象指针
 * @param[in] shared 进程间共享标志(当前未使用)
 * @param[in] value 信号量初始值
 * @return 成功返回0，失败返回-1并设置errno
 */
int sem_init(sem_t *sem, int shared, unsigned int value)
{
    UINT32 semHandle = 0;  // 信号量句柄
    UINT32 ret;            // 返回值

    (VOID)shared;  // 未使用进程间共享功能，忽略该参数
    // 检查信号量指针有效性和初始值范围
    if ((sem == NULL) || (value > OS_SEM_COUNT_MAX)) {
        errno = EINVAL;  // 设置错误码为参数无效
        return -1;       // 返回错误
    }

    // 调用内核接口创建信号量，初始值为value
    ret = LOS_SemCreate(value, &semHandle);
    if (map_errno(ret) != ENOERR) {  // 检查创建结果
        return -1;                   // 创建失败返回-1
    }

    sem->sem = GET_SEM(semHandle);  // 将信号量句柄转换为信号量控制块指针

    return 0;  // 初始化成功返回0
}

/**
 * @brief 销毁无名信号量
 * @param[in] sem 信号量对象指针
 * @return 成功返回0，失败返回-1并设置errno
 */
int sem_destroy(sem_t *sem)
{
    UINT32 ret;  // 返回值

    // 检查信号量指针及其控制块有效性
    if ((sem == NULL) || (sem->sem == NULL)) {
        errno = EINVAL;  // 设置错误码为参数无效
        return -1;       // 返回错误
    }

    // 调用内核接口删除信号量
    ret = LOS_SemDelete(sem->sem->semID);
    if (map_errno(ret) != ENOERR) {  // 检查删除结果
        return -1;                   // 删除失败返回-1
    }
    return 0;  // 销毁成功返回0
}

/* Decrement value if >0 or wait for a post. */
/**
 * @brief 等待信号量(若信号量值>0则减1，否则阻塞等待)
 * @param[in] sem 信号量对象指针
 * @return 成功返回0，失败返回-1并设置errno
 */
int sem_wait(sem_t *sem)
{
    UINT32 ret;  // 返回值

    // 检查信号量指针及其控制块有效性
    if ((sem == NULL) || (sem->sem == NULL)) {
        errno = EINVAL;  // 设置错误码为参数无效
        return -1;       // 返回错误
    }

    // 调用内核接口等待信号量，永久阻塞(LOS_WAIT_FOREVER)
    ret = LOS_SemPend(sem->sem->semID, LOS_WAIT_FOREVER);
    if (map_errno(ret) == ENOERR) {  // 检查等待结果
        return 0;                    // 成功获取信号量返回0
    } else {
        return -1;                   // 获取失败返回-1
    }
}

/* Decrement value if >0, return -1 if not. */
/**
 * @brief 尝试等待信号量(若信号量值>0则减1，否则立即返回)
 * @param[in] sem 信号量对象指针
 * @return 成功返回0，失败返回-1并设置errno
 */
int sem_trywait(sem_t *sem)
{
    UINT32 ret;  // 返回值

    // 检查信号量指针及其控制块有效性
    if ((sem == NULL) || (sem->sem == NULL)) {
        errno = EINVAL;  // 设置错误码为参数无效
        return -1;       // 返回错误
    }

    // 调用内核接口尝试等待信号量，不阻塞(LOS_NO_WAIT)
    ret = LOS_SemPend(sem->sem->semID, LOS_NO_WAIT);
    if (map_errno(ret) == ENOERR) {  // 检查尝试结果
        return 0;                    // 成功获取信号量返回0
    } else {
        // 若不是无效参数错误且信号量不可用，设置错误码为EAGAIN
        if ((errno != EINVAL) || (ret == LOS_ERRNO_SEM_UNAVAILABLE)) {
            errno = EAGAIN;  // 资源暂时不可用
        }
        return -1;           // 获取失败返回-1
    }
}

/**
 * @brief 带超时的信号量等待
 * @param[in] sem 信号量对象指针
 * @param[in] timeout 超时时间结构体指针
 * @return 成功返回0，失败返回-1并设置errno
 */
int sem_timedwait(sem_t *sem, const struct timespec *timeout)
{
    UINT32 ret;       // 返回值
    UINT32 tickCnt;   // 超时时间( ticks )

    // 检查信号量指针及其控制块有效性
    if ((sem == NULL) || (sem->sem == NULL)) {
        errno = EINVAL;  // 设置错误码为参数无效
        return -1;       // 返回错误
    }

    // 检查超时时间结构体有效性
    if (!ValidTimeSpec(timeout)) {
        errno = EINVAL;  // 设置错误码为参数无效
        return -1;       // 返回错误
    }

    tickCnt = OsTimeSpec2Tick(timeout);  // 将timespec转换为系统ticks
    // 调用内核接口等待信号量，带超时时间
    ret = LOS_SemPend(sem->sem->semID, tickCnt);
    if (map_errno(ret) == ENOERR) {  // 检查等待结果
        return 0;                    // 成功获取信号量返回0
    } else {
        return -1;                   // 获取失败返回-1
    }
}

/**
 * @brief 发布信号量(信号量值加1)
 * @param[in] sem 信号量对象指针
 * @return 成功返回0，失败返回-1并设置errno
 */
int sem_post(sem_t *sem)
{
    UINT32 ret;  // 返回值

    // 检查信号量指针及其控制块有效性
    if ((sem == NULL) || (sem->sem == NULL)) {
        errno = EINVAL;  // 设置错误码为参数无效
        return -1;       // 返回错误
    }

    // 调用内核接口释放信号量
    ret = LOS_SemPost(sem->sem->semID);
    if (map_errno(ret) != ENOERR) {  // 检查释放结果
        return -1;                   // 释放失败返回-1
    }

    return 0;  // 发布成功返回0
}

/**
 * @brief 获取信号量当前值
 * @param[in] sem 信号量对象指针
 * @param[out] currVal 当前信号量值输出指针
 * @return 成功返回0，失败返回-1并设置errno
 */
int sem_getvalue(sem_t *sem, int *currVal)
{
    INT32 val;  // 临时存储信号量值

    // 检查信号量指针和输出指针有效性
    if ((sem == NULL) || (currVal == NULL)) {
        errno = EINVAL;  // 设置错误码为参数无效
        return -1;       // 返回错误
    }
    val = sem->sem->semCount;  // 获取信号量计数值
    if (val < 0) {             // 若计数值为负(有等待线程)
        val = 0;               // POSIX标准要求返回0
    }

    *currVal = val;  // 将结果存入输出参数
    return 0;        // 成功返回0
}

/**
 * @brief 打开命名信号量(当前未实现)
 * @param[in] name 信号量名称
 * @param[in] openFlag 打开标志
 * @return 始终返回NULL并设置errno为ENOSYS
 */
sem_t *sem_open(const char *name, int openFlag, ...)
{
    (VOID)name;    // 未使用参数，避免编译警告
    (VOID)openFlag;
    errno = ENOSYS;  // 设置错误码为功能未实现
    return NULL;     // 返回NULL表示失败
}

/**
 * @brief 关闭命名信号量(当前未实现)
 * @param[in] sem 信号量对象指针
 * @return 始终返回-1并设置errno为ENOSYS
 */
int sem_close(sem_t *sem)
{
    (VOID)sem;     // 未使用参数，避免编译警告
    errno = ENOSYS;  // 设置错误码为功能未实现
    return -1;       // 返回-1表示失败
}

/**
 * @brief 删除命名信号量(当前未实现)
 * @param[in] name 信号量名称
 * @return 始终返回-1并设置errno为ENOSYS
 */
int sem_unlink(const char *name)
{
    (VOID)name;    // 未使用参数，避免编译警告
    errno = ENOSYS;  // 设置错误码为功能未实现
    return -1;       // 返回-1表示失败
}