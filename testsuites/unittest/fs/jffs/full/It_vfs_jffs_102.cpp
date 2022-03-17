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
    INT32 ret, i, scandirCount;
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = { JFFS_MAIN_DIR0 };
    CHAR pathname2[JFFS_STANDARD_NAME_LENGTH] = { JFFS_MAIN_DIR0 };
    CHAR pathname3[JFFS_NAME_LIMITTED_SIZE] = { JFFS_MAIN_DIR0 };
    CHAR pathname4[JFFS_NAME_LIMITTED_SIZE] = "";
    CHAR pathname5[JFFS_NAME_LIMITTED_SIZE] = "";
    CHAR pathname6[JFFS_STANDARD_NAME_LENGTH] = "/jf";
    CHAR pathname7[JFFS_STANDARD_NAME_LENGTH] = "/jf";

    struct dirent **namelist = NULL;

    ret = chdir(pathname2);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    (void)memset_s(pathname4, JFFS_NAME_LIMITTED_SIZE, 0, JFFS_NAME_LIMITTED_SIZE);
    (void)memset_s(pathname5, JFFS_NAME_LIMITTED_SIZE, 0, JFFS_NAME_LIMITTED_SIZE);
    strcat_s(pathname3, sizeof(pathname3), "/");

    while (i < 252) { // 252 means loop times
        i++;
        strcat_s(pathname4, sizeof(pathname4), "t");
        strcat_s(pathname5, sizeof(pathname5), "t");
        strcat_s(pathname3, sizeof(pathname3), "t");
    }
    ICUNIT_GOTO_EQUAL(strlen(pathname3), 259, strlen(pathname3), EXIT); // 259 means current name len
    ICUNIT_GOTO_EQUAL(strlen(pathname4), 252, strlen(pathname4), EXIT); // 252 means current name len

    fd = open(pathname3, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, EXIT);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

    fd = open(pathname4, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(fd, -1, fd, EXIT2);
    ICUNIT_GOTO_EQUAL(errno, EEXIST, errno, EXIT2);

    strcat_s(pathname5, sizeof(pathname5), "t");
    fd = open(pathname5, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(fd, -1, fd, EXIT1);
    ICUNIT_GOTO_EQUAL(errno, ENAMETOOLONG, errno, EXIT2);

    strcat_s(pathname3, sizeof(pathname3), "t");
    ICUNIT_GOTO_EQUAL(strlen(pathname3), 260, strlen(pathname3), EXIT); // 260 means current name len
    fd = open(pathname3, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(fd, -1, fd, EXIT1);
    ICUNIT_GOTO_EQUAL(errno, ENAMETOOLONG, errno, EXIT2);

    ret = umount(JFFS_MOUNT_DIR0);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT3);

    ret = mount(JFFS_DEV_PATH0, pathname6, JFFS_FILESYS_TYPE, 0, NULL);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT4);

    ret = chdir(pathname6);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT4);

    ret = access(pathname4, F_OK);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT4);

    fd = open(pathname5, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, EXIT4);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT4);

    scandirCount = scandir(pathname7, &namelist, 0, alphasort);
    ICUNIT_GOTO_EQUAL(scandirCount, 2, scandirCount, EXIT5); // 2 means total scaned file

    JffsScandirFree(namelist, scandirCount);

    ret = umount(pathname6);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT4);

    ret = remove(pathname6);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT4);

    ret = mount(JFFS_DEV_PATH0, JFFS_MAIN_DIR0, JFFS_FILESYS_TYPE, 0, NULL);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT4);

    ret = chdir(pathname1);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT4);

    scandirCount = scandir(pathname1, &namelist, 0, alphasort);
    ICUNIT_GOTO_EQUAL(scandirCount, 2, scandirCount, EXIT5); // 2 means total scaned file

    JffsScandirFree(namelist, scandirCount);

    ret = remove(pathname4);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT4);

    ret = remove(pathname5);
    ICUNIT_GOTO_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT4);

    ret = umount(JFFS_MOUNT_DIR0);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT4);

    ret = mount(JFFS_DEV_PATH0, pathname6, JFFS_FILESYS_TYPE, 0, NULL);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT4);

    ret = chdir(pathname6);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT4);

    ret = remove(pathname5);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);

    ret = umount(pathname6);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT4);

    ret = remove(pathname6);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT4);

    ret = mount(JFFS_DEV_PATH0, JFFS_MAIN_DIR0, JFFS_FILESYS_TYPE, 0, NULL);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT4);

    return JFFS_NO_ERROR;
EXIT5:
    JffsScandirFree(namelist, scandirCount);
EXIT4:
    umount(pathname6);
    remove(pathname6);
EXIT3:
    mount(JFFS_DEV_PATH0, JFFS_MAIN_DIR0, JFFS_FILESYS_TYPE, 0, NULL);
EXIT2:
    close(fd);
    remove(pathname5);
    remove(pathname3);
EXIT1:
    close(fd);
    remove(pathname4);
EXIT:
    close(fd);
    remove(pathname1);
    return JFFS_NO_ERROR;
}


VOID ItFsJffs102(VOID)
{
    TEST_ADD_CASE("IT_FS_JFFS_102", Testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL2, TEST_FUNCTION);
}

