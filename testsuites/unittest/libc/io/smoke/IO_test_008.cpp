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

static INT32 Vdpfunc(int fd, char *format, ...)
{
    va_list aptr;
    INT32 ret;

    va_start(aptr, format);
    ret = vdprintf(fd, format, aptr);
    va_end(aptr);

    return ret;
}

static UINT32 Testcase(VOID)
{
    CHAR file[30] = "/jffs0/vdprintf";
    CHAR *str1 = "123456789";
    CHAR buffer[20];
    INT32 ret, fd;
    CHAR str[20] = "789";
    const int testBufLen = 20;

    fd = open(file, O_CREAT | O_RDWR | O_TRUNC);
    ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, EXIT);

    ret = Vdpfunc(fd, "%s%s", "123456", str);
    if (ret < 0) {
        goto EXIT1;
    }

    ret = close(fd);

    fd = open(file, O_RDWR);
    ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, EXIT1);

    read(fd, buffer, testBufLen);
    close(fd);

    ICUNIT_GOTO_STRING_EQUAL(buffer, str1, buffer, EXIT2);

    unlink(file);
    return (0);
EXIT1:
    close(fd);
EXIT2:
    unlink(file);
EXIT:
    return -1;
}

VOID ItTestIo008(void)
{
    TEST_ADD_CASE(__FUNCTION__, Testcase, TEST_LIB, TEST_LIBC, TEST_LEVEL1, TEST_FUNCTION);
}
