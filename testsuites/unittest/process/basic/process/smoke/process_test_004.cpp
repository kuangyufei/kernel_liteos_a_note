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

static int Testcase(void)
{
    int ret;
    int status;
    int pid;

    ret = fork();
    if (ret == 0) {
        exit(5); // 5, exit args
    } else if (ret > 0) {
        pid = ret;
        ret = waitpid(0, &status, 0);
        ICUNIT_ASSERT_EQUAL(ret, pid, ret);
        ICUNIT_ASSERT_EQUAL(WEXITSTATUS(status), 5, status); // 5, assert that function Result is equal to this.
        ICUNIT_ASSERT_EQUAL(WIFEXITED(status), 1, status);
    }
    ICUNIT_ASSERT_WITHIN_EQUAL(ret, 0, 100000, ret); // 100000, assert that function Result is equal to this.

    ret = fork();
    if (ret == 0) {
        WAIT_PARENT_FIRST_TO_RUN(1);
        exit(6); // 6, exit args
    } else if (ret > 0) {
        pid = ret;
        ret = waitpid(0, &status, 0);
        status = WEXITSTATUS(status);
        ICUNIT_ASSERT_EQUAL(ret, pid, ret);
        ICUNIT_ASSERT_EQUAL(status, 6, status); // 6, assert that function Result is equal to this.
    }

    ICUNIT_ASSERT_WITHIN_EQUAL(ret, 0, 100000, ret); // 100000, assert that function Result is equal to this.

    ret = fork();
    if (ret == 0) {
        exit(1);
    } else if (ret > 0) {
        pid = ret;
        ret = wait(&status);
        status = WEXITSTATUS(status);
        ICUNIT_ASSERT_EQUAL(ret, pid, ret);
        ICUNIT_ASSERT_EQUAL(status, 1, status);
    }

    ICUNIT_ASSERT_WITHIN_EQUAL(ret, 0, 100000, ret); // 100000, assert that function Result is equal to this.

    ret = fork();
    if (ret == 0) {
        WAIT_PARENT_FIRST_TO_RUN(1);
        exit(2); // 2, exit args
    } else if (ret > 0) {
        pid = ret;
        ret = wait(&status);
        status = WEXITSTATUS(status);
        ICUNIT_ASSERT_EQUAL(ret, pid, ret);
        ICUNIT_ASSERT_EQUAL(status, 2, status); // 2, assert that function Result is equal to this.
    }
    ICUNIT_ASSERT_WITHIN_EQUAL(ret, 0, 100000, ret); // 100000, assert that function Result is equal to this.

    ret = fork();
    if (ret == 0) {
        exit(3); // 3, exit args
    } else if (ret > 0) {
        pid = ret;
        ret = waitpid(pid, &status, 0);
        status = WEXITSTATUS(status);
        ICUNIT_ASSERT_EQUAL(ret, pid, ret);
        ICUNIT_ASSERT_EQUAL(status, 3, status); // 3, assert that function Result is equal to this.
    }
    ICUNIT_ASSERT_WITHIN_EQUAL(ret, 0, 100000, ret); // 100000, assert that function Result is equal to this.

    ret = fork();
    if (ret == 0) {
        WAIT_PARENT_FIRST_TO_RUN(1);
        exit(4); // 4, exit args
    } else if (ret > 0) {
        pid = ret;
        ret = waitpid(ret, &status, 0);
        status = WEXITSTATUS(status);
        ICUNIT_ASSERT_EQUAL(ret, pid, ret);
        ICUNIT_ASSERT_EQUAL(status, 4, status); // 4, assert that function Result is equal to this.
    }
    ICUNIT_ASSERT_WITHIN_EQUAL(ret, 0, 100000, ret); // 100000, assert that function Result is equal to this.

    return 0;
}

void ItTestProcess004(void)
{
    TEST_ADD_CASE("IT_POSIX_PROCESS_004", Testcase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
