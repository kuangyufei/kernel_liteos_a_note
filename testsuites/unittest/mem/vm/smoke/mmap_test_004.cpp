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

#define MAP_TEST_FILE "/dev/shm/test"

static int Testcase(void)
{
    char *p1 = NULL;
    char *p2 = NULL;
    char *p3 = NULL;
    int fd, pageSize;
    int ret;

    pageSize = getpagesize();
    fd = open(MAP_TEST_FILE, O_RDWR);
    ICUNIT_ASSERT_NOT_EQUAL(fd, -1, fd);

    p1 = (char *)mmap(NULL, pageSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    ICUNIT_ASSERT_NOT_EQUAL(p1, MAP_FAILED, p1);
    (void)memset_s(p1, pageSize, 0, pageSize);

    p2 = (char *)mmap(NULL, pageSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    ICUNIT_ASSERT_NOT_EQUAL(p2, MAP_FAILED, p2);
    (void)memset_s(p2, pageSize, 0, pageSize);

    ret = memcmp(p1, p2, pageSize);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    p1[0] = 1;
    ICUNIT_ASSERT_EQUAL(p2[0], 1, p2[0]);

    p2[0] = 2;
    ICUNIT_ASSERT_EQUAL(p1[0], 2, p1[0]);

    p3 = (char *)mmap(NULL, pageSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    ICUNIT_ASSERT_NOT_EQUAL(p3, MAP_FAILED, p3);
    ICUNIT_ASSERT_EQUAL(p3[0], 2, p3[0]);

    ret = munmap(p1, pageSize);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = munmap(p2, pageSize);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = munmap(p3, pageSize);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = close(fd);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    return 0;
}

void ItTestMmap004(void)
{
    TEST_ADD_CASE("IT_MEM_MMAP_004", Testcase, TEST_LOS, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
