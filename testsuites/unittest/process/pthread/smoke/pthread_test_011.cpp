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
#include "sys/syscall.h"

void child1(void)
{
    int count = 0;
    int policy, pri, newPolicy;
    struct sched_param param = { 0 };
    pthread_t pthread = pthread_self();
    int tid = Syscall(SYS_gettid, 0, 0, 0, 0);

    int ret = pthread_getschedparam(pthread, &policy, &param);

    pri = param.sched_priority;

    while (1) {
        ret = pthread_getschedparam(pthread, &newPolicy, &param);
        if (ret != 0) {
            printf("pthread_getschedparam failed  ! %d erro: %d\n", __LINE__, errno);
            exit(255); // 255, set a special exit code.
        }

        if (newPolicy != policy || pri != param.sched_priority) {
            printf("pthread_getschedparam failed  ! %d policy %d newPolicy pri %d param.sched_priority :%d\n", __LINE__,
                policy, newPolicy, pri, param.sched_priority);
            exit(255); // 255, set a special exit code.
        }

        count++;
    };

    exit(0);
}

void child(void)
{
    int ret;
    int loop = 0;
    int status = 0;
    const int loopNum = 50;
    const int countNum = 128;
    pid_t pid = fork();
    if (pid == 0) {
        child1();
    }

    int tid = Syscall(SYS_gettid, 0, 0, 0, 0);

    for (int count = 0; count < countNum; count++) {
        if (count == tid) {
            continue;
        }

        ret = Syscall(SYS_pthread_join, count, 0, 0, 0);
        if (ret == 0) {
            exit(1);
        }

        ret = Syscall(SYS_pthread_set_detach, count, 0, 0, 0);
        if (ret == 0) {
            exit(1);
        }

        ret = Syscall(SYS_pthread_deatch, count, 0, 0, 0);
        if (ret == 0) {
            exit(1);
        }

        ret = Syscall(SYS_sched_setparam, count, 20, -1, 0); // 20, param test
        if (ret == 0) {
            exit(1);
        }

        ret = Syscall(SYS_sched_getparam, count, -1, 0, 0);
        if (ret == 0) {
            exit(1);
        }

        ret = Syscall(SYS_sched_setscheduler, count, SCHED_RR, 20, -1); // 20, param test
        if (ret == 0) {
            exit(1);
        }

        ret = Syscall(SYS_sched_getscheduler, count, -1, 0, 0);
        if (ret == 0) {
            exit(1);
        }

        ret = Syscall(SYS_tkill, count, SIGPWR, 0, 0);
        if (ret == 0) {
            exit(1);
        }

        if (count == countNum - 1) {
            loop++;
            if (loop == loopNum) {
                break;
            } else {
                count = 0;
            }
        }
    }

    kill(pid, SIGKILL);

    ret = waitpid(pid, &status, 0);
    ICUNIT_GOTO_EQUAL(ret, pid, ret, EXIT);

    exit(255); // 255, set a special exit code.
EXIT:
    exit(0);
}

static int Testcase(void)
{
    int ret;
    int status = 0;
    pid_t pid = fork();
    ICUNIT_GOTO_WITHIN_EQUAL(pid, 0, 100000, pid, EXIT); // 100000, The pid will never exceed 100000.
    if (pid == 0) {
        child();
        printf("[Failed] - [Errline : %d RetCode : 0x%x\n", g_iCunitErrLineNo, g_iCunitErrCode);
        exit(0);
    }
    ret = waitpid(pid, &status, 0);
    ICUNIT_GOTO_EQUAL(ret, pid, ret, EXIT);
    status = WEXITSTATUS(status);
    ICUNIT_GOTO_EQUAL(status, 255, status, EXIT); // 255, assert exit code.
    return 0;
EXIT:
    return 1;
}

void ItTestPthread011(void)
{
    TEST_ADD_CASE("IT_POSIX_PTHREAD_011", Testcase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
