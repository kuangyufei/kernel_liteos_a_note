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
static void SigPrint(int signum)
{
    g_sigCount++;
    printf("signal receive sucess\n");
}

static UINT32 TestCase(VOID)
{
    int pid;
    int ret, status;
    sigset_t newset;
    struct sigaction act;

    act.sa_handler = SigPrint;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    pid = fork();
    if (pid < 0) {
        printf("Fork error\n");
        return LOS_NOK;
    } else if (pid == 0) {
        ret = sigaction(SIGUSR1, &act, nullptr);
        if (ret != 0) {
            exit(LOS_NOK);
        }
        sigemptyset(&newset);
        sigaddset(&newset, SIGCHLD);
        ret = sigsuspend(&newset);
        printf("ret = %d    errno = %d   EINTR = %d\n", ret, errno, EINTR);
        if (ret != -1 || errno != EINTR) {
            exit(LOS_NOK);
        }

        ret = sigsuspend(NULL);
        if (ret != -1) {
            exit(ret);
        }
        if (errno != EFAULT) {
            exit(errno);
        }

        ret = sigsuspend((sigset_t *)2); // 2, suspend signal num.
        if (ret != -1) {
            exit(ret);
        }
        if (errno != EFAULT) {
            exit(errno);
        }

        ret = sigpending(NULL);
        if (ret != -1) {
            exit(ret);
        }
        if (errno != EFAULT) {
            exit(errno);
        }

        ret = sigpending((sigset_t *)2); // 2, pending signal num.
        if (ret != -1) {
            exit(ret);
        }
        if (errno != EFAULT) {
            exit(errno);
        }
        exit(LOS_OK);
    }
    sleep(1);
    ret = kill(pid, SIGUSR1);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    printf("kill sucess\n");
    ret = waitpid(pid, &status, 0);
    ICUNIT_ASSERT_EQUAL(ret, pid, ret);
    ICUNIT_ASSERT_EQUAL(WEXITSTATUS(status), LOS_OK, WEXITSTATUS(status));

    return LOS_OK;
}

void ItPosixSignal029(void)
{
    TEST_ADD_CASE(__FUNCTION__, TestCase, TEST_POSIX, TEST_SIGNAL, TEST_LEVEL0, TEST_FUNCTION);
}