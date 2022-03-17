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
    INT32 pfd = -100;
    INT32 pfd2 = -1;
    INT32 ret, len;
    CHAR filebuf1[JFFS_SHORT_ARRAY_LENGTH] = "liteos ";
    CHAR readbuf[JFFS_STANDARD_NAME_LENGTH] = "liteos";
    CHAR pathname[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    off_t off;
    struct stat buf1;

    fd = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, EXIT1);

    pfd = dup(fd);
    ICUNIT_GOTO_NOT_EQUAL(pfd, -1, pfd, EXIT2);

    len = write(fd, filebuf1, strlen(filebuf1));
    ICUNIT_GOTO_EQUAL(len, strlen(filebuf1), len, EXIT2);

    off = lseek(fd, 3, SEEK_SET); // 3 means seek len
    ICUNIT_GOTO_EQUAL(off, 3, off, EXIT2); // 3 means current file position

    pfd2 = dup(fd);
    ICUNIT_GOTO_NOT_EQUAL(pfd2, -1, pfd2, EXIT3);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    fd = open(pathname, O_NONBLOCK | O_RDWR, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, EXIT1);

    (void)memset_s(readbuf, sizeof(readbuf), 0, sizeof(readbuf));
    len = read(fd, readbuf, 50); // 50 means read len
    ICUNIT_GOTO_EQUAL(len, 7, len, EXIT1); // 7 means length of actually read data
    ICUNIT_GOTO_STRING_EQUAL(readbuf, "liteos ", readbuf, EXIT1);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT3);

    ret = close(pfd);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT3);

    ret = close(pfd2);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT3);

    ret = unlink(pathname);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    return JFFS_NO_ERROR;
EXIT3:
    close(pfd2);
EXIT2:
    close(pfd);
EXIT1:
    close(fd);
EXIT:
    remove(pathname);
    return JFFS_NO_ERROR;
}

VOID ItFsJffs069(VOID)
{
    TEST_ADD_CASE("IT_FS_JFFS_069", Testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL2, TEST_FUNCTION);
}

