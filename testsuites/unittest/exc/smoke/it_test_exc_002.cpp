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
#include "it_test_exc.h"
#include "pthread.h"

static void *ThreadFunc2(void *arg)
{
    while (1) {
    }

    return nullptr;
}

static void *ThreadFunc1(void *arg)
{
    int *test = nullptr;
    *test = 0x1;

    while (1) {
    }
    return nullptr;
}

static int TestThread(void)
{
    pthread_t newThread;
    int ret;
    int currThreadPolicy = 3;
    int currThreadPri;
    pthread_attr_t a = { 0 };
    struct sched_param param = { 0 };
    int count = 5;
    while (count > 0) {
        ret = pthread_create(&newThread, nullptr, ThreadFunc2, nullptr);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);
        count--;
    }

    ret = pthread_getschedparam(pthread_self(), &currThreadPolicy, &param);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    currThreadPri = param.sched_priority;

    ret = pthread_attr_init(&a);
    param.sched_priority = currThreadPri - 1;
    pthread_attr_setschedparam(&a, &param);
    ret = pthread_create(&newThread, &a, ThreadFunc1, nullptr);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    return 0;
}

static int TestCase(void)
{
    int *test = nullptr;
    int count = 5;
    int status = 0;
    int ret;
    pid_t pid = fork();
    ICUNIT_ASSERT_WITHIN_EQUAL(pid, 0, INVALID_PROCESS_ID, pid);
    if (pid == 0) {
        (void)TestThread();
        while (1) {
        }
    }

    ret = waitpid(pid, &status, 0);
    ICUNIT_ASSERT_EQUAL(ret, pid, ret);
    ret = WIFEXITED(status);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = WIFSIGNALED(status);
    ICUNIT_ASSERT_EQUAL(ret, 1, ret);
    ret = WTERMSIG(status);
    ICUNIT_ASSERT_EQUAL(ret, SIGUSR2, ret);

    return 0;
}

void ItTestExc002(void)
{
    TEST_ADD_CASE("IT_TEST_EXC_002", TestCase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
