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

static const int TEST_COUNT = 10;

static void *ThreadFunc2(void *arg)
{
    exit(254); // 254, exit args
}

static int ProcessTest001(void)
{
    int ret;
    int status;
    int pid;
    int data;
    int pri;
    pthread_t newPthread, newPthread1;
    int count = 4;
    int currProcessPri = getpriority(PRIO_PROCESS, getpid());
    ICUNIT_ASSERT_WITHIN_EQUAL(currProcessPri, 0, 31, currProcessPri); // 31, assert that function Result is equal to this.

    ret = setpriority(PRIO_PROCESS, getpid(), currProcessPri - 2); // 2, Used to calculate priorities.
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    data = ret;
    ret = pthread_create(&newPthread, NULL, ThreadFunc2, &data);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    exit(255); // 255, exit args
    return 0;
}

static int Testcase(void)
{
    int ret;
    int status;
    int pid;
    int count = TEST_COUNT;
    int temp = GetCpuCount();
    if (temp <= 1) {
        return 0;
    }

    while (count > 0) {
        ret = fork();
        if (ret == 0) {
            ret = ProcessTest001();
            exit(10); // 10, exit args
        } else if (ret > 0) {
            pid = ret;
            ret = wait(&status);
            status = WEXITSTATUS(status);
            ICUNIT_ASSERT_EQUAL(ret, pid, ret);
            ICUNIT_ASSERT_TWO_EQUAL(status, 255, 254, status); // 255, 254, assert that function Result is equal to this.
        }

        ICUNIT_ASSERT_WITHIN_EQUAL(ret, 0, 100000, ret); // 100000, assert that function Result is equal to this.
        count--;
    }
    return 0;
}

void ItTestProcess007(void)
{
    TEST_ADD_CASE("IT_POSIX_PROCESS_007", Testcase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
