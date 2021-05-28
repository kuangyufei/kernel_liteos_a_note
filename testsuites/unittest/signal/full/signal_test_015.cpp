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
static void SigPrint(int sig)
{
    g_sigCount = 1;
}

static int TestPauseNormal()
{
    void (*retSig)(int);
    int retValue;

    int status;
    errno = 0;
    retSig = signal(SIGPWR, SigPrint);
    ICUNIT_ASSERT_NOT_EQUAL(retSig, NULL, retSig);
    ICUNIT_ASSERT_EQUAL(errno, 0, errno);

    int fpid = fork();
    ICUNIT_ASSERT_WITHIN_EQUAL(fpid, 0, UINT_MAX, fpid);
    if (fpid == 0) {
        printf("child pause 1\n");
        sigset_t newset;
        sigemptyset(&newset);
        sigprocmask(SIG_SETMASK, &newset, nullptr);
        sigaddset(&newset, SIGUSR1);
        sigprocmask(SIG_BLOCK, &newset, nullptr);
        int ret1 = pause();
        printf("child pause end 1 retSig = %d, errno=%d\n", ret1, errno);
        exit(errno);
    }

    sleep(1);
    printf("father kill SIGUSR1 child not exec\n");
    retValue = kill(fpid, SIGUSR1);
    sleep(1);
    printf("father sleep then send signal to wake child\n");

    retValue = kill(fpid, SIGPWR);
    ICUNIT_ASSERT_EQUAL(retValue, 0, retValue);
    retValue = waitpid(fpid, &status, 0);
    ICUNIT_ASSERT_EQUAL(retValue, fpid, retValue);
    ICUNIT_ASSERT_EQUAL(WEXITSTATUS(status), EINTR, WEXITSTATUS(status));

    /* Child pause, then father kill child */
    fpid = fork();
    ICUNIT_ASSERT_WITHIN_EQUAL(fpid, 0, UINT_MAX, fpid);
    if (fpid == 0) {
        printf("child pause 2\n");
        pause();
        if (g_sigCount != 1) {
            exit(g_sigCount);
        }
        printf("child pause end 2\n");
        exit(0);
    }
    printf("father sleep 2 then send signal to wake child\n");
    sleep(1);
    printf("father kill fpid = %d\n", fpid);
    retValue = kill(fpid, SIGKILL);
    ICUNIT_ASSERT_EQUAL(retValue, 0, retValue);
    retValue = waitpid(fpid, &status, 0);
    ICUNIT_ASSERT_EQUAL(retValue, fpid, retValue);
    ICUNIT_ASSERT_EQUAL(WIFEXITED(status), 0, WIFEXITED(status));
    ICUNIT_ASSERT_EQUAL(WIFSIGNALED(status), 1, WIFSIGNALED(status));
    ICUNIT_ASSERT_EQUAL(WTERMSIG(status), SIGKILL, WTERMSIG(status));

    return 0;
}

void ItPosixSignal015(void)
{
    TEST_ADD_CASE(__FUNCTION__, TestPauseNormal, TEST_POSIX, TEST_SIGNAL, TEST_LEVEL0, TEST_FUNCTION);
}
