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

static void *ThreadFuncTest3(void *arg)
{
    while (1) {
        sleep(1);
    };

    return NULL;
}

static int GroupProcess(void)
{
    int ret;
    int status = 0;
    pid_t pid, pid1;
    pthread_t newPthread;
    struct timespec ts = { 0 };

    ret = sched_rr_get_interval(getpid(), &ts);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ICUNIT_ASSERT_EQUAL(ts.tv_sec, 0, ts.tv_sec);
    if (ts.tv_nsec <= 5000000 || ts.tv_nsec > 20000000) { // 5000000, 20000000, expected range of tv_nsec.
        ICUNIT_ASSERT_EQUAL(ts.tv_nsec, -1, ts.tv_nsec);
    }

    ret = pthread_create(&newPthread, NULL, ThreadFuncTest3, 0);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = sched_rr_get_interval(getpid(), &ts);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ICUNIT_ASSERT_EQUAL(ts.tv_sec, 0, ts.tv_sec);
    if (ts.tv_nsec <= 10000000 || ts.tv_nsec > 40000000) { // 10000000, 40000000, expected range of tv_nsec.
        ICUNIT_ASSERT_EQUAL(ts.tv_nsec, -1, ts.tv_nsec);
    }

    exit(255); // 255, exit args
EXIT:
    return 0;
}

static int TestCase(void)
{
    int ret;
    int status = 0;
    pid_t pid, pid1;
    int currGid = getpgrp();
    pid = fork();
    ICUNIT_GOTO_WITHIN_EQUAL(pid, 0, 100000, pid, EXIT); // 100000, assert pid equal to this.

    if (pid == 0) {
        prctl(PR_SET_NAME, "mainFork", 0UL, 0UL, 0UL);
        GroupProcess();
        printf("[Failed] - [Errline : %d RetCode : 0x%x\n", g_iCunitErrLineNo, g_iCunitErrCode);
        exit(g_iCunitErrLineNo);
    }

    ret = waitpid(pid, &status, 0);
    ICUNIT_ASSERT_EQUAL(ret, pid, ret);
    status = WEXITSTATUS(status);
    ICUNIT_GOTO_EQUAL(status, 255, status, EXIT); // 255, assert status equal to this.

    return 0;
EXIT:
    return 1;
}

void ItTestProcess044(void)
{
    TEST_ADD_CASE("IT_POSIX_PROCESS_044", TestCase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
