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

/* ***************************************************************************
 * Public Functions
 * ************************************************************************** */

static int g_currThreadPri, g_currThreadPolicy;
static int g_testPthredCount;
static void *ThreadFuncTest2(void *arg)
{
    (void)arg;
    int ret;
    int policy;
    struct sched_param param = { 0 };
    pthread_t pthread = pthread_self();

    g_testPthredCount++;

    ret = pthread_getschedparam(pthread, &policy, &param);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ICUNIT_GOTO_EQUAL(g_currThreadPri, param.sched_priority, param.sched_priority, EXIT);
    ICUNIT_GOTO_EQUAL(g_currThreadPolicy, policy, policy, EXIT);
    ICUNIT_GOTO_EQUAL(g_testPthredCount, 2, g_testPthredCount, EXIT); // 2, here assert the result.
EXIT:
    return NULL;
}

static void *ThreadFuncTest3(void *arg)
{
    (void)arg;
    int ret;
    int policy;
    struct sched_param param = { 0 };
    pthread_t pthread = pthread_self();

    g_testPthredCount++;

    ret = pthread_getschedparam(pthread, &policy, &param);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ICUNIT_GOTO_EQUAL(g_currThreadPri, param.sched_priority, param.sched_priority, EXIT);
    ICUNIT_GOTO_EQUAL(policy, SCHED_FIFO, policy, EXIT);
    ICUNIT_GOTO_EQUAL(g_testPthredCount, 4, g_testPthredCount, EXIT); // 4, here assert the result.

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
    ret = pthread_create(&newPthread, NULL, ThreadFuncTest2, 0);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_join(newPthread, &res);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ICUNIT_ASSERT_EQUAL(g_testPthredCount, 2, g_testPthredCount); // 2, here assert the result.
    g_testPthredCount++;

    param.sched_priority = g_currThreadPri;
    ret = pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_create(&newPthread, NULL, ThreadFuncTest3, 0);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_join(newPthread, &res);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ICUNIT_ASSERT_EQUAL(g_testPthredCount, 4, g_testPthredCount); // 4, here assert the result.

    param.sched_priority = g_currThreadPri;
    ret = pthread_setschedparam(pthread_self(), SCHED_RR, &param);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    return 0;
}

void ItTestPthread003(void)
{
    TEST_ADD_CASE("IT_POSIX_PTHREAD_003", Testcase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
