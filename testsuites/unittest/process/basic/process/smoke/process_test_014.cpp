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
#include "it_test_process.h"

static int g_testThreadExit = 0;
static int g_testExit = 0;

static void *ThreadFunc(void *arg)
{
    int count = 0;

    while (count < 5) { // 5, Number of cycles.
        g_testExit++;
        count++;
        usleep(1000 * 10 * 1); // 1000, 10, Used to calculate the delay time.
    }

    return NULL;
}

static int TestCase(void)
{
    int ret;
    int status = 100;
    pthread_t newPthread;
    pid_t pid;
    int count = 0;
    g_testThreadExit = 0;
    g_testExit = 0;

    ret = pthread_create(&newPthread, NULL, ThreadFunc, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    pid = fork();
    ICUNIT_GOTO_WITHIN_EQUAL(pid, 0, 100000, pid, EXIT); // 100000, assert pid equal to this.

    if (pid == 0) {
        while (count < 10) { // 10, Number of cycles.
            count++;
            usleep(1000 * 10 * 2); // 1000, 10, 2, Used to calculate the delay time.
        }
        exit(0);
    } else {
        ret = waitpid(pid, &status, 0);
        ICUNIT_GOTO_EQUAL(ret, pid, ret, EXIT);
        status = WEXITSTATUS(status);
        ICUNIT_GOTO_EQUAL(status, 0, status, EXIT);
        ICUNIT_GOTO_EQUAL(g_testExit, 5, g_testExit, EXIT); // 5, assert status equal to this.
    }

    ret = pthread_join(newPthread, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

EXIT:
    return 0;
}

void ItTestProcess014(void)
{
    TEST_ADD_CASE("IT_POSIX_PROCESS_014", TestCase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
