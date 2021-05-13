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
    CHAR pathname1[50] = { JFFS_PATH_NAME0 };
    CHAR bufdir1[50] = { JFFS_PATH_NAME0 };
    CHAR bufdir2[50] = { JFFS_PATH_NAME0 };

    ret = mkdir(pathname1, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    strcat_s(pathname1, sizeof(pathname1), "/dir");
    ret = mkdir(pathname1, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);

    strcat_s(bufdir1, sizeof(bufdir1), "/dir/dirfile1");
    ret = mkdir(bufdir1, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);

    strcat_s(bufdir2, sizeof(bufdir2), "/dir/dirfile1/dirfile2");
    ret = rename(bufdir2, bufdir1);
    ICUNIT_GOTO_NOT_EQUAL(ret, 0, ret, EXIT3);

    ret = rmdir(bufdir1);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);

    ret = rmdir(bufdir2);
    ICUNIT_GOTO_NOT_EQUAL(ret, 0, ret, EXIT3);

    ret = rmdir(pathname1);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);

    ret = rmdir(JFFS_PATH_NAME0);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);
    return JFFS_NO_ERROR;

EXIT3:
    JffsStrcat2(pathname1, "/dir/dirfile1/dirfile2", strlen(pathname1));
    remove(pathname1);
    goto EXIT1;
EXIT2:
    JffsStrcat2(pathname1, "/dir/dirfile1", strlen(pathname1));
    remove(pathname1);
EXIT1:
    JffsStrcat2(pathname1, "dir", strlen(pathname1));
    remove(pathname1);
EXIT:
    remove(JFFS_PATH_NAME0);
    return JFFS_NO_ERROR;
}

VOID ItFsJffs025(VOID)
{
    TEST_ADD_CASE("IT_FS_JFFS_025", Testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL2, TEST_FUNCTION);
}

