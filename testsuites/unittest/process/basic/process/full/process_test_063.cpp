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

static const int FILE_NAME_BYTES = 200; // 200, set test name len.
static const int LONG_FILE_NAME_BYTES = 500; // 500, set test name len.
static const int RANDOM_MAX = 127; // 127, set random max.
static const unsigned int INVALID_USER_VADDR = 0x1200000;

static int GetRandomNumber(int max)
{
    int n = 0;

    if (max > 0) {
        n = rand() % max;
    }

    return n + 1;
}

static void GetRandomData(char **buf, int bufSize)
{
    char *p = *buf;
    int i;

    srand((unsigned)time(0));
    for (i = 0; i < bufSize - 1; ++i) {
        int r = GetRandomNumber(RANDOM_MAX);
        *(p + i) = (char)r;
    }
    *(p + i) = (char)0;
}

static int TestCase(VOID)
{
    int ret;
    int err;
    pid_t pid;
    char *fileName = NULL;
    char *childFileName = NULL;
    char **childArgv = NULL;
    char **childEnvp = NULL;

    ret = posix_spawn(&pid, NULL, NULL, NULL, NULL, NULL);
    ICUNIT_ASSERT_EQUAL(ret, EINVAL, ret);

    childFileName = (char *)1;
    ret = posix_spawn(&pid, childFileName, NULL, NULL, NULL, NULL);
    ICUNIT_ASSERT_EQUAL(ret, EINVAL, ret);

    childArgv = (char **)1;
    ret = posix_spawn(&pid, "/usr/bin/testsuits_app", NULL, NULL, childArgv, NULL);
    ICUNIT_ASSERT_EQUAL(ret, EINVAL, ret);

    childEnvp = (char **)1;
    ret = posix_spawn(&pid, "/usr/bin/testsuits_app", NULL, NULL, NULL, childEnvp);
    ICUNIT_ASSERT_EQUAL(ret, EINVAL, ret);

    ret = posix_spawn(&pid, "/bin", NULL, NULL, NULL, NULL);
    ICUNIT_ASSERT_EQUAL(ret, ENOENT, ret);

    fileName = (char *)malloc(FILE_NAME_BYTES);
    ICUNIT_ASSERT_NOT_EQUAL(fileName, NULL, fileName);
    GetRandomData(&fileName, FILE_NAME_BYTES);
    ret = posix_spawn(&pid, fileName, NULL, NULL, NULL, NULL);
    free(fileName);
    ICUNIT_ASSERT_EQUAL(ret, ENOENT, ret);

    fileName = (char *)malloc(LONG_FILE_NAME_BYTES);
    ICUNIT_ASSERT_NOT_EQUAL(fileName, NULL, fileName);
    GetRandomData(&fileName, LONG_FILE_NAME_BYTES);
    ret = posix_spawn(&pid, fileName, NULL, NULL, NULL, NULL);
    free(fileName);
    ICUNIT_ASSERT_EQUAL(ret, ENAMETOOLONG, ret);

    ret = posix_spawn(&pid, (char *)INVALID_USER_VADDR, NULL, NULL, NULL, NULL);
    ICUNIT_ASSERT_EQUAL(ret, EFAULT, ret);

    return 0;
}
void ItTestProcess063(void)
{
    TEST_ADD_CASE("IT_POSIX_PROCESS_063", TestCase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}