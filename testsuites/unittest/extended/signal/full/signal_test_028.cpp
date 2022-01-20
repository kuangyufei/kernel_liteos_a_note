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

// print the sigset bit by biy
static void PrintSigset(sigset_t *set)
{
    for (int i = 0; i < 32; ++i) { // 32, Number of cycles
        if (sigismember(set, i)) {
            putchar('1');
        } else {
            putchar('0');
        }
    }
    puts("");
}

static int TestSigset()
{
    // record mask words
    sigset_t sigset;

    void *ret;
    int retValue, status;

    int fpid = fork();
    if (fpid == 0) {
        // clean the signal set
        printf("EmptySet\n");
        retValue = sigemptyset(&sigset);
        if (retValue != 0) {
            exit(retValue);
        }
        printf("Current set is: \n");
        PrintSigset(&sigset);
        printf("EmptySet End\n\n");

        printf("IsEmptySet\n");
        retValue = sigisemptyset(&sigset);
        if (retValue != 1) {
            exit(retValue);
        }
        if (retValue == 1) {
            printf("The set is empty\n");
        } else {
            printf("The set is not empty\n");
        }
        printf("IsEmptySet End\n\n");

        printf("AddSet\n");
        retValue = sigaddset(&sigset, SIGINT);
        if (retValue != 0) {
            exit(retValue);
        }
        printf("Add SIGINT\n");
        printf("Current set is: \n");
        PrintSigset(&sigset);
        printf("AddSet End\n\n");

        printf("IsMember\n");
        retValue = sigismember(&sigset, SIGINT);
        if (retValue != 1) {
            exit(retValue);
        }
        if (retValue == 1) {
            printf("The SIGINT is a member of set\n");
        } else {
            printf("The SIGINT is not a member of set\n");
        }
        printf("IsMember End\n\n");

        printf("DelSet\n");
        retValue = sigdelset(&sigset, SIGINT);
        if (retValue != 0) {
            exit(retValue);
        }
        printf("Delete signal SIGINT\n");
        printf("Current set is: \n");
        PrintSigset(&sigset);
        printf("DelSet End\n\n");

        printf("FillSet\n");
        retValue = sigfillset(&sigset);
        if (retValue != 0) {
            exit(retValue);
        }
        printf("Current set is: \n");
        PrintSigset(&sigset);
        printf("FillSet End\n\n");

        sigset_t sigsetTmp;
        sigemptyset(&sigsetTmp);
        sigset_t sigsetTarget;
        sigfillset(&sigset);
        printf("AndSet\n");
        retValue = sigandset(&sigsetTarget, &sigset, &sigsetTmp);
        if (retValue != 0) {
            exit(retValue);
        }
        printf("And fullset with emptyset\n");
        printf("Current set is: \n");
        PrintSigset(&sigset);
        if (sigisemptyset(&sigsetTarget)) {
            printf("AndSet Succeed\n");
        } else {
            printf("AndSet Fail\n");
        }
        printf("AndSet End\n\n");

        sigemptyset(&sigsetTarget);
        printf("OrSet\n");
        retValue = sigorset(&sigsetTarget, &sigset, &sigsetTmp);
        if (retValue != 0) {
            exit(retValue);
        }
        printf("Or fullset with emptyset\n");
        printf("Current set is: \n");
        PrintSigset(&sigset);
        if (!sigisemptyset(&sigsetTarget)) {
            printf("OrSet Succeed\n");
        } else {
            printf("OrSet Fail\n");
        }
        printf("OrSet End\n\n");

        exit(0);
    }

    retValue = waitpid(fpid, &status, 0);
    ICUNIT_ASSERT_EQUAL(retValue, fpid, retValue);
    ICUNIT_ASSERT_EQUAL(WEXITSTATUS(status), 0, WEXITSTATUS(status));

    return 0;
}

void ItPosixSignal028(void)
{
    TEST_ADD_CASE(__FUNCTION__, TestSigset, TEST_POSIX, TEST_SIGNAL, TEST_LEVEL0, TEST_FUNCTION);
}