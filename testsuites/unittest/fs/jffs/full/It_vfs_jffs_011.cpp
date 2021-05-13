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
    INT32 ret;
    INT32 fd = -1;
    CHAR pathname[50] = { JFFS_PATH_NAME0 };
    DIR *dir = NULL;
    struct dirent *ptr = NULL;
    INT32 offset;

    ret = mkdir(pathname, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    dir = opendir(JFFS_PATH_NAME0);
    ICUNIT_GOTO_NOT_EQUAL(dir, NULL, dir, EXIT1);

    strcat_s(pathname, sizeof(pathname), "/0test");
    ret = mkdir(pathname, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);

    strcat_s(pathname, sizeof(pathname), "/1test");
    ret = mkdir(pathname, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT3);

    strcat_s(pathname, sizeof(pathname), "/2test");
    ret = mkdir(pathname, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT4);

    strcat_s(pathname, sizeof(pathname), "/0file");
    fd = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, EXIT5);

    JffsStrcat2(pathname, "/0test/1test/2test", sizeof(pathname));
    ret = rmdir(pathname);
    ICUNIT_GOTO_NOT_EQUAL(ret, 0, ret, EXIT5);

    JffsStrcat2(pathname, "/0test/1test", sizeof(pathname));
    ret = rmdir(pathname);
    ICUNIT_GOTO_NOT_EQUAL(ret, 0, ret, EXIT5);

    JffsStrcat2(pathname, "/0test", sizeof(pathname));
    ret = rmdir(pathname);
    ICUNIT_GOTO_NOT_EQUAL(ret, 0, ret, EXIT5);

    JffsStrcat2(pathname, "/0test/1test/2test/0file", sizeof(pathname));
    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT5);
    ret = remove(pathname);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT5);

    JffsStrcat2(pathname, "/0test/1test/2test", sizeof(pathname));
    ret = rmdir(pathname);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT4);

    JffsStrcat2(pathname, "/0test", sizeof(pathname));
    ret = rmdir(pathname);
    ICUNIT_GOTO_NOT_EQUAL(ret, 0, ret, EXIT4);

    JffsStrcat2(pathname, "/0test/1test", sizeof(pathname));
    ret = rmdir(pathname);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT3);

    JffsStrcat2(pathname, "/0test", sizeof(pathname));
    ret = rmdir(pathname);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);

    ret = closedir(dir);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);

    ret = rmdir(JFFS_PATH_NAME0);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = rmdir(JFFS_PATH_NAME0);
    ICUNIT_GOTO_NOT_EQUAL(ret, 0, ret, EXIT);
    return JFFS_NO_ERROR;

EXIT5:
    JffsStrcat2(pathname, "/0test/1test/2test/0file", sizeof(pathname));
    close(fd);
    remove(pathname);
EXIT4:
    JffsStrcat2(pathname, "/0test/1test/2test", sizeof(pathname));
    rmdir(pathname);
EXIT3:
    JffsStrcat2(pathname, "/0test/1test", sizeof(pathname));
    rmdir(pathname);
EXIT2:
    JffsStrcat2(pathname, "/0test", sizeof(pathname));
    rmdir(pathname);
EXIT1:
    ret = closedir(dir);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);
EXIT:
    rmdir(JFFS_PATH_NAME0);
    return JFFS_NO_ERROR;
}

VOID ItFsJffs011(VOID)
{
    TEST_ADD_CASE("IT_FS_JFFS_011", Testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL2, TEST_FUNCTION);
}

