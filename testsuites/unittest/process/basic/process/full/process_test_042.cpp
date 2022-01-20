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
#include "sys/shm.h"

static const int TEST_THREAD = 40;
static const int TEST_LOOP = 3000;

static void Child2(int shmid)
{
    int count = 2; // 2, Set the calculation number to determine the cycle status.
    int *shared = (int *)shmat(shmid, nullptr, 0);
    ICUNIT_ASSERT_NOT_EQUAL_VOID(shared, (void *)-1, shared);

    while ((*shared) < (TEST_LOOP + 2)) { // 2, Set the cycle number.
        ICUNIT_ASSERT_EQUAL_VOID(*shared, count, *shared);
        (*shared)++;
        count += 3; // 3, Set the calculation number to determine the cycle status.
        sched_yield();
    }

    exit(255); // 255, exit args
    return;
}

static void Child1(int shmid)
{
    int count = 1;
    int *shared = (int *)shmat(shmid, nullptr, 0);
    ICUNIT_ASSERT_NOT_EQUAL_VOID(shared, (void *)-1, shared);

    while ((*shared) < (TEST_LOOP + 1)) {
        ICUNIT_ASSERT_EQUAL_VOID(*shared, count, *shared);
        (*shared)++;
        count += 3; // 3, Set the calculation number to determine the cycle status.
        sched_yield();
    }

    (*shared) = 100000; // 100000, shared num.

    exit(255); // 255, exit args
    return;
}

static int GroupProcess(void)
{
    int testPid;
    int ret;
    int policy = 0;
    struct sched_param param = { 0 };
    int status = 0;
    pid_t pid, pid1;
    const int memSize = 1024;
    int shmid;
    int *shared = NULL;

    ret = sched_getparam(getpid(), &param);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    int processPrio = param.sched_priority;

    ret = pthread_getschedparam(pthread_self(), &policy, &param);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    shmid = shmget((key_t)1234, memSize, 0666 | IPC_CREAT); // 1234, Sets the shmget key; 0666 config of shmget
    ICUNIT_ASSERT_NOT_EQUAL(shmid, -1, shmid);

    pid = fork();
    ICUNIT_GOTO_WITHIN_EQUAL(pid, 0, 100000, pid, EXIT); // 100000, assert pid equal to this.
    if (pid == 0) {
        Child1(shmid);
        printf("[Failed] - [Errline : %d RetCode : 0x%x\n", g_iCunitErrLineNo, g_iCunitErrCode);
        exit(0);
    }

    pid1 = fork();
    ICUNIT_GOTO_WITHIN_EQUAL(pid1, 0, 100000, pid1, EXIT); // 100000, assert pid equal to this.
    if (pid1 == 0) {
        Child2(shmid);
        printf("[Failed] - [Errline : %d RetCode : 0x%x\n", g_iCunitErrLineNo, g_iCunitErrCode);
        exit(0);
    }

    shared = (int *)shmat(shmid, nullptr, 0);
    ICUNIT_ASSERT_NOT_EQUAL(shared, (void *)-1, shared);

    (*shared) = 0;

    while ((*shared) < TEST_LOOP) {
        (*shared)++;
        sched_yield();
    }

    (*shared) = TEST_LOOP + 10; // 10, Set the cycle number.

    param.sched_priority = processPrio - 2; // 2, set pthread priority.
    ret = sched_setscheduler(pid, SCHED_RR, &param);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ICUNIT_ASSERT_EQUAL(*shared, 100000, *shared); // 100000, assert that function Result is equal to this.

    ret = waitpid(pid, &status, 0);
    ICUNIT_ASSERT_EQUAL(ret, pid, ret);
    status = WEXITSTATUS(status);
    ICUNIT_ASSERT_EQUAL(status, 255, status); // 255, assert that function Result is equal to this.

    ret = waitpid(pid1, &status, 0);
    ICUNIT_ASSERT_EQUAL(ret, pid1, ret);
    status = WEXITSTATUS(status);
    ICUNIT_ASSERT_EQUAL(status, 255, status); // 255, assert that function Result is equal to this.

    exit(255); // 255, exit args
EXIT:
    return 0;
}

static int TestCase(void)
{
    int ret;
    int status = 0;
    pid_t pid, pid1;

    int temp = GetCpuCount();
    if (temp != 1) {
        return 0;
    }

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

void ItTestProcess042(void)
{
    TEST_ADD_CASE("IT_POSIX_PROCESS_042", TestCase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
