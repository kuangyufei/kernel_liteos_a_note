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

#include "it_test_vm.h"

#define INVALID_PROCESS_ID 100000
#define MMAP_SIZE 0x1000
#define PARAM_INIT 0x123

/* Test PROT_NONE flag */
static int Testcase(void)
{
    int ret, param;
    int *ptr = NULL;
    int status = 0;
    pid_t pid = fork();
    ICUNIT_ASSERT_WITHIN_EQUAL(pid, 0, INVALID_PROCESS_ID, pid);
    if (pid == 0) {
        ptr = (int *)mmap(0, MMAP_SIZE, PROT_NONE, MAP_ANON | MAP_PRIVATE, -1, 0);
        ICUNIT_ASSERT_NOT_EQUAL(ptr, MAP_FAILED, ptr);
        printf("%d\n", *ptr);
        exit(0);
    }

    ret = waitpid(pid, &status, 0);
    ICUNIT_ASSERT_EQUAL(ret, pid, ret);
    ret = WIFEXITED(status);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = WIFSIGNALED(status);
    ICUNIT_ASSERT_EQUAL(ret, 1, ret);
    ret = WTERMSIG(status);
    ICUNIT_ASSERT_EQUAL(ret, SIGUSR2, ret);

    pid = fork();
    ICUNIT_ASSERT_WITHIN_EQUAL(pid, 0, INVALID_PROCESS_ID, pid);
    if (pid == 0) {
        ptr = (int *)mmap(0, MMAP_SIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
        ICUNIT_ASSERT_NOT_EQUAL(ptr, MAP_FAILED, ptr);
        *ptr = PARAM_INIT;
        ret = mprotect(ptr, MMAP_SIZE, PROT_READ);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);
        ICUNIT_ASSERT_EQUAL(*ptr, PARAM_INIT, *ptr);
        ret = mprotect(ptr, MMAP_SIZE, PROT_NONE);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);
        printf("%d\n", *ptr);
        exit(0);
    }

    ret = waitpid(pid, &status, 0);
    ICUNIT_ASSERT_EQUAL(ret, pid, ret);
    ret = WIFEXITED(status);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = WIFSIGNALED(status);
    ICUNIT_ASSERT_EQUAL(ret, 1, ret);
    ret = WTERMSIG(status);
    ICUNIT_ASSERT_EQUAL(ret, SIGUSR2, ret);

    return 0;
}

void ItTestMmap009(void)
{
    TEST_ADD_CASE("IT_MEM_MMAP_009", Testcase, TEST_LOS, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}

