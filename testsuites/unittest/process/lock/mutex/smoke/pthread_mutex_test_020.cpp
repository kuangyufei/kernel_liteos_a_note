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

static const unsigned int TEST_COUNT = 10;
static const unsigned int NEW_THREAD_COUNT = 10;

static volatile int g_testToCount001 = 0;
static volatile int g_testToCount002 = 0;
static volatile int g_testToCount000 = 0;

static void *ThreadFuncTest2(void *a)
{
    int ret;
    pthread_t thread = pthread_self();
    struct timespec time;
    struct timeval timeVal = { 0 };

    ret = pthread_detach(thread);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = pthread_mutex_lock(&g_muxLock001);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = pthread_mutex_unlock(&g_muxLock001);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    g_testToCount002 = 1;

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

    ret = pthread_mutex_lock(&g_muxLock001);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    while (g_testToCount002 != 1) {
        if (g_testToCount002 == -1) {
            return nullptr;
        }
    }

    ret = pthread_mutex_unlock(&g_muxLock001);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    g_testToCount001++;
    return nullptr;

EXIT:
    g_testToCount001 = -1;
    return nullptr;
}

static void *ThreadFuncTest0(void *a)
{
    int ret;
    pthread_t thread = pthread_self();
    struct timespec time;
    struct timeval timeVal = { 0 };

    gettimeofday(&timeVal, nullptr);

    time.tv_sec = timeVal.tv_sec + 0;
    time.tv_nsec = (timeVal.tv_usec + 1000 * 10 * 10) * 1000; // 1000, 10 ms to ns

    ret = pthread_mutex_timedlock(&g_muxLock001, &time);

    while (g_testToCount002 != 1) {
        if (g_testToCount002 == -1) {
            return nullptr;
        }
    }

    if (ret == 0) {
        ret = pthread_mutex_unlock(&g_muxLock001);
        ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);
    }

    g_testToCount000++;
    return nullptr;

EXIT:
    g_testToCount000 = -1;
    return nullptr;
}

/* futexlist:
 * 21(20) -> -1(-1) -> 28(22) -> 27(22) -> 26(22) -> 25(22) -> 24(22) -> 23(22) ->
 * -1(-1) -> 28(22) -> 27(22) -> 26(22) -> 25(22) -> 24(22) -> 23(22) ->
 * 27(22) -> 26(22) -> 25(22) -> 24(22) -> 23(22) ->
 */
static int Testcase(void)
{
    struct sched_param param = { 0 };
    int ret;
    int threadCount;
    void *res = nullptr;
    pthread_attr_t a = { 0 };
    pthread_t thread = pthread_self();
    pthread_t newPthread[10], newPthread1;
    pthread_mutexattr_t mutex;
    int index = TEST_COUNT;
    int gCurrThreadPri, gCurrThreadPolicy;
    struct timeval time = { 0 };
    struct timeval timeVal = { 0 };

    ret = pthread_getschedparam(pthread_self(), &gCurrThreadPolicy, &param);
    ICUNIT_ASSERT_EQUAL(ret, 0, -ret);

    gCurrThreadPri = param.sched_priority;

    ret = pthread_attr_init(&a);
    // 2, Set the priority according to the task purpose,a smaller number means a higher priority.
    param.sched_priority = gCurrThreadPri + 2;
    pthread_attr_setinheritsched(&a, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedparam(&a, &param);

    while (index) {
        pthread_mutexattr_settype(&mutex, PTHREAD_MUTEX_NORMAL);
        pthread_mutex_init(&g_muxLock001, &mutex);

        g_testToCount001 = 0;
        g_testToCount002 = 0;
        g_testToCount000 = 0;
        threadCount = 0;

        ret = pthread_mutex_lock(&g_muxLock001);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);

        ret = pthread_create(&newPthread[threadCount], &a, ThreadFuncTest0, 0);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);

        threadCount = 1;
        ret = pthread_create(&newPthread[threadCount], &a, ThreadFuncTest0, 0);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);

        threadCount = 2; // 2
        while (threadCount < NEW_THREAD_COUNT) {
            ret = pthread_create(&newPthread[threadCount], &a, ThreadFuncTest1, 0);
            ICUNIT_ASSERT_EQUAL(ret, 0, ret);
            threadCount++;
        }

        usleep(1000 * 10 * 8); // 1000, 10, 8

        ret = pthread_create(&newPthread1, nullptr, ThreadFuncTest2, 0);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);

        gettimeofday(&timeVal, nullptr);
        while (1) {
            gettimeofday(&time, nullptr);
            if ((time.tv_sec - timeVal.tv_sec) >= 1) {
                break;
            }
        }

        ret = pthread_mutex_unlock(&g_muxLock001);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);

        threadCount = 0;
        while (threadCount < NEW_THREAD_COUNT) {
            ret = pthread_join(newPthread[threadCount], nullptr);
            ICUNIT_ASSERT_EQUAL(ret, 0, ret);
            threadCount++;
        }

        ICUNIT_ASSERT_NOT_EQUAL(g_testToCount001, -1, g_testToCount001);
        ICUNIT_ASSERT_NOT_EQUAL(g_testToCount002, -1, g_testToCount002);
        ICUNIT_ASSERT_EQUAL(g_testToCount001, NEW_THREAD_COUNT - 2, g_testToCount001); // 2, test value

        pthread_mutex_destroy(&g_muxLock003);
        index--;
    }
    return 0;
}

void ItTestPthreadMutex020(void)
{
    TEST_ADD_CASE("IT_POSIX_PTHREAD_MUTEX_020", Testcase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
