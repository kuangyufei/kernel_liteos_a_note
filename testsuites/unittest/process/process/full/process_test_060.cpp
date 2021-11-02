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
#include <spawn.h>

static int TestGroupError(void)
{
    int ret;
    posix_spawnattr_t attr;

    posix_spawnattr_init(&attr);
    posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETPGROUP);
    posix_spawnattr_setpgroup(&attr, 1);
    ret = posix_spawnp(NULL, "/bin/tftp", NULL, &attr, NULL, NULL);
    ICUNIT_ASSERT_EQUAL(ret, EPERM, ret);

    posix_spawnattr_setpgroup(&attr, 65); // 65, set group num.
    ret = posix_spawnp(NULL, "/bin/tftp", NULL, &attr, NULL, NULL);
    ICUNIT_ASSERT_EQUAL(ret, EINVAL, ret);

    return 0;
}
static int TestPolError(void)
{
    int ret;
    posix_spawnattr_t attr;
    struct sched_param val = { -1 };

    posix_spawnattr_init(&attr);
    posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSCHEDULER);
    val.sched_priority = 15; // 15, set pthread priority.
    posix_spawnattr_setschedparam(&attr, &val);
    posix_spawnattr_setschedpolicy(&attr, SCHED_FIFO);
    ret = posix_spawnp(NULL, "/bin/tftp", NULL, &attr, NULL, NULL);
    ICUNIT_ASSERT_EQUAL(ret, EINVAL, ret);

    return 0;
}
static int TestPrioError(void)
{
    int ret;
    posix_spawnattr_t attr;
    struct sched_param val = { -1 };

    posix_spawnattr_init(&attr);
    posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSCHEDPARAM);
    posix_spawnattr_getschedparam(&attr, &val);

    val.sched_priority = 0;
    ret = posix_spawnattr_setschedparam(&attr, &val);
    ret = posix_spawnp(NULL, "/bin/tftp", NULL, &attr, NULL, NULL);
    ICUNIT_ASSERT_EQUAL(ret, EINVAL, ret);

    val.sched_priority = 32; // 32, set pthread priority.
    ret = posix_spawnattr_setschedparam(&attr, &val);
    ret = posix_spawnp(NULL, "/bin/tftp", NULL, &attr, NULL, NULL);
    ICUNIT_ASSERT_EQUAL(ret, EINVAL, ret);

    return 0;
}
static int TestCase(void)
{
    pid_t pid;
    posix_spawnattr_t attr;
    int status = 1;
    int ret;

    posix_spawnattr_init(&attr);

    ret = posix_spawnattr_setflags(&attr, -1);
    ICUNIT_ASSERT_EQUAL(ret, EINVAL, ret);

    ret = posix_spawnattr_setflags(&attr, 128); // 128, set flags num.
    ICUNIT_ASSERT_EQUAL(ret, EINVAL, ret);

    ret = posix_spawnattr_setflags(&attr, 0xff);
    ICUNIT_ASSERT_EQUAL(ret, EINVAL, ret);

    ret = posix_spawnattr_setschedpolicy(&attr, 0);
    ICUNIT_ASSERT_EQUAL(ret, EINVAL, ret);

    ret = posix_spawnattr_setschedpolicy(&attr, 3); // 3, set policy num.
    ICUNIT_ASSERT_EQUAL(ret, EINVAL, ret);

    posix_spawnattr_destroy(&attr);

    ret = TestGroupError();
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = TestPolError();
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = TestPrioError();
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    return 0;
}

void ItTestProcess060(void)
{
    TEST_ADD_CASE("IT_POSIX_PROCESS_060", TestCase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
