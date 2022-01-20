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
#include "it_test_process.h"

static int g_waitPid;
static int g_backPid;
static int g_backPid1;

static int g_thread001;
static int g_thread002;
static void *ThreadFunc(void *arg)
{
    int status = 0;

    int ret = waitpid(0, &status, 0);
    printf("3333333333333 pid : %u\n", ret);
    ICUNIT_ASSERT_NOT_EQUAL_NULL(ret, -1, ret);
    ICUNIT_ASSERT_NOT_EQUAL_NULL(g_thread002, 0, g_thread002);
    g_thread001++;

EXIT:
    return NULL;
}

static void *ThreadFunc2(void *arg)
{
    int status = 0;

    int ret = waitpid(0, &status, 0);
    printf("222222222222222222 pid : %u\n", ret);
    ICUNIT_ASSERT_NOT_EQUAL_NULL(ret, -1, ret);
    ICUNIT_ASSERT_NOT_EQUAL_NULL(g_thread001, 0, g_thread001);
    g_thread002++;

EXIT:
    return NULL;
}

static int ProcessTest(void)
{
    int ret;
    int status = 100;
    pthread_t newPthread, newPthread1;
    pid_t pid, pid1, pid2;
    int count = 0;

    ret = setpgrp();
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    g_thread001 = 0;
    g_thread002 = 0;

    pid = fork();
    ICUNIT_GOTO_WITHIN_EQUAL(pid, 0, 100000, pid, EXIT); // 100000, assert pid equal to this.
    g_backPid = pid;

    if (pid == 0) {
        usleep(1000 * 10 * 30); // 1000, 10, 30, Used to calculate the delay time.
        exit(3); // 3, exit args
    }

    pid1 = fork();
    ICUNIT_GOTO_WITHIN_EQUAL(pid1, 0, 100000, pid, EXIT); // 100000, assert pid1 equal to this.
    g_backPid1 = pid1;
    if (pid1 == 0) {
        usleep(1000 * 10 * 20); // 1000, 10, 20, Used to calculate the delay time.
        exit(4); // 4, exit args
    }

    pid2 = fork();
    ICUNIT_GOTO_WITHIN_EQUAL(pid2, 0, 100000, pid, EXIT); // 100000, assert pid2 equal to this.
    if (pid2 == 0) {
        usleep(1000 * 10 * 15); // 1000, 10, 15, Used to calculate the delay time.
        exit(4); // 4, exit args
    }

    ret = pthread_create(&newPthread, NULL, ThreadFunc, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    usleep(1000 * 10 * 5); // 1000, 10, 5, Used to calculate the delay time.
    ret = pthread_create(&newPthread1, NULL, ThreadFunc2, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    usleep(1000 * 10 * 5); // 1000, 10, 5, Used to calculate the delay time.

    ret = waitpid(0, &status, 0);
    printf("111111111111111 pid : %u\n", ret);
    ICUNIT_ASSERT_NOT_EQUAL(ret, -1, ret);
    g_thread001++;
    ICUNIT_ASSERT_NOT_EQUAL(g_thread002, 1, g_thread001);

    ret = pthread_join(newPthread, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_join(newPthread1, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ICUNIT_ASSERT_EQUAL(g_thread001, 2, g_thread001); // 2, assert that function Result is equal to this.

    exit(255); // 255, exit args
EXIT:
    return 0;
}

static int TestCase(void)
{
    int ret;
    int status = 0;
    pid_t pid = fork();
    ICUNIT_GOTO_WITHIN_EQUAL(pid, 0, 100000, pid, EXIT); // 100000, assert pid equal to this.
    if (pid == 0) {
        ProcessTest();
        exit(g_iCunitErrLineNo);
    }

    ret = waitpid(pid, &status, 0);
    ICUNIT_GOTO_EQUAL(ret, pid, ret, EXIT);
    status = WEXITSTATUS(status);
    ICUNIT_GOTO_EQUAL(status, 255, status, EXIT); // 255, assert status equal to this.
    return 0;
EXIT:
    return 1;
}

void ItTestProcess022(void)
{
    TEST_ADD_CASE("IT_POSIX_PROCESS_022", TestCase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
