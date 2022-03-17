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

static int testcase(void)
{
    int shmfd;
    int ret;
    int count;
    int pageSize = getpagesize();
    char *writebuf = NULL;
    char *readbuf = NULL;

    shmfd = shm_open("test_1", O_RDWR | O_CREAT | O_EXCL, 0644);
    ICUNIT_ASSERT_NOT_EQUAL(shmfd, -1, shmfd);

    writebuf = (char*)malloc(pageSize);
    ICUNIT_ASSERT_NOT_EQUAL(writebuf, NULL, writebuf);    
    readbuf = (char*)malloc(pageSize);
    ICUNIT_ASSERT_NOT_EQUAL(readbuf, NULL, readbuf);

    (void)memset_s(writebuf, pageSize, 0xf, pageSize);

    count = write(shmfd, writebuf, pageSize);
    ICUNIT_ASSERT_EQUAL(count, pageSize, count);

    ret = lseek(shmfd, 0, SEEK_SET);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    count = read(shmfd, readbuf, pageSize);
    ICUNIT_ASSERT_EQUAL(count, pageSize, count);
    free(readbuf);
    free(writebuf);
    close(shmfd);
    ret = shm_unlink("test_1");
    return 0;
}

void it_test_shm_013(void)
{
    TEST_ADD_CASE("IT_MEM_SHM_013", testcase, TEST_LOS, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
