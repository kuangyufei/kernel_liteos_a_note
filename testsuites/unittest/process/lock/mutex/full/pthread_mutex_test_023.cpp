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

static pthread_mutex_t g_mutexLock;
static volatile int g_testToCount001 = 0;
static volatile int g_testToCount002 = 0;

static void *ThreadFuncTest1(void *arg)
{
    int ret = pthread_mutex_lock(&g_mutexLock);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    g_testToCount001++;
    return nullptr;

EXIT:
    g_testToCount001 = -1;
    return nullptr;
}

static void *ThreadFuncTest2(void *arg)
{
    int ret = pthread_mutex_lock(&g_mutexLock);
    ICUNIT_GOTO_EQUAL(ret, EOWNERDEAD, ret, EXIT);

    ret = pthread_mutex_consistent(&g_mutexLock);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    g_testToCount002++;
    ret = pthread_mutex_unlock(&g_mutexLock);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    return nullptr;

EXIT:
    g_testToCount002 = -1;
    return nullptr;
}

static int Testcase(void)
{
    int ret;
    pthread_t tid1, tid2;
    pthread_mutexattr_t attr;
    int robust;

    pthread_mutexattr_init(&attr);

    pthread_mutexattr_getrobust(&attr, &robust);
    ICUNIT_ASSERT_EQUAL(robust, 0, robust);

    ret = pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    pthread_mutexattr_getrobust(&attr, &robust);
    ICUNIT_ASSERT_EQUAL(robust, PTHREAD_MUTEX_ROBUST, robust);

    pthread_mutex_init(&g_mutexLock, &attr);

    pthread_mutexattr_destroy(&attr);

    g_testToCount001 = 0;
    g_testToCount002 = 0;

    ret = pthread_create(&tid1, nullptr, ThreadFuncTest1, nullptr);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_create(&tid2, nullptr, ThreadFuncTest2, nullptr);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    pthread_join(tid1, nullptr);
    pthread_join(tid2, nullptr);

    while (g_testToCount001 == 0 || g_testToCount002 == 0) {
    }
    ICUNIT_ASSERT_NOT_EQUAL(g_testToCount001, -1, g_testToCount001);
    ICUNIT_ASSERT_NOT_EQUAL(g_testToCount002, -1, g_testToCount002);

    pthread_mutex_destroy(&g_mutexLock);

    return 0;
}

void ItTestPthreadMutex023(void)
{
    TEST_ADD_CASE("IT_POSIX_PTHREAD_MUTEX_023", Testcase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
