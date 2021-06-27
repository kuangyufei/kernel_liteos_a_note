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
    INT32 fd = -1;
    INT32 fd1 = -1;
    INT32 len, ret;
    CHAR filebuf[JFFS_STANDARD_NAME_LENGTH] = "1234567890abcde&";
    CHAR readbuf[JFFS_STANDARD_NAME_LENGTH] = {0};
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR pathname2[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME01 };
    CHAR pathname3[JFFS_STANDARD_NAME_LENGTH] = "originfile";
    CHAR pathname4[JFFS_STANDARD_NAME_LENGTH] = "linkfile";
    INT32 olddirFd = -1;
    INT32 newdirFd = -1;
    DIR *olddir = NULL;
    DIR *newdir = NULL;

    ret = mkdir(pathname1, 0777);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    olddir = opendir(pathname1);
    ICUNIT_GOTO_NOT_EQUAL(olddir, NULL, olddir, EXIT1);

    olddirFd = dirfd(olddir);
    ICUNIT_GOTO_NOT_EQUAL(olddirFd, JFFS_IS_ERROR, olddirFd, EXIT2);

    ret = mkdir(pathname2, 0777);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

    newdir = opendir(pathname2);
    ICUNIT_GOTO_NOT_EQUAL(newdir, NULL, newdir, EXIT3);

    newdirFd = dirfd(newdir);
    ICUNIT_GOTO_NOT_EQUAL(newdirFd, JFFS_IS_ERROR, newdirFd, EXIT4);

    fd = openat(olddirFd, pathname3, O_NONBLOCK | O_CREAT | O_RDWR, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd, JFFS_IS_ERROR, fd, EXIT4);

    len = write(fd, filebuf, strlen(filebuf));
    ICUNIT_GOTO_EQUAL(len, strlen(filebuf), len, EXIT6);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT6);

    ret = linkat(olddirFd, pathname3, newdirFd, pathname4, 0);
    ICUNIT_GOTO_NOT_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT5);

    ret = unlinkat(olddirFd, pathname3, 0);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT5);

    fd1 = openat(newdirFd, pathname4, O_NONBLOCK | O_RDWR, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd, JFFS_IS_ERROR, fd, EXIT5);

    len = read(fd1, readbuf, strlen(filebuf));
    ICUNIT_GOTO_EQUAL(len, strlen(filebuf), len, EXIT8);
    ICUNIT_GOTO_STRING_EQUAL(readbuf, filebuf, readbuf, EXIT8);

    ret = close(fd1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT8);

    ret = unlinkat(newdirFd, pathname4, 0);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT7);

    ret = closedir(newdir);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT4);

    ret = rmdir(pathname2);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT3);

    ret = closedir(olddir);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

    ret = rmdir(pathname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    return JFFS_NO_ERROR;

EXIT8:
    close(fd1);
EXIT7:
    unlinkat(newdirFd, pathname4, 0);
    goto EXIT5;
EXIT6:
    close(fd);
EXIT5:
    unlinkat(olddirFd, pathname3, 0);
EXIT4:
    closedir(newdir);
EXIT3:
    rmdir(pathname2);
EXIT2:
    closedir(olddir);
EXIT1:
    rmdir(pathname1);
EXIT:
    return JFFS_NO_ERROR;
}

VOID ItFsTestLinkat001(VOID)
{
    TEST_ADD_CASE("IT_FS_TEST_LINKAT_001", testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL0, TEST_FUNCTION);
}
