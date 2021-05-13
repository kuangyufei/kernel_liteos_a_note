/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of
 *    conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list
 *    of conditions and the following disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used
 *    to endorse or promote products derived from this software without specific prior written
 *    permission.
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

#include "It_vfs_jffs.h"

#define TEST_STR "abcdefghijk"
#define EXPECT_TEST_STR "ghijk"
static constexpr int OFFSET_NUM = 6;

static UINT32 Testcase(VOID)
{
    INT32 ret, len;
    INT32 fd = -1;
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR buf[JFFS_STANDARD_NAME_LENGTH] = { TEST_STR };
    CHAR str[JFFS_STANDARD_NAME_LENGTH];
    FILE *ptr = NULL;
    off_t offset = OFFSET_NUM;
    size_t fret;

    fd = open(pathname1, O_CREAT | O_RDWR, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, EXIT);

    len = write(fd, buf, JFFS_STANDARD_NAME_LENGTH);
    ICUNIT_GOTO_NOT_EQUAL(len, -1, len, EXIT);

    ret = close(fd);
    ICUNIT_GOTO_NOT_EQUAL(ret, -1, ret, EXIT1);

    ptr = fopen(pathname1, "r+");
    ICUNIT_GOTO_NOT_EQUAL(ptr, NULL, ptr, EXIT2);

    ret = fseeko(ptr, offset, SEEK_SET);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);

    fret = fread(str, 1, JFFS_STANDARD_NAME_LENGTH, ptr);
    ICUNIT_GOTO_EQUAL(fret, (JFFS_STANDARD_NAME_LENGTH - OFFSET_NUM), fret, EXIT2);

    ret = strcmp(EXPECT_TEST_STR, str);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);

    ret = fclose(ptr);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);

    ret = unlink(pathname1);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    return JFFS_NO_ERROR;

EXIT2:
    fclose(ptr);
    unlink(pathname1);
    return JFFS_NO_ERROR;
EXIT1:
    close(fd);
EXIT:
    unlink(pathname1);
    return JFFS_NO_ERROR;
}

VOID ItJffs034(VOID)
{
    TEST_ADD_CASE(__FUNCTION__, Testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL2, TEST_FUNCTION);
}

