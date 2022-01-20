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


static pthread_mutex_t g_mutexLock001;
static pthread_mutex_t g_mutexLock002;

static void *TaskF01(void *arg)
{
    unsigned int ret;
    ret = pthread_mutex_lock(&g_mutexLock001);
    ICUNIT_TRACK_EQUAL(ret, 0, ret);

    ret = pthread_mutex_unlock(&g_mutexLock001);
    ICUNIT_TRACK_EQUAL(ret, 0, ret);

    return nullptr;
}

static unsigned int TestCase(void)
{
    unsigned int ret;
    pthread_t newThread;
    pthread_attr_t attr;
    pthread_mutexattr_t mutexAttr;
    struct sched_param sp;

    g_testCount = 0;

    pthread_mutexattr_init(&mutexAttr);

    pthread_mutexattr_settype(&mutexAttr, PTHREAD_MUTEX_ERRORCHECK);

    ret = pthread_mutex_init(&g_mutexLock001, &mutexAttr);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_mutex_init(&g_mutexLock002, nullptr);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_mutex_lock(&g_mutexLock001);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_mutex_lock(&g_mutexLock001);
    ICUNIT_ASSERT_EQUAL(ret, EDEADLK, ret);

    ret = pthread_mutex_unlock(&g_mutexLock001);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_attr_init(&attr);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    sp.sched_priority = 4; // 4, Set the priority according to the task purpose,a smaller number means a higher priority.
    ret = pthread_attr_setschedparam(&attr, &sp);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    ret = pthread_create(&newThread, &attr, TaskF01, nullptr);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    LosTaskDelay(5); // 5, delay enouge time

    ret = pthread_mutex_destroy(&g_mutexLock001);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_mutex_unlock(&g_mutexLock001);
    ICUNIT_ASSERT_NOT_EQUAL(ret, 0, ret);

    ret = pthread_mutex_destroy(&g_mutexLock001);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_mutex_destroy(&g_mutexLock001);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_mutex_destroy(&g_mutexLock002);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_join(newThread, nullptr);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_attr_destroy(&attr);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    return 0;
}

void ItTestPthreadMutex010(void)
{
    TEST_ADD_CASE("IT_POSIX_PTHREAD_MUTEX_010", TestCase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
