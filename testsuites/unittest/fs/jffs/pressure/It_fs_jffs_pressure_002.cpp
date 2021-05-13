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

#include "It_vfs_jffs.h"

static UINT32 TestCase(VOID)
{
    INT32 fd = -1;
    INT32 fd1 = -1;
    INT32 ret, len;
    INT32 i = 0;
    signed long long offset;
    CHAR filebuf[JFFS_STANDARD_NAME_LENGTH] = "0000000000111111111122222222223333333333";
    CHAR readbuf[JFFS_STANDARD_NAME_LENGTH] = "liteos";
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR pathname2[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR pathname3[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR pathname4[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR buffile[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    DIR *dir = NULL;
    struct dirent *ptr = NULL;
    struct stat buf1 = { 0 };
    struct stat buf2 = { 0 };

    ret = mkdir(pathname1, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = chdir(pathname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    strcat_s(pathname2, JFFS_STANDARD_NAME_LENGTH, "/test");
    ret = mkdir(pathname2, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    strcat_s(pathname1, JFFS_STANDARD_NAME_LENGTH, "/test/test1");
    ret = mkdir(pathname1, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    JffsStrcat2(pathname1, "/test/test2", strlen(pathname1));
    ret = mkdir(pathname1, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    JffsStrcat2(pathname1, "/test/test3", strlen(pathname1));
    ret = mkdir(pathname1, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    JffsStrcat2(pathname1, "/test/jffs_1601.txt", strlen(pathname1));
    fd = open(pathname1, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, EXIT);

    len = write(fd, filebuf, strlen(filebuf));
    ICUNIT_GOTO_EQUAL(len, strlen(filebuf), len, EXIT);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    strcat_s(pathname3, JFFS_STANDARD_NAME_LENGTH, "/123");

    ret = stat(buffile, &buf1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);
    JffsStatPrintf(buf1);

    for (i = 0; i < JFFS_PRESSURE_CYCLES; i++) {
        ret = rename(pathname2, pathname3);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

        ret = rename(pathname3, pathname2);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);
    }
    ret = stat(buffile, &buf2);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);
    JffsStatPrintf(buf2);

    dir = opendir(pathname2);
    ICUNIT_GOTO_NOT_EQUAL(dir, NULL, dir, EXIT);

    offset = telldir(dir);
    ICUNIT_GOTO_EQUAL(offset, 0, offset, EXIT); // dir offset: 0
    ptr = readdir(dir);
    ICUNIT_GOTO_NOT_EQUAL(ptr, NULL, ptr, EXIT);

    offset = telldir(dir);
    ICUNIT_GOTO_EQUAL(offset, 1, offset, EXIT3); // dir offset: 1

    ptr = readdir(dir);
    ICUNIT_GOTO_NOT_EQUAL(ptr, NULL, ptr, EXIT);

    offset = telldir(dir);
    ICUNIT_GOTO_EQUAL(offset, 2, offset, EXIT); // dir offset: 2

    ptr = readdir(dir);
    ICUNIT_GOTO_NOT_EQUAL(ptr, NULL, ptr, EXIT);

    offset = telldir(dir);
    ICUNIT_GOTO_EQUAL(offset, 3, offset, EXIT); // dir offset: 3

    ptr = readdir(dir);
    ICUNIT_GOTO_NOT_EQUAL(ptr, NULL, ptr, EXIT);

    offset = telldir(dir);
    ICUNIT_GOTO_EQUAL(offset, 4, offset, EXIT); // dir offset: 4

    ptr = readdir(dir);
    ICUNIT_GOTO_EQUAL(ptr, NULL, ptr, EXIT);

    offset = telldir(dir);
    ICUNIT_GOTO_EQUAL(offset, 4, offset, EXIT); // dir offset: 4

    ret = closedir(dir);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    JffsStrcat2(pathname1, "/test/jffs_1601.txt", strlen(pathname1));
    ret = unlink(pathname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    JffsStrcat2(pathname1, "/test/test1", strlen(pathname1));
    ret = rmdir(pathname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    JffsStrcat2(pathname1, "/test/test2", strlen(pathname1));
    ret = rmdir(pathname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    JffsStrcat2(pathname1, "/test/test3", strlen(pathname1));
    ret = rmdir(pathname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    JffsStrcat2(pathname2, "/test", strlen(pathname2));
    ret = rmdir(pathname2);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = chdir(JFFS_MAIN_DIR0);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = rmdir(pathname4);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    return JFFS_NO_ERROR;
EXIT3:
    close(fd);
EXIT2:
    JffsStrcat2(pathname1, "/test/jffs_1601.txt", strlen(pathname1));
    remove(pathname1);
    JffsStrcat2(pathname1, "/test/test3", strlen(pathname1));
    remove(pathname1);
    JffsStrcat2(pathname1, "/test/test2", strlen(pathname1));
    remove(pathname1);
    JffsStrcat2(pathname1, "/test/test1", strlen(pathname1));
    remove(pathname1);
EXIT1:
    JffsStrcat2(pathname1, "/123/jffs_1601.txt", strlen(pathname1));
    remove(pathname1);
    JffsStrcat2(pathname1, "/123/test3", strlen(pathname1));
    remove(pathname1);
    JffsStrcat2(pathname1, "/123/test2", strlen(pathname1));
    remove(pathname1);
    JffsStrcat2(pathname1, "/123/test1", strlen(pathname1));
    remove(pathname1);
EXIT:
    remove(pathname3);
    return JFFS_NO_ERROR;
}

VOID ItFsJffsPressure002(VOID)
{
    TEST_ADD_CASE("ItFsJffsPressure002", TestCase, TEST_VFS, TEST_JFFS, TEST_LEVEL3, TEST_PRESSURE);
}
