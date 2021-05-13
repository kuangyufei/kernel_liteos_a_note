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


static pthread_mutex_t g_mutex075;
static sem_t g_sem075;
static int g_pshared075;

static VOID *TaskF01(void *arg)
{
    UINT32 ret;
    ret = pthread_mutex_lock(&g_mutex075);
    ICUNIT_TRACK_EQUAL(ret, 0, ret);

    g_testCount++;
    ICUNIT_TRACK_EQUAL(g_testCount, 3, g_testCount); // 3, Here, assert that g_testCount is equal to 3.

    ret = sem_wait(&g_sem075);
    ICUNIT_TRACK_EQUAL(ret, 0, ret);

    g_testCount++;
    ICUNIT_TRACK_EQUAL(g_testCount, 6, g_testCount); // 6, Here, assert that g_testCount is equal to 6.

    ret = sem_post(&g_sem075);
    ICUNIT_TRACK_EQUAL(ret, 0, ret);

    g_testCount++;
    ICUNIT_TRACK_EQUAL(g_testCount, 7, g_testCount); // 7, Here, assert that g_testCount is equal to 7.

    ret = pthread_mutex_unlock(&g_mutex075);
    ICUNIT_TRACK_EQUAL(ret, 0, ret);

    return NULL;
}
static VOID *TaskF02(void *arg)
{
    UINT32 ret;
    LOS_TaskDelay(2); // 2, set delay time.

    g_testCount++;
    ICUNIT_TRACK_EQUAL(g_testCount, 4, g_testCount); // 4, Here, assert that g_testCount is equal to 4.

    ret = pthread_mutex_lock(&g_mutex075);
    ICUNIT_TRACK_EQUAL(ret, 0, ret); //

    g_testCount++;
    ICUNIT_TRACK_EQUAL(g_testCount, 8, g_testCount); // 8, Here, assert that g_testCount is equal to 8.

    ret = pthread_mutex_unlock(&g_mutex075);
    ICUNIT_TRACK_EQUAL(ret, 0, ret);

    return NULL;
}
static VOID *TaskF03(void *arg)
{
    UINT32 ret;

    g_testCount++;
    ICUNIT_TRACK_EQUAL(g_testCount, 1, g_testCount);

    g_testCount++;
    ICUNIT_TRACK_EQUAL(g_testCount, 2, g_testCount); // 2, Here, assert that g_testCount is equal to 2.

    LOS_TaskDelay(3); // 3, set delay time.

    g_testCount++;
    ICUNIT_TRACK_EQUAL(g_testCount, 5, g_testCount); // 5, Here, assert that g_testCount is equal to 5.

    ret = sem_post(&g_sem075);
    ICUNIT_TRACK_EQUAL(ret, 0, ret);

    LOS_TaskDelay(1);
    g_testCount++;
    ICUNIT_TRACK_EQUAL(g_testCount, 9, g_testCount); // 9, Here, assert the g_testCount.

    return NULL;
}

static UINT32 Testcase(VOID)
{
    UINT32 ret;
    pthread_t newTh;
    pthread_attr_t attr;
    pthread_t newTh2;
    pthread_attr_t attr2;
    pthread_t newTh3;
    pthread_attr_t attr3;
    g_testCount = 0;

    ret = pthread_mutex_init(&g_mutex075, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = sem_init(&g_sem075, g_pshared075, 0);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = PosixPthreadInit(&attr, 10); // 10, Set thread priority.
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    ret = pthread_create(&newTh, &attr, TaskF03, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = PosixPthreadInit(&attr2, 15); // 15, Set thread priority.
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    ret = pthread_create(&newTh2, &attr2, TaskF01, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = PosixPthreadInit(&attr3, 4); // 4, Set thread priority.
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    ret = pthread_create(&newTh3, &attr3, TaskF02, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    LOS_TaskDelay(5); // 5, set delay time.
    ret = pthread_mutex_destroy(&g_mutex075);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = sem_destroy(&g_sem075);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = PosixPthreadDestroy(&attr, newTh);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = PosixPthreadDestroy(&attr2, newTh2);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = PosixPthreadDestroy(&attr3, newTh3);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    return LOS_OK;
}


VOID ItPosixMux075(void)
{
    TEST_ADD_CASE("ItPosixMux075", Testcase, TEST_POSIX, TEST_MUX, TEST_LEVEL2, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
