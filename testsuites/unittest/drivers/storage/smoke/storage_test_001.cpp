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
#include "it_test_storage.h"
#include "fcntl.h"
#include "sys/ioctl.h"

#define STORAGE_DEV_PATH "/dev/sdap0"
#define STORAGE_DATA_LEN 32
#define DIOC_GETPRIV 0x1000
#define INVALID_PTR 0x11111111

static int Testcase(VOID)
{
    int ret;
    int fd = -1;
    int i;
    ssize_t n;
    char *buf = NULL;
    void *bch = NULL;

    ret = access(STORAGE_DEV_PATH, 0);
    if (!ret) {
        fd = open(STORAGE_DEV_PATH, O_RDWR, 0666);
        ICUNIT_ASSERT_NOT_EQUAL(fd, -1, fd);

        buf = (char *)malloc(STORAGE_DATA_LEN);
        ICUNIT_ASSERT_NOT_EQUAL(buf, NULL, buf);
        (void)memset_s(buf, STORAGE_DATA_LEN, 0, STORAGE_DATA_LEN);

        ret = ioctl(fd, DIOC_GETPRIV, &bch);
        printf("bch = %#x, &bch = %#x, errno = %d\n", bch, &bch, errno);
        ICUNIT_ASSERT_NOT_EQUAL(ret, -1, ret);


        printf("Reading storage data ... \n");
        n = read(fd, buf, STORAGE_DATA_LEN);
        ICUNIT_ASSERT_EQUAL(n, STORAGE_DATA_LEN, n);
        printf("storage data [%#x %#x %#x %#x]\n", buf[0], buf[1], buf[2], buf[3]);

        n = read(fd, (void *)INVALID_PTR, STORAGE_DATA_LEN);
        ICUNIT_ASSERT_NOT_EQUAL(n, STORAGE_DATA_LEN, n);

        ret = close(fd);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);
        free(buf);
    }

    return 0;
}

void ItTestStorage001(void)
{
    TEST_ADD_CASE("IT_DRIVERS_STORAGE_001", Testcase, TEST_LOS, TEST_DRIVERBASE, TEST_LEVEL0, TEST_FUNCTION);
}
