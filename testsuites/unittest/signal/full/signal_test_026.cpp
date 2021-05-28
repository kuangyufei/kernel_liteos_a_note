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
#define INVALID_SIG 1000

static void SigPrint(int signum)
{
    printf("Signal Triggered\n");
}

static UINT32 TestCase(VOID)
{
    int ret, sig;
    struct itimerval newValue, oldValue;
    siginfo_t info;
    sigset_t newset;
    struct timespec timeout;

    timeout.tv_nsec = 0;
    timeout.tv_sec = 1;

    int fpid, status;
    fpid = fork();
    if (fpid == 0) {
        (void)signal(SIGCHLD, SigPrint);
        sigemptyset(&newset);
        sigaddset(&newset, SIGCHLD);
        ret = sigtimedwait(&newset, &info, &timeout);
        if (ret != -1) {
            exit(ret);
        }
        if (errno != EAGAIN) {
            exit(errno);
        }
        printf("---------------\n");

        sigemptyset(&newset);
        sigaddset(&newset, SIGCHLD);
        timeout.tv_nsec = 1;
        timeout.tv_sec = -1;
        ret = sigtimedwait(&newset, &info, &timeout);
        printf("ret = %d    errno = %d   EINVAL = %d\n", ret, errno, EINVAL);
        if (ret != -1) {
            exit(ret);
        }
        if (errno != EINVAL) {
            exit(errno);
        }

        sigemptyset(&newset);
        sigaddset(&newset, SIGCHLD);
        timeout.tv_nsec = 1;
        timeout.tv_sec = 3; // 3, set the sec of timeout.
        ret = sigtimedwait(&newset, (siginfo_t *)2, &timeout); // 2, wait for signal num
        printf("ret = %d    errno = %d   EFAULT = %d\n", ret, errno, EFAULT);
        if (ret != -1) {
            exit(ret);
        }
        if (errno != EFAULT) {
            exit(errno);
        }
        exit(0);
    }

    sleep(2); // 2, sleep 10 second.
    kill(fpid, SIGCHLD);
    ret = waitpid(fpid, &status, 0);
    ICUNIT_ASSERT_EQUAL(ret, fpid, ret);
    ICUNIT_ASSERT_EQUAL(WEXITSTATUS(status), 0, WEXITSTATUS(status));
    return LOS_OK;
}

void ItPosixSignal026(void)
{
    TEST_ADD_CASE(__FUNCTION__, TestCase, TEST_POSIX, TEST_SIGNAL, TEST_LEVEL0, TEST_FUNCTION);
}