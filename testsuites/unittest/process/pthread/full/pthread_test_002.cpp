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
static volatile int g_testCnt1 = 0;
static volatile int g_testCnt2 = 0;
static volatile int g_testPthredCount = 0;

static void ThreadWaitCount(int scount, volatile int *ent)
{
    int count = 0xf0000;
    while (*ent < scount) {
        while (count > 0) {
            count--;
        }
        count = 0xf0000;
        (*ent)++;
    }
}

static void *ThreadFuncTest2(void *arg)
{
    (void)arg;
    int ret;
    struct sched_param param = { 0 };
    int threadPolicy, threadPri;
    int old;
    const int waitCount1 = 20;
    const int waitCount2 = 50;
    g_testCnt2 = 0;

    ThreadWaitCount(waitCount1 + g_testCnt2, &g_testCnt2);

    while (g_testCnt1 < 10) { // 10, wait until g_testCnt1 >= 10.
        printf("\r");
    }

    g_testPthredCount++;

    param.sched_priority = g_currThreadPri + 1;
    ret = pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    old = g_testCnt1;
    param.sched_priority = 0;
    ret = pthread_getschedparam(pthread_self(), &threadPolicy, &param);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ICUNIT_GOTO_EQUAL(threadPolicy, SCHED_FIFO, threadPolicy, EXIT);
    ICUNIT_GOTO_EQUAL(param.sched_priority, (g_currThreadPri + 1), param.sched_priority, EXIT);

    ThreadWaitCount(waitCount2 + g_testCnt2, &g_testCnt2);

    ICUNIT_GOTO_EQUAL(g_testCnt1, old, g_testCnt1, EXIT);

    param.sched_priority = g_currThreadPri;
    ret = pthread_setschedparam(pthread_self(), SCHED_RR, &param);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    old = g_testCnt1;
    param.sched_priority = 0;
    ret = pthread_getschedparam(pthread_self(), &threadPolicy, &param);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ICUNIT_GOTO_EQUAL(threadPolicy, SCHED_RR, threadPolicy, EXIT);
    ICUNIT_GOTO_EQUAL(param.sched_priority, g_currThreadPri, param.sched_priority, EXIT);

    ThreadWaitCount(waitCount2 + g_testCnt2, &g_testCnt2);

    ICUNIT_GOTO_EQUAL(g_testCnt1, old, g_testCnt1, EXIT);

    ret = pthread_setschedprio(pthread_self(), g_currThreadPri + 1);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ThreadWaitCount(waitCount2 + g_testCnt2, &g_testCnt2);
    ICUNIT_GOTO_EQUAL(g_testCnt2, 170, g_testCnt2, EXIT); // 170, here assert the result.
    g_testPthredCount++;

EXIT:
    return NULL;
}

static void *ThreadFuncTest3(void *arg)
{
    (void)arg;
    struct sched_param param = { 0 };
    int threadPolicy, threadPri;
    int ret;
    int old = 0;
    const int waitCount1 = 20;
    const int waitCount2 = 50;

    g_testCnt1 = 0;

    ThreadWaitCount(waitCount1 + g_testCnt1, &g_testCnt1);

    while (g_testCnt2 < 10) { // 10, wait until g_testCnt2 >= 10.
        printf("\r");
    }

    g_testPthredCount++;
    param.sched_priority = g_currThreadPri + 1;
    ret = pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    old = g_testCnt2;
    param.sched_priority = 0;
    ret = pthread_getschedparam(pthread_self(), &threadPolicy, &param);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ICUNIT_GOTO_EQUAL(threadPolicy, SCHED_FIFO, threadPolicy, EXIT);
    ICUNIT_GOTO_EQUAL(param.sched_priority, (g_currThreadPri + 1), param.sched_priority, EXIT);

    ThreadWaitCount(waitCount2 + g_testCnt1, &g_testCnt1);

    ICUNIT_GOTO_EQUAL(g_testCnt2, old, g_testCnt2, EXIT);

    param.sched_priority = g_currThreadPri;
    ret = pthread_setschedparam(pthread_self(), SCHED_RR, &param);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    old = g_testCnt2;
    param.sched_priority = 0;
    ret = pthread_getschedparam(pthread_self(), &threadPolicy, &param);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ICUNIT_GOTO_EQUAL(threadPolicy, SCHED_RR, threadPolicy, EXIT);
    ICUNIT_GOTO_EQUAL(param.sched_priority, g_currThreadPri, param.sched_priority, EXIT);

    ThreadWaitCount(waitCount2 + g_testCnt1, &g_testCnt1);

    ICUNIT_GOTO_EQUAL(g_testCnt2, old, g_testCnt2, EXIT);
    ICUNIT_GOTO_EQUAL(g_testCnt1, 120, g_testCnt1, EXIT); // 120, here assert the result.
    g_testPthredCount++;

EXIT:
    return NULL;
}

static int Testcase()
{
    struct sched_param param = { 0 };
    int ret;
    void *res = NULL;
    pthread_attr_t a = { 0 };
    pthread_t newPthread, newPthread1;

    g_testPthredCount = 0;

    ret = pthread_getschedparam(pthread_self(), &g_currThreadPolicy, &param);
    ICUNIT_ASSERT_EQUAL(ret, 0, -ret);

    g_currThreadPri = param.sched_priority;

    g_testPthredCount++;

    ret = pthread_attr_init(&a);
    param.sched_priority = g_currThreadPri + 1;
    pthread_attr_setinheritsched(&a, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedparam(&a, &param);
    ret = pthread_create(&newPthread, &a, ThreadFuncTest2, 0);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ICUNIT_ASSERT_EQUAL(g_testPthredCount, 1, g_testPthredCount);
    g_testPthredCount++; // 2

    ret = pthread_create(&newPthread1, &a, ThreadFuncTest3, 0);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ICUNIT_ASSERT_EQUAL(g_testPthredCount, 2, g_testPthredCount); // 2, here assert the result.
    g_testPthredCount++;                                          // 3

    ret = pthread_join(newPthread, &res);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_join(newPthread1, &res);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ICUNIT_ASSERT_EQUAL(g_testPthredCount, 7, g_testPthredCount); // 7, here assert the result.

    return 0;
}

void ItTestPthread002(void)
{
    TEST_ADD_CASE("IT_POSIX_PTHREAD_002", Testcase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
