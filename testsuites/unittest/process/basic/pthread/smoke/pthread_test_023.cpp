/*
 * Copyright (c) 2023-2023 Huawei Device Co., Ltd. All rights reserved.
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

static void *ThreadFuncTest(void *args)
{
    return (void *)NULL;
}

static int ChildProcess(void)
{
    int ret;
    int currPolicy = 0;
    int oldPolicy;
    pthread_t newUserThread;
    pthread_attr_t a = { 0 };
    struct sched_param hpfparam = { 0 };
    volatile unsigned int count = 0;
    struct sched_param currSchedParam = { 0 };
    int currTID = Syscall(SYS_gettid, 0, 0, 0, 0);
    struct sched_param param = {
        .sched_deadline = 3000000,  /* 3000000, 3s */
        .sched_runtime = 200000,    /* 200000, 200ms */
        .sched_period = 5000000,    /* 5000000, 5s */
    };

    ret = pthread_getschedparam(pthread_self(), &oldPolicy, &hpfparam);
    ICUNIT_ASSERT_EQUAL(ret, 0, -ret);

    ret = sched_setscheduler(getpid(), SCHED_DEADLINE, &param);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_getschedparam(pthread_self(), &currPolicy, &currSchedParam);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ICUNIT_ASSERT_EQUAL(currPolicy, SCHED_DEADLINE, LOS_NOK);
    ICUNIT_ASSERT_EQUAL(currSchedParam.sched_deadline, 3000000, LOS_NOK);    /* 3000000, 3s */
    ICUNIT_ASSERT_EQUAL(currSchedParam.sched_runtime, 200000, LOS_NOK);      /* 200000, 200ms */
    ICUNIT_ASSERT_EQUAL(currSchedParam.sched_period, 5000000, LOS_NOK);      /* 5000000, 5s */

    ret = pthread_attr_init(&a);
    pthread_attr_setschedpolicy(&a, SCHED_RR);
    pthread_attr_setinheritsched(&a, PTHREAD_EXPLICIT_SCHED);
    ret = pthread_create(&newUserThread, &a, ThreadFuncTest, NULL);
    ICUNIT_ASSERT_NOT_EQUAL(ret, 0, ret);

    printf("--- edf2 -- 1 -- Tid[%d] thread end ---\n\r", currTID);

    return 0;
}

static int TestCase(void)
{
    int ret, pid, status;

    pid = fork();
    if (pid == 0) {
        ret = ChildProcess();
        if (ret != 0) {
            exit(-1);
        }
        exit(0);
    } else if (pid > 0) {
        waitpid(pid, &status, 0);
    } else {
        exit(__LINE__);
    }

    return WEXITSTATUS(status) == 0 ? 0 : -1;
}

void ItTestPthread023(void)
{
    TEST_ADD_CASE("IT_POSIX_PTHREAD_023", TestCase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
