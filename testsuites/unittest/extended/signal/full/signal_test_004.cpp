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

/* child process kill msg to father , father receive msg and do func , wake up the pend thread  */
static int TestRaiseWake()
{
    int count = 0;
    int fpid;
    int fatherPid;
    int status, retValue;
    void (*retSig)(int);

    g_sigCount = 0;
    retSig = signal(SIGTERM, SigPrint);
    ICUNIT_ASSERT_NOT_EQUAL(retSig, NULL, retSig);
    fatherPid = getpid();
    fpid = fork();
    // fpid check err
    ICUNIT_ASSERT_WITHIN_EQUAL(fpid, 0, UINT_MAX, fpid);
    if (fpid == 0) {
        while (1) {
            retValue = kill(fatherPid, SIGTERM);
            if (retValue != 0) {
                exit(retValue);
            }
            usleep(100000); // 100000, Used to calculate the delay time.
            count++;
            if (count == SIG_TEST_COUNT) {
                break;
            }
        }
        exit(0);
    } else { // parent threa
        usleep(10); // 10, Used to calculate the delay time.
        while (1) {
            count++;
            usleep(3000); // 3000, Used to calculate the delay time.
            if (g_sigCount == SIG_TEST_COUNT) {
                break;
            }
            // check child process num
            ICUNIT_ASSERT_WITHIN_EQUAL(count, 0, SIG_TEST_COUNT * 30, count); // 30, assert that function Result is equal to this.
        }
        retValue = waitpid(fpid, &status, 0);
        ICUNIT_ASSERT_EQUAL(retValue, fpid, retValue);
        ICUNIT_ASSERT_EQUAL(WEXITSTATUS(status), 0, WEXITSTATUS(status));
    }
    return g_sigCount;
}

static int TestCase(void)
{
    int retValue, status;

    int fpid = fork();
    if (fpid == 0) {
        retValue = TestRaiseWake();
        if (retValue != SIG_TEST_COUNT) {
            exit(retValue);
        }
        exit(0);
    }
    retValue = waitpid(fpid, &status, 0);
    ICUNIT_ASSERT_EQUAL(retValue, fpid, retValue);
    ICUNIT_ASSERT_EQUAL(WEXITSTATUS(status), 0, WEXITSTATUS(status));
    return 0;
}

void ItPosixSignal004(void)
{
    TEST_ADD_CASE("IT_POSIX_SIGNAL_004", TestCase, TEST_POSIX, TEST_SIGNAL, TEST_LEVEL0, TEST_FUNCTION);
}
