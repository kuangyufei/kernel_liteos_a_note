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

static int g_sigCount = 0;
static int g_sigCount1 = 0;
static void SigPrint(int sig)
{
    g_sigCount++;
    printf("signal receive sucess\n");
}

static UINT32 TestCase(VOID)
{
    int pid = 0;
    int ret = 0;
    void *retptr = NULL;
    sigset_t newset, oldset;

    sigemptyset(&newset);
    sigprocmask(SIG_SETMASK, &newset, NULL);

    signal(SIGURG, SIG_IGN);
    raise(SIGURG);
    signal(SIGURG, SigPrint);
    raise(SIGURG);
    printf("g_sigCount=%d\n", g_sigCount);
    sleep(1);
    ICUNIT_ASSERT_EQUAL(g_sigCount, 1, g_sigCount);

    signal(SIGHUP, SigPrint);
    signal(SIGUSR1, SigPrint);
    pid = fork();
    if (pid == 0) {
        signal(SIGHUP, SigPrint);
        signal(SIGUSR1, SigPrint);
        sigset_t set, set1;
        sigemptyset(&set);
        sigemptyset(&set1);
        sigaddset(&set, SIGHUP);
        sigaddset(&set1, SIGUSR1);
        sigaddset(&set1, SIGHUP);
        sigprocmask(SIG_BLOCK, &set1, NULL);
        raise(SIGHUP);
        raise(SIGUSR1);
        printf("child\n");
        int rt = sigsuspend(&set);
        printf("sigsuspend rt = %d, errno=%d\n", rt, errno);
        exit(2); // 2, exit args
    } else {
        sleep(1);
        kill(pid, SIGHUP);
        printf("sighug\n");
        kill(pid, SIGUSR1);
        wait(&ret);
        printf("ret1 = %d,%d\n", ret, WEXITSTATUS(ret));
        ICUNIT_ASSERT_EQUAL(WEXITSTATUS(ret), 2, WEXITSTATUS(ret)); // 2, assert that function Result is equal to this.
    }
    return LOS_OK;
}

void ItPosixSignal040(void)
{
    TEST_ADD_CASE(__FUNCTION__, TestCase, TEST_POSIX, TEST_SIGNAL, TEST_LEVEL0, TEST_FUNCTION);
}