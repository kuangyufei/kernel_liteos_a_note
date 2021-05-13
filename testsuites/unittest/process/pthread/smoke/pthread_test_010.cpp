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
static int g_threadTest[30];

static void *ThreadFuncTest0(void *a)
{
    struct sched_param param = { 0 };
    int ret;
    int currThreadPolicy;
    ret = pthread_getschedparam(pthread_self(), &currThreadPolicy, &param);
    ICUNIT_ASSERT_EQUAL_NULL(ret, 0, (void *)(uintptr_t)ret);
    int currThreadPri = param.sched_priority;

    g_testToCount001++;

EXIT:
    return NULL;
}

static int Testcase(void)
{
    struct sched_param param = { 0 };
    int ret;
    pthread_attr_t a = { 0 };
    pthread_t newPthread[30];
    int index = 0;
    int currThreadPri, currThreadPolicy;
    const int testCount = 30;

    ret = pthread_getschedparam(pthread_self(), &currThreadPolicy, &param);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    currThreadPri = param.sched_priority;
    g_testToCount001 = 0;
    ret = pthread_attr_init(&a);
    pthread_attr_setdetachstate(&a, PTHREAD_CREATE_DETACHED);
    pthread_attr_setinheritsched(&a, PTHREAD_EXPLICIT_SCHED);
    param.sched_priority = currThreadPri - 2; // 2, adjust the priority.
    pthread_attr_setschedparam(&a, &param);

    while (index < testCount) {
        ret = pthread_create(&newPthread[index], &a, ThreadFuncTest0, NULL);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);
        index++;
    }

    ICUNIT_ASSERT_EQUAL(g_testToCount001, testCount, g_testToCount001);

    return 0;
}

void ItTestPthread010(void)
{
    TEST_ADD_CASE("IT_POSIX_PTHREAD_010", Testcase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
