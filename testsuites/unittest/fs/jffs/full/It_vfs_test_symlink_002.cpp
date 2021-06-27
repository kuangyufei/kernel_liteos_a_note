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
    INT32 len, ret;
    CHAR readbuf[JFFS_STANDARD_NAME_LENGTH] = {0};
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR pathname2[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME01 };

    ret = symlink(pathname1, pathname2);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    len = readlink(pathname2, readbuf, sizeof(readbuf));
    ICUNIT_GOTO_EQUAL(len, strlen(pathname1), len, EXIT1);
    ICUNIT_GOTO_STRING_EQUAL(readbuf, pathname1, readbuf, EXIT1);

    ret = symlink(pathname2, pathname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    memset(readbuf, 0, sizeof(readbuf));
    len = readlink(pathname1, readbuf, sizeof(readbuf));
    ICUNIT_GOTO_EQUAL(len, strlen(pathname2), len, EXIT2);
    ICUNIT_GOTO_STRING_EQUAL(readbuf, pathname2, readbuf, EXIT2);

    fd = open(pathname2, O_NONBLOCK | O_RDWR, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(fd, JFFS_IS_ERROR, fd, EXIT2);
    ICUNIT_GOTO_EQUAL(errno, ELOOP, errno, EXIT2);

    ret = unlink(pathname2);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

    ret = unlink(pathname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    return JFFS_NO_ERROR;

EXIT2:
    unlink(pathname1);
EXIT1:
    unlink(pathname2);
EXIT:
    return JFFS_NO_ERROR;
}

VOID ItFsTestSymlink002(VOID)
{
    TEST_ADD_CASE("IT_FS_TEST_SYMLINK_002", testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL0, TEST_FUNCTION);
}

