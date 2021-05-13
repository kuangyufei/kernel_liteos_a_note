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
#include "it_test_signal.h"
#include "signal.h"
#include "sys/wait.h"

static const int SIG_TEST_COUNT = 3;
static int g_sigCount = 0;
static void SigPrint(int sig)
{
    g_sigCount++;
}

static int TestRaiseMuliti()
{
    int sig = SIGTERM;
    int count = 0;
    void (*ret)(int);
    int retValue;

    g_sigCount = 0;
    ret = signal(sig, SigPrint);
    ICUNIT_ASSERT_NOT_EQUAL(ret, NULL, ret);

    while (1) {
        retValue = raise(sig);
        ICUNIT_ASSERT_EQUAL(retValue, 0, retValue);
        usleep(10000); // 10000, Used to calculate the delay time.
        count++;
        if (count >= SIG_TEST_COUNT) {
            break;
        }
    }

    return g_sigCount;
}

static int TestCase(void)
{
    int count = TestRaiseMuliti();
    ICUNIT_ASSERT_EQUAL(count, SIG_TEST_COUNT, count);

    int pid = fork();
    if (pid == 0) {
        int retValue;
        printf("sigprocmask(1, NULL, NULL);\n");
        retValue = sigprocmask(1, NULL, NULL);
        if (retValue != 0) {
            printf("errline  = %d\n", __LINE__);
            exit(-1);
        }
        printf("raise(SIGSTOP);\n");
        raise(SIGSTOP);

        int ret = kill(10, 0); // 10, kill process pid.
        if (retValue != -1 || errno != ESRCH) {
            exit(-1);
        }

        ret = kill(99999, 0); // 99999, kill process pid.
        if (retValue != -1 || errno != ESRCH) {
            printf("errline  = %d\n", __LINE__);
            exit(-1);
        }
        ret = kill(10, 31); // 10, kill process pid; 31, sigal.
        if (retValue != -1 || errno != EINVAL) {
            printf("errline  = %d\n", __LINE__);
            exit(-1);
        }
        ret = kill(10, 32); // 10, kill process pid; 32, sigal.
        if (retValue != -1 || errno != EINVAL) {
            printf("errline  = %d\n", __LINE__);
            exit(-1);
        }

        ret = kill(2, 32); // 2, kill process pid; 32, sigal.
        if (retValue != -1 || errno != EINVAL) {
            printf("errline  = %d\n", __LINE__);
            exit(-1);
        }

        printf("test EPERM begin\n");
        ret = kill(2, 5); // 2, kill process pid; 5, sigal.
        if (retValue != -1 || errno != EPERM) {
            printf("errline  = %d\n", __LINE__);
            exit(-1);
        }

        ret = kill(3, 5); // 3, kill process pid; 5, sigal.
        if (retValue != -1 || errno != EPERM) {
            printf("errline  = %d\n", __LINE__);
            exit(-1);
        }

        ret = kill(0, 5); // 5, kill sigal num.
        if (retValue != -1 || errno != EPERM) {
            printf("errline  = %d\n", __LINE__);
            exit(-1);
        }

        ret = kill(1, 5); // 5, kill sigal num .
        if (retValue != -1 || errno != EPERM) {
            printf("errline  = %d\n", __LINE__);
            exit(-1);
        }

        printf("test kill ok\n");
        retValue = raise(SIGSTOP);
        if (retValue != 0) {
            printf("errline  = %d\n", __LINE__);
            exit(-1);
        }
        int status, rt;
        signal(SIGALRM, SigPrint);
        sigset_t sigmask, oldmask, pending;
        sigemptyset(&sigmask);
        sigemptyset(&oldmask);
        sigemptyset(&pending);
        sigpending(&pending);
        if (sigisemptyset(&pending) != 1) {
            printf("errline  = %d\n", __LINE__);
            exit(-1);
        }
        sigaddset(&sigmask, SIGALRM);
        sigaddset(&sigmask, SIGUSR1);
        sigprocmask(SIG_BLOCK, &sigmask, &oldmask);
        sigpending(&pending);
        if (sigisemptyset(&pending) != 1) {
            printf("errline  = %d\n", __LINE__);
            exit(-1);
        }
        if (sigisemptyset(&oldmask) != 1) {
            printf("errline  = %d\n", __LINE__);
            exit(-1);
        }

        printf("1 pending=%d\n", pending.__bits[0]);
        printf("1 oldmask=%d\n", oldmask.__bits[0]);
        printf("before raise\n");
        raise(SIGALRM);
        printf("after raise\n");
        sigpending(&pending);
        if (sigismember(&pending, SIGALRM) != 1) {
            printf("errline  = %d\n", __LINE__);
            exit(-1);
        }
        printf("pending=%d,sigismem = %d\n", pending.__bits[0], sigismember(&pending, SIGALRM));
        exit(0);
    }
    sleep(1);
    int status;
    signal(SIGALRM, SIG_DFL);
    int retValue = waitpid(pid, &status, 0);
    ICUNIT_ASSERT_EQUAL(retValue, pid, retValue);
    return 0;
}

void ItPosixSignal001(void)
{
    TEST_ADD_CASE("IT_POSIX_SIGNAL_001", TestCase, TEST_POSIX, TEST_SIGNAL, TEST_LEVEL0, TEST_FUNCTION);
}
