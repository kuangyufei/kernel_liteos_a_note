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

//创建并初始化一个无名信号量
/* Initialize semaphore to value, shared is not supported in Huawei LiteOS. */
int sem_init(sem_t *sem, int shared, unsigned int value)//初始化信号量，Huawei LiteOS 不支持共享
{
    UINT32 semHandle = 0;
    UINT32 ret;

    (VOID)shared;
    if ((sem == NULL) || (value > OS_SEM_COUNT_MAX)) {
        errno = EINVAL;
        return -1;
    }

    ret = LOS_SemCreate(value, &semHandle);
    if (map_errno(ret) != ENOERR) {
        return -1;
    }

    sem->sem = GET_SEM(semHandle);

    return 0;
}
///销毁指定的信号量
int sem_destroy(sem_t *sem)
{
    UINT32 ret;

    if ((sem == NULL) || (sem->sem == NULL)) {
        errno = EINVAL;
        return -1;
    }

    ret = LOS_SemDelete(sem->sem->semID);
    if (map_errno(ret) != ENOERR) {
        return -1;
    }
    return 0;
}
///获取信号量
/* Decrement value if >0 or wait for a post. */
int sem_wait(sem_t *sem)
{
    UINT32 ret;

    if ((sem == NULL) || (sem->sem == NULL)) {
        errno = EINVAL;
        return -1;
    }

    ret = LOS_SemPend(sem->sem->semID, LOS_WAIT_FOREVER);
    if (map_errno(ret) == ENOERR) {
        return 0;
    } else {
        return -1;
    }
}
///尝试获取信号量
/* Decrement value if >0, return -1 if not. */
int sem_trywait(sem_t *sem)
{
    UINT32 ret;

    if ((sem == NULL) || (sem->sem == NULL)) {
        errno = EINVAL;
        return -1;
    }

    ret = LOS_SemPend(sem->sem->semID, LOS_NO_WAIT);
    if (map_errno(ret) == ENOERR) {
        return 0;
    } else {
        if ((errno != EINVAL) || (ret == LOS_ERRNO_SEM_UNAVAILABLE)) {
            errno = EAGAIN;
        }
        return -1;
    }
}
///设置获取信号量时间,时间到了不管是否获取也返回.
int sem_timedwait(sem_t *sem, const struct timespec *timeout)
{
    UINT32 ret;
    UINT32 tickCnt;

    if ((sem == NULL) || (sem->sem == NULL)) {
        errno = EINVAL;
        return -1;
    }

    if (!ValidTimeSpec(timeout)) {
        errno = EINVAL;
        return -1;
    }

    tickCnt = OsTimeSpec2Tick(timeout);
    ret = LOS_SemPend(sem->sem->semID, tickCnt);
    if (map_errno(ret) == ENOERR) {
        return 0;
    } else {
        return -1;
    }
}
///增加信号量计数
int sem_post(sem_t *sem)
{
    UINT32 ret;

    if ((sem == NULL) || (sem->sem == NULL)) {
        errno = EINVAL;
        return -1;
    }

    ret = LOS_SemPost(sem->sem->semID);
    if (map_errno(ret) != ENOERR) {
        return -1;
    }

    return 0;
}

int sem_getvalue(sem_t *sem, int *currVal)
{
    INT32 val;

    if ((sem == NULL) || (currVal == NULL)) {
        errno = EINVAL;
        return -1;
    }
    val = sem->sem->semCount;
    if (val < 0) {
        val = 0;
    }

    *currVal = val;
    return 0;
}

sem_t *sem_open(const char *name, int openFlag, ...)
{
    (VOID)name;
    (VOID)openFlag;
    errno = ENOSYS;
    return NULL;
}

int sem_close(sem_t *sem)
{
    (VOID)sem;
    errno = ENOSYS;
    return -1;
}

int sem_unlink(const char *name)
{
    (VOID)name;
    errno = ENOSYS;
    return -1;
}

