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

static constexpr int TEST_STRLEN = 30;
static UINT32 Testcase(VOID)
{
    INT32 ret;
    INT32 fd = -1;
    CHAR pathname[TEST_STRLEN] = { JFFS_BASE_DIR };
    CHAR pathname1[TEST_STRLEN] = { JFFS_PATH_NAME0 };
    CHAR pathname2[TEST_STRLEN] = { JFFS_PATH_NAME0 };
    CHAR *pathname3 = NULL;
    CHAR buf1[TEST_STRLEN] = "";
    CHAR buf2[TEST_STRLEN] = "";
    CHAR *pret = NULL;
    DIR *dir = NULL;

    ret = mkdir(pathname1, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    pathname3 = pathname2;
    strcat_s(pathname2, TEST_STRLEN, "/test1");
    fd = open(pathname2, O_NONBLOCK | O_CREAT | O_EXCL, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, EXIT1);

    pret = getcwd(buf1, TEST_STRLEN);
    ICUNIT_GOTO_NOT_EQUAL(pret, NULL, pret, EXIT1);
    ICUNIT_GOTO_STRING_EQUAL(buf1, pathname, buf1, EXIT1);

    ret = fchdir(fd);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    pret = getcwd(buf2, TEST_STRLEN);
    ICUNIT_GOTO_NOT_EQUAL(pret, NULL, pret, EXIT1);
    ICUNIT_GOTO_STRING_EQUAL(buf2, pathname3, buf2, EXIT1);

    ret = chdir(pathname);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    ret = unlink(pathname2);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    ret = rmdir(pathname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    return JFFS_NO_ERROR;
EXIT1:
    close(fd);
    unlink(pathname2);
EXIT:
    rmdir(pathname1);
    return JFFS_IS_ERROR;
}

VOID ItJffs028(VOID)
{
    TEST_ADD_CASE(__FUNCTION__, Testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL2, TEST_FUNCTION);
}

