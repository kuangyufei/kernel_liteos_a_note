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
#include "it_test_shm.h"
#include "sys/types.h"

#define SHMNAME "shm_ram"
#define OPEN_FLAG (O_RDWR | O_CREAT | O_EXCL)
#define OPEN_MODE 00777

static int Testcase(void)
{
    int fd = shm_open(SHMNAME, OPEN_FLAG, OPEN_MODE);
    ICUNIT_ASSERT_NOT_EQUAL(fd, -1, fd);

    errno = 0;
    fd = shm_open(SHMNAME, OPEN_FLAG, OPEN_MODE);
    ICUNIT_ASSERT_EQUAL(fd, -1, fd);
    ICUNIT_ASSERT_EQUAL(errno, EEXIST, errno);

    errno = 0;
    fd = shm_open("SHM_RAM", O_RDONLY, OPEN_MODE);
    ICUNIT_ASSERT_EQUAL(fd, -1, fd);
    ICUNIT_ASSERT_EQUAL(errno, ENOENT, errno);

    errno = 0;
    fd = shm_open("..../1.txt/123", OPEN_FLAG, OPEN_MODE);
    ICUNIT_ASSERT_EQUAL(fd, -1, fd);
    ICUNIT_ASSERT_EQUAL(errno, EINVAL, errno);

    errno = 0;
    fd = shm_open("SHM_RAM", OPEN_FLAG, 0);
    ICUNIT_ASSERT_NOT_EQUAL(fd, -1, fd);
    int ret = shm_unlink(SHMNAME);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = shm_unlink("SHM_RAM");
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    errno = 0;
    ret = shm_unlink("shm_ram_unlink");
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ICUNIT_ASSERT_EQUAL(errno, ENOENT, errno);

    return 0;
}

void ItTestShm012(void)
{
    TEST_ADD_CASE("IT_MEM_SHM_012", Testcase, TEST_LOS, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}

