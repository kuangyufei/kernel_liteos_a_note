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
#include "it_rwlock_test.h"
#include "string.h"

static const int WRITE_THREAD_COUNT = 5; // 5, set w thread count.
static const int READ_THREAD_COUNT = 5; // 5, set read thread count.

static const int READ_LOOP_COUNT = 10; // 10, set read loop count.
static pthread_rwlock_t g_rwlockLock;
static const int TEST_DATA_SIZE = 100000; // 100000, set test data size.
static int g_rwlockData[TEST_DATA_SIZE];
static int g_rwlockMask;
static volatile int g_isWriting[WRITE_THREAD_COUNT];
static volatile int g_isReading[READ_THREAD_COUNT];
static volatile int g_isReadExit[READ_THREAD_COUNT];
static volatile int g_isWriteExit[WRITE_THREAD_COUNT];
static int g_writePar[WRITE_THREAD_COUNT];
static int g_readPar[READ_THREAD_COUNT];

static void RwlockWait()
{
    int count;
    int count1;
    for (count = 0xFFFFFFF; count != 0; count--) {
    }
}

static int CheckReadThreadExit(void)
{
    int count = 0;

    for (int i = 0; i < READ_THREAD_COUNT; i++) {
        if (g_isReadExit[i] == 5) { // 5, set read exit data.
            count++;
        }
    }

    if (count == READ_THREAD_COUNT) {
        return 0;
    }

    return 1;
}

static int CheckWriteThreadExit(void)
{
    int count = 0;

    for (int i = 1; i < WRITE_THREAD_COUNT; i++) {
        if (g_isWriteExit[i] == 6) { // 6, The current possible value of the variable.
            count++;
        }
    }

    if (count == (WRITE_THREAD_COUNT - 1)) {
        return 0;
    }

    return 1;
}

static void *ThreadReadFunc(void *a)
{
    int ret;
    int count = 0;
    pthread_t thread = pthread_self();
    int loop = READ_LOOP_COUNT;
    int threadCount = *((int *)a);

    ret = pthread_detach(thread);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    while (loop > 0) {
        SLEEP_AND_YIELD(1);
        ret = pthread_rwlock_rdlock(&g_rwlockLock);
        ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);
        g_isReading[threadCount] = 1;

        for (count = 0; count < TEST_DATA_SIZE; count++) {
            ICUNIT_GOTO_EQUAL(g_rwlockData[count], g_rwlockMask, g_rwlockData[count], EXIT);
            for (int index = 0; index < WRITE_THREAD_COUNT; index++) {
                ICUNIT_GOTO_EQUAL(g_isWriting[index], 0, g_isWriting[index], EXIT);
            }
        }

        SLEEP_AND_YIELD(1);
        g_isReading[threadCount] = 0;

        ret = pthread_rwlock_unlock(&g_rwlockLock);
        ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

        RwlockWait();
        loop--;
    }

    g_isReadExit[threadCount] = 5; // 5, set read exit data.

EXIT:
    return nullptr;
}

static void *ThreadWriteFunc1(void *a)
{
    int ret;
    int count = 0;
    int oldRwlockMask;
    pthread_t thread = pthread_self();
    int threadCount = *((int *)a);

    ret = pthread_detach(thread);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    while (CheckReadThreadExit()) {
        SLEEP_AND_YIELD(1);
        ret = pthread_rwlock_wrlock(&g_rwlockLock);
        ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

        g_isWriting[threadCount] = 1;

        oldRwlockMask = g_rwlockMask;

        g_rwlockMask++;

        for (count = 0; count < TEST_DATA_SIZE; count++) {
            ICUNIT_GOTO_EQUAL(g_rwlockData[count], oldRwlockMask, g_rwlockData[count], EXIT);
            for (int i = 0; i < READ_THREAD_COUNT; i++) {
                ICUNIT_GOTO_EQUAL(g_isReading[i], 0, g_isReading[i], EXIT);
            }
            g_rwlockData[count] = g_rwlockMask;
        }

        SLEEP_AND_YIELD(1);

        g_isWriting[threadCount] = 0;

        ret = pthread_rwlock_unlock(&g_rwlockLock);
        ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);
        SLEEP_AND_YIELD(1);
    }

    g_isWriteExit[threadCount] = 6; // 6, The current possible value of the variable.

EXIT:
    return nullptr;
}

static void *ThreadWriteFunc(void *a)
{
    int ret;
    int count = 0;
    int oldRwlockMask;
    pthread_t thread = pthread_self();
    int threadCount = *((int *)a);

    while (CheckReadThreadExit() || CheckWriteThreadExit()) {
        SLEEP_AND_YIELD(1);
        ret = pthread_rwlock_wrlock(&g_rwlockLock);
        ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

        g_isWriting[threadCount] = 1;

        oldRwlockMask = g_rwlockMask;

        g_rwlockMask++;

        for (count = 0; count < TEST_DATA_SIZE; count++) {
            ICUNIT_GOTO_EQUAL(g_rwlockData[count], oldRwlockMask, g_rwlockData[count], EXIT);
            for (int i = 0; i < READ_THREAD_COUNT; i++) {
                ICUNIT_GOTO_EQUAL(g_isReading[i], 0, g_isReading[i], EXIT);
            }
            g_rwlockData[count] = g_rwlockMask;
        }

        SLEEP_AND_YIELD(1);

        g_isWriting[threadCount] = 0;

        ret = pthread_rwlock_unlock(&g_rwlockLock);
        ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

        SLEEP_AND_YIELD(1);
    }

EXIT:
    return nullptr;
}

static int PthreadRwlockTest(void)
{
    struct sched_param param = { 0 };
    int ret;
    void *res = nullptr;
    int count = 0;
    pthread_attr_t a = { 0 };
    pthread_t thread = pthread_self();
    pthread_t newPthread, newPthread1;
    pthread_rwlockattr_t rwlock;
    int index = 0;
    int curThreadPri, curThreadPolicy;

    (void)memset_s((void *)g_rwlockData, sizeof(int) * TEST_DATA_SIZE, 0, sizeof(int) * TEST_DATA_SIZE);
    g_rwlockMask = 0;

    pthread_rwlock_init(&g_rwlockLock, NULL);

    ret = pthread_getschedparam(pthread_self(), &curThreadPolicy, &param);
    ICUNIT_ASSERT_EQUAL(ret, 0, -ret);

    curThreadPri = param.sched_priority;

    while (index < 2) { // 2, Number of cycles.
        ret = pthread_attr_init(&a);
        pthread_attr_setinheritsched(&a, PTHREAD_EXPLICIT_SCHED);
        param.sched_priority = curThreadPri + 1;
        pthread_attr_setschedparam(&a, &param);

        g_writePar[0] = 0;
        ret = pthread_create(&newPthread1, &a, ThreadWriteFunc, &g_writePar[0]);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);

        count = 1;
        while (count < WRITE_THREAD_COUNT) {
            g_writePar[count] = count;
            g_isWriting[count] = 0;
            g_isWriteExit[count] = 0;
            ret = pthread_create(&newPthread, &a, ThreadWriteFunc1, &g_writePar[count]);
            ICUNIT_ASSERT_EQUAL(ret, 0, ret);
            count++;
        }

        count = 0;
        while (count < READ_THREAD_COUNT) {
            g_readPar[count] = count;
            g_isReading[count] = 0;
            g_isReadExit[count] = 0;
            ret = pthread_create(&newPthread, &a, ThreadReadFunc, &g_readPar[count]);
            ICUNIT_ASSERT_EQUAL(ret, 0, ret);
            count++;
        }

        ret = pthread_join(newPthread1, &res);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);
        index++;
    }

    pthread_rwlock_destroy(&g_rwlockLock);
    return 0;
}

static int PthreadRwlockTest1(void)
{
    PthreadRwlockTest();
    exit(255); // 255, exit args.
}

static int Testcase(void)
{
    int ret;
    int status = 0;
    pid_t pid = fork();
    ICUNIT_ASSERT_WITHIN_EQUAL(pid, 0, 100000, pid); // 100000, assert that function Result is equal to this.
    if (pid == 0) {
        PthreadRwlockTest1();
        exit(__LINE__);
    }

    PthreadRwlockTest();

    ret = wait(&status);
    status = WEXITSTATUS(status);
    ICUNIT_ASSERT_EQUAL(ret, pid, ret);
    ICUNIT_ASSERT_EQUAL(status, 255, status); // 255, assert that function Result is equal to this.
    return 0;
}

void ItTestPthreadRwlock002(void)
{
    TEST_ADD_CASE("IT_POSIX_PTHREAD_RWLOCK_002", Testcase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
