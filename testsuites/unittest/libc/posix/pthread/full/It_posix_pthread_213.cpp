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
#include "It_posix_pthread.h"

static VOID pthread_f02(VOID)
{
    INT32 ret = 0;
    ret = pthread_mutex_lock(&g_pthreadMutexTest1);
    ICUNIT_TRACK_EQUAL(ret, PTHREAD_NO_ERROR, ret);

    g_testCount++;

    ret = pthread_mutex_unlock(&g_pthreadMutexTest1);
    ICUNIT_TRACK_EQUAL(ret, PTHREAD_NO_ERROR, ret);

    return;
}

static VOID *pthread_f01(VOID *arg)
{
    INT32 ret;

    ret = pthread_once((pthread_once_t *)arg, pthread_f02);
    ICUNIT_TRACK_EQUAL(ret, PTHREAD_NO_ERROR, ret);

    return NULL;
}


static UINT32 Testcase(VOID)
{
    INT32 ret, i;

    pthread_once_t myctl[PTHREAD_THREADS_NUM] = {PTHREAD_ONCE_INIT, };

    pthread_t th[PTHREAD_THREADS_NUM];

    g_pthreadMutexTest1 = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;

    g_testCount = 0;

    for (i = 0; i < PTHREAD_THREADS_NUM; i++) {
        ret = pthread_create(&th[i], NULL, pthread_f01, &myctl[i]);
        ICUNIT_ASSERT_EQUAL(ret, PTHREAD_NO_ERROR, ret);
    }

    for (i = 0; i < PTHREAD_THREADS_NUM; i++) {
        ret = pthread_join(th[i], NULL);
        ICUNIT_ASSERT_EQUAL(ret, PTHREAD_NO_ERROR, ret);
    }

    ret = pthread_mutex_lock(&g_pthreadMutexTest1);
    ICUNIT_ASSERT_EQUAL(ret, PTHREAD_NO_ERROR, ret);

    ICUNIT_ASSERT_EQUAL(g_testCount, 3, g_testCount);

    ret = pthread_mutex_unlock(&g_pthreadMutexTest1);
    ICUNIT_ASSERT_EQUAL(ret, PTHREAD_NO_ERROR, ret);

    return PTHREAD_NO_ERROR;
}


VOID ItPosixPthread213(VOID)
{
    TEST_ADD_CASE("IT_POSIX_PTHREAD_213", Testcase, TEST_POSIX, TEST_PTHREAD, TEST_LEVEL2, TEST_FUNCTION);
}
