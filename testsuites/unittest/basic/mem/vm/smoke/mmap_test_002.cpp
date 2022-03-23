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

#define MAP_RESERVED0080 0x0080

static int CheckedMmap(int prot, int flags, int fd)
{
    void *p = NULL;
    int pageSize = getpagesize();
    int ret;

    if (pageSize < 0) {
        printf("err: mmap size invalid\n");
        return -1;
    }
    p = mmap(NULL, pageSize, prot, flags, fd, 0);
    if (p == MAP_FAILED) {
        return errno;
    } else {
        ret = munmap(p, pageSize);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);
        return 0;
    }
}

static int Testcase(void)
{
    int shmfd;
    int ret;
    int count;
    int pageSize = getpagesize();
    char *buf = NULL;

    shmfd = shm_open("/test", O_RDWR | O_CREAT, 0644);
    ICUNIT_ASSERT_NOT_EQUAL(shmfd, -1, shmfd);

    if (pageSize <= 0) {
        printf("err: malloc size invalid\n");
        return -1;
    }
    if (pageSize <= 0) {
        printf("err: malloc size invalid\n");
        return -1;
    }
    buf = (char *)malloc(pageSize);
    ICUNIT_ASSERT_NOT_EQUAL(buf, NULL, buf);
    (void)memset_s(buf, pageSize, 0xf, pageSize);

    count = write(shmfd, buf, pageSize);
    if (count != pageSize) {
        free(buf);
        ICUNIT_ASSERT_EQUAL(count, pageSize, count);
    }
    free(buf);

    /* Simple MAP_ANONYMOUS */
    ret = CheckedMmap(PROT_READ | PROT_WRITE, MAP_ANONYMOUS, -1);
    ICUNIT_ASSERT_EQUAL(ret, EINVAL, ret);

    /* Simple shm fd shared */
    ret = CheckedMmap(PROT_READ | PROT_WRITE, MAP_SHARED, shmfd);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    /* Simple shm fd private */
    ret = CheckedMmap(PROT_READ | PROT_WRITE, MAP_PRIVATE, shmfd);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    /* MAP_ANONYMOUS with extra PROT flags */
    ret = CheckedMmap(PROT_READ | PROT_WRITE | 0x100000, MAP_ANONYMOUS, -1);
    ICUNIT_ASSERT_EQUAL(ret, EINVAL, ret);

    /* Shm fd with garbage PROT */
    ret = CheckedMmap(0xffff, MAP_SHARED, shmfd);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    /* Undefined flag. */
    ret = CheckedMmap(PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_RESERVED0080, -1);
    ICUNIT_ASSERT_EQUAL(ret, EINVAL, ret);

    /* Both MAP_SHARED and MAP_PRIVATE */
    ret = CheckedMmap(PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE | MAP_SHARED, -1);
    ICUNIT_ASSERT_EQUAL(ret, EINVAL, ret);

    /* Shm fd with both SHARED and PRIVATE */
    ret = CheckedMmap(PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_SHARED, shmfd);
    ICUNIT_ASSERT_EQUAL(ret, EINVAL, ret);

    /* At least one of MAP_SHARED or MAP_PRIVATE without ANON */
    ret = CheckedMmap(PROT_READ | PROT_WRITE, 0, shmfd);
    ICUNIT_ASSERT_EQUAL(ret, EINVAL, ret);

    /* MAP_ANONYMOUS with either sharing flag (impacts fork). */
    ret = CheckedMmap(PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = CheckedMmap(PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = CheckedMmap(PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, 0);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = close(shmfd);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    return 0;
}

void ItTestMmap002(void)
{
    TEST_ADD_CASE("IT_MEM_MMAP_002", Testcase, TEST_LOS, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
