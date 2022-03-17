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
    INT32 ret, i, scandirCount;
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR pathname2[JFFS_NAME_LIMITTED_SIZE] = "";
    CHAR pathname3[JFFS_NAME_LIMITTED_SIZE] = "";
    CHAR pathname4[JFFS_STANDARD_NAME_LENGTH] = "/jf";
    CHAR pathname5[JFFS_STANDARD_NAME_LENGTH] = "/jf/test";
    CHAR pathname6[JFFS_NAME_LIMITTED_SIZE] = { JFFS_PATH_NAME0 };
    struct dirent **namelist = NULL;

    ret = mkdir(pathname1, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = chdir(pathname1);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    (void)memset_s(pathname2, JFFS_NAME_LIMITTED_SIZE, 0, JFFS_NAME_LIMITTED_SIZE);
    (void)memset_s(pathname3, JFFS_NAME_LIMITTED_SIZE, 0, JFFS_NAME_LIMITTED_SIZE);
    strcat_s(pathname6, sizeof(pathname6), "/");

    while (i < 247) { // 247 means loop times
        i++;
        strcat_s(pathname2, sizeof(pathname2), "t");
        strcat_s(pathname3, sizeof(pathname3), "t");
        strcat_s(pathname6, sizeof(pathname6), "t");
    }
    ICUNIT_GOTO_EQUAL(strlen(pathname6), 259, strlen(pathname6), EXIT); // 259 means current name len
    ICUNIT_GOTO_EQUAL(strlen(pathname2), 247, strlen(pathname2), EXIT); // 247 means current name len
    ret = mkdir(pathname2, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    ret = mkdir(pathname6, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT1);
    ICUNIT_GOTO_EQUAL(errno, EEXIST, errno, EXIT1);

    strcat_s(pathname3, sizeof(pathname3), "t");
    ret = mkdir(pathname3, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT2);
    ICUNIT_GOTO_EQUAL(errno, ENAMETOOLONG, errno, EXIT2);

    strcat_s(pathname6, sizeof(pathname6), "t");
    ICUNIT_GOTO_EQUAL(strlen(pathname6), 260, strlen(pathname6), EXIT); // 260 means current name len
    ret = mkdir(pathname6, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT2);
    ICUNIT_GOTO_EQUAL(errno, ENAMETOOLONG, errno, EXIT2);

    ret = umount(JFFS_MOUNT_DIR0);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT3);

    ret = mount(JFFS_DEV_PATH0, pathname4, JFFS_FILESYS_TYPE, 0, NULL);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT4);

    ret = chdir(pathname5);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT4);

    ret = access(pathname2, F_OK);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT4);

    ret = mkdir(pathname3, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT4);

    scandirCount = scandir(pathname5, &namelist, 0, alphasort);
    ICUNIT_GOTO_EQUAL(scandirCount, 2, scandirCount, EXIT5); // 2 means total scaned file

    JffsScandirFree(namelist, scandirCount);

    ret = umount(pathname4);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT4);

    ret = remove(pathname4);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT4);

    ret = mount(JFFS_DEV_PATH0, JFFS_MAIN_DIR0, JFFS_FILESYS_TYPE, 0, NULL);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT4);

    ret = chdir(pathname1);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT4);

    scandirCount = scandir(pathname1, &namelist, 0, alphasort);
    ICUNIT_GOTO_EQUAL(scandirCount, 2, scandirCount, EXIT5); // 2 means total scaned file

    JffsScandirFree(namelist, scandirCount);

    ret = rmdir(pathname3);
    ICUNIT_GOTO_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT4);
    ICUNIT_GOTO_EQUAL(errno, ENAMETOOLONG, errno, EXIT4);

    ret = rmdir(pathname6);
    ICUNIT_GOTO_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT4);
    ICUNIT_GOTO_EQUAL(errno, ENAMETOOLONG, errno, EXIT4);

    ret = rmdir(pathname2);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT4);

    ret = umount(JFFS_MOUNT_DIR0);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT4);

    ret = mount(JFFS_DEV_PATH0, pathname4, JFFS_FILESYS_TYPE, 0, NULL);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT4);

    ret = chdir(pathname5);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT4);

    ret = rmdir(pathname3);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

    ret = umount(pathname4);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT4);

    ret = remove(pathname4);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT4);

    ret = mount(JFFS_DEV_PATH0, JFFS_MAIN_DIR0, JFFS_FILESYS_TYPE, 0, NULL);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT4);

    ret = rmdir(pathname1);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    return JFFS_NO_ERROR;
EXIT5:
    JffsScandirFree(namelist, scandirCount);
EXIT4:
    umount(pathname4);
    remove(pathname4);
EXIT3:
    mount(JFFS_DEV_PATH0, JFFS_MAIN_DIR0, JFFS_FILESYS_TYPE, 0, NULL);
EXIT2:
    remove(pathname3);
    remove(pathname6);
EXIT1:
    rmdir(pathname2);
EXIT:
    rmdir(pathname1);
    return JFFS_NO_ERROR;
}

VOID ItFsJffs101(VOID)
{
    TEST_ADD_CASE("IT_FS_JFFS_101", Testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL2, TEST_FUNCTION);
}

