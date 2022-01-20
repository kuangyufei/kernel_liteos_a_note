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

static pthread_barrier_t g_barrier;
static int g_testToCount001 = 0;
static int g_threadTest[10];

static void *ThreadFuncTest0(void *a)
{
    int ret;
    int count = *((int *)a);
    g_testToCount001++;

    ret = pthread_barrier_wait(&g_barrier);
    ICUNIT_GOTO_EQUAL(ret, PTHREAD_BARRIER_SERIAL_THREAD, ret, EXIT);

    g_threadTest[count] = count;

EXIT:
    return NULL;
}

static void *ThreadFuncTest2(void *a)
{
    int ret;
    int count = *((int *)a);
    g_testToCount001++;

    ret = pthread_barrier_wait(&g_barrier);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    g_threadTest[count] = count;

EXIT:
    return NULL;
}

static void *ThreadFuncTest1(void *a)
{
    int ret;
    int count = *((int *)a);
    g_testToCount001++;

    ret = pthread_barrier_wait(&g_barrier);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    g_threadTest[count] = count;
EXIT:
    return NULL;
}

static int Testcase(void)
{
    struct sched_param param = { 0 };
    int ret;
    void *res = NULL;
    pthread_attr_t a = { 0 };
    pthread_t thread;
    pthread_t newPthread[10], newPthread1;
    pthread_mutexattr_t mutex;
    int index = 0;
    int currThreadPri, currThreadPolicy;
    int threadParam[10];
    ret = pthread_getschedparam(pthread_self(), &currThreadPolicy, &param);
    ICUNIT_ASSERT_EQUAL(ret, 0, -ret);
    currThreadPri = param.sched_priority;
    const int testCount = 10;

    g_testToCount001 = 0;

    ret = pthread_attr_init(&a);
    pthread_attr_setinheritsched(&a, PTHREAD_EXPLICIT_SCHED);
    param.sched_priority = currThreadPri - 1;
    pthread_attr_setschedparam(&a, &param);

    ret = pthread_barrier_init(&g_barrier, NULL, testCount);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    threadParam[0] = 0;
    ret = pthread_create(&newPthread[index], &a, ThreadFuncTest0, &threadParam[0]);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    g_threadTest[0] = 0;

    index = 1;
    while (index < (testCount - 1)) {
        threadParam[index] = index;
        ret = pthread_create(&newPthread[index], &a, ThreadFuncTest1, &threadParam[index]);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);
        g_threadTest[index] = 0;
        index++;
    }

    ICUNIT_ASSERT_EQUAL(g_testToCount001, testCount - 1, g_testToCount001);

    threadParam[index] = index;
    ret = pthread_create(&newPthread[index], &a, ThreadFuncTest2, &threadParam[index]);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    sleep(1);

    ICUNIT_ASSERT_EQUAL(g_testToCount001, testCount, g_testToCount001);

    index = 0;
    while (index < testCount) {
        threadParam[index] = index;
        ret = pthread_join(newPthread[index], NULL);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);
        ICUNIT_ASSERT_EQUAL(g_threadTest[index], index, g_threadTest[index]);
        index++;
    }

    ret = pthread_barrier_destroy(&g_barrier);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    return 0;
}

void ItTestPthread006(void)
{
    TEST_ADD_CASE("IT_POSIX_PTHREAD_006", Testcase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
