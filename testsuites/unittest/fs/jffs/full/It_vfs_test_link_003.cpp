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
    ret = link(filebuf, pathname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT);
    ICUNIT_GOTO_EQUAL(errno, ENAMETOOLONG, errno, EXIT);

    ret = link("dddddddddddddd", pathname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT);
    ICUNIT_GOTO_EQUAL(errno, ENOENT, errno, EXIT);

    ret = link(JFFS_MAIN_DIR0, pathname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT);
    ICUNIT_GOTO_EQUAL(errno, EPERM, errno, EXIT);

    fd = creat(pathname2, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd, JFFS_IS_ERROR, fd, EXIT);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

    ret = link(pathname2, pathname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    fd = creat(pathname3, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd, JFFS_IS_ERROR, fd, EXIT3);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT5);

    ret = link(pathname3, pathname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT4);
    ICUNIT_GOTO_EQUAL(errno, EEXIST, errno, EXIT4);

    ret = unlink(pathname3);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT4);

    ret = unlink(pathname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT3);

    ret = unlink(pathname2);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    return JFFS_NO_ERROR;

EXIT5:
    close(fd);
EXIT4:
    unlink(pathname3);
EXIT3:
    unlink(pathname1);
    goto EXIT1;
EXIT2:
    close(fd);
EXIT1:
    unlink(pathname2);
EXIT:
    return JFFS_NO_ERROR;
}

VOID ItFsTestLink003(VOID)
{
    TEST_ADD_CASE("IT_FS_TEST_LINK_003", testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL0, TEST_FUNCTION);
}

