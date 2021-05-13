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

static int g_currThreadPri, g_currThreadPolicy;
static volatile int g_pthreadTestCount = 0;
static int g_threadTestCount1 = 0;

static void *ThreadFunc5(void *ptr)
{
    pid_t pid = getpid();
    int ret;

    ICUNIT_GOTO_EQUAL(g_threadTestCount1, 2, g_threadTestCount1, EXIT); // 2, here assert the result.
    g_threadTestCount1++;

    ret = pthread_detach(pthread_self());
    ICUNIT_GOTO_EQUAL(ret, EINVAL, ret, EXIT);

EXIT:
    return NULL;
}

static void *ThreadFunc4(void *arg)
{
    pthread_t pthread = pthread_self();
    int i = 0;
    unsigned int ret;
    pid_t pid = getpid();

    ICUNIT_GOTO_EQUAL(g_pthreadTestCount, 9, g_pthreadTestCount, EXIT); // 9, here assert the result.
    g_pthreadTestCount++;                                               // 10

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

    while (1) {
        pthread_testcancel();
        if (++i == 5) { // 5, in loop 5, set cancel state.
            pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
        }

        if (i == 10) { // 10, in loop 10, cancel pthread.
            ret = pthread_cancel(pthread);
        }
    }
    return (void *)i;

EXIT:
    return NULL;
}

static void *ThreadFunc3(void *arg)
{
    pthread_t pthread = pthread_self();
    int i = 0;
    unsigned int ret;
    pid_t pid = getpid();

    g_pthreadTestCount++; // 7

    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

    while (1) {
        pthread_testcancel();
        if (++i == 5) { // 5, in loop 5, set cancel state.
            pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        }

        if (i == 10) { // 10, in loop 10, cancel pthread.
            ret = pthread_cancel(pthread);
        }
    }

    ICUNIT_GOTO_EQUAL(i, 10, i, EXIT); // 10, here assert the result.
    return (void *)i;
EXIT:
    return NULL;
}

static void *ThreadFunc2(void *arg)
{
    pthread_t pthread = pthread_self();
    pid_t pid = getpid();
    unsigned int ret;

    ICUNIT_GOTO_EQUAL(g_pthreadTestCount, 3, g_pthreadTestCount, EXIT); // 3, here assert the result.

    g_pthreadTestCount++; // 4

    ret = pthread_join(pthread_self(), 0);
    ICUNIT_GOTO_EQUAL(ret, EINVAL, ret, EXIT);

    ret = pthread_detach(pthread_self());
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

EXIT:
    return NULL;
}

static void *ThreadFunc6(void *arg)
{
    pid_t pid = getpid();

    ICUNIT_GOTO_EQUAL(g_pthreadTestCount, 12, g_pthreadTestCount, EXIT); // 12, here assert the result.
    g_pthreadTestCount++;                                                // 13

    return (void *)pthread_self();

EXIT:
    return NULL;
}

static void *ThreadFunc(void *arg)
{
    unsigned int ret;
    void *res = NULL;
    pthread_attr_t a = { 0 };
    pthread_t newPthread;
    struct sched_param param = { 0 };
    pid_t pid = getpid();
    int curThreadPri, curThreadPolicy;

    ICUNIT_GOTO_EQUAL(g_pthreadTestCount, 2, g_pthreadTestCount, EXIT); // 2, here assert the result.

    g_pthreadTestCount++; // 3

    ret = pthread_getschedparam(pthread_self(), &curThreadPolicy, &param);
    ICUNIT_GOTO_EQUAL(ret, 0, -ret, EXIT);

    curThreadPri = param.sched_priority;

    ret = pthread_attr_init(&a);
    pthread_attr_setinheritsched(&a, PTHREAD_EXPLICIT_SCHED);
    param.sched_priority = curThreadPri - 1;
    pthread_attr_setschedparam(&a, &param);
    ret = pthread_create(&newPthread, &a, ThreadFunc2, 0);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

#ifdef LOSCFG_USER_TEST_SMP
    usleep(1000 * 10 * 2); // 1000 * 10 * 2, for timing control.
#endif
    ICUNIT_GOTO_EQUAL(g_pthreadTestCount, 4, g_pthreadTestCount, EXIT); // 4, here assert the result.

    g_pthreadTestCount++; // 5

    ret = pthread_attr_init(&a);
    pthread_attr_setinheritsched(&a, PTHREAD_EXPLICIT_SCHED);
    param.sched_priority = curThreadPri + 1;
    pthread_attr_setschedparam(&a, &param);
    ret = pthread_create(&newPthread, &a, ThreadFunc3, 0);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    g_pthreadTestCount++; // 6

    ret = pthread_join(newPthread, &res);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ICUNIT_GOTO_EQUAL(g_pthreadTestCount, 7, g_pthreadTestCount, EXIT); // 7, here assert the result.
    g_pthreadTestCount++;                                               // 8

    ret = pthread_attr_init(&a);
    pthread_attr_setinheritsched(&a, PTHREAD_EXPLICIT_SCHED);
    param.sched_priority = curThreadPri + 2; // 2, adjust the priority.
    pthread_attr_setschedparam(&a, &param);
    ret = pthread_create(&newPthread, &a, ThreadFunc4, 0);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ICUNIT_GOTO_EQUAL(g_pthreadTestCount, 8, g_pthreadTestCount, EXIT); // 8, here assert the result.
    g_pthreadTestCount++;                                               // 9

    ret = pthread_join(newPthread, &res);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ICUNIT_GOTO_EQUAL(g_pthreadTestCount, 10, g_pthreadTestCount, EXIT); // 10, here assert the result.
    g_pthreadTestCount++;                                                // 11

EXIT:
    return NULL;
}

int PthreadTest001()
{
    struct sched_param param = { 0 };
    int ret;
    void *res = NULL;
    pthread_attr_t a = { 0 };
    pthread_t newPthread, newPthread1;
    int count = 0xf0000;
    g_threadTestCount1 = 0;
    g_pthreadTestCount = 0;

    ret = pthread_getschedparam(pthread_self(), &g_currThreadPolicy, &param);
    ICUNIT_ASSERT_EQUAL(ret, 0, -ret);

    g_currThreadPri = param.sched_priority;

    g_pthreadTestCount++;

    ret = pthread_attr_init(&a);
    pthread_attr_setinheritsched(&a, PTHREAD_EXPLICIT_SCHED);
    param.sched_priority = g_currThreadPri + 1;
    pthread_attr_setschedparam(&a, &param);
    pthread_attr_setschedpolicy(&a, g_currThreadPolicy);
    ret = pthread_create(&newPthread, &a, ThreadFunc, 0);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ICUNIT_ASSERT_EQUAL(g_pthreadTestCount, 1, g_pthreadTestCount);
    g_pthreadTestCount++; // 2

    ret = pthread_create(&newPthread1, &a, ThreadFunc5, 0);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    g_threadTestCount1++;
    ICUNIT_ASSERT_EQUAL(g_threadTestCount1, 1, g_threadTestCount1);

    ret = pthread_detach(newPthread1);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    g_threadTestCount1++;
    ICUNIT_ASSERT_EQUAL(g_threadTestCount1, 2, g_threadTestCount1); // 2, here assert the result.
    ret = pthread_join(newPthread, &res);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ICUNIT_ASSERT_EQUAL(g_threadTestCount1, 3, g_threadTestCount1);  // 3, here assert the result.
    ICUNIT_ASSERT_EQUAL(g_pthreadTestCount, 11, g_pthreadTestCount); // 11, here assert the result.
    g_pthreadTestCount++;                                            // 12

    param.sched_priority = g_currThreadPri - 1;
    pthread_attr_setschedparam(&a, &param);
    ret = pthread_create(&newPthread1, &a, ThreadFunc6, 0);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

#ifdef LOSCFG_USER_TEST_SMP
    usleep(1000 * 10 * 10); // 1000 * 10 * 10, for timing control.
#endif

    ICUNIT_ASSERT_EQUAL(g_pthreadTestCount, 13, g_pthreadTestCount); // 13, here assert the result.
    g_pthreadTestCount++;                                            // 14

    ret = pthread_detach(newPthread1);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ICUNIT_ASSERT_EQUAL(g_pthreadTestCount, 14, g_pthreadTestCount); // 14, here assert the result.

    return 0;
}

void ItTestPthread001(void)
{
    TEST_ADD_CASE("IT_POSIX_PTHREAD_001", PthreadTest001, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
