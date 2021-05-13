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
    CHAR filebuf[20] = "1234567890abcde";
    CHAR readbuf[JFFS_SHORT_ARRAY_LENGTH] = { 0 };
    CHAR pathname[50] = { JFFS_PATH_NAME0 };
    off_t off;

    fd = open(pathname, O_CREAT | O_RDWR, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, EXIT1);

    len = write(fd, filebuf, strlen(filebuf));
    ICUNIT_GOTO_EQUAL(len, strlen(filebuf), len, EXIT1);

    off = lseek(fd, 0, SEEK_SET);
    ICUNIT_GOTO_NOT_EQUAL(off, -1, off, EXIT1);

    len = read(fd, readbuf, 4); // 4 means read len
    ICUNIT_GOTO_EQUAL(len, 4, len, EXIT1); // 4 means read len
    ICUNIT_GOTO_STRING_EQUAL(readbuf, "1234", readbuf, EXIT1);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    memset_s(readbuf, JFFS_SHORT_ARRAY_LENGTH, 0, JFFS_SHORT_ARRAY_LENGTH);

    len = read(fd, readbuf, 2); // 2 means read len
    ICUNIT_GOTO_EQUAL(len, -1, len, EXIT);
    ICUNIT_GOTO_EQUAL(errno, EBADF, errno, EXIT);
    ICUNIT_GOTO_EQUAL(readbuf[0], 0, readbuf[0], EXIT);

    len = write(fd, filebuf, strlen(filebuf));
    ICUNIT_GOTO_EQUAL(len, -1, len, EXIT);
    ICUNIT_GOTO_EQUAL(errno, EBADF, errno, EXIT);

    fd = open(pathname, O_CREAT | O_RDWR, HIGHEST_AUTHORITY); // ÔÙ´Î´ò¿ªÎÄ¼þ
    ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, EXIT1);

    len = write(fd, filebuf, strlen(filebuf));
    ICUNIT_GOTO_EQUAL(len, strlen(filebuf), len, EXIT1);

    off = lseek(fd, 0, SEEK_SET);
    ICUNIT_GOTO_NOT_EQUAL(off, -1, off, EXIT1);

    len = read(fd, readbuf, 4); // 4 means read len
    ICUNIT_GOTO_EQUAL(len, 4, len, EXIT1); // 4 means read len
    ICUNIT_GOTO_STRING_EQUAL(readbuf, "1234", readbuf, EXIT1);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    ret = unlink(pathname);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    len = read(fd, readbuf, 2); // 2 means read len
    ICUNIT_GOTO_EQUAL(len, -1, len, EXIT);
    ICUNIT_GOTO_EQUAL(errno, EBADF, errno, EXIT);
    ICUNIT_GOTO_STRING_EQUAL(readbuf, "1234", readbuf, EXIT);

    len = write(fd, filebuf, strlen(filebuf));
    ICUNIT_GOTO_EQUAL(len, -1, len, EXIT);
    ICUNIT_GOTO_EQUAL(errno, EBADF, errno, EXIT);

    return JFFS_NO_ERROR;
EXIT1:
    close(fd);
EXIT:
    remove(pathname);
    return JFFS_NO_ERROR;
}

VOID ItFsJffs053(VOID)
{
    TEST_ADD_CASE("IT_FS_JFFS_053", Testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL2, TEST_FUNCTION);
}

