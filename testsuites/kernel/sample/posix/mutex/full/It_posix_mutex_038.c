/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of
 * conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list
 * of conditions and the following disclaimer in the documentation and/or other materials
 * provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used
 * to endorse or promote products derived from this software without specific prior written
 * permission.
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

#include "It_posix_mutex.h"
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cpluscplus */
#endif /* __cpluscplus */


static pthread_mutex_t g_mutex038;
static sem_t g_sem038;

/* pthread_mutex_lock 4-1.c
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

 * This sample test aims to check the following assertion:
 *
 * If the mutex type is PTHREAD_MUTEX_RECURSIVE,
 * then the mutex maintains the concept of a lock count.
 * When a thread successfully acquires a mutex for the first time,
 * the lock count is set to one. Every time a thread relocks this mutex,
 * the lock count is incremented by one.
 * Each time the thread unlocks the mutex,
 * the lock count is decremented by one.
 * When the lock count reaches zero,
 * the mutex becomes available and others threads can acquire it.

 * The steps are:
 * ->Create a mutex with recursive attribute
 * ->Create a threads
 * ->Parent locks the mutex twice, unlocks once.
 * ->Child attempts to lock the mutex.
 * ->Parent unlocks the mutex.
 * ->Parent unlocks the mutex (shall fail)
 * ->Child unlocks the mutex.
 */
static void *TaskF01(void *arg)
{
    int ret;

    /* Try to lock the mutex once. The call must fail here. */
    ret = pthread_mutex_trylock(&g_mutex038);
    if (ret == 0) {
        ICUNIT_GOTO_EQUAL(1, 0, ret, EXIT);
    }

    /* Free the parent thread and lock the mutex (must success) */
    if ((ret = sem_post(&g_sem038))) {
        ICUNIT_GOTO_EQUAL(1, 0, ret, EXIT);
    }

    if ((ret = pthread_mutex_lock(&g_mutex038))) {
        ICUNIT_GOTO_EQUAL(1, 0, ret, EXIT);
    }

    /* Wait for the parent to let us go on */
    if ((ret = sem_post(&g_sem038))) {
        ICUNIT_GOTO_EQUAL(1, 0, ret, EXIT);
    }

    /* Unlock and exit */
    if ((ret = pthread_mutex_unlock(&g_mutex038))) {
        ICUNIT_GOTO_EQUAL(1, 0, ret, EXIT);
    }
EXIT:
    return NULL;
}

static UINT32 Testcase(VOID)
{
    int ret;
    int i;
    pthread_mutexattr_t ma;
    pthread_t child;

    /* Initialize the semaphore */
    if ((ret = sem_init(&g_sem038, 0, 0))) {
        ICUNIT_ASSERT_EQUAL(1, 0, ret);
    }

    /* We initialize the recursive mutex */
    if ((ret = pthread_mutexattr_init(&ma))) {
        ICUNIT_ASSERT_EQUAL(1, 0, ret);
    }

    if ((ret = pthread_mutexattr_settype(&ma, PTHREAD_MUTEX_RECURSIVE))) {
        ICUNIT_ASSERT_EQUAL(1, 0, ret);
    }

    if ((ret = pthread_mutex_init(&g_mutex038, &ma))) {
        ICUNIT_ASSERT_EQUAL(1, 0, ret);
    }

    if ((ret = pthread_mutexattr_destroy(&ma))) {
        ICUNIT_ASSERT_EQUAL(1, 0, ret);
    }

    /* -- The mutex is now ready for testing -- */

    /* First, we lock it twice and unlock once */
    if ((ret = pthread_mutex_lock(&g_mutex038))) {
        ICUNIT_ASSERT_EQUAL(1, 0, ret);
    }

    if ((ret = pthread_mutex_lock(&g_mutex038))) {
        ICUNIT_ASSERT_EQUAL(1, 0, ret);
    }

    if ((ret = pthread_mutex_unlock(&g_mutex038))) {
        ICUNIT_ASSERT_EQUAL(1, 0, ret);
    }

    /* Here this thread owns the mutex and the internal count is "1" */

    /* We create the child thread */
    if ((ret = pthread_create(&child, NULL, TaskF01, NULL))) {
        ICUNIT_ASSERT_EQUAL(1, 0, ret);
    }

    /* then wait for child to be ready */
    if ((ret = sem_wait(&g_sem038))) {
        ICUNIT_ASSERT_EQUAL(1, 0, ret);
    }

    /* We can now unlock the mutex */
    if ((ret = pthread_mutex_unlock(&g_mutex038))) {
        ICUNIT_ASSERT_EQUAL(1, 0, ret);
    }

    /* We wait for the child to lock the mutex */
    if ((ret = sem_wait(&g_sem038))) {
        ICUNIT_ASSERT_EQUAL(1, 0, ret);
    }

    /* Then, try to unlock the mutex (owned by the child or unlocked) */
    ret = pthread_mutex_unlock(&g_mutex038);
    if (ret == ENOERR) {
        ICUNIT_ASSERT_EQUAL(1, 0, ret);
    }

    /* Everything seems OK here */
    if ((ret = pthread_join(child, NULL))) {
        ICUNIT_ASSERT_EQUAL(1, 0, ret);
    }

    /* Simple loop to double-check */
    for (i = 0; i < 50; i++) { // 50, The loop frequency.
        if ((ret = pthread_mutex_lock(&g_mutex038))) {
            ICUNIT_ASSERT_EQUAL(1, 0, ret);
        }
    }
    for (i = 0; i < 50; i++) { // 50, The loop frequency.
        if ((ret = pthread_mutex_unlock(&g_mutex038))) {
            ICUNIT_ASSERT_EQUAL(1, 0, ret);
        }
    }

    ret = pthread_mutex_unlock(&g_mutex038);
    if (ret == 0) {
        ICUNIT_ASSERT_EQUAL(1, 0, ret);
    }
    /* The test passed, we destroy the mutex */
    if ((ret = pthread_mutex_destroy(&g_mutex038))) {
        ICUNIT_ASSERT_EQUAL(1, 0, ret);
    }

    ret = sem_destroy(&g_sem038);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    return LOS_OK;
}


VOID ItPosixMux038(void)
{
    TEST_ADD_CASE("ItPosixMux038", Testcase, TEST_POSIX, TEST_MUX, TEST_LEVEL2, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */