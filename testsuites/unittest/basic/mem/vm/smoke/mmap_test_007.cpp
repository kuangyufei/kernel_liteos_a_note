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

#define MAP_LEN 0x200000
#define MAP_ADDR 0x1200000
#define MAP_OFFSET 0x100000

static int Testcase(void)
{
    size_t len = MAP_LEN;
    unsigned long fixAddr = MAP_ADDR;
    unsigned long before, after;
    size_t shinkLen;
    unsigned int flags = MAP_ANON | MAP_SHARED | MAP_FIXED;
    void *mem = NULL;
    void *prev = NULL;
    void *next = NULL;
    void *belong = NULL;
    int ret;

    mem = mmap((void *)fixAddr, len, PROT_READ | PROT_WRITE, flags, -1, 0);
    ICUNIT_GOTO_NOT_EQUAL(mem, MAP_FAILED, mem, EXIT);

    before = fixAddr - MAP_OFFSET;
    prev = mmap((void *)before, len, PROT_READ | PROT_WRITE, flags, -1, 0);
    ICUNIT_GOTO_NOT_EQUAL(prev, MAP_FAILED, prev, EXIT1);

    after = fixAddr + MAP_OFFSET;
    next = mmap((void *)after, len, PROT_READ | PROT_WRITE, flags, -1, 0);
    ICUNIT_GOTO_NOT_EQUAL(next, MAP_FAILED, next, EXIT2);

    shinkLen = len - MAP_OFFSET;
    belong = mmap((void *)after, shinkLen, PROT_READ | PROT_WRITE, flags, -1, 0);
    ICUNIT_GOTO_NOT_EQUAL(belong, MAP_FAILED, belong, EXIT3);

    ret = munmap(prev, len);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = munmap(next, len);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    return LOS_OK;

EXIT1:
    ret = munmap(mem, len);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    return LOS_NOK;
EXIT2:
    ret = munmap(mem, len);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = munmap(prev, len);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    return LOS_NOK;
EXIT3:
    ret = munmap(prev, len);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = munmap(next, len);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
EXIT:
    return LOS_NOK;
}

void ItTestMmap007(void)
{
    TEST_ADD_CASE("IT_MEM_MMAP_007", Testcase, TEST_LOS, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
