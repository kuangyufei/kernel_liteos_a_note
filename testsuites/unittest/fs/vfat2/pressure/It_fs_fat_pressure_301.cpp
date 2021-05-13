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
    INT32 fd = -1;
    INT32 len;
    INT32 i = FAT_SHORT_ARRAY_LENGTH;
    CHAR pathname1[FAT_STANDARD_NAME_LENGTH] = FAT_PATH_NAME;
    CHAR buffile[FAT_STANDARD_NAME_LENGTH] = FAT_PATH_NAME;
    CHAR filebuf[20 + 1] = "12345678901234567890";
    struct statfs buf1 = { 0 };
    struct statfs buf2 = { 0 };
    struct statfs buf3 = { 0 };
    struct statfs buf4 = { 0 };

    ret = chdir("/");
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT0);

    ret = umount(FAT_MOUNT_DIR);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

    ret = format(FAT_DEV_PATH, 8, 2); // cluster size 8 2 for FAT32
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

    ret = mount(FAT_DEV_PATH1, FAT_MOUNT_DIR, "vfat", 0, NULL);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

    ret = mkdir(pathname1, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    (void)strcat_s(pathname1, FAT_STANDARD_NAME_LENGTH, "/cdir");
    ret = mkdir(pathname1, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    (void)strcat_s(buffile, FAT_STANDARD_NAME_LENGTH, "/cdir/files");
    fd = open(buffile, O_NONBLOCK | O_CREAT | O_RDWR, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_NOT_EQUAL(fd, FAT_IS_ERROR, fd, EXIT);

    ret = statfs(pathname1, &buf1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);
    FatStatfsPrintf(buf1);

    ret = statfs(buffile, &buf2);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);
    FatStatfsPrintf(buf2);

    ICUNIT_GOTO_EQUAL(buf1.f_type, buf2.f_type, buf1.f_type, EXIT);
    ICUNIT_GOTO_EQUAL(buf1.f_bavail, buf2.f_bavail, buf1.f_bavail, EXIT);
    ICUNIT_GOTO_EQUAL(buf1.f_bsize, buf2.f_bsize, buf1.f_bsize, EXIT);
    ICUNIT_GOTO_EQUAL(buf1.f_blocks, buf2.f_blocks, buf1.f_blocks, EXIT);
    ICUNIT_GOTO_EQUAL(buf1.f_files, buf2.f_files, buf1.f_files, EXIT);

    while (i--) {
        len = write(fd, filebuf, strlen(filebuf));
        ICUNIT_GOTO_EQUAL((UINT32)len, strlen(filebuf), len, EXIT);
    }

    ret = statfs(pathname1, &buf3);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);
    FatStatfsPrintf(buf3);

    ret = statfs(buffile, &buf4);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);
    FatStatfsPrintf(buf4);

    ICUNIT_GOTO_EQUAL(buf3.f_type, 0x4d44, buf3.f_type, EXIT);
    ICUNIT_GOTO_EQUAL(buf4.f_type, 0x4d44, buf4.f_type, EXIT);
    ICUNIT_GOTO_EQUAL(buf3.f_namelen, 0xFF, buf3.f_namelen, EXIT);
    ICUNIT_GOTO_EQUAL(buf4.f_namelen, 0xFF, buf4.f_namelen, EXIT);
    ICUNIT_GOTO_EQUAL(buf3.f_bsize, 4096, buf3.f_bsize, EXIT); // 4096 for size 4k
    ICUNIT_GOTO_EQUAL(buf4.f_bsize, 4096, buf4.f_bsize, EXIT); // 4096 for size 4k

    i = 250; // loop time 250
    while (i--) {
        len = write(fd, filebuf, strlen(filebuf));
        ICUNIT_GOTO_EQUAL((UINT32)len, strlen(filebuf), len, EXIT);
    }

    ret = statfs(pathname1, &buf3);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);
    FatStatfsPrintf(buf3);

    ret = statfs(buffile, &buf4);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);
    FatStatfsPrintf(buf4);

    FatDeleteFile(fd, buffile);

    ret = rmdir(pathname1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    ret = rmdir(FAT_PATH_NAME);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    return FAT_NO_ERROR;
EXIT2:
    mount(FAT_DEV_PATH1, FAT_MOUNT_DIR, "vfat", 0, NULL);
    return FAT_NO_ERROR;
EXIT:
    format(FAT_DEV_PATH, 8, 0); // cluster size 8, 0 for auto
EXIT0:
    return FAT_NO_ERROR;
}

VOID ItFsFatPressure301(VOID)
{
    TEST_ADD_CASE("IT_FS_FAT_PRESSURE_301", TestCase, TEST_VFS, TEST_VFAT, TEST_LEVEL3, TEST_PRESSURE);
}