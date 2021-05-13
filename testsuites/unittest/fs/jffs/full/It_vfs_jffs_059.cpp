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

static constexpr int FILE_NAME_LEN = 10;
static constexpr int MAX_FILE_NAME_LEN = 50;

static UINT32 Testcase(VOID)
{
    INT32 fd = -1;
    INT32 ret, len;
    CHAR filebuf[20] = "abcdeabcde";
    CHAR readbuf[MAX_FILE_NAME_LEN] = "liteos";
    CHAR pathname[MAX_FILE_NAME_LEN] = { JFFS_PATH_NAME0 };
    off_t off;
    struct stat buf1;

    fd = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, EXIT1);

    len = write(fd, filebuf, strlen(filebuf));
    ICUNIT_GOTO_EQUAL(len, FILE_NAME_LEN, len, EXIT1);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    fd = creat(pathname, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, EXIT1);

    len = read(fd, readbuf, MAX_FILE_NAME_LEN);
    ICUNIT_GOTO_EQUAL(len, -1, len, EXIT1);
    ICUNIT_GOTO_STRING_EQUAL(readbuf, "liteos", readbuf, EXIT1);

    ret = stat(pathname, &buf1);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);
    ICUNIT_GOTO_EQUAL(buf1.st_size, 0, buf1.st_size, EXIT1);

    len = write(fd, filebuf, strlen(filebuf));
    ICUNIT_GOTO_EQUAL(len, FILE_NAME_LEN, len, EXIT1);

    len = read(fd, readbuf, MAX_FILE_NAME_LEN);
    ICUNIT_GOTO_EQUAL(len, -1, len, EXIT1);

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

VOID ItFsJffs059(VOID)
{
    TEST_ADD_CASE("IT_FS_JFFS_059", Testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL2, TEST_FUNCTION);
}

