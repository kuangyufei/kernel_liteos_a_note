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

static void SigPrint(int sig)
{
    printf("%s\n", __FUNCTION__, __LINE__);
}

static int TestSigTimeWaitKillProcess()
{
    sigset_t set;
    int sig, fpid, retValue, status;
    siginfo_t si;
    struct timespec ts;

    ts.tv_sec = 2; // 2, set the sec of timeout.
    ts.tv_nsec = 0;
    void (*retSig)(int);
    retSig = signal(SIGALRM, SigPrint);
    ICUNIT_ASSERT_NOT_EQUAL(retSig, NULL, retSig);

    retSig = signal(SIGUSR1, SigPrint);
    ICUNIT_ASSERT_NOT_EQUAL(retSig, NULL, retSig);
    retValue = sigemptyset(&set);
    ICUNIT_ASSERT_EQUAL(retValue, 0, retValue);
    retValue = sigaddset(&set, SIGALRM);
    ICUNIT_ASSERT_EQUAL(retValue, 0, retValue);
    retValue = sigaddset(&set, SIGUSR1);
    ICUNIT_ASSERT_EQUAL(retValue, 0, retValue);
    int count = 0;
    fpid = fork();
    ICUNIT_ASSERT_WITHIN_EQUAL(fpid, 0, UINT_MAX, fpid);
    if (fpid == 0) {
        printf("sig wait begin\n");
        while (1) {
            retValue = sigtimedwait(&set, &si, &ts);
            if (retValue != SIGUSR1) {
                exit(retValue);
            }
            printf("sig wait end\n");
            count++;
            if (count == 3) { // 3, Possible values for the current parameter
                break;
            }
        }
        exit(0);
    }
    sleep(1);
    retValue = kill(fpid, SIGUSR1);
    ICUNIT_ASSERT_EQUAL(retValue, 0, retValue);
    sleep(1);
    retValue = kill(fpid, SIGKILL);
    ICUNIT_ASSERT_EQUAL(retValue, 0, retValue);

    retValue = waitpid(fpid, &status, 0);
    ICUNIT_ASSERT_EQUAL(retValue, fpid, retValue);
    ICUNIT_ASSERT_EQUAL(WIFEXITED(status), 0, WIFEXITED(status));
    ICUNIT_ASSERT_EQUAL(WIFSIGNALED(status), 1, WIFSIGNALED(status));
    ICUNIT_ASSERT_EQUAL(WTERMSIG(status), SIGKILL, WTERMSIG(status));

    signal(SIGALRM, SIG_DFL);
    signal(SIGUSR1, SIG_DFL);
    return 0;
}


void ItPosixSignal018(void)
{
    TEST_ADD_CASE(__FUNCTION__, TestSigTimeWaitKillProcess, TEST_POSIX, TEST_SIGNAL, TEST_LEVEL0, TEST_FUNCTION);
}
