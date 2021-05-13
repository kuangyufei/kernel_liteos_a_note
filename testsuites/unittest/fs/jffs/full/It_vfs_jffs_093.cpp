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

static constexpr int MAX_FILE_LEN = 10;

static UINT32 Testcase(VOID)
{
    INT32 fd[10] = { -1 };
    INT32 ret, i, len;
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR pathname2[10][JFFS_STANDARD_NAME_LENGTH] = {JFFS_PATH_NAME0};
    CHAR writebuf[JFFS_STANDARD_NAME_LENGTH] = "0123456789";
    CHAR readbuf[JFFS_STANDARD_NAME_LENGTH] = "";
    CHAR bufname[JFFS_SHORT_ARRAY_LENGTH] = "";
    INT32 randTest1[10] = { 2, 6, 3, 1, 0, 8, 7, 9, 4, 5 };
    INT32 randTest2[10] = { 5, 3, 1, 6, 7, 2, 4, 9, 0, 8 };

    ret = mkdir(pathname1, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    for (i = 0; i < MAX_FILE_LEN; i++) {
        memset_s(bufname, JFFS_SHORT_ARRAY_LENGTH, 0, JFFS_SHORT_ARRAY_LENGTH);
        memset_s(pathname2[i], JFFS_STANDARD_NAME_LENGTH, 0, JFFS_STANDARD_NAME_LENGTH);
        snprintf_s(bufname, JFFS_SHORT_ARRAY_LENGTH - 1, JFFS_SHORT_ARRAY_LENGTH, "/1071_%d", i);
        strcat_s(pathname2[i], JFFS_STANDARD_NAME_LENGTH, pathname1);
        strcat_s(pathname2[i], JFFS_STANDARD_NAME_LENGTH, bufname);
        fd[i] = open(pathname2[i], O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY);
        ICUNIT_GOTO_NOT_EQUAL(fd[i], -1, fd[i], EXIT2);
    }
    for (i = 0; i < MAX_FILE_LEN; i++) {
        len = write(fd[randTest1[i]], writebuf, strlen(writebuf));
        ICUNIT_GOTO_EQUAL(len, strlen(writebuf), len, EXIT2);

        ret = close(fd[randTest1[i]]);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);
    }

    for (i = 0; i < MAX_FILE_LEN; i++) {
        fd[randTest2[i]] = open(pathname2[randTest2[i]], O_CREAT | O_RDWR, HIGHEST_AUTHORITY);
        ICUNIT_GOTO_NOT_EQUAL(fd[i], -1, fd[i], EXIT2);

        len = read(fd[randTest2[i]], readbuf, JFFS_STANDARD_NAME_LENGTH - 1);
        ICUNIT_GOTO_EQUAL(len, 10, len, EXIT2); // 10 means length of actually read data

        ret = close(fd[randTest2[i]]);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);
    }

    for (i = 0; i < MAX_FILE_LEN; i++) {
        ret = remove(pathname2[randTest2[i]]);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);
    }

    ret = remove(pathname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    return JFFS_NO_ERROR;
EXIT2:
    for (i = 0; i < MAX_FILE_LEN; i++) {
        close(fd[randTest1[i]]);
    }
EXIT1:
    for (i = 0; i < MAX_FILE_LEN; i++) {
        remove(pathname2[randTest2[i]]);
    }
EXIT:
    remove(pathname1);
    return JFFS_NO_ERROR;
}

VOID ItFsJffs093(VOID)
{
    TEST_ADD_CASE("IT_FS_JFFS_093", Testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL3, TEST_FUNCTION);
}

