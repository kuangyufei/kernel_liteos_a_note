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
#include "It_test_IO.h"

static UINT32 Testcase(VOID)
{
    char *srcStr = "helloworld";
    int nRet, fd;
    char pathname[50]; // 50, path name size
    char *filename = "/crtreadvtest1";
    char buf1[6] = { 0 };
    char buf2[6] = { 0 };
    struct iovec iov[2]; // 2, read 2 block
    ssize_t nread;
    const int testStrLen = 10;

    iov[0].iov_base = buf1;
    iov[0].iov_len = sizeof(buf1) - 1;
    iov[1].iov_base = buf2;
    iov[1].iov_len = sizeof(buf2) - 1;

    strncpy(pathname, g_ioTestPath, 50); // 50, path name size
    strcat(pathname, filename);
    fd = open(pathname, O_CREAT | O_RDWR | O_TRUNC, 0666); // 0666, file authority
    if (fd < 0) {
        printf("error: can`t open file\n");
    }
    nRet = write(fd, srcStr, testStrLen);
    ICUNIT_GOTO_EQUAL(nRet, testStrLen, nRet, EXIT);
    close(fd);

    fd = open(pathname, O_RDWR);
    if (fd < 0) {
        printf("error: can`t open file\n");
    }
    nread = readv(fd, iov, 2); // 2, read 2 block
    ICUNIT_GOTO_EQUAL(nread, testStrLen, nread, EXIT);
    nRet = strcmp(buf2, "world");
    ICUNIT_GOTO_EQUAL(nRet, 0, nRet, EXIT);
    nRet = strcmp(buf1, "hello");
    ICUNIT_GOTO_EQUAL(nRet, 0, nRet, EXIT);

    close(fd);
    remove(pathname);
    return LOS_OK;
EXIT:
    close(fd);
    remove(pathname);
    return LOS_NOK;
}

VOID ItStdioReadv001(void)
{
    TEST_ADD_CASE(__FUNCTION__, Testcase, TEST_LIB, TEST_LIBC, TEST_LEVEL1, TEST_FUNCTION);
}
