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

/* Sample : Father process want to free zombine child process */
void SigChildResponse(int signo)
{
    wait(nullptr);
}

/* Register SIGCHLD, through signal to restore the child memery */
static int TestSigKillWaitFromSigChild()
{
    void (*ret)(int);
    int sig = SIGINT;
    int fpid, fpids, status, retValue;
    ret = signal(sig, SIG_IGN);
    ICUNIT_ASSERT_NOT_EQUAL(ret, NULL, ret);

    ret = signal(sig, SigChildResponse);
    ICUNIT_ASSERT_NOT_EQUAL(ret, NULL, ret);
    fpid = fork();
    ICUNIT_ASSERT_WITHIN_EQUAL(fpid, 0, UINT_MAX, fpid);
    if (fpid == 0) {
        fpids = fork();
        if (fpids == 0) {
            while (1) {
                sleep(2); // 2, sleep 10 second.
            }
        }
        usleep(10000); // 10000, Used to calculate the delay time.
        retValue = kill(fpids, SIGKILL);
        if (retValue != 0) {
            exit(retValue);
        }
        retValue = waitpid(fpids, &status, 0);
        if (retValue != fpids) {
            exit(retValue);
        }
        exit(0);
    }
    retValue = waitpid(fpid, &status, 0);
    ICUNIT_ASSERT_EQUAL(retValue, fpid, retValue);
    ICUNIT_ASSERT_EQUAL(WEXITSTATUS(status), 0, WEXITSTATUS(status));

    ret = signal(sig, SIG_DFL);
    ICUNIT_ASSERT_NOT_EQUAL(ret, NULL, ret);

    return 0;
}

void ItPosixSignal008(void)
{
    TEST_ADD_CASE("IT_POSIX_SIGNAL_008", TestSigKillWaitFromSigChild, TEST_POSIX, TEST_SIGNAL, TEST_LEVEL0,
        TEST_FUNCTION);
}
