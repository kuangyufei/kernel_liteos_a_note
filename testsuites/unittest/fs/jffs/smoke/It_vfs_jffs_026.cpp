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
    CHAR readbuf[301] = {0};
    struct statfs buf1 = { 0 };
    struct statfs buf2 = { 0 };
    struct statfs buf3 = { 0 };
    struct statfs buf4 = { 0 };

    ret = mkdir(pathname1, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    strcat(pathname1, "/dir");
    ret = mkdir(pathname1, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    strcat(buffile, "/dir/files");
    fd = open(buffile, O_NONBLOCK | O_CREAT | O_RDWR, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, EXIT3);

    ret = statfs(pathname1, &buf1);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT3);

    ret = statfs(buffile, &buf2);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT3);

    ICUNIT_GOTO_EQUAL(buf1.f_type, buf2.f_type, buf1.f_type, EXIT3);
    ICUNIT_GOTO_EQUAL(buf1.f_bavail, buf2.f_bavail, buf1.f_bavail, EXIT3);
    ICUNIT_GOTO_EQUAL(buf1.f_bsize, buf2.f_bsize, buf1.f_bsize, EXIT3);
    ICUNIT_GOTO_EQUAL(buf1.f_bfree, buf2.f_bfree, buf1.f_bfree, EXIT3);

    len = write(fd,
        "12345678901234567890123456789012345678901234567890abcdefghjk12345678901234567890123456789012345678901234567890"
        "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "12345678901234567890123456789012345678901234567890123456789012345678901234567890",
        300); // write length: 300
    ICUNIT_GOTO_EQUAL(len, 300, len, EXIT3);

    off = lseek(fd, 0, SEEK_SET);
    ICUNIT_GOTO_NOT_EQUAL(off, -1, off, EXIT3);

    len = read(fd, readbuf, 300); // read length: 300
    ICUNIT_GOTO_EQUAL(len, 300, len, EXIT3);
    ICUNIT_GOTO_STRING_EQUAL(readbuf,
        "12345678901234567890123456789012345678901234567890abcdefghjk12345678901234567890123456789012345678901234567890"
        "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "12345678901234567890123456789012345678901234567890123456789012345678901234567890",
        readbuf, EXIT3);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT3);

    ret = statfs(pathname1, &buf3);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);

    ret = statfs(buffile, &buf4);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);

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
    JffsStrcat2(pathname1, "/dir/files", strlen(pathname1));
    remove(pathname1);
EXIT1:
    JffsStrcat2(pathname1, "dir", strlen(pathname1));
    remove(pathname1);
EXIT:
    remove(JFFS_PATH_NAME0);
    return JFFS_NO_ERROR;
}

VOID ItFsJffs026(VOID)
{
    TEST_ADD_CASE("IT_FS_JFFS_026", testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL0, TEST_FUNCTION);
}
