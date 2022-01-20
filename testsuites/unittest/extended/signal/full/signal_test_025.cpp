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

static void SigAlrm(int signum)
{
    printf("Alert rings.\n");
}

static int TestBlock()
{
    int sig = SIGALRM;
    void *ret;
    int retValue, status;

    int fpid = fork();
    if (fpid == 0) {
        sigset(SIGALRM, SigAlrm);
        int clock = 2;
        printf("Alarm after %d seconds\n", clock);
        alarm(clock);
        for (int i = 0; i < 4; i++) { // 4, Number of cycles
            printf("time = %d s\n", i);
            sleep(1);
        }
        printf("\n\n");

        printf("Hold the signal\n");
        retValue = sighold(sig);
        if (retValue != 0) {
            exit(retValue);
        }
        alarm(clock);
        for (int i = 0; i < 4; i++) { // 4, Number of cycles
            printf("time = %d s\n", i);
            sleep(1);
        }
        printf("\n\n");

        printf("Release the signal\n");
        retValue = sigrelse(sig);
        if (retValue != 0) {
            exit(retValue);
        }
        alarm(clock);
        for (int i = 0; i < 4; i++) { // 4, Number of cycles
            printf("time = %d s\n", i);
            sleep(1);
        }
        printf("\n\n");

        printf("Signal Pause(1)\n");
        printf("The process sleeps until alter rings (after 2 seconds)\n");
        alarm(clock);
        retValue = sigpause(sig);
        if (retValue != -1) {
            exit(retValue);
        }
        printf("\n\n");

        printf("Signal Pause(2)\n");
        printf("To test whether pause can relese the signal\n");
        alarm(clock);
        retValue = sighold(sig);
        if (retValue != 0) {
            exit(retValue);
        }
        retValue = sigpause(sig);
        if (retValue != -1) {
            exit(retValue);
        }
        printf("\n\n");

        printf("Ignore the signal\n");
        retValue = sigignore(sig);
        if (retValue != 0) {
            exit(retValue);
        }
        alarm(clock);
        for (int i = 0; i < 4; i++) { // 4, Number of cycles
            printf("time = %d s\n", i);
            sleep(1);
        }
        printf("\n\n");
        printf("Test Ends\n");

        exit(0);
    }
    retValue = waitpid(fpid, &status, 0);
    ICUNIT_ASSERT_EQUAL(retValue, fpid, retValue);
    ICUNIT_ASSERT_EQUAL(WEXITSTATUS(status), 0, WEXITSTATUS(status));
    return 0;
}

void ItPosixSignal025(void)
{
    TEST_ADD_CASE(__FUNCTION__, TestBlock, TEST_POSIX, TEST_SIGNAL, TEST_LEVEL0, TEST_FUNCTION);
}