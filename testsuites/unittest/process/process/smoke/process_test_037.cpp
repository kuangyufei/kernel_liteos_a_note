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

static int Child1(int currGid)
{
    int ret;

    ICUNIT_ASSERT_EQUAL(currGid, getpgrp(), getpgrp());

    ret = setpgrp();
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ICUNIT_ASSERT_EQUAL(getpid(), getpgrp(), getpgrp());

    usleep(1000 * 10 * 12); // 1000, 10, 12, Used to calculate the delay time.

    exit(255); // 255, exit args
}

static int Child2(int currGid, int gid)
{
    int ret;

    ICUNIT_ASSERT_EQUAL(currGid, getpgrp(), getpgrp());

    ret = setpgid(getpid(), getpid());
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ICUNIT_ASSERT_EQUAL(getpgrp(), getpid(), getpgrp());

    usleep(1000 * 10 * 12); // 1000, 10, 12, Used to calculate the delay time.

    ICUNIT_ASSERT_EQUAL(getpgrp(), gid, getpgrp());

    exit(255); // 255, exit args
}

static int ProcessGroup(void)
{
    int ret;
    int status = 0;
    pid_t pid, pid1;
    int currGid = getpgrp();
    ret = setpgrp();
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    currGid = getpgrp();

    pid = fork();
    ICUNIT_GOTO_WITHIN_EQUAL(pid, 0, 100000, pid, EXIT); // 100000, assert pid equal to this.

    if (pid == 0) {
        Child1(currGid);
        exit(g_iCunitErrLineNo);
    }

    usleep(1000 * 10 * 2); // 1000, 10, 2, Used to calculate the delay time.

    pid1 = fork();
    ICUNIT_GOTO_WITHIN_EQUAL(pid1, 0, 100000, pid1, EXIT); // 100000, assert pid1 equal to this.
    ICUNIT_GOTO_NOT_EQUAL(pid1, pid, pid1, EXIT);

    if (pid1 == 0) {
        Child2(currGid, pid);
        exit(g_iCunitErrLineNo);
    }

    usleep(1000 * 10 * 2); // 1000, 10, 2, Used to calculate the delay time.

    ret = setpgid(pid1, pid);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = waitpid(pid, &status, 0);
    ICUNIT_ASSERT_EQUAL(ret, pid, ret);
    status = WEXITSTATUS(status);
    ICUNIT_GOTO_EQUAL(status, 255, status, EXIT); // 255, assert status equal to this.

    status = 0;
    ret = waitpid(pid1, &status, 0);
    ICUNIT_ASSERT_EQUAL(ret, pid1, ret);
    status = WEXITSTATUS(status);
    ICUNIT_GOTO_EQUAL(status, 255, status, EXIT); // 255, assert status equal to this.

    exit(255); // 255, exit args
EXIT:
    return 0;
}

static int TestCase(void)
{
    int ret;
    int status = 0;
    pid_t pid, pid1;
    int currGid = getpgrp();
    pid = fork();
    ICUNIT_GOTO_WITHIN_EQUAL(pid, 0, 100000, pid, EXIT); // 100000, assert pid equal to this.

    if (pid == 0) {
        ProcessGroup();
        exit(g_iCunitErrLineNo);
    }

    ret = waitpid(pid, &status, 0);
    ICUNIT_ASSERT_EQUAL(ret, pid, ret);
    status = WEXITSTATUS(status);
    ICUNIT_GOTO_EQUAL(status, 255, status, EXIT); // 255, assert status equal to this.

    return 0;
EXIT:
    return 1;
}

void ItTestProcess037(void)
{
    TEST_ADD_CASE("IT_POSIX_PROCESS_037", TestCase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
