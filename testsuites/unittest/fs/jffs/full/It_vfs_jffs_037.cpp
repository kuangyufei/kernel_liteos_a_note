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
    INT32 fd1 = -1;
    INT32 fd2 = -1;
    INT32 ret, len;
    CHAR filebuf[20] = "1234567890";
    CHAR readbuf[50] = { 0 };
    CHAR pathname[50] = "/storage/liteos";
    CHAR pathname1[50] = "/storage/liteosliteosli";
    CHAR pathname2[50] = "/storage/liteosliteoslit";
    CHAR pathname3[50] = "/storage/liteos";
    CHAR pathname4[50] = "/storage/liteosliteosli";
    CHAR pathname5[50] = "/jffs/liteosliteoslit";
    off_t off;
    ret = mkdir(pathname, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);
    ret = rmdir(pathname);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = mkdir(pathname1, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);
    ret = rmdir(pathname1);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);

    ret = mkdir(pathname2, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);

    ret = rmdir(pathname2);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);

    fd = open(pathname3, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, EXIT3);
    JffsDeletefile(fd, pathname3);

    fd = open(pathname4, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, EXIT4);
    JffsDeletefile(fd, pathname4);

    fd = open(pathname5, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(fd, -1, fd, EXIT5);

    return JFFS_NO_ERROR;
EXIT5:
    close(fd);
    remove(pathname5);
    return JFFS_NO_ERROR;
EXIT4:
    close(fd);
    remove(pathname4);
    return JFFS_NO_ERROR;
EXIT3:
    close(fd);
    remove(pathname3);
    return JFFS_NO_ERROR;
EXIT2:
    remove(pathname2);
    return JFFS_NO_ERROR;
EXIT1:
    remove(pathname1);
    return JFFS_NO_ERROR;
EXIT:
    remove(pathname);
    return JFFS_NO_ERROR;
}

VOID ItFsJffs037(VOID)
{
    TEST_ADD_CASE("IT_FS_JFFS_037", Testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL2, TEST_FUNCTION);
}

