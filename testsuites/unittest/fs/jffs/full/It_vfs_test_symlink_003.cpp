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
    INT32 ret;
    CHAR filebuf[PATH_MAX + 2] = {""};
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR pathname2[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME01 };
    CHAR pathname3[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME02 };
    CHAR pathname4[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME03 };
    CHAR pathname5[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME04 };

    for (int i = 0; i < PATH_MAX + 1; i++) {
        strcat(filebuf, "d");
    }
    filebuf[PATH_MAX + 1] = '\0';
    ret = symlink(filebuf, pathname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT);
    ICUNIT_GOTO_EQUAL(errno, ENAMETOOLONG, errno, EXIT);

    ret = symlink("dddddddddddddd", pathname2);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    fd = open(pathname2, O_NONBLOCK | O_RDWR, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(fd, JFFS_IS_ERROR, fd, EXIT1);
    ICUNIT_GOTO_EQUAL(errno, ENOENT, errno, EXIT1);

    ret = symlink("aaaaaaaaaaaaaaa", pathname2);
    ICUNIT_GOTO_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT1);
    ICUNIT_GOTO_EQUAL(errno, EEXIST, errno, EXIT1);

    ret = symlink("", pathname3);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

    fd = open(pathname3, O_NONBLOCK | O_RDWR, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(fd, JFFS_IS_ERROR, fd, EXIT2);
    ICUNIT_GOTO_EQUAL(errno, EINVAL, errno, EXIT2);

    fd = creat(pathname4, 0777);
    ICUNIT_GOTO_NOT_EQUAL(fd, JFFS_IS_ERROR, fd, EXIT2);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT4);

    ret = symlink(pathname4, pathname5);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT3);

    fd = open(pathname5, O_NONBLOCK | O_RDWR, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd, JFFS_IS_ERROR, fd, EXIT5);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT6);

    ret = unlink(pathname4);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT5);

    fd = open(pathname5, O_NONBLOCK | O_RDWR, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(fd, JFFS_IS_ERROR, fd, EXIT7);
    ICUNIT_GOTO_EQUAL(errno, ENOENT, errno, EXIT7);

    ret = unlink(pathname5);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT7);

    ret = unlink(pathname3);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

    ret = unlink(pathname2);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    return JFFS_NO_ERROR;

EXIT7:
    unlink(pathname5);
    goto EXIT2;
EXIT6:
    close(fd);
EXIT5:
    unlink(pathname5);
    goto EXIT3;
EXIT4:
    close(fd);
EXIT3:
    unlink(pathname4);
EXIT2:
    unlink(pathname3);
EXIT1:
    unlink(pathname2);
EXIT:
    return JFFS_NO_ERROR;
}

VOID ItFsTestSymlink003(VOID)
{
    TEST_ADD_CASE("IT_FS_TEST_SYMLINK_003", testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL0, TEST_FUNCTION);
}

