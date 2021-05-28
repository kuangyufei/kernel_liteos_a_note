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

static int TestRaise()
{
    int count = 0;
    int status = 0;
    int fpid;
    int fatherPid;
    int sonPid;
    void (*ret)(int) = NULL;
    int retValue;

    g_sigCount = 0;
    ret = signal(SIGTERM, SigPrint);
    ICUNIT_ASSERT_NOT_EQUAL(ret, NULL, ret);
    retValue = raise(SIGTERM);
    ICUNIT_ASSERT_EQUAL(retValue, 0, retValue);
    fatherPid = getpid();
    fpid = fork();
    // fpid check err
    ICUNIT_ASSERT_WITHIN_EQUAL(fpid, 0, UINT_MAX, fpid);
    if (fpid == 0) {
        while (1) {
            usleep(500000); // 500000, Used to calculate the delay time.
            count++;
            if (g_sigCount == SIG_TEST_COUNT) {
                break;
            }
            // check child process num
            // 0 means count should be positive and 2 controls the upper bound of count
            if ((count) < (0) || (count) > (SIG_TEST_COUNT * 2)) {
                exit(count);
            }
        }
        exit(0);
    } else { // parent threa
        sonPid = fpid;
        usleep(10); // 10, Used to calculate the delay time.
        while (1) {
            retValue = kill(sonPid, SIGTERM);

            sleep(1);
            count++;
            if (count == SIG_TEST_COUNT) {
                break;
            }
        }
        retValue = waitpid(sonPid, &status, 0);
        ICUNIT_ASSERT_EQUAL(retValue, sonPid, retValue);
        ICUNIT_ASSERT_EQUAL(WEXITSTATUS(status), 0, WEXITSTATUS(status));
    }
    return g_sigCount;
}

static int TestCase(void)
{
    int count = TestRaise();
    // count should be 1, for global variables are not shared between different processors
    ICUNIT_ASSERT_EQUAL(count, 1, count);
    return 0;
}

void ItPosixSignal003(void)
{
    TEST_ADD_CASE("IT_POSIX_SIGNAL_003", TestCase, TEST_POSIX, TEST_SIGNAL, TEST_LEVEL0, TEST_FUNCTION);
}
