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
#include "it_pthread_test.h"
static pthread_cond_t g_pthread_cond;
static pthread_mutex_t g_pthread_mutex;
#define TEST_THREAD_COUNT 5

static void *pthread_cond_func002(void *arg)
{
    int ret;
    struct timespec ts;

    g_testCount++;

    ret = pthread_mutex_lock(&g_pthread_mutex);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += 2; /* 2: wait 2 seconds */

    ret = pthread_cond_timedwait(&g_pthread_cond, &g_pthread_mutex, &ts);
    ICUNIT_GOTO_EQUAL(ret, ETIMEDOUT, ret, EXIT);

    ret = pthread_mutex_unlock(&g_pthread_mutex);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    g_testCount++;
EXIT:
    return NULL;
}

static VOID *pthread_f07(void *argument)
{
    int policy;
    int ret;
    int i;
    pthread_attr_t attr;
    struct sched_param schedParam = { 0 };
    pthread_t thread[TEST_THREAD_COUNT];

    g_testCount = 0;

    ret = pthread_attr_init(&attr);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = pthread_getschedparam(pthread_self(), &policy, &schedParam);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    schedParam.sched_priority -= 1;
    ret = pthread_attr_setschedparam(&attr, &schedParam);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    for (i = 0; i < TEST_THREAD_COUNT; i++) {
        ret = pthread_create(&thread[i], &attr, pthread_cond_func002, NULL);
        ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);
    }

    sleep(1);

    ICUNIT_GOTO_EQUAL(g_testCount, 5, g_testCount, EXIT); /* 5: Five threads */

    for (i = 0; i < TEST_THREAD_COUNT; i++) {
        ret = pthread_join(thread[i], NULL);
        ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);
    }

    ICUNIT_GOTO_EQUAL(g_testCount, 10, g_testCount, EXIT); /* 10: Twice per thread */

EXIT:
    return NULL;
}

static int TestCase(void)
{
    int policy;
    pthread_attr_t attr;
    pthread_t newTh;
    struct sched_param schedParam = { 0 };
    int ret;

    ret = pthread_mutex_init(&g_pthread_mutex, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_cond_init(&g_pthread_cond, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_attr_init(&attr);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_getschedparam(pthread_self(), &policy, &schedParam);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    schedParam.sched_priority -= 1;
    ret = pthread_attr_setschedparam(&attr, &schedParam);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_create(&newTh, &attr, pthread_f07, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_join(newTh, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    return 0;
}

void ItTestPthreadCond004(void)
{
    TEST_ADD_CASE("IT_POSIX_PTHREAD_COND_004", TestCase, TEST_POSIX, TEST_PTHREAD, TEST_LEVEL2, TEST_FUNCTION);
}
