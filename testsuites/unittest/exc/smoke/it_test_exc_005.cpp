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
#include "it_test_exc.h"

static void Child(void)
{
    while (1) {
        printf("@@@@@@@@@@@@@ pid : %d getppid : %d @@@@@@@@@@@@@@@@\n", getpid(), getppid());
    }
}
static void TestKill(int sig)
{
    exit(0);
}

static int TestCase(void)
{
    int ret;
    void (*retptr)(int) = NULL;
    int *test = NULL;
    int status = 0;
    pid_t pid1;

    retptr = signal(SIGTERM, TestKill);
    ICUNIT_ASSERT_NOT_EQUAL(retptr, NULL, retptr);

    pid_t pid = fork();
    ICUNIT_ASSERT_WITHIN_EQUAL(pid, 0, INVALID_PROCESS_ID, pid);
    if (pid == 0) {
        Child();
        exit(0);
    }

    for (int i = 0; i < 2; i++) {
        pid1 = fork();
        ICUNIT_ASSERT_WITHIN_EQUAL(pid1, 0, INVALID_PROCESS_ID, pid1);
        if (pid1 == 0) {
            *test = 0x1;
            exit(0);
        } else {
            ret = waitpid(pid1, &status, 0);
            ICUNIT_ASSERT_EQUAL(ret, pid1, ret);
        }
    }

    kill(pid, SIGTERM);

    ret = waitpid(pid, &status, 0);
    ICUNIT_ASSERT_EQUAL(ret, pid, ret);

    return 0;
}

void ItTestExc005(void)
{
    TEST_ADD_CASE("IT_TEST_EXC_005", TestCase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
