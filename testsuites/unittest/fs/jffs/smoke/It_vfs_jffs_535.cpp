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
    INT32 fd, len, ret;
    CHAR filebuf[JFFS_STANDARD_NAME_LENGTH] = "1234567890abcde&";
    CHAR readbuf[JFFS_STANDARD_NAME_LENGTH] = {0};
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    long off;

    fd = open(pathname1, O_NONBLOCK | O_CREAT | O_RDWR, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd, JFFS_IS_ERROR, fd, EXIT1);

    len = write(fd, filebuf, strlen(filebuf));
    ICUNIT_GOTO_EQUAL(len, 16, len, EXIT1);

    len = write(fd, filebuf, strlen(filebuf));
    ICUNIT_GOTO_EQUAL(len, 16, len, EXIT1);

    len = write(fd, filebuf, strlen(filebuf));
    ICUNIT_GOTO_EQUAL(len, 16, len, EXIT1);

    len = write(fd, "END", 3); // write length: 3
    ICUNIT_GOTO_EQUAL(len, 3, len, EXIT1);

    off = lseek64(fd, JFFS_SHORT_ARRAY_LENGTH, SEEK_SET);
    ICUNIT_GOTO_EQUAL(off, JFFS_SHORT_ARRAY_LENGTH, off, EXIT1);

    len = read(fd, readbuf, 6); // read length: 6
    ICUNIT_GOTO_EQUAL(len, 6, len, EXIT1);
    ICUNIT_GOTO_STRING_EQUAL(readbuf, "abcde&", readbuf, EXIT1);

    off = lseek64(fd, 2, SEEK_CUR); // seek offset: 2
    ICUNIT_GOTO_EQUAL(off, 18, off, EXIT1); // file position: 18

    memset(readbuf, 0, sizeof(readbuf));

    len = read(fd, readbuf, 5); // read length: 5
    ICUNIT_GOTO_EQUAL(len, 5, len, EXIT1);
    ICUNIT_GOTO_STRING_EQUAL(readbuf, "34567", readbuf, EXIT1);

    off = lseek64(fd, 0, SEEK_CUR);
    ICUNIT_GOTO_EQUAL(off, 23, off, EXIT1); // file position: 23

    len = read(fd, readbuf, 5); // read length: 5
    ICUNIT_GOTO_EQUAL(len, 5, len, EXIT1);
    ICUNIT_GOTO_STRING_EQUAL(readbuf, "890ab", readbuf, EXIT1);

    off = lseek64(fd, -2, SEEK_CUR); // seek offset: -2
    ICUNIT_GOTO_EQUAL(off, 26, off, EXIT1); // file position: 26

    len = read(fd, readbuf, 6); // read length: 6
    ICUNIT_GOTO_EQUAL(len, 6, len, EXIT1);
    ICUNIT_GOTO_STRING_EQUAL(readbuf, "abcde&", readbuf, EXIT1);

    off = lseek64(fd, -2, SEEK_END); // seek offset: -2
    ICUNIT_GOTO_EQUAL(off, 49, off, EXIT1); // file size: 49

    memset(readbuf, 0, sizeof(readbuf));
    len = read(fd, readbuf, 5); // read length: 5
    ICUNIT_GOTO_EQUAL(len, 2, len, EXIT1); // file size: 2
    ICUNIT_GOTO_STRING_EQUAL(readbuf, "ND", readbuf, EXIT1);

    off = lseek64(fd, 0, SEEK_END);
    ICUNIT_GOTO_EQUAL(off, 51, off, EXIT1); // file size: 51

    len = read(fd, readbuf, 6); // read length: 6
    ICUNIT_GOTO_EQUAL(len, JFFS_NO_ERROR, len, EXIT1);

    off = lseek64(fd, 2, SEEK_END); // seek offset: 2
    ICUNIT_GOTO_EQUAL(off, 53, off, EXIT1); // file size: 53
    len = read(fd, readbuf, 6); // read length: 6
    ICUNIT_GOTO_EQUAL(len, JFFS_NO_ERROR, len, EXIT1);

    len = write(fd, "mmm", 3); // write length: 3
    ICUNIT_GOTO_EQUAL(len, 3, len, EXIT1);

    off = lseek64(fd, -3, SEEK_END); // seek offset: -3
    ICUNIT_GOTO_EQUAL(off, 53, off, EXIT1); // file size:49

    len = read(fd, readbuf, 5); // read length: 5
    ICUNIT_GOTO_EQUAL(len, 3, len, EXIT1); // file length: 3
    ICUNIT_GOTO_STRING_EQUAL(readbuf, "mmm", readbuf, EXIT1);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    ret = unlink(pathname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    return JFFS_NO_ERROR;
EXIT1:
    close(fd);
EXIT:
    remove(pathname1);
    return JFFS_NO_ERROR;
}

VOID ItFsJffs535(VOID)
{
    TEST_ADD_CASE("IT_FS_JFFS_535", testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL0, TEST_FUNCTION);
}
