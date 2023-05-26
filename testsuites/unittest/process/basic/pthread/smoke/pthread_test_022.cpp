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

static int EdfProcess(void)
{
    int ret;
    int currPolicy = 0;
    struct sched_param currSchedParam = { 0 };
    volatile unsigned int count = 0;
    int currTID = Syscall(SYS_gettid, 0, 0, 0, 0);

    ret = pthread_getschedparam(pthread_self(), &currPolicy, &currSchedParam);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ICUNIT_ASSERT_EQUAL(currPolicy, SCHED_DEADLINE, LOS_NOK);
    ICUNIT_ASSERT_EQUAL(currSchedParam.sched_deadline, 1000000, LOS_NOK);    /* 1000000, 1s */
    ICUNIT_ASSERT_EQUAL(currSchedParam.sched_runtime, 20000, LOS_NOK);      /* 20000, 20ms */
    ICUNIT_ASSERT_EQUAL(currSchedParam.sched_period, 1000000, LOS_NOK);      /* 1000000, 1s */

    printf("--- edf2 -- 1 -- Tid[%d] thread start ---\n\r", currTID);
    do {
        for (volatile int i = 0; i < 10000; i++) { /* 10000, no special meaning */
            for (volatile int j = 0; j < 5; j++) { /* 5, no special meaning */
                int tmp = i - j;
            }
        }
        if (count % 3 == 0) {  /* 3, no special meaning */
            printf("--- edf2 -- 2 -- Tid[%d] thread running ---\n\r", currTID);
        }
        count++;
    } while (count <= 6); /* 6, no special meaning */
    printf("--- edf2 -- 3 -- Tid[%d] thread end ---\n\r", currTID);

    return 0;
}

static int ChildProcess(void)
{
    int *childThreadRetval = nullptr;
    pthread_t newUserThread;
    int pid, status, ret;
    int currPolicy = 0;
    volatile unsigned int count = 0;
    struct sched_param currSchedParam = { 0 };
    int currTID = Syscall(SYS_gettid, 0, 0, 0, 0);
    struct sched_param param = {
        .sched_deadline = 1000000,  /* 1000000, 1s */
        .sched_runtime = 20000,     /* 20000, 20ms */
        .sched_period = 1000000,    /* 1000000, 1s */
    };

    ret = sched_setscheduler(getpid(), SCHED_DEADLINE, &param);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_getschedparam(pthread_self(), &currPolicy, &currSchedParam);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ICUNIT_ASSERT_EQUAL(currPolicy, SCHED_DEADLINE, LOS_NOK);
    ICUNIT_ASSERT_EQUAL(currSchedParam.sched_deadline, 1000000, LOS_NOK);    /* 1000000, 1s */
    ICUNIT_ASSERT_EQUAL(currSchedParam.sched_runtime, 20000, LOS_NOK);       /* 20000, 20ms */
    ICUNIT_ASSERT_EQUAL(currSchedParam.sched_period, 1000000, LOS_NOK);      /* 1000000, 1s */

    pid = fork();
    if (pid == 0) {
        ret = EdfProcess();
        if (ret != 0) {
            exit(-1);
        }
        exit(0);
    } else if (pid > 0) {
        printf("--- edf1 -- 1 -- Tid[%d] thread start ---\n\r", currTID);
        do {
            for (volatile int i = 0; i < 50000; i++) { /* 50000, no special meaning */
                    int tmp = i + 1;
            }
            if (count % 5 == 0) {  /* 5, no special meaning */
                printf("--- edf1 -- 2 -- Tid[%d] thread running ---\n\r", currTID);
            }
            count++;
        } while (count <= 10); /* 10, no special meaning */
        printf("--- edf1 -- 3 -- Tid[%d] thread end ---\n\r", currTID);
        waitpid(pid, &status, 0);
    } else {
        exit(__LINE__);
    }

    return WEXITSTATUS(status) == 0 ? 0 : -1;
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

void ItTestPthread022(void)
{
    TEST_ADD_CASE("IT_POSIX_PTHREAD_022", TestCase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
