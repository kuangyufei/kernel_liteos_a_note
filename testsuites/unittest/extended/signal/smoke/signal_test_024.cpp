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

static void OldAction(int signum)
{
    (void)signum;
    printf("Here is the old action\n");
}

static void NewAction(int signum)
{
    (void)signum;
    printf("Here is the new action\n");
}

static int TestSigaction(void)
{
    int retValue, status;
    int fpid = fork();
    if (fpid == 0) {
        struct sigaction curAction, oldAction;
        curAction.sa_handler = OldAction;
        retValue = sigemptyset(&curAction.sa_mask);
        if (retValue != 0) {
            exit(retValue);
        }
        curAction.sa_flags = 0;

        printf("Execute Signal\n");
        retValue = sigaction(SIGINT, &curAction, NULL);
        if (retValue != 0) {
            exit(retValue);
        }
        retValue = raise(SIGINT);
        if (retValue != 0) {
            exit(retValue);
        }
        printf("\n\n");

        printf("Test backup\n");
        retValue = sigaction(SIGINT, &curAction, &oldAction);
        if (retValue != 0) {
            exit(retValue);
        }
        curAction.sa_handler = NewAction;
        retValue = sigaction(SIGINT, &curAction, NULL);
        if (retValue != 0) {
            exit(retValue);
        }
        retValue = sigaction(SIGALRM, &oldAction, NULL);
        if (retValue != 0) {
            exit(retValue);
        }
        retValue = raise(SIGINT);
        if (retValue != 0) {
            exit(retValue);
        }
        retValue = raise(SIGALRM);
        if (retValue != 0) {
            exit(retValue);
        }
        printf("\n\n");

        exit(0);
    }

    retValue = waitpid(fpid, &status, 0);
    ICUNIT_ASSERT_EQUAL(retValue, fpid, retValue);
    ICUNIT_ASSERT_EQUAL(WEXITSTATUS(status), 0, WEXITSTATUS(status));

    return 0;
}

void ItPosixSignal024(void)
{
    TEST_ADD_CASE(__FUNCTION__, TestSigaction, TEST_POSIX, TEST_SIGNAL, TEST_LEVEL0, TEST_FUNCTION);
}
