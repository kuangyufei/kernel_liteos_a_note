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
static volatile int g_threadTestCount1 = 0;
static pthread_spinlock_t g_spinTestLock;

static void *ThreadFunc8(void *arg)
{
    pthread_t pthread = pthread_self();
    int ret;

    ret = pthread_detach(pthread);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = pthread_spin_lock(&g_spinTestLock);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);
    g_threadTestCount1++;
    ret = pthread_spin_unlock(&g_spinTestLock);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

EXIT:
    return NULL;
}

static void *ThreadFunc7(void *arg)
{
    pthread_t pthread = pthread_self();
    int ret;
    pthread_attr_t a = { 0 };
    struct sched_param param = { 0 };
    pthread_t newPthread;
    int curThreadPri, curThreadPolicy;

    ret = pthread_detach(pthread);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = pthread_getschedparam(pthread_self(), &curThreadPolicy, &param);
    ICUNIT_GOTO_EQUAL(ret, 0, -ret, EXIT);

    curThreadPri = param.sched_priority;

    ret = pthread_attr_init(&a);
    pthread_attr_setinheritsched(&a, PTHREAD_EXPLICIT_SCHED);
    param.sched_priority = curThreadPri;
    pthread_attr_setschedparam(&a, &param);
    ret = pthread_create(&newPthread, &a, ThreadFunc8, 0);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = pthread_spin_lock(&g_spinTestLock);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);
    g_threadTestCount1++;
    ret = pthread_spin_unlock(&g_spinTestLock);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    sleep(2); // 2, delay
EXIT:
    return NULL;
}

static void *ThreadFunc5(void *arg)
{
    pthread_attr_t a = { 0 };
    struct sched_param param = { 0 };
    int ret;
    void *res = NULL;
    pthread_t newPthread;
    int curThreadPri, curThreadPolicy;

    ret = pthread_detach(pthread_self());
    ICUNIT_GOTO_EQUAL(ret, EINVAL, ret, EXIT);

    ret = pthread_getschedparam(pthread_self(), &curThreadPolicy, &param);
    ICUNIT_GOTO_EQUAL(ret, 0, -ret, EXIT);

    curThreadPri = param.sched_priority;

    ret = pthread_attr_init(&a);
    pthread_attr_setinheritsched(&a, PTHREAD_EXPLICIT_SCHED);
    param.sched_priority = curThreadPri + 1;
    pthread_attr_setschedparam(&a, &param);
    ret = pthread_create(&newPthread, &a, ThreadFunc7, 0);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = pthread_join(newPthread, &res);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = pthread_spin_lock(&g_spinTestLock);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);
    g_threadTestCount1++;
    ret = pthread_spin_unlock(&g_spinTestLock);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

EXIT:
    return NULL;
}

static void *ThreadFunc6(void *arg)
{
    pthread_t pthread = pthread_self();

    ICUNIT_GOTO_EQUAL(g_pthreadTestCount, 12, g_pthreadTestCount, EXIT); // 12, here assert the result.
    g_pthreadTestCount++;                                                // 13

    return (void *)(uintptr_t)pthread;

EXIT:
    return NULL;
}

static void *ThreadFunc4(void *arg)
{
    pthread_t pthread = pthread_self();
    int i = 0;
    unsigned int ret;

    ret = pthread_spin_lock(&g_spinTestLock);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);
    ICUNIT_GOTO_EQUAL(g_pthreadTestCount, 9, g_pthreadTestCount, EXIT); // 9, here assert the result.
    g_pthreadTestCount++;                                               // 10
    ret = pthread_spin_unlock(&g_spinTestLock);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

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

    ret = pthread_spin_lock(&g_spinTestLock);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);
    ICUNIT_GOTO_EQUAL(g_pthreadTestCount, 6, g_pthreadTestCount, EXIT); // 6, here assert the result.
    g_pthreadTestCount++;                                               // 7
    ret = pthread_spin_unlock(&g_spinTestLock);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

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

static void *threadFunc2(void *arg)
{
    unsigned int ret;

    ret = pthread_spin_lock(&g_spinTestLock);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);
    ICUNIT_GOTO_EQUAL(g_pthreadTestCount, 3, g_pthreadTestCount, EXIT); // 3, here assert the result.
    g_pthreadTestCount++;
    ret = pthread_spin_unlock(&g_spinTestLock);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = pthread_join(pthread_self(), 0);
    ICUNIT_GOTO_EQUAL(ret, EINVAL, ret, EXIT);

    ret = pthread_detach(pthread_self());
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

EXIT:
    return NULL;
}

static void *threadFunc(void *arg)
{
    unsigned int ret;
    void *res = NULL;
    pthread_attr_t a = { 0 };
    pthread_t newPthread;
    struct sched_param param = { 0 };

    ret = pthread_spin_lock(&g_spinTestLock);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);
    ICUNIT_GOTO_EQUAL(g_pthreadTestCount, 2, g_pthreadTestCount, EXIT); // 2, here assert the result.
    g_pthreadTestCount++;
    ret = pthread_spin_unlock(&g_spinTestLock);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = pthread_attr_init(&a);
    pthread_attr_setinheritsched(&a, PTHREAD_EXPLICIT_SCHED);
    param.sched_priority = g_currThreadPri;
    pthread_attr_setschedparam(&a, &param);
    ret = pthread_create(&newPthread, &a, threadFunc2, 0);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = pthread_spin_lock(&g_spinTestLock);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);
    ICUNIT_GOTO_EQUAL(g_pthreadTestCount, 4, g_pthreadTestCount, EXIT); // 4, here assert the result.
    g_pthreadTestCount++;                                               // 5
    ret = pthread_spin_unlock(&g_spinTestLock);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = pthread_attr_init(&a);
    pthread_attr_setinheritsched(&a, PTHREAD_EXPLICIT_SCHED);
    param.sched_priority = g_currThreadPri + 2; // 2, adjust priority.
    pthread_attr_setschedparam(&a, &param);
    ret = pthread_create(&newPthread, &a, ThreadFunc3, 0);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = pthread_spin_lock(&g_spinTestLock);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);
    ICUNIT_GOTO_EQUAL(g_pthreadTestCount, 5, g_pthreadTestCount, EXIT); // 5, here assert the result.
    g_pthreadTestCount++;                                               // 6
    ret = pthread_spin_unlock(&g_spinTestLock);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = pthread_join(newPthread, &res);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = pthread_spin_lock(&g_spinTestLock);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);
    ICUNIT_GOTO_EQUAL(g_pthreadTestCount, 7, g_pthreadTestCount, EXIT); // 7, here assert the result.
    g_pthreadTestCount++;                                               // 8
    ret = pthread_spin_unlock(&g_spinTestLock);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = pthread_attr_init(&a);
    pthread_attr_setinheritsched(&a, PTHREAD_EXPLICIT_SCHED);
    param.sched_priority = g_currThreadPri + 3; // 3, adjust priority.
    pthread_attr_setschedparam(&a, &param);
    ret = pthread_create(&newPthread, &a, ThreadFunc4, 0);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = pthread_spin_lock(&g_spinTestLock);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);
    ICUNIT_GOTO_EQUAL(g_pthreadTestCount, 8, g_pthreadTestCount, EXIT); // 8, here assert the result.
    g_pthreadTestCount++;                                               // 9
    ret = pthread_spin_unlock(&g_spinTestLock);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = pthread_join(newPthread, &res);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = pthread_spin_lock(&g_spinTestLock);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);
    ICUNIT_GOTO_EQUAL(g_pthreadTestCount, 10, g_pthreadTestCount, EXIT); // 10, here assert the result.
    g_pthreadTestCount++;                                                // 11
    ret = pthread_spin_unlock(&g_spinTestLock);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

EXIT:
    return NULL;
}

int PthreadTest004()
{
    int exitCount = 0;
    struct sched_param param = { 0 };
    int ret;
    void *res = NULL;
    pthread_attr_t a = { 0 };
    pthread_t newPthread, newPthread1;
    const int testCount = 5;
    int count = 5;
    pthread_spin_init(&g_spinTestLock, 0);

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
    ret = pthread_create(&newPthread, &a, threadFunc, 0);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ICUNIT_ASSERT_EQUAL(g_pthreadTestCount, 1, g_pthreadTestCount);
    g_pthreadTestCount++;

    while (count > 0) {
        ret = pthread_create(&newPthread1, &a, ThreadFunc5, 0);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);

        ret = pthread_detach(newPthread1);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);
        count--;
    }

    g_threadTestCount1++;
    ICUNIT_ASSERT_EQUAL(g_threadTestCount1, 1, g_threadTestCount1);

    ret = pthread_join(newPthread, &res);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    while (g_threadTestCount1 != (1 + (testCount * 3))) { // 3
        sleep(2); // 2, delay
        printf("Wait for the concurrent thread to end \n");
        exitCount++;
        if (exitCount > 10) { // wait time > 10 * 2s, cancel wait.
            break;
        }
    }

    ICUNIT_ASSERT_EQUAL(g_pthreadTestCount, 11, g_pthreadTestCount); // 11, here assert the result.
    g_pthreadTestCount++;                                            // 12

    param.sched_priority = g_currThreadPri - 1;
    pthread_attr_setschedparam(&a, &param);
    ret = pthread_create(&newPthread1, &a, ThreadFunc6, 0);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_join(newPthread1, &res);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ICUNIT_ASSERT_EQUAL(g_pthreadTestCount, 13, g_pthreadTestCount); // 13, here assert the result.
    g_pthreadTestCount++;                                            // 14

    ICUNIT_ASSERT_EQUAL(g_pthreadTestCount, 14, g_pthreadTestCount); // 14, here assert the result.

    pthread_spin_destroy(&g_spinTestLock);

    return 0;
}

void ItTestPthread004(void)
{
    TEST_ADD_CASE("IT_POSIX_PTHREAD_004", PthreadTest004, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
