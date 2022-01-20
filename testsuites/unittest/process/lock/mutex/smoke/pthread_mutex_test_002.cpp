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
#include "it_mutex_test.h"

static pthread_mutex_t g_muxLock;

static const unsigned int TEST_COUNT = 5;
static int g_testInfo[5] = { 0 };
static int g_testToCount = 0;
static int g_testBackCount = 0;

static void *ThreadFuncTest3(void *a)
{
    int ret;
    int tid = Gettid();
    pthread_t thread = pthread_self();

    ret = pthread_detach(thread);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    g_testInfo[g_testToCount] = tid;
    g_testToCount++;

    ret = pthread_mutex_lock(&g_muxLock);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    g_testBackCount--;

    ret = pthread_mutex_unlock(&g_muxLock);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

EXIT:
    return nullptr;
}

static int Testcase(void)
{
    struct sched_param param = { 0 };
    int ret;
    void *res = nullptr;
    pthread_attr_t a = { 0 };
    pthread_t thread = pthread_self();
    pthread_t newPthread, newPthread1;
    pthread_mutexattr_t mutex;
    int index = 0;
    int currThreadPri, currThreadPolicy;

    pthread_mutexattr_settype(&mutex, PTHREAD_MUTEX_NORMAL);
    pthread_mutex_init(&g_muxLock, &mutex);

    param.sched_priority = 20; // 20, Set Priorities
    ret = pthread_setschedparam(pthread_self(), SCHED_RR, &param);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_getschedparam(pthread_self(), &currThreadPolicy, &param);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    currThreadPri = param.sched_priority;

    while (index < 2) { // 2
        g_testToCount = 0;
        g_testBackCount = TEST_COUNT - 1;
        ret = pthread_mutex_lock(&g_muxLock);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);

        for (int count = 0; count < TEST_COUNT; count++) {
            ret = pthread_attr_init(&a);
            param.sched_priority = currThreadPri - count - 1;
            if (param.sched_priority < 0) {
                param.sched_priority = 0;
                printf("sched_priority is zero !!!\n");
            }
            pthread_attr_setschedparam(&a, &param);
            pthread_attr_setschedpolicy(&a, currThreadPolicy);
            pthread_attr_setinheritsched(&a, PTHREAD_EXPLICIT_SCHED);
            ret = pthread_create(&newPthread, &a, ThreadFuncTest3, 0);
            ICUNIT_ASSERT_EQUAL(ret, 0, ret);
        }

        ret = pthread_mutex_unlock(&g_muxLock);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);

        SLEEP_AND_YIELD(1);

        ICUNIT_ASSERT_EQUAL(g_testBackCount, -1, g_testBackCount);
        ICUNIT_ASSERT_EQUAL(g_testToCount, TEST_COUNT, g_testToCount);

        index++;
    }

    pthread_mutex_destroy(&g_muxLock);
    return 0;
}

void ItTestPthreadMutex002(void)
{
    TEST_ADD_CASE("IT_POSIX_PTHREAD_MUTEX_002", Testcase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
