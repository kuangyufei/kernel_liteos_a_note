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
#include "It_fs_jffs.h"

#define TEST_STR "abcdefghijk"

static int TestCase(void)
{
    INT32 ret = JFFS_IS_ERROR;
    INT32 fd = 0;
    INT32 len = 0;
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = JFFS_PATH_NAME0;
    CHAR buf[JFFS_STANDARD_NAME_LENGTH] = TEST_STR;
    CHAR str[JFFS_STANDARD_NAME_LENGTH] = "";
    FILE *ptr = NULL;
    fd = open(pathname1, O_CREAT | O_RDWR, 0777);
    ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, EXIT);
    len = write(fd, buf, JFFS_STANDARD_NAME_LENGTH);
    ICUNIT_GOTO_NOT_EQUAL(len, -1, len, EXIT);
    ret = close(fd);
    ICUNIT_GOTO_NOT_EQUAL(ret, -1, ret, EXIT1);
    ptr = freopen(pathname1, "rb", stdin);
    ICUNIT_GOTO_NOT_EQUAL(ptr, NULL, ptr, EXIT1);
    ret = scanf_s("%s", JFFS_STANDARD_NAME_LENGTH, str); 
    ICUNIT_GOTO_NOT_EQUAL(ret, 0, ret, EXIT1);
    ret = strcmp(buf, str);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);

    fclose(ptr);

    ret = unlink(pathname1);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    return JFFS_NO_ERROR;
EXIT1:
    if (fd > 0) {
        close(fd);
    }
EXIT:
    unlink(pathname1);
    return JFFS_IS_ERROR;
}

void ItTestFsJffs106(void)
{
    TEST_ADD_CASE("It_Test_Fs_Jffs_106", TestCase, TEST_VFS, TEST_JFFS, TEST_LEVEL0, TEST_FUNCTION);
}
