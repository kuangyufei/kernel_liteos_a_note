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
    size_t readSize = 3;
    CHAR filebuf[PATH_MAX + 2] = {""};
    CHAR readbuf[JFFS_STANDARD_NAME_LENGTH] = {0};
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR pathname2[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME01 };

    for (int i = 0; i < PATH_MAX + 1; i++) {
        strcat(filebuf, "d");
    }
    filebuf[PATH_MAX + 1] = '\0';

    fd = creat(pathname1, 0777);
    ICUNIT_GOTO_NOT_EQUAL(fd, JFFS_IS_ERROR, fd, EXIT);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

    ret = symlink(pathname1, pathname2);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    ret = readlink(pathname1, readbuf, JFFS_STANDARD_NAME_LENGTH);
    ICUNIT_GOTO_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT3);
    ICUNIT_GOTO_EQUAL(errno, EINVAL, errno, EXIT3);

    ret = readlink(pathname2, readbuf, 0);
    ICUNIT_GOTO_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT3);
    ICUNIT_GOTO_EQUAL(errno, EINVAL, errno, EXIT3);

    ret = readlink(filebuf, readbuf, JFFS_STANDARD_NAME_LENGTH);
    ICUNIT_GOTO_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT3);
    ICUNIT_GOTO_EQUAL(errno, ENAMETOOLONG, errno, EXIT3);

    ret = readlink(pathname2, readbuf, JFFS_STANDARD_NAME_LENGTH);
    ICUNIT_GOTO_EQUAL(ret, strlen(pathname1), ret, EXIT3);
    ICUNIT_GOTO_STRING_EQUAL(readbuf, pathname1, readbuf, EXIT3);

    ret = readlink(pathname2, readbuf, readSize);
    ICUNIT_GOTO_EQUAL(ret, readSize - 1, ret, EXIT3);
    ICUNIT_GOTO_STRING_EQUAL(readbuf, "/s", readbuf, EXIT3);

    ret = unlink(pathname2);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT3);

    ret = unlink(pathname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

    return JFFS_NO_ERROR;

EXIT3:
    unlink(pathname2);
    goto EXIT1;
EXIT2:
    close(fd);
EXIT1:
    unlink(pathname1);
EXIT:
    return JFFS_NO_ERROR;
}

VOID ItFsTestReadlink001(VOID)
{
    TEST_ADD_CASE("IT_FS_TEST_READLINK_001", testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL0, TEST_FUNCTION);
}

