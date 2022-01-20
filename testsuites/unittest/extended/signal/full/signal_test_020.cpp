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

static void SigHandler(int sig) // 信号处理程序
{
    if (sig == SIGINT) {
        printf("SIGINT sig\n");
    } else if (sig == SIGQUIT) {
        printf("SIGQUIT sig\n");
    } else {
        printf("SIGUSR1 sig\n");
    }
}

static int TestSigSuspend()
{
    int status, retValue;
    int fpid = fork();
    ICUNIT_ASSERT_WITHIN_EQUAL(fpid, 0, UINT_MAX, fpid);
    if (fpid == 0) {
        sigset_t newset, old, wait;
        struct sigaction act;
        printf("SIGUSR1 sig\n");
        act.sa_handler = SigHandler;
        retValue = sigemptyset(&act.sa_mask);
        if (retValue != 0) {
            exit(retValue);
        }
        act.sa_flags = 0;
        sigaction(SIGINT, &act, nullptr);
        sigaction(SIGQUIT, &act, nullptr);
        sigaction(SIGUSR1, &act, nullptr);
        sigaction(SIGALRM, &act, nullptr);

        retValue = sigemptyset(&newset);
        if (retValue != 0) {
            exit(retValue);
        }
        retValue = sigaddset(&newset, SIGALRM);
        if (retValue != 0) {
            exit(retValue);
        }
        retValue = sigprocmask(SIG_BLOCK, &newset, &old);
        if (retValue != 0) {
            exit(retValue);
        }
        printf("newset 1 = %x\n", newset.__bits[0]);
        printf("old 1 = %x\n", old.__bits[0]);

        retValue = sigemptyset(&newset);
        if (retValue != 0) {
            exit(retValue);
        }
        retValue = sigaddset(&newset, SIGQUIT);
        if (retValue != 0) {
            exit(retValue);
        }
        retValue = sigprocmask(SIG_BLOCK, &newset, &old);
        if (retValue != 0) {
            exit(retValue);
        }
        printf("newset 2 = %x\n", newset.__bits[0]);
        printf("old 2 = %x\n", old.__bits[0]);

        retValue = sigemptyset(&newset);
        if (retValue != 0) {
            exit(retValue);
        }
        retValue = sigaddset(&newset, SIGINT);
        if (retValue != 0) {
            exit(retValue);
        }

        retValue = sigprocmask(SIG_BLOCK, &newset, &old);
        if (retValue != 0) {
            exit(retValue);
        }
        printf("newset 1 = %x\n", newset.__bits[0]);
        printf("old 1 = %x\n", old.__bits[0]);

        retValue = sigemptyset(&wait);
        if (retValue != 0) {
            exit(retValue);
        }
        retValue = sigaddset(&wait, SIGUSR1);
        if (retValue != 0) {
            exit(retValue);
        }
        printf("wait = %x\n", wait.__bits[0]);

        if (sigsuspend(&wait) != -1) {
            printf("sigsuspend error\n");
            return -1;
        }
        printf("After sigsuspend\n");
        if (retValue != 0) {
            exit(retValue);
        }
        printf("old 2= %x\n", old.__bits[0]);

        sigset_t pending;
        retValue = sigemptyset(&pending);
        if (retValue != 0) {
            exit(retValue);
        }
        printf("pending 1= %x\n", pending.__bits[0]);
        retValue = raise(SIGINT);
        if (retValue != 0) {
            exit(retValue);
        }
        retValue = sigpending(&pending);
        if (retValue != 0) {
            exit(retValue);
        }
        printf("pending 2= %x\n", pending.__bits[0]);

        retValue = raise(SIGALRM);
        if (retValue != 0) {
            exit(retValue);
        }
        retValue = sigpending(&pending);
        if (retValue != 0) {
            exit(retValue);
        }
        printf("pending 3= %x\n", pending.__bits[0]);
        exit(0);
    }

    sleep(1);
    printf("kill SIGUSR1\n");
    retValue = kill(fpid, SIGUSR1);
    ICUNIT_ASSERT_EQUAL(retValue, 0, retValue);
    sleep(1);
    printf("kill SIGINT\n");
    retValue = kill(fpid, SIGINT);
    ICUNIT_ASSERT_EQUAL(retValue, 0, retValue);
    retValue = waitpid(fpid, &status, 0);
    ICUNIT_ASSERT_EQUAL(retValue, fpid, retValue);
    ICUNIT_ASSERT_EQUAL(WEXITSTATUS(status), 0, WEXITSTATUS(status));

    sigset_t new1, old1;
    struct sigaction act;

    fpid = fork();
    if (fpid == 0) {
        act.sa_handler = SigHandler;
        retValue = sigemptyset(&act.sa_mask);
        act.sa_flags = 0;
        sigaction(SIGINT, &act, nullptr);
        sigaction(SIGQUIT, &act, nullptr);
        sigaction(SIGUSR1, &act, nullptr);

        retValue = sigemptyset(&new1);
        if (retValue != 0) {
            exit(retValue);
        }
        retValue = sigaddset(&new1, SIGINT);
        if (retValue != 0) {
            exit(retValue);
        }
        retValue = sigprocmask(SIG_BLOCK, &new1, &old1);
        if (retValue != 0) {
            exit(retValue);
        }
        printf("new 1 = %x\n", new1.__bits[0]);
        printf("old 1 = %x\n", old1.__bits[0]);

        retValue = kill(getpid(), SIGINT);
        if (retValue != 0) {
            exit(retValue);
        }
        retValue = raise(SIGUSR1);
        if (retValue != 0) {
            exit(retValue);
        }
        printf("raise 1 = %x\n", new1.__bits[0]);

        exit(0);
    }

    retValue = waitpid(fpid, &status, 0);
    ICUNIT_ASSERT_EQUAL(retValue, fpid, retValue);
    ICUNIT_ASSERT_EQUAL(WEXITSTATUS(status), 0, WEXITSTATUS(status));

    return 0;
}

void ItPosixSignal020(void)
{
    TEST_ADD_CASE(__FUNCTION__, TestSigSuspend, TEST_POSIX, TEST_SIGNAL, TEST_LEVEL0, TEST_FUNCTION);
}
