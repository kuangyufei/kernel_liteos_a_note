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
    (void)args;
    printf("hpf thread run...\r\n");
    return NULL;
}

static int ChildProcess(void)
{
    int ret, currThreadPolicy;
    pthread_attr_t a = { 0 };
    struct sched_param hpfparam = { 0 };
    pthread_t newUserThread;
    volatile unsigned int count = 0;
    int currTID = Syscall(SYS_gettid, 0, 0, 0, 0);
    struct sched_param param = {
        .sched_deadline = 1000000,  /* 1000000, 1s */
        .sched_runtime = 20000,    /* 20000, 20ms */
        .sched_period = 1000000,    /* 1000000, 1s */
    };

    ret = pthread_getschedparam(pthread_self(), &currThreadPolicy, &hpfparam);
    ICUNIT_ASSERT_EQUAL(ret, 0, -ret);
    ret = pthread_attr_init(&a);
    hpfparam.sched_priority = hpfparam.sched_priority - 1;
    pthread_attr_setschedparam(&a, &hpfparam);
    ret = pthread_create(&newUserThread, &a, ThreadFuncTest, (void *)currTID);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = sched_setscheduler(getpid(), SCHED_DEADLINE, &param);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);

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

void ItTestPthread024(void)
{
    TEST_ADD_CASE("IT_POSIX_PTHREAD_024", TestCase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
