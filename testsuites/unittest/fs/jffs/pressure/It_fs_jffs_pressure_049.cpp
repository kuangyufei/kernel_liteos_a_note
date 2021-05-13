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
    INT32 ret;
    INT32 i = 0;
    INT32 j = 0;
    INT32 k = 0;
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR pathname2[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR pathname3[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR bufname[JFFS_STANDARD_NAME_LENGTH] = "";
    struct stat statbuf1;
    struct stat statbuf2;

    g_TestCnt = 0;

    ret = access(pathname1, F_OK);
    ICUNIT_GOTO_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT);

    ret = mkdir(pathname1, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = stat(pathname1, &statbuf1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);
    ICUNIT_GOTO_EQUAL(statbuf1.st_size, 0, statbuf1.st_size, EXIT);

    while (i < 100) { // loop times: 100
        memset_s(bufname, JFFS_STANDARD_NAME_LENGTH, 0, JFFS_STANDARD_NAME_LENGTH);
        memset_s(pathname3, JFFS_STANDARD_NAME_LENGTH, 0, JFFS_STANDARD_NAME_LENGTH);
        snprintf_s(bufname, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "/_%d", i);
        strcat_s(pathname3, JFFS_STANDARD_NAME_LENGTH, pathname2);
        strcat_s(pathname3, JFFS_STANDARD_NAME_LENGTH, bufname);
        ret = mkdir(pathname3, HIGHEST_AUTHORITY);
        printf("[%d]mkdir cycle:%d name: %s, errno:%d\n", __LINE__, i, pathname3, errno);
        if (ret == -1) {
            break;
        }

        i++;
    }

    for (k = 0; k < JFFS_MAXIMUM_OPERATIONS; k++) {
        ret = chdir("/");
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

        ret = umount(JFFS_MOUNT_DIR0);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

        ret = mount(JFFS_DEV_PATH0, JFFS_MOUNT_DIR0, JFFS_FILESYS_TYPE, 0, NULL);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

        ret = chdir(JFFS_MAIN_DIR0);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

        ret = access(pathname1, F_OK);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);
    }

    ret = stat(pathname1, &statbuf2);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);
    ICUNIT_GOTO_EQUAL(statbuf2.st_size, statbuf1.st_size, statbuf2.st_size, EXIT1);

    for (j = 0; j < i; j++) {
        errno = 0;
        memset_s(bufname, JFFS_STANDARD_NAME_LENGTH, 0, JFFS_STANDARD_NAME_LENGTH);
        memset_s(pathname3, JFFS_STANDARD_NAME_LENGTH, 0, JFFS_STANDARD_NAME_LENGTH);
        snprintf_s(bufname, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "/_%d", j);
        strcat_s(pathname3, JFFS_STANDARD_NAME_LENGTH, pathname2);
        strcat_s(pathname3, JFFS_STANDARD_NAME_LENGTH, bufname);

        ret = remove(pathname3);
        printf("[%d]remove cycle:%d name: %s, errno:%d\n", __LINE__, j, pathname3, errno);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);
    }

    ret = stat(pathname1, &statbuf1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);
    ICUNIT_GOTO_EQUAL(statbuf2.st_size, statbuf1.st_size, statbuf2.st_size, EXIT);

    ret = chdir(JFFS_MAIN_DIR0);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = remove(pathname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    return JFFS_NO_ERROR;
EXIT2:
    mount(JFFS_DEV_PATH0, JFFS_MOUNT_DIR0, JFFS_FILESYS_TYPE, 0, NULL);
EXIT1:
    for (j = 0; j < i; j++) {
        memset_s(bufname, JFFS_STANDARD_NAME_LENGTH, 0, JFFS_STANDARD_NAME_LENGTH);
        memset_s(pathname3, JFFS_STANDARD_NAME_LENGTH, 0, JFFS_STANDARD_NAME_LENGTH);
        snprintf_s(bufname, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "/_%d", j);
        strcat_s(pathname3, JFFS_STANDARD_NAME_LENGTH, pathname2);
        strcat_s(pathname3, JFFS_STANDARD_NAME_LENGTH, bufname);

        remove(pathname3);
    }
EXIT:
    remove(pathname1);
    return JFFS_NO_ERROR;
}

VOID ItFsJffsPressure049(VOID)
{
    TEST_ADD_CASE("ItFsJffsPressure049", TestCase, TEST_VFS, TEST_JFFS, TEST_LEVEL3, TEST_PRESSURE);
}
