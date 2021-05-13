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

#define MMAP_SIZE 0x1000
#define PARAM_INIT 0x123

/* Test PROT_WRITE and PROT_EXEC flag only */
static int Testcase(void)
{
    int ret;
    int *ptr = NULL;

    ptr = (int *)mmap(0, MMAP_SIZE, PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    ICUNIT_ASSERT_NOT_EQUAL(ptr, MAP_FAILED, ptr);
    *ptr = PARAM_INIT;
    ICUNIT_ASSERT_EQUAL(*ptr, PARAM_INIT, *ptr);
    ret = munmap(ptr, MMAP_SIZE);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ptr = (int *)mmap(0, MMAP_SIZE, PROT_EXEC, MAP_ANON | MAP_PRIVATE, -1, 0);
    ICUNIT_ASSERT_NOT_EQUAL(ptr, MAP_FAILED, ptr);
    ICUNIT_ASSERT_EQUAL(*ptr, 0, *ptr);
    ret = munmap(ptr, MMAP_SIZE);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    return 0;
}

void ItTestMmap010(void)
{
    TEST_ADD_CASE("IT_MEM_MMAP_010", Testcase, TEST_LOS, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}

