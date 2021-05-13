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
    INT32 fd = -1;
    INT32 len, ret;
    CHAR filebuf[20] = "1234567890abcde&";
    CHAR readbuf[50] = { 0 };
    CHAR pathname[50] = { JFFS_PATH_NAME0 };
    off_t off;

    fd = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, EXIT1);

    len = write(fd, filebuf, strlen(filebuf));
    ICUNIT_GOTO_EQUAL(len, 16, len, EXIT1); // 16 means write len

    len = write(fd, filebuf, strlen(filebuf));
    ICUNIT_GOTO_EQUAL(len, 16, len, EXIT1); // 16 means write len

    len = write(fd, filebuf, strlen(filebuf));
    ICUNIT_GOTO_EQUAL(len, 16, len, EXIT1); // 16 means write len

    len = write(fd, "END", 3); // 3 means name len
    ICUNIT_GOTO_EQUAL(len, 3, len, EXIT1); // 3 means write len

    off = lseek(fd, 10, SEEK_SET); // 10 means file seek len
    ICUNIT_GOTO_EQUAL(off, 10, off, EXIT1); // 10 means current file positon

    len = read(fd, readbuf, 6); // 6 means read len
    ICUNIT_GOTO_EQUAL(len, 6, len, EXIT1); // 6 means read len
    ICUNIT_GOTO_STRING_EQUAL(readbuf, "abcde&", readbuf, EXIT1);

    off = lseek(fd, 2, SEEK_CUR); // 2 means file seek len
    ICUNIT_GOTO_EQUAL(off, 18, off, EXIT1); // 18 means current file positon

    memset_s(readbuf, sizeof(readbuf), 0, sizeof(readbuf));

    len = read(fd, readbuf, 5); // 5 means read len
    ICUNIT_GOTO_EQUAL(len, 5, len, EXIT1); // 5 means read len
    ICUNIT_GOTO_STRING_EQUAL(readbuf, "34567", readbuf, EXIT1);

    off = lseek(fd, 0, SEEK_CUR);
    ICUNIT_GOTO_EQUAL(off, 23, off, EXIT1); // 23 means current file positon

    len = read(fd, readbuf, 5); // 5 means read len
    ICUNIT_GOTO_EQUAL(len, 5, len, EXIT1); // 5 means read len
    ICUNIT_GOTO_STRING_EQUAL(readbuf, "890ab", readbuf, EXIT1);

    off = lseek(fd, -2, SEEK_CUR); // -2 means file seek back len
    ICUNIT_GOTO_EQUAL(off, 26, off, EXIT1); // 26 means current file positon

    len = read(fd, readbuf, 6); // 6 means read len
    ICUNIT_GOTO_EQUAL(len, 6, len, EXIT1); // 6 means read len
    ICUNIT_GOTO_STRING_EQUAL(readbuf, "abcde&", readbuf, EXIT1);

    memset_s(readbuf, sizeof(readbuf), 0, sizeof(readbuf));

    off = lseek(fd, -2, SEEK_END); // -2 means file seek back len
    ICUNIT_GOTO_EQUAL(off, 49, off, EXIT1); // 49 means current file positon

    len = read(fd, readbuf, 5); // 5 means read len
    ICUNIT_GOTO_EQUAL(len, 2, len, EXIT1); // 2 means read len
    ICUNIT_GOTO_STRING_EQUAL(readbuf, "ND", readbuf, EXIT1);

    off = lseek(fd, 0, SEEK_END);
    ICUNIT_GOTO_EQUAL(off, 51, off, EXIT1); // 51 means current file positon

    len = read(fd, readbuf, 6); // 6 means read len
    ICUNIT_GOTO_EQUAL(len, 0, len, EXIT1);

    off = lseek(fd, 2, SEEK_END); // 2 means file seek len
    ICUNIT_GOTO_EQUAL(off, 53, off, EXIT1); // 53 means current file positon
    len = read(fd, readbuf, 6); // 6 means read len
    ICUNIT_GOTO_EQUAL(len, 0, len, EXIT1);

    len = write(fd, "mmm", 3); // 3 means write len
    ICUNIT_GOTO_EQUAL(len, 3, len, EXIT1); // 3 means write len

    off = lseek(fd, -3, SEEK_END); // -3 means file seek back len
    ICUNIT_GOTO_EQUAL(off, 53, off, EXIT1); // 53 means current file positon

    len = read(fd, readbuf, 5); // 5 means read len
    ICUNIT_GOTO_EQUAL(len, 3, len, EXIT1); // 3 means length of actually read data
    ICUNIT_GOTO_STRING_EQUAL(readbuf, "mmm", readbuf, EXIT1);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    ret = unlink(pathname);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    return JFFS_NO_ERROR;
EXIT1:
    close(fd);
EXIT:
    remove(pathname);
    return JFFS_NO_ERROR;
}

VOID ItFsJffs056(VOID)
{
    TEST_ADD_CASE("IT_FS_JFFS_056", Testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL2, TEST_FUNCTION);
}

