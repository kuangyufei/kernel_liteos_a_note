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

static UINT32 Testcase(VOID)
{
    INT32 fd[5] = { -1 };
    INT32 ret, len;
    CHAR filebuf[20] = "1234567890";
    CHAR readbuf[100] = { 0 };
    CHAR pathname[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    off_t off;

    fd[0] = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR, HIGHEST_AUTHORITY); // 0 means first fd
    ICUNIT_GOTO_NOT_EQUAL(fd[0], -1, fd[0], EXIT); // 0 means first fd

    strcat_s(pathname, sizeof(pathname), "0");
    fd[1] = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR, HIGHEST_AUTHORITY); // 1 means second fd
    ICUNIT_GOTO_NOT_EQUAL(fd[1], -1, fd[1], EXIT1); // 1 means second fd

    strcat_s(pathname, sizeof(pathname), "1");
    fd[2] = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR, HIGHEST_AUTHORITY); // 2 means third fd
    ICUNIT_GOTO_NOT_EQUAL(fd[2], -1, fd[2], EXIT2); // 2 means third fd

    strcat_s(pathname, sizeof(pathname), "2");
    fd[3] = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR, HIGHEST_AUTHORITY); // 3 means four fd
    ICUNIT_GOTO_NOT_EQUAL(fd[3], -1, fd[3], EXIT3); // 3 means four fd

    strcat_s(pathname, sizeof(pathname), "3");
    fd[4] = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR, HIGHEST_AUTHORITY); // 4 means fifth fd
    ICUNIT_GOTO_NOT_EQUAL(fd[4], -1, fd[4], EXIT4); // 4 means fifth fd

    len = write(fd[0], filebuf, strlen(filebuf)); // 0 means first fd
    ICUNIT_GOTO_EQUAL(len, JFFS_SHORT_ARRAY_LENGTH, len, EXIT4);

    off = lseek(fd[0], 0, SEEK_SET); // 0 means first fd
    ICUNIT_GOTO_NOT_EQUAL(off, -1, off, EXIT4);

    len = read(fd[0], readbuf, JFFS_STANDARD_NAME_LENGTH); // 0 means first fd
    ICUNIT_GOTO_EQUAL(len, JFFS_SHORT_ARRAY_LENGTH, len, EXIT4);
    ICUNIT_GOTO_STRING_EQUAL(readbuf, filebuf, readbuf, EXIT4);

    len = write(fd[1], filebuf, strlen(filebuf)); // 1 means second fd
    ICUNIT_GOTO_EQUAL(len, JFFS_SHORT_ARRAY_LENGTH, len, EXIT4);

    off = lseek(fd[1], 0, SEEK_SET); // 1 means second fd
    ICUNIT_GOTO_NOT_EQUAL(off, -1, off, EXIT4);

    len = read(fd[1], readbuf, JFFS_STANDARD_NAME_LENGTH); // 1 means second fd
    ICUNIT_GOTO_EQUAL(len, JFFS_SHORT_ARRAY_LENGTH, len, EXIT4);
    ICUNIT_GOTO_STRING_EQUAL(readbuf, filebuf, readbuf, EXIT4);

    len = write(fd[2], filebuf, strlen(filebuf)); // 2 means third fd
    ICUNIT_GOTO_EQUAL(len, JFFS_SHORT_ARRAY_LENGTH, len, EXIT4);

    off = lseek(fd[2], 0, SEEK_SET); // 2 means third fd
    ICUNIT_GOTO_NOT_EQUAL(off, -1, off, EXIT4);

    len = read(fd[2], readbuf, JFFS_STANDARD_NAME_LENGTH); // 2 means third fd
    ICUNIT_GOTO_EQUAL(len, JFFS_SHORT_ARRAY_LENGTH, len, EXIT4);
    ICUNIT_GOTO_STRING_EQUAL(readbuf, filebuf, readbuf, EXIT4);

    len = write(fd[3], filebuf, strlen(filebuf)); // 3 means four fd
    ICUNIT_GOTO_EQUAL(len, JFFS_SHORT_ARRAY_LENGTH, len, EXIT4);

    off = lseek(fd[3], 0, SEEK_SET); // 3 means four fd
    ICUNIT_GOTO_NOT_EQUAL(off, -1, off, EXIT4);

    len = read(fd[3], readbuf, JFFS_STANDARD_NAME_LENGTH); // 3 means four fd
    ICUNIT_GOTO_EQUAL(len, JFFS_SHORT_ARRAY_LENGTH, len, EXIT4);
    ICUNIT_GOTO_STRING_EQUAL(readbuf, filebuf, readbuf, EXIT4);

    len = write(fd[4], filebuf, strlen(filebuf)); // 4 means fifth fd
    ICUNIT_GOTO_EQUAL(len, JFFS_SHORT_ARRAY_LENGTH, len, EXIT4);

    off = lseek(fd[4], 0, SEEK_SET); // 4 means fifth fd
    ICUNIT_GOTO_NOT_EQUAL(off, -1, off, EXIT4);

    len = read(fd[4], readbuf, JFFS_STANDARD_NAME_LENGTH); // 4 means fifth fd
    ICUNIT_GOTO_EQUAL(len, JFFS_SHORT_ARRAY_LENGTH, len, EXIT4);
    ICUNIT_GOTO_STRING_EQUAL(readbuf, filebuf, readbuf, EXIT4);

EXIT4:
    JffsStrcat2(pathname, "0123", sizeof(pathname));
    close(fd[4]); // 4 means fifth fd
    remove(pathname);
EXIT3:
    JffsStrcat2(pathname, "012", sizeof(pathname));
    close(fd[3]); // 3 means four fd
    remove(pathname);
EXIT2:
    JffsStrcat2(pathname, "01", sizeof(pathname));
    close(fd[2]); // 2 means third fd
    remove(pathname);
EXIT1:
    JffsStrcat2(pathname, "0", sizeof(pathname));
    close(fd[1]); // 1 means second fd
    remove(pathname);
EXIT:
    close(fd[0]); // 0 means first fd
    remove(JFFS_PATH_NAME0);
    return JFFS_NO_ERROR;
}

VOID ItFsJffs013(VOID)
{
    TEST_ADD_CASE("IT_FS_JFFS_013", Testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL2, TEST_FUNCTION);
}

