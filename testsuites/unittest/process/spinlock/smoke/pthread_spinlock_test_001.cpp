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
#include "it_spinlock_test.h"
#include "string.h"

static const int WRITE_THREAD_COUNT = 2; // 2, set test count.
static const int TEST_LOOP_COUNT = 1;
static const int TEST_COUNT = 2; // 2, set write thread count.
static pthread_spinlock_t g_spinlockLock;

static const int TEST_DATA_SIZE = 100000; // 100000, set test data size.
static int g_spinlockData[TEST_DATA_SIZE];
static int g_spinlockMask;
static volatile int g_isWriting[WRITE_THREAD_COUNT];
static volatile int g_isWriteExit[WRITE_THREAD_COUNT];
static int g_writePar[WRITE_THREAD_COUNT];

static void *ThreadWriteFunc1(void *a)
{
    int ret;
    int testCount = TEST_LOOP_COUNT;
    int oldRwlockMask;
    pthread_t thread = pthread_self();
    int threadCount = *((int *)a);
    int count;

    ret = pthread_detach(thread);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    while (testCount > 0) {
        ret = pthread_spin_lock(&g_spinlockLock);
        ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

        g_isWriting[threadCount] = 1;

        oldRwlockMask = g_spinlockMask;

        g_spinlockMask++;

        for (count = 0; count < TEST_DATA_SIZE; count++) {
            ICUNIT_GOTO_EQUAL(g_spinlockData[count], oldRwlockMask, g_spinlockData[count], EXIT);
            g_spinlockData[count] = g_spinlockMask;
        }

        g_isWriting[threadCount] = 0;

        ret = pthread_spin_unlock(&g_spinlockLock);
        ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

        SLEEP_AND_YIELD(1);
        testCount--;
    }

    g_isWriteExit[threadCount] = 6; // 6, The current possible value of the variable.

EXIT:
    return nullptr;
}

static void *ThreadWriteFunc(void *a)
{
    int ret;
    int testCount = TEST_LOOP_COUNT;
    int oldRwlockMask;
    pthread_t thread = pthread_self();
    int threadCount = *((int *)a);
    int count;

    while (testCount > 0) {
        ret = pthread_spin_lock(&g_spinlockLock);
        ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

        g_isWriting[threadCount] = 1;

        oldRwlockMask = g_spinlockMask;

        g_spinlockMask++;

        for (count = 0; count < TEST_DATA_SIZE; count++) {
            ICUNIT_GOTO_EQUAL(g_spinlockData[count], oldRwlockMask, g_spinlockData[count], EXIT);
            g_spinlockData[count] = g_spinlockMask;
        }

        g_isWriting[threadCount] = 0;

        ret = pthread_spin_unlock(&g_spinlockLock);
        ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

        SLEEP_AND_YIELD(1);
        testCount--;
    }

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
    pthread_rwlockattr_t rwlock;
    int index = 0;
    int curThreadPri, curThreadPolicy;

    (void)memset_s((void *)g_spinlockData, sizeof(int) * TEST_DATA_SIZE, 0, sizeof(int) * TEST_DATA_SIZE);
    g_spinlockMask = 0;

    pthread_spin_init(&g_spinlockLock, 0);

    ret = pthread_getschedparam(pthread_self(), &curThreadPolicy, &param);
    ICUNIT_ASSERT_EQUAL(ret, 0, -ret);

    curThreadPri = param.sched_priority;

    while (index < TEST_COUNT) {
        ret = pthread_attr_init(&a);
        pthread_attr_setinheritsched(&a, PTHREAD_EXPLICIT_SCHED);
        param.sched_priority = curThreadPri + 1;
        pthread_attr_setschedparam(&a, &param);

        g_writePar[0] = 0;
        ret = pthread_create(&newPthread1, &a, ThreadWriteFunc, &g_writePar[0]);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);

        g_writePar[1] = 1;
        ret = pthread_create(&newPthread, &a, ThreadWriteFunc1, &g_writePar[1]);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);
        g_isWriting[1] = 0;
        g_isWriteExit[1] = 0;

        ret = pthread_join(newPthread1, &res);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);
        index++;
    }

    pthread_spin_destroy(&g_spinlockLock);

    return 0;
}

void ItTestPthreadSpinlock001(void)
{
    TEST_ADD_CASE("IT_POSIX_PTHREAD_SPINLOCK_001", Testcase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
