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
    INT32 ret, fd, len;
    off_t off;
    CHAR pathname1[50] = { JFFS_PATH_NAME0 };
    CHAR buffile[50] = { JFFS_PATH_NAME0 };
    struct stat buf1 = { 0 };
    struct stat buf2 = { 0 };
    struct stat buf3 = { 0 };
    CHAR writebuf[20] = "ABCDEFGHJK";
    CHAR readbuf[20] = {0};

    ret = mkdir(pathname1, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    strcat(pathname1, "/dir");
    ret = mkdir(pathname1, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);

    strcat(buffile, "/dir/files");
    fd = open(buffile, O_NONBLOCK | O_CREAT | O_RDWR, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, EXIT3);

    len = write(fd, writebuf, strlen(writebuf));
    ICUNIT_GOTO_EQUAL(len, strlen(writebuf), len, EXIT3);

    off = lseek(fd, 0, SEEK_SET);
    ICUNIT_GOTO_NOT_EQUAL(off, -1, off, EXIT3);

    len = read(fd, readbuf, strlen(writebuf));
    ICUNIT_GOTO_EQUAL(len, strlen(writebuf), len, EXIT3);
    ICUNIT_GOTO_STRING_EQUAL(readbuf, writebuf, readbuf, EXIT3);

    ret = stat(buffile, &buf1);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT3);
    JffsStatPrintf(buf1);

    ret = fstat(fd, &buf2);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT3);
    JffsStatPrintf(buf2);

    ret = stat(pathname1, &buf3);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT3);
    JffsStatPrintf(buf3);

    ICUNIT_GOTO_EQUAL(buf1.st_size, 10, buf1.st_size, EXIT3);
    ICUNIT_GOTO_EQUAL(buf2.st_size, 10, buf2.st_size, EXIT3);
    ICUNIT_GOTO_EQUAL(buf3.st_size, 0, buf3.st_size, EXIT3);

    ICUNIT_GOTO_EQUAL(buf1.st_mode & S_IFMT, S_IFREG, buf1.st_mode & S_IFMT, EXIT3);
    ICUNIT_GOTO_EQUAL(buf2.st_mode & S_IFMT, S_IFREG, buf2.st_mode & S_IFMT, EXIT3);
    ICUNIT_GOTO_EQUAL(buf3.st_mode & S_IFMT, S_IFDIR, buf3.st_mode & S_IFMT, EXIT3);

    ICUNIT_GOTO_EQUAL(buf1.st_blocks, buf2.st_blocks, buf1.st_blocks, EXIT3);
    ICUNIT_GOTO_EQUAL(buf1.st_size, buf2.st_size, buf1.st_size, EXIT3);
    ICUNIT_GOTO_EQUAL(buf1.st_blksize, buf2.st_blksize, buf1.st_blksize, EXIT3);
    ICUNIT_GOTO_EQUAL(buf1.st_ino, buf2.st_ino, buf1.st_ino, EXIT3);
    ICUNIT_GOTO_EQUAL(buf1.st_mode, buf2.st_mode, buf1.st_mode, EXIT3);
    ICUNIT_GOTO_EQUAL(buf1.st_mtim.tv_nsec, buf2.st_mtim.tv_nsec, buf1.st_mtim.tv_nsec, EXIT3);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT3);

    ret = unlink(buffile);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

    ret = rmdir(pathname1);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);

    ret = rmdir(JFFS_PATH_NAME0);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);
    return JFFS_NO_ERROR;

EXIT3:
    close(fd);
EXIT2:
    remove(buffile);
EXIT1:
    remove(pathname1);
EXIT:
    remove(JFFS_PATH_NAME0);
    return JFFS_NO_ERROR;
}

VOID ItFsJffs027(VOID)
{
    TEST_ADD_CASE("IT_FS_JFFS_027", testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL0, TEST_FUNCTION);
}
