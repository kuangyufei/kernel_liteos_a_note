/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of
 * conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list
 * of conditions and the following disclaimer in the documentation and/or other materials
 * provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used
 * to endorse or promote products derived from this software without specific prior written
 * permission.
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

static UINT32 TestCase(VOID)
{
    INT32 fd = -1;
    INT32 fd1 = -1;
    INT32 ret, len;
    INT32 i = 0;
    static UINT32 flag = 0;
    signed long long offset;

    CHAR filebuf[JFFS_STANDARD_NAME_LENGTH] = "0000000000111111111122222222223333333333";
    CHAR readbuf[JFFS_STANDARD_NAME_LENGTH] = "liteos";
    CHAR bufname[JFFS_SHORT_ARRAY_LENGTH] = "";
    CHAR pathname1[JFFS_NAME_LIMITTED_SIZE] = { JFFS_PATH_NAME0 };
    CHAR pathname2[JFFS_NAME_LIMITTED_SIZE] = { JFFS_PATH_NAME0 };
    CHAR pathname3[JFFS_NAME_LIMITTED_SIZE] = { JFFS_PATH_NAME0 };
    CHAR pathname4[JFFS_NAME_LIMITTED_SIZE] = { JFFS_PATH_NAME0 };
    CHAR pathname5[JFFS_NAME_LIMITTED_SIZE] = { JFFS_PATH_NAME0 };
    CHAR pathname6[JFFS_NAME_LIMITTED_SIZE] = { JFFS_PATH_NAME0 };
    CHAR buffile[JFFS_NAME_LIMITTED_SIZE] = { JFFS_PATH_NAME0 };
    struct stat buf1 = { 0 };
    struct stat buf2 = { 0 };

    DIR *dir1 = NULL;
    DIR *dir2 = NULL;
    DIR *dir3 = NULL;
    DIR *dir4 = NULL;
    DIR *dir5 = NULL;

    ret = mkdir(pathname1, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = chdir(pathname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = stat(buffile, &buf1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);
    JffsStatPrintf(buf1);

    strcat_s(pathname1, JFFS_NAME_LIMITTED_SIZE, "/test1");
    strcat_s(pathname2, JFFS_NAME_LIMITTED_SIZE, "/test2");
    strcat_s(pathname3, JFFS_NAME_LIMITTED_SIZE, "/test3");
    strcat_s(pathname4, JFFS_NAME_LIMITTED_SIZE, "/test4");
    strcat_s(pathname5, JFFS_NAME_LIMITTED_SIZE, "/test5");

    for (i = 0; i < JFFS_PRESSURE_CYCLES; i++) {
        ret = mkdir(pathname1, HIGHEST_AUTHORITY);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

        ret = mkdir(pathname2, HIGHEST_AUTHORITY);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

        ret = mkdir(pathname3, HIGHEST_AUTHORITY);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT3);

        ret = mkdir(pathname4, HIGHEST_AUTHORITY);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT4);

        ret = mkdir(pathname5, HIGHEST_AUTHORITY);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT5);

        dir1 = opendir(pathname1);
        ICUNIT_GOTO_NOT_EQUAL(dir1, NULL, dir1, EXIT6);

        dir2 = opendir(pathname2);
        ICUNIT_GOTO_NOT_EQUAL(dir2, NULL, dir2, EXIT7);

        dir3 = opendir(pathname3);
        ICUNIT_GOTO_NOT_EQUAL(dir3, NULL, dir3, EXIT8);

        dir4 = opendir(pathname4);
        ICUNIT_GOTO_NOT_EQUAL(dir4, NULL, dir4, EXIT9);

        dir5 = opendir(pathname5);
        ICUNIT_GOTO_NOT_EQUAL(dir5, NULL, dir5, EXIT10);

        ret = closedir(dir1);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

        ret = closedir(dir2);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

        ret = closedir(dir3);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

        ret = closedir(dir4);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

        ret = closedir(dir5);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

        ret = rmdir(pathname1);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT5);

        ret = rmdir(pathname2);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT5);

        ret = rmdir(pathname3);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT5);

        ret = rmdir(pathname4);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT5);

        ret = rmdir(pathname5);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT5);
    }

    ret = stat(buffile, &buf2);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);
    JffsStatPrintf(buf2);

    ret = chdir(JFFS_MAIN_DIR0);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = rmdir(pathname6);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    return JFFS_NO_ERROR;
EXIT10:
    closedir(dir5);
EXIT9:
    closedir(dir4);
EXIT8:
    closedir(dir3);
EXIT7:
    closedir(dir2);
EXIT6:
    closedir(dir1);
EXIT5:
    rmdir(pathname5);
EXIT4:
    rmdir(pathname4);
EXIT3:
    rmdir(pathname3);
EXIT2:
    rmdir(pathname2);
EXIT1:
    rmdir(pathname1);
EXIT:
    rmdir(pathname6);
    return JFFS_NO_ERROR;
}

VOID ItFsJffsPressure005(VOID)
{
    TEST_ADD_CASE("ItFsJffsPressure005", TestCase, TEST_VFS, TEST_JFFS, TEST_LEVEL3, TEST_PRESSURE);
}
