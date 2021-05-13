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
    INT32 ret;
    INT32 offset;
    CHAR pathname[50] = { JFFS_PATH_NAME0 };
    DIR *dir = NULL;
    struct dirent *ptr;

    g_TestCnt = 0;

    ret = mkdir(pathname, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    strcat(pathname, "/0test");

    ret = mkdir(pathname, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);

    JffsStrcat2(pathname, "/1test", sizeof(pathname));

    ret = mkdir(pathname, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);

    dir = opendir(JFFS_PATH_NAME0);
    ICUNIT_GOTO_NOT_EQUAL(dir, NULL, dir, EXIT2);

    ptr = readdir(dir);
    ICUNIT_GOTO_NOT_EQUAL(ptr, NULL, ptr, EXIT3);
    ICUNIT_GOTO_STRING_EQUAL(ptr->d_name, "1test", ptr->d_name, EXIT3);

    ptr = readdir(dir);
    ICUNIT_GOTO_NOT_EQUAL(ptr, NULL, ptr, EXIT3);
    ICUNIT_GOTO_STRING_EQUAL(ptr->d_name, "0test", ptr->d_name, EXIT3);

    ptr = readdir(dir);
    ICUNIT_GOTO_EQUAL(ptr, NULL, ptr, EXIT3);

    ret = closedir(dir);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT3);

    dir = opendir(JFFS_PATH_NAME0);
    ICUNIT_GOTO_NOT_EQUAL(dir, NULL, dir, EXIT2);

    ret = closedir(dir);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT3);

    ret = rmdir(JFFS_PATH_NAME0);
    ICUNIT_ASSERT_NOT_EQUAL(ret, 0, ret);

    JffsStrcat2(pathname, "/1test", sizeof(pathname));
    ret = rmdir(pathname);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);

    JffsStrcat2(pathname, "/0test", sizeof(pathname));
    ret = rmdir(pathname);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);

    ret = rmdir(JFFS_PATH_NAME0);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    dir = opendir(JFFS_PATH_NAME0);
    ICUNIT_GOTO_EQUAL(dir, NULL, dir, EXIT4);

    return JFFS_NO_ERROR;
EXIT4:
    closedir(dir);
    goto EXIT;
EXIT3:
    closedir(dir);
EXIT2:
    JffsStrcat2(pathname, "/1test", sizeof(pathname));
    rmdir(pathname);
EXIT1:
    JffsStrcat2(pathname, "/0test", sizeof(pathname));
    rmdir(pathname);
EXIT:
    rmdir(JFFS_PATH_NAME0);
    return JFFS_NO_ERROR;
}

VOID ItFsJffs002(VOID)
{
    TEST_ADD_CASE("IT_FS_JFFS_002", testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL0, TEST_FUNCTION);
}
