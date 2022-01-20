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

static pthread_mutex_t g_muxLock001;
static pthread_mutex_t g_muxLock002;
static pthread_mutex_t g_muxLock003;

static const unsigned int TEST_COUNT = 1;
static volatile int g_testToCount001 = 0;
static volatile int g_testToCount002 = 0;
static volatile int g_testToCount003 = 0;

static void *ThreadFuncTest3(void *a)
{
    int ret;
    pthread_t thread = pthread_self();
    int currThreadPri, currThreadPolicy;
    struct sched_param param = { 0 };
    struct timespec time;
    struct timeval timeVal = { 0 };

    ret = pthread_detach(thread);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    gettimeofday(&timeVal, nullptr);

    time.tv_sec = timeVal.tv_sec + 10; // 10, for test
    time.tv_nsec = timeVal.tv_usec + 0;

    ret = pthread_mutex_timedlock(&g_muxLock003, &time);
    ICUNIT_GOTO_EQUAL(ret, ETIMEDOUT, ret, EXIT);

    g_testToCount003++;

    while (g_testToCount002 == 0) {
        SLEEP_AND_YIELD(2); // 2, delay enouge time
    }

    ret = pthread_mutex_unlock(&g_muxLock003);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    return nullptr;

EXIT:
    g_testToCount003 = -1;
    return nullptr;
}

static void *ThreadFuncTest2(void *a)
{
    int ret;
    pthread_t thread = pthread_self();
    struct timespec time;
    struct timeval timeVal = { 0 };

    ret = pthread_detach(thread);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    gettimeofday(&timeVal, nullptr);

    time.tv_sec = timeVal.tv_sec + 5; // 5, for test
    time.tv_nsec = timeVal.tv_usec + 0;

    ret = pthread_mutex_timedlock(&g_muxLock002, &time);
    ICUNIT_GOTO_EQUAL(ret, ETIMEDOUT, ret, EXIT);

    g_testToCount002++;

    while (g_testToCount001 == 0) {
        SLEEP_AND_YIELD(2); // 2, delay enouge time
    }

    ret = pthread_mutex_unlock(&g_muxLock002);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    return nullptr;

EXIT:
    g_testToCount002 = -1;
    return nullptr;
}

static void *ThreadFuncTest1(void *a)
{
    int ret;
    pthread_t thread = pthread_self();
    struct timespec time;
    struct timeval timeVal = { 0 };

    ret = pthread_detach(thread);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    gettimeofday(&timeVal, nullptr);

    time.tv_sec = timeVal.tv_sec + 2; // 2, for test
    time.tv_nsec = timeVal.tv_usec + 0;

    ret = pthread_mutex_timedlock(&g_muxLock001, &time);
    ICUNIT_GOTO_EQUAL(ret, ETIMEDOUT, ret, EXIT);

    g_testToCount001++;

    ret = pthread_mutex_unlock(&g_muxLock001);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    return nullptr;

EXIT:
    g_testToCount001 = -1;
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
    pthread_mutexattr_t mutex = { 0 };
    int index = TEST_COUNT;

    while (index) {
        pthread_mutexattr_settype(&mutex, PTHREAD_MUTEX_NORMAL);
        pthread_mutex_init(&g_muxLock001, &mutex);
        pthread_mutex_init(&g_muxLock002, &mutex);
        pthread_mutex_init(&g_muxLock003, &mutex);

        g_testToCount001 = 0;
        g_testToCount002 = 0;
        g_testToCount003 = 0;

        ret = pthread_mutex_lock(&g_muxLock001);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);

        ret = pthread_mutex_lock(&g_muxLock002);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);

        ret = pthread_mutex_lock(&g_muxLock003);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);

        ret = pthread_create(&newPthread, nullptr, ThreadFuncTest1, 0);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);

        ret = pthread_create(&newPthread, nullptr, ThreadFuncTest2, 0);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);

        ret = pthread_create(&newPthread, nullptr, ThreadFuncTest3, 0);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);

        while (g_testToCount001 == 0 || g_testToCount002 == 0 || g_testToCount003 == 0) {
        }

        ICUNIT_ASSERT_NOT_EQUAL(g_testToCount001, -1, g_testToCount001);
        ICUNIT_ASSERT_NOT_EQUAL(g_testToCount002, -1, g_testToCount002);
        ICUNIT_ASSERT_NOT_EQUAL(g_testToCount003, -1, g_testToCount003);

        ret = pthread_mutex_unlock(&g_muxLock001);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);

        ret = pthread_mutex_unlock(&g_muxLock002);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);

        ret = pthread_mutex_unlock(&g_muxLock003);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);
        index--;

        SLEEP_AND_YIELD(2); // 2, delay enouge time
    }

    pthread_mutex_destroy(&g_muxLock001);
    pthread_mutex_destroy(&g_muxLock002);
    pthread_mutex_destroy(&g_muxLock003);
    return 0;
}

void ItTestPthreadMutex007(void)
{
    TEST_ADD_CASE("IT_POSIX_PTHREAD_MUTEX_007", Testcase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
