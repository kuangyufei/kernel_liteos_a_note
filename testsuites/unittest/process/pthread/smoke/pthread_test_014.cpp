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
#include "threads.h"
static const int TEST_LOOP = 5000;

static int g_testCnt = 0;
static void *ThreadFunc(void *arg)
{
    int count = 1;
    while (g_testCnt < TEST_LOOP) {
        if (g_testCnt != count) {
            printf("%s %d g_testCnt should : %d but is : %d\n", __FUNCTION__, __LINE__, count, g_testCnt);
            return NULL;
        }
        g_testCnt++;
        count += 5; // total 5 pthreads.
        thrd_yield();
    }

    g_testCnt = TEST_LOOP + 1000; // 1000, here set a special num.
    return NULL;
}

static void *ThreadFunc1(void *arg)
{
    int count = 2;
    while (g_testCnt < TEST_LOOP) {
        if (g_testCnt != count) {
            printf("%s %d g_testCnt should : %d but is : %d\n", __FUNCTION__, __LINE__, count, g_testCnt);
            return NULL;
        }
        g_testCnt++;
        count += 5; // total 5 pthreads.
        thrd_yield();
    }

    return NULL;
}

static void *ThreadFunc2(void *arg)
{
    int count = 3;
    while (g_testCnt < TEST_LOOP) {
        if (g_testCnt != count) {
            printf("%s %d g_testCnt should : %d but is : %d\n", __FUNCTION__, __LINE__, count, g_testCnt);
            return NULL;
        }
        g_testCnt++;
        count += 5; // total 5 pthreads.
        thrd_yield();
    }

    return NULL;
}

static void *ThreadFunc3(void *arg)
{
    int count = 4;
    while (g_testCnt < TEST_LOOP) {
        if (g_testCnt != count) {
            printf("%s %d g_testCnt should : %d but is : %d\n", __FUNCTION__, __LINE__, count, g_testCnt);
            return NULL;
        }
        g_testCnt++;
        count += 5; // total 5 pthreads.
        thrd_yield();
    }

    return NULL;
}

static int TestCase(void)
{
    int ret;
    pthread_t tid, tid1, tid2, tid3;
    pthread_attr_t attr = { 0 };
    int scope = 0;
    int threadPolicy = 0;
    struct sched_param param = { 0 };

    g_testCnt = 0;

    ret = pthread_getschedparam(pthread_self(), &threadPolicy, &param);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = pthread_create(&tid, NULL, ThreadFunc, 0);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_create(&tid1, NULL, ThreadFunc1, 0);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_create(&tid2, NULL, ThreadFunc2, 0);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_create(&tid3, NULL, ThreadFunc3, 0);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    while (g_testCnt < TEST_LOOP) {
        g_testCnt++;
        thrd_yield();
    }

    param.sched_priority -= 2; // 2, adjust the priority.
    ret = pthread_setschedparam(tid, SCHED_FIFO, &param);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ICUNIT_GOTO_EQUAL(g_testCnt, TEST_LOOP + 1000, g_testCnt, EXIT); // 1000, here assert the special num.

    ret = pthread_join(tid, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = pthread_join(tid1, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = pthread_join(tid2, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = pthread_join(tid3, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    param.sched_priority += 2; // 2, adjust the priority.
    ret = pthread_setschedparam(pthread_self(), SCHED_RR, &param);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

EXIT:
    return 0;
}

void ItTestPthread014(void)
{
    TEST_ADD_CASE("IT_POSIX_PTHREAD_014", TestCase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
