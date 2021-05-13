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
#define INVALID_FD 0xFFFFFFFF
#define INVALID_VADDR 0xAFFFFFFF
#define VALIDE_ADDR 0x12000000
#define ADDR_OFFSET 0X123
#define OVER_LEN 0x40000000
#define MAP_LEN 0x1000
#define FLAG_NUM 3

static int Testcase(void)
{
    void *mem = NULL;
    void *invalueAddr = NULL;
    size_t len = MAP_LEN;
    char file[] = "/storage/testMmapEINVAL.txt";
    int flags[FLAG_NUM] = {0, MAP_ANON, MAP_PRIVATE | MAP_SHARED};
    int ret, fd;

    fd = open(file, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_ASSERT_NOT_EQUAL(fd, -1, fd);

    invalueAddr = (void *)(VALIDE_ADDR | ADDR_OFFSET);
    mem = mmap(invalueAddr, len, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0);
    ICUNIT_GOTO_EQUAL(mem, MAP_FAILED, mem, EXIT);
    ICUNIT_GOTO_EQUAL(errno, EINVAL, errno, EXIT);

    mem = mmap((void *)INVALID_VADDR, len, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0);
    ICUNIT_GOTO_EQUAL(mem, MAP_FAILED, mem, EXIT);
    ICUNIT_GOTO_EQUAL(errno, EINVAL, errno, EXIT);

    mem = mmap((void *)INVALID_VADDR, len, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0);
    ICUNIT_GOTO_NOT_EQUAL(mem, MAP_FAILED, mem, EXIT);
    ret = munmap(mem, len);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    len = OVER_LEN;
    mem = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    ICUNIT_GOTO_EQUAL(mem, MAP_FAILED, mem, EXIT);
    ICUNIT_GOTO_EQUAL(errno, EINVAL, errno, EXIT);

    len = MAP_LEN;
    mem = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, INVALID_FD);
    ICUNIT_GOTO_EQUAL(mem, MAP_FAILED, mem, EXIT);
    ICUNIT_GOTO_EQUAL(errno, EINVAL, errno, EXIT);

    len = 0;
    mem = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    ICUNIT_GOTO_EQUAL(mem, MAP_FAILED, mem, EXIT);
    ICUNIT_GOTO_EQUAL(errno, EINVAL, errno, EXIT);

    len = MAP_LEN;
    for (int i = 0; i < FLAG_NUM; i++) {
        mem = mmap(NULL, len, PROT_READ | PROT_WRITE, flags[i], fd, 0);
        ICUNIT_GOTO_EQUAL(mem, MAP_FAILED, mem, EXIT);
        ICUNIT_GOTO_EQUAL(errno, EINVAL, errno, EXIT);
    }

EXIT:
    ret = close(fd);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = remove(file);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    return 0;
}

void ItTestMmap006(void)
{
    TEST_ADD_CASE("IT_MEM_MMAP_006", Testcase, TEST_LOS, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
