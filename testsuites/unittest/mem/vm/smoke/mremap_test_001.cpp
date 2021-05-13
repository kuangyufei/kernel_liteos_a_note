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

static int Testcase(void)
{
    char *p = NULL;
    void *newAddr = NULL;
    int pageSize;
    int size;
    int ret;

    pageSize = getpagesize();
    size = pageSize << 1;

    p = (char *)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    ICUNIT_ASSERT_NOT_EQUAL(p, MAP_FAILED, p);

    /* Parameter check */
    newAddr = mremap(0, 0, 0, 0, 0);
    ICUNIT_ASSERT_EQUAL(newAddr, MAP_FAILED, newAddr);
    ICUNIT_ASSERT_EQUAL(errno, EINVAL, errno);

    newAddr = mremap(p, size, size, MREMAP_FIXED, 0);
    ICUNIT_ASSERT_EQUAL(newAddr, MAP_FAILED, newAddr);
    ICUNIT_ASSERT_EQUAL(errno, EINVAL, errno);

    /* Shrink a region */
    newAddr = mremap(p, size, pageSize, MREMAP_MAYMOVE, 0);
    ICUNIT_ASSERT_EQUAL(newAddr, p, newAddr);

    /* Remap a region by same size */
    newAddr = mremap(p, pageSize, pageSize, MREMAP_MAYMOVE, 0);
    ICUNIT_ASSERT_EQUAL(newAddr, p, newAddr);

    /* Expand a region */
    newAddr = mremap(p, pageSize, size, MREMAP_MAYMOVE, 0);
    ICUNIT_ASSERT_EQUAL(newAddr, p, newAddr);

    /* New region overlaping with the old one */
    newAddr = mremap(p, size, pageSize, MREMAP_MAYMOVE | MREMAP_FIXED, p + pageSize);
    ICUNIT_ASSERT_EQUAL(newAddr, MAP_FAILED, newAddr);
    ICUNIT_ASSERT_EQUAL(errno, EINVAL, errno);

    newAddr = mremap(p, size, size, MREMAP_MAYMOVE | MREMAP_FIXED, p - pageSize);
    ICUNIT_ASSERT_EQUAL(newAddr, MAP_FAILED, newAddr);
    ICUNIT_ASSERT_EQUAL(errno, EINVAL, errno);

    newAddr = mremap(p, size, size + pageSize, MREMAP_MAYMOVE | MREMAP_FIXED, p - pageSize);
    ICUNIT_ASSERT_EQUAL(newAddr, MAP_FAILED, newAddr);
    ICUNIT_ASSERT_EQUAL(errno, EINVAL, errno);

    /* Remap to new addr */
    newAddr = mremap(p, size, size, MREMAP_MAYMOVE | MREMAP_FIXED, p + size);
    ICUNIT_ASSERT_EQUAL(newAddr, p + size, newAddr);

    newAddr = mremap(newAddr, size, size, MREMAP_MAYMOVE | MREMAP_FIXED, (char *)newAddr - size);
    ICUNIT_ASSERT_EQUAL(newAddr, p, newAddr);

    ret = munmap(p, size);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    return 0;
}

void ItTestMremap001(void)
{
    TEST_ADD_CASE("IT_MEM_MREMAP_001", Testcase, TEST_LOS, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
