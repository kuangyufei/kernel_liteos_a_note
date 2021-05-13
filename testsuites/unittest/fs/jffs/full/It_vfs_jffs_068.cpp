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

static constexpr int MAX_FILE_NAME_LEN = 50;

static UINT32 Testcase(VOID)
{
    INT32 fd = -1;
    INT32 fd1 = -1;
    INT32 fd2 = -1;
    INT32 pfd = -1;
    INT32 ret, len;
    CHAR filebuf1[10] = "liteos ";
    CHAR filebuf2[10] = "good";
    CHAR readbuf[MAX_FILE_NAME_LEN] = { 0 };
    CHAR pathname1[MAX_FILE_NAME_LEN] = { JFFS_PATH_NAME0 };
    CHAR pathname2[MAX_FILE_NAME_LEN] = { JFFS_PATH_NAME0 };
    off_t off;
    struct stat buf1;

    errno = 0;
    fd = open(pathname1, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, EXIT1);

    pfd = dup2(20, 20); // 20 means dup length
    ICUNIT_GOTO_EQUAL(pfd, -1, pfd, EXIT2);

    pfd = dup2(fd, fd);
    ICUNIT_GOTO_EQUAL(pfd, fd, pfd, EXIT2);

    strcat_s(pathname2, sizeof(pathname2), "T");
    fd1 = open(pathname2, O_NONBLOCK | O_CREAT | O_RDWR, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd1, -1, fd1, EXIT4);

    pfd = dup2(fd, fd1);
    printf("[%d] fd:%d, fd1:%d, pfd:%d, errno:%d\n", __LINE__, fd, fd1, pfd, errno);
    ICUNIT_GOTO_EQUAL(pfd, fd1, pfd, EXIT4);

    len = write(fd1, filebuf1, strlen(filebuf1));
    ICUNIT_GOTO_EQUAL(len, strlen(filebuf1), len, EXIT4);

    ret = close(fd1);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT4);

    len = write(fd, filebuf2, strlen(filebuf2));
    ICUNIT_GOTO_EQUAL(len, strlen(filebuf2), len, EXIT4);

    fd2 = open(pathname2, O_NONBLOCK | O_CREAT | O_RDWR, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd2, -1, fd2, EXIT5);

    errno = 0;
    len = read(fd2, readbuf, MAX_FILE_NAME_LEN);
    ICUNIT_GOTO_EQUAL(len, 0, len, EXIT5);

    ICUNIT_GOTO_EQUAL((INT32)lseek(fd2, 0, SEEK_CUR), 0, (INT32)lseek(fd2, 0, SEEK_CUR), EXIT5);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT5);

    len = write(fd, filebuf2, strlen(filebuf2));
    ICUNIT_GOTO_EQUAL(len, -1, len, EXIT5);

    fd = open(pathname1, O_NONBLOCK | O_RDWR, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, EXIT5);

    memset_s(readbuf, sizeof(readbuf), 0, strlen(readbuf));
    len = read(fd, readbuf, MAX_FILE_NAME_LEN);
    printf("[%d] fd:%d, errno:%d,readbuf:%s\n", __LINE__, fd, errno, readbuf);
    ICUNIT_GOTO_EQUAL(len, strlen(filebuf1) + strlen(filebuf2), len, EXIT5);
    ICUNIT_GOTO_STRING_EQUAL(readbuf, "liteos good", readbuf, EXIT5);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT5);

    ret = unlink(pathname1);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT5);

    ret = close(fd2);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT5);

    ret = unlink(pathname2);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT3);

    return JFFS_NO_ERROR;
EXIT5:
    close(fd2);
    goto EXIT3;
EXIT4:
    close(fd1);
EXIT3:
    remove(pathname2);
EXIT2:
    close(pfd);
EXIT1:
    close(fd);
EXIT:
    remove(pathname1);
    return JFFS_NO_ERROR;
}

VOID ItFsJffs068(VOID)
{
    TEST_ADD_CASE("IT_FS_JFFS_068", Testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL2, TEST_FUNCTION);
}

