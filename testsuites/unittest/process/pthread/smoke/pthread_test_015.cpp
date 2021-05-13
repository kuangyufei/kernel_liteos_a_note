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

static VOID *PthreadTest115(VOID *arg) {}

static int GroupProcess(void)
{
    int ret;
    pthread_t th1;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    int policy = 0;
    struct sched_param param = { 0 };
    const int testThreadCount = 2000;

    ret = pthread_getschedparam(pthread_self(), &policy, &param);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    param.sched_priority += 2; // 2, adjust the priority.
    ret = pthread_attr_setschedparam(&attr, &param);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    for (int i = 0; i < testThreadCount; i++) {
        ret = pthread_create(&th1, &attr, PthreadTest115, 0);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);

        ret = pthread_join(th1, NULL);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    }

    exit(255); // 255, set a special exit code.
}

static int TestCase(void)
{
    int ret;
    int status = 0;
    pid_t pid, pid1;
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
    status = WEXITSTATUS(status);
    ICUNIT_GOTO_EQUAL(status, 255, status, EXIT); // 255, assert the special exit code.

    return 0;
EXIT:
    return 1;
}

void ItTestPthread015(void)
{
    TEST_ADD_CASE("IT_POSIX_PTHREAD_015", TestCase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
