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
    INT32 fd, ret, len;
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR pathname2[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR writebuf[JFFS_STANDARD_NAME_LENGTH] = "1234567890";
    CHAR readbuf[JFFS_STANDARD_NAME_LENGTH] = "";
    off_t off;

    ret = mkdir(pathname1, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = chdir(pathname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    strcat(pathname2, "/test");
    fd = open(pathname2, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, EXIT2);

    len = write(fd, writebuf, strlen(writebuf));
    ICUNIT_GOTO_EQUAL(len, 10, len, EXIT2); // compare ret len with target len 10

    off = lseek(fd, 0, SEEK_CUR);
    ICUNIT_GOTO_EQUAL(off, 10, off, EXIT2); // compare ret off with target off 10

    len = pwrite(fd, writebuf, JFFS_STANDARD_NAME_LENGTH, 0);
    ICUNIT_GOTO_EQUAL(len, JFFS_STANDARD_NAME_LENGTH, len, EXIT2);

    off = lseek(fd, 0, SEEK_SET);
    ICUNIT_GOTO_EQUAL(off, 0, off, EXIT2);

    memset(readbuf, 0, JFFS_STANDARD_NAME_LENGTH);
    len = read(fd, readbuf, 20); // read buffer len 20
    ICUNIT_GOTO_EQUAL(len, 20, len, EXIT2); // compare ret len with target len 20
    ICUNIT_GOTO_STRING_EQUAL(readbuf, writebuf, readbuf, EXIT2);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);

    ret = unlink(pathname2);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);

    ret = chdir(JFFS_MAIN_DIR0);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = rmdir(pathname1);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    strcpy(pathname2, JFFS_PATH_NAME0);

    ret = mkdir(pathname1, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = chdir(pathname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    strcat(pathname2, "/test");
    fd = open(pathname2, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, EXIT2);

    len = write(fd, writebuf, strlen(writebuf));
    ICUNIT_GOTO_EQUAL(len, 10, len, EXIT2); // compare ret len with target len 10

    off = lseek(fd, 0, SEEK_CUR);
    ICUNIT_GOTO_EQUAL(off, 10, off, EXIT2); // compare ret off with target off 10

    len = pwrite64(fd, writebuf, JFFS_STANDARD_NAME_LENGTH, 0);
    ICUNIT_GOTO_EQUAL(len, JFFS_STANDARD_NAME_LENGTH, len, EXIT2);

    off = lseek(fd, 0, SEEK_SET);
    ICUNIT_GOTO_EQUAL(off, 0, off, EXIT2);

    memset(readbuf, 0, JFFS_STANDARD_NAME_LENGTH);
    len = read(fd, readbuf, 20); // read buffer len 20
    ICUNIT_GOTO_EQUAL(len, 20, len, EXIT2); // compare ret len with target len 20
    ICUNIT_GOTO_STRING_EQUAL(readbuf, writebuf, readbuf, EXIT2);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);

    ret = unlink(pathname2);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);

    ret = chdir(JFFS_MAIN_DIR0);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = rmdir(pathname1);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);
    return JFFS_NO_ERROR;
EXIT2:
    close(fd);
EXIT1:
    remove(pathname2);
EXIT:
    remove(pathname1);
    return JFFS_NO_ERROR;
}

/*
 *
testcase brief in English
 *
 */

VOID ItFsJffs669(VOID)
{
    TEST_ADD_CASE("IT_FS_JFFS_669", Testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL2, TEST_FUNCTION);
}
