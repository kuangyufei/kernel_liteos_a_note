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
#define TEST_THREAD_COUNT 2000

static int g_testCnt = 0;

static void *PthreadTest116(VOID *arg)
{
    return NULL;
}

static void *PthreadTest115(VOID *arg)
{
    pthread_t th1;

    int ret = pthread_create(&th1, NULL, PthreadTest116, 0);
    ICUNIT_ASSERT_EQUAL_NULL(ret, 0, (void *)(intptr_t)ret);

    ret = pthread_join(th1, NULL);
    ICUNIT_ASSERT_EQUAL_NULL(ret, 0, (void *)(intptr_t)ret);

    g_testCnt++;

    return NULL;
}

static int GroupProcess(void)
{
    int ret;
    pthread_t th1;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    int policy = 0;
    unsigned int *getStack = NULL;
    size_t getSize;
    struct sched_param param = { 0 };
    int stackSize = 2047;
    void *stack = (void *)&getSize;

    g_testCnt = 0;

    ret = pthread_attr_setstack(&attr, stack, stackSize);
    ICUNIT_ASSERT_EQUAL(ret, EINVAL, ret);

    ret = pthread_attr_getstack(&attr, (void **)&getStack, &getSize);
    ICUNIT_ASSERT_EQUAL(ret, EINVAL, ret);

    stackSize = 3000; // 3000, change stackSize, test again.

    pthread_attr_init(&attr);
    stack = (void *)0x1000;

    ret = pthread_attr_setstack(&attr, stack, stackSize);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_attr_getstack(&attr, (void **)&getStack, &getSize);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ICUNIT_ASSERT_EQUAL(getStack, stack, getStack);
    ICUNIT_ASSERT_EQUAL(getSize, stackSize, getSize);

    ret = pthread_create(&th1, &attr, PthreadTest115, 0);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    exit(255); // 255, set a special exit code.
    return 0;
}

static int TestCase(void)
{
    int ret;
    int status = 0;
    pid_t pid;
    pid = fork();
    ICUNIT_GOTO_WITHIN_EQUAL(pid, 0, 100000, pid, EXIT); // 100000, The pid will never exceed 100000.

    if (pid == 0) {
        prctl(PR_SET_NAME, "mainFork", 0UL, 0UL, 0UL);
        GroupProcess();
        printf("[Failed] - [Errline : %d RetCode : 0x%x\n", g_iCunitErrLineNo, g_iCunitErrCode);
        exit(g_iCunitErrLineNo);
    }

    ret = waitpid(pid, &status, 0);
    ICUNIT_ASSERT_EQUAL(ret, pid, ret);
    ICUNIT_ASSERT_EQUAL(WIFEXITED(status), 0, WIFEXITED(status));
    ICUNIT_ASSERT_EQUAL(WTERMSIG(status), SIGUSR2, WTERMSIG(status));

EXIT:
    return 0;
}

void ItTestPthread019(void)
{
    TEST_ADD_CASE("IT_POSIX_PTHREAD_019", TestCase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
