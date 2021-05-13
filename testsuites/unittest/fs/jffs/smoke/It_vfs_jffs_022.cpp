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

static UINT32 testcase(VOID)
{
    INT32 fd1, fd2, ret, len;
    CHAR pathname1[50] = { JFFS_PATH_NAME0 };
    CHAR pathname2[50] = { JFFS_PATH_NAME0 };
    CHAR buffile1[50] = { JFFS_PATH_NAME0 };
    CHAR buffile2[50] = { JFFS_PATH_NAME0 };
    CHAR filebuf[20] = "1234567890";
    CHAR readbuf[20] = {0};
    off_t off;

    ret = mkdir(pathname1, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    strcat(pathname1, "/dir1");
    ret = mkdir(pathname1, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);

    strcat(pathname2, "/dir2");
    ret = mkdir(pathname2, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);

    strcat(buffile1, "/dir1/file1");
    fd1 = open(buffile1, O_NONBLOCK | O_CREAT | O_RDWR, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd1, -1, fd1, EXIT6);

    len = write(fd1, filebuf, strlen(filebuf));
    ICUNIT_GOTO_EQUAL(len, strlen(filebuf), len, EXIT6);

    strcat(buffile2, "/dir2/file2");
    fd2 = open(buffile2, O_NONBLOCK | O_CREAT | O_RDWR, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd2, -1, fd2, EXIT4);

    off = lseek(fd2, 0, SEEK_SET);
    ICUNIT_GOTO_NOT_EQUAL(off, -1, off, EXIT8);

    len = read(fd2, readbuf, 20); // read length: 20
    ICUNIT_GOTO_EQUAL(len, 0, len, EXIT8);

    off = lseek(fd1, 0, SEEK_SET);
    ICUNIT_GOTO_NOT_EQUAL(off, -1, off, EXIT8); // file position: 20

    len = read(fd1, readbuf, 20); // read length: 20
    ICUNIT_GOTO_EQUAL(len, strlen(filebuf), len, EXIT8);
    ICUNIT_GOTO_STRING_EQUAL(readbuf, filebuf, readbuf, EXIT8);

    ret = close(fd1); // É¾³ý³É¹¦"/dir2/file2"
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT8);

    ret = close(fd2);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT8);

    ret = rename(buffile1, buffile2);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT4);

    ret = rmdir(pathname1); // dir1É¾³ý³É¹¦
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT8);

    fd2 = open(buffile2, O_NONBLOCK | O_RDWR, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd2, -1, fd2, EXIT4);

    len = read(fd2, readbuf, 20); // read length: 20
    ICUNIT_GOTO_EQUAL(len, strlen(filebuf), len, EXIT4);
    ICUNIT_GOTO_STRING_EQUAL(readbuf, "1234567890", readbuf, EXIT4);

    ret = close(fd2);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT4);

    ret = unlink(buffile2);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT4);

    ret = rmdir(pathname2);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);

    ret = rmdir(JFFS_PATH_NAME0);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    return JFFS_NO_ERROR;
EXIT8:
    close(fd2);
EXIT7:
    JffsStrcat2(pathname1, "/dir2/file2", strlen(pathname1));
    remove(pathname1);
EXIT6:
    close(fd1);
EXIT5:
    JffsStrcat2(pathname1, "/dir1/file1", strlen(pathname1));
    remove(pathname1);
    goto EXIT2;
EXIT4:
    close(fd2);
EXIT3:
    JffsStrcat2(pathname1, "/dir2/file2", strlen(pathname1));
    remove(pathname1);
    goto EXIT6;
EXIT2:
    JffsStrcat2(pathname1, "/dir2", strlen(pathname1));
    remove(pathname1);
EXIT1:
    JffsStrcat2(pathname1, "dir1", strlen(pathname1));
    remove(pathname1);
EXIT:
    remove(JFFS_PATH_NAME0);
    return JFFS_NO_ERROR;
}

VOID ItFsJffs022(VOID)
{
    TEST_ADD_CASE("IT_FS_JFFS_022", testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL0, TEST_FUNCTION);
}
