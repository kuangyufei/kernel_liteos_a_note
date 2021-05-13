/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2021 Huawei
 * Device Co., Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source
 * code must retain the above copyright notice, this list of
 * conditions and the following disclaimer.
 *
 * 2.
 * Redistributions in binary form must reproduce the above copyright notice, this list
 * of conditions and the
 * following disclaimer in the documentation and/or other materials
 * provided with the distribution.
 *
 * 3.
 * Neither the name of the copyright holder nor the names of its contributors may be used
 * to endorse or promote
 * products derived from this software without specific prior written
 * permission.
 *
 * THIS SOFTWARE IS PROVIDED
 * BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "It_vfs_fat.h"

static UINT32 TestCase(VOID)
{
    INT32 ret;
    INT32 i = 0;
    CHAR pathname1[FAT_STANDARD_NAME_LENGTH] = FAT_PATH_NAME;
    CHAR pathname2[50][FAT_STANDARD_NAME_LENGTH] = {FAT_PATH_NAME, };
    CHAR bufname[FAT_SHORT_ARRAY_LENGTH] = "";
    DIR *dir = NULL;
    DIR *dir1[50] = {NULL, };

    ret = chdir("/");
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT0);

    ret = umount(FAT_MOUNT_DIR);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT4);

    ret = format(FAT_DEV_PATH, 8, 2); // cluster size 8 2 for FAT32
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT4);

    ret = mount(FAT_DEV_PATH1, FAT_MOUNT_DIR, "vfat", 0, NULL);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT4);

    ret = mkdir(pathname1, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    dir = opendir(pathname1);
    ICUNIT_GOTO_NOT_EQUAL(dir, NULL, dir, EXIT1);

    for (i = 0; i < 50; i++) { // loop 50 times
        (void)memset_s(bufname, FAT_SHORT_ARRAY_LENGTH, 0, FAT_SHORT_ARRAY_LENGTH);
        (void)memset_s(pathname2[i], FAT_STANDARD_NAME_LENGTH, 0, FAT_SHORT_ARRAY_LENGTH);
        (void)snprintf_s(bufname, FAT_SHORT_ARRAY_LENGTH, FAT_SHORT_ARRAY_LENGTH, "/test%d", i);
        FatStrcat2(pathname2[i], bufname, strlen(bufname));

        ret = mkdir(pathname2[i], S_IRWXU | S_IRWXG | S_IRWXO);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);
    }

    for (i = 0; i < 50; i++) { // loop 50 times
        dir1[i] = opendir(pathname2[i]);
        ICUNIT_GOTO_NOT_EQUAL(dir1[i], NULL, dir1[i], EXIT3);
    }

    for (i = 0; i < 50; i++) { // loop 50 times
        ret = closedir(dir1[i]);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT3);
    }

    ret = closedir(dir);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

    for (i = 0; i < 50; i++) { // loop 50 times
        ret = remove(pathname2[i]);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);
    }

    ret = rmdir(pathname1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    return FAT_NO_ERROR;
EXIT4:
    mount(FAT_DEV_PATH1, FAT_MOUNT_DIR, "vfat", 0, NULL);
    goto EXIT;
EXIT3:
    for (i = 0; i < 50; i++) { // loop 50 times
        closedir(dir1[i]);
    }
EXIT2:
    for (i = 0; i < 50; i++) { // loop 50 times
        remove(pathname2[i]);
    }
EXIT1:
    closedir(dir);
EXIT:
    remove(pathname1);
EXIT0:
    return FAT_NO_ERROR;
}

VOID ItFsFatPressure306(VOID)
{
    TEST_ADD_CASE("IT_FS_FAT_PRESSURE_306", TestCase, TEST_VFS, TEST_VFAT, TEST_LEVEL3, TEST_PRESSURE);
}
