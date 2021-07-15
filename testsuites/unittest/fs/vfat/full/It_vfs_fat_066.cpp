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

static const int CLUSTER_SIZE4 = 4;
static const int CLUSTER_SIZE8 = 8;
static const int CLUSTER_SIZE16 = 16;
static const int CLUSTER_SIZE32 = 32;
static const int CLUSTER_SIZE64 = 64;
static const int CLUSTER_SIZE128 = 128;
static const int CLUSTER_SIZE255 = 255;
static const int CLUSTER_SIZE256 = 256;
static const int SECTOR_SIZE = 512;

static UINT32 TestCase(VOID)
{
    INT32 ret;
    struct stat buf = { 0 };

    ret = chdir("/");
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = umount(FAT_MOUNT_DIR);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

    ret = format(NULL, CLUSTER_SIZE8, FAT_FILE_SYSTEMTYPE_AUTO);
    ICUNIT_GOTO_EQUAL(ret, -1, ret, EXIT2);

    ret = mount(FAT_DEV_PATH1, FAT_MOUNT_DIR, "vfat", 0, NULL);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

    ret = umount(FAT_MOUNT_DIR);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

    ret = format(FAT_DEV_PATH, CLUSTER_SIZE8, FAT_FILE_SYSTEMTYPE_AUTO);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

    ret = mount(FAT_DEV_PATH1, FAT_MOUNT_DIR, "vfat", 0, NULL);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

    ret = mkdir(FAT_PATH_NAME, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = access(FAT_PATH_NAME, F_OK);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = umount(FAT_MOUNT_DIR);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

    ret = format(FAT_DEV_PATH, 0, FAT_FILE_SYSTEMTYPE_AUTO);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

    ret = mount(FAT_DEV_PATH1, FAT_MOUNT_DIR, "vfat", 0, NULL);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

    ret = access(FAT_PATH_NAME, F_OK);
    ICUNIT_GOTO_EQUAL(ret, -1, ret, EXIT1);

    ret = mkdir(FAT_PATH_NAME, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = access(FAT_PATH_NAME, F_OK);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = stat(FAT_PATH_NAME, &buf);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);
    if (g_fatFilesystem != FAT_FILE_SYSTEMTYPE_EXFAT)
        ICUNIT_GOTO_EQUAL(buf.st_blksize, CLUSTER_SIZE64 * SECTOR_SIZE, buf.st_blksize, EXIT1);

    ret = umount(FAT_MOUNT_DIR);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

    ret = format(FAT_DEV_PATH, CLUSTER_SIZE16, FAT_FILE_SYSTEMTYPE_AUTO);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

    ret = mount(FAT_DEV_PATH1, FAT_MOUNT_DIR, "vfat", 0, NULL);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

    ret = access(FAT_PATH_NAME, F_OK);
    ICUNIT_GOTO_EQUAL(ret, -1, ret, EXIT1);

    ret = mkdir(FAT_PATH_NAME, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = access(FAT_PATH_NAME, F_OK);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = stat(FAT_PATH_NAME, &buf);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ICUNIT_GOTO_EQUAL(buf.st_blksize, CLUSTER_SIZE16 * SECTOR_SIZE, buf.st_blksize, EXIT1);

    ret = umount(FAT_MOUNT_DIR);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

    ret = format(FAT_DEV_PATH, CLUSTER_SIZE32, FAT_FILE_SYSTEMTYPE_AUTO);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

    ret = mount(FAT_DEV_PATH1, FAT_MOUNT_DIR, "vfat", 0, NULL);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

    ret = access(FAT_PATH_NAME, F_OK);
    ICUNIT_GOTO_EQUAL(ret, -1, ret, EXIT1);

    ret = mkdir(FAT_PATH_NAME, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = access(FAT_PATH_NAME, F_OK);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = stat(FAT_PATH_NAME, &buf);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);
    ICUNIT_GOTO_EQUAL(buf.st_blksize, CLUSTER_SIZE32 * SECTOR_SIZE, buf.st_blksize, EXIT1);

    ret = umount(FAT_MOUNT_DIR);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

    ret = format(FAT_DEV_PATH, CLUSTER_SIZE64, FAT_FILE_SYSTEMTYPE_AUTO);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

    ret = mount(FAT_DEV_PATH1, FAT_MOUNT_DIR, "vfat", 0, NULL);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

    ret = access(FAT_PATH_NAME, F_OK);
    ICUNIT_GOTO_EQUAL(ret, -1, ret, EXIT1);

    ret = mkdir(FAT_PATH_NAME, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = access(FAT_PATH_NAME, F_OK);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = stat(FAT_PATH_NAME, &buf);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);
    ICUNIT_GOTO_EQUAL(buf.st_blksize, CLUSTER_SIZE64 * SECTOR_SIZE, buf.st_blksize, EXIT1);

    ret = umount(FAT_MOUNT_DIR);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

    ret = format(FAT_DEV_PATH, CLUSTER_SIZE128, FAT_FILE_SYSTEMTYPE_AUTO);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

    ret = mount(FAT_DEV_PATH1, FAT_MOUNT_DIR, "vfat", 0, NULL);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

    ret = access(FAT_PATH_NAME, F_OK);
    ICUNIT_GOTO_EQUAL(ret, -1, ret, EXIT1);

    ret = mkdir(FAT_PATH_NAME, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = access(FAT_PATH_NAME, F_OK);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = stat(FAT_PATH_NAME, &buf);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);
    ICUNIT_GOTO_EQUAL(buf.st_blksize, CLUSTER_SIZE128 * SECTOR_SIZE, buf.st_blksize, EXIT1);

    ret = umount(FAT_MOUNT_DIR);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

    ret = format(FAT_DEV_PATH, CLUSTER_SIZE4, FAT_FILE_SYSTEMTYPE_AUTO);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = mount(FAT_DEV_PATH1, FAT_MOUNT_DIR, "vfat", 0, NULL);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

    ret = access(FAT_PATH_NAME, F_OK);
    ICUNIT_GOTO_EQUAL(ret, -1, ret, EXIT1);

    ret = mkdir(FAT_PATH_NAME, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = access(FAT_PATH_NAME, F_OK);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = stat(FAT_PATH_NAME, &buf);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ICUNIT_GOTO_EQUAL(buf.st_blksize, CLUSTER_SIZE4 * SECTOR_SIZE, buf.st_blksize, EXIT1);

    ret = umount(FAT_MOUNT_DIR);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

    ret = format(FAT_DEV_PATH, 1, FAT_FILE_SYSTEMTYPE_AUTO);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

    ret = mount(FAT_DEV_PATH1, FAT_MOUNT_DIR, "vfat", 0, NULL);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

    ret = access(FAT_PATH_NAME, F_OK);
    ICUNIT_GOTO_EQUAL(ret, -1, ret, EXIT1);

    ret = mkdir(FAT_PATH_NAME, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = access(FAT_PATH_NAME, F_OK);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = stat(FAT_PATH_NAME, &buf);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ICUNIT_GOTO_EQUAL(buf.st_blksize, SECTOR_SIZE, buf.st_blksize, EXIT1);

    ret = umount(FAT_MOUNT_DIR);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

    ret = format(FAT_DEV_PATH, CLUSTER_SIZE255, FAT_FILE_SYSTEMTYPE_AUTO);
    ICUNIT_GOTO_EQUAL(ret, FAT_IS_ERROR, ret, EXIT2);
    ICUNIT_GOTO_EQUAL(errno, EINVAL, errno, EXIT2);

    ret = mount(FAT_DEV_PATH1, FAT_MOUNT_DIR, "vfat", 0, NULL);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

    ret = umount(FAT_MOUNT_DIR);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

    ret = format(FAT_DEV_PATH, CLUSTER_SIZE256, FAT_FILE_SYSTEMTYPE_AUTO);
    if (g_fatFilesystem == FAT_FILE_SYSTEMTYPE_EXFAT)
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);
    else {
        ICUNIT_GOTO_EQUAL(ret, FAT_IS_ERROR, ret, EXIT2);
        ICUNIT_GOTO_EQUAL(errno, EINVAL, errno, EXIT2);
    }

    ret = mount(FAT_DEV_PATH1, FAT_MOUNT_DIR, "vfat", 0, NULL);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

    ret = remove(FAT_PATH_NAME);
    if (g_fatFilesystem == FAT_FILE_SYSTEMTYPE_EXFAT) {
        ICUNIT_GOTO_EQUAL(ret, FAT_IS_ERROR, ret, EXIT1);
        ICUNIT_GOTO_EQUAL(errno, ENOENT, errno, EXIT1);
    } else
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = mkdir(FAT_PATH_NAME, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = access(FAT_PATH_NAME, F_OK);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = stat(FAT_PATH_NAME, &buf);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = umount(FAT_MOUNT_DIR);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

    ret = format(FAT_DEV_PATH, CLUSTER_SIZE8, FAT_FILE_SYSTEMTYPE_AUTO);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

    ret = mount(FAT_DEV_PATH1, FAT_MOUNT_DIR, "vfat", 0, NULL);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

    ret = access(FAT_PATH_NAME, F_OK);
    ICUNIT_GOTO_EQUAL(ret, FAT_IS_ERROR, ret, EXIT1);

    ret = mkdir(FAT_PATH_NAME, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = access(FAT_PATH_NAME, F_OK);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = stat(FAT_PATH_NAME, &buf);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ICUNIT_GOTO_EQUAL(buf.st_blksize, CLUSTER_SIZE8 * SECTOR_SIZE, buf.st_blksize, EXIT1);

    ret = rmdir(FAT_PATH_NAME);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    return FAT_NO_ERROR;
EXIT2:
    mount(FAT_DEV_PATH1, FAT_MOUNT_DIR, "vfat", 0, NULL);
EXIT1:
    rmdir(FAT_PATH_NAME);

    return FAT_NO_ERROR;
}

/* *
* -@test IT_FS_FAT_066
* -@tspec The function test for filesystem
* -@ttitle Format the SD card with some different parameters
* -@tprecon The filesystem module is open
* -@tbrief
1. create a directory;
2. use access to check the directory whether is existed
3. format the SD card with different parameters;
4. repeat the above process several times.
* -@texpect
1. Return successed
2. Return successed
3. Return successed
4. Sucessful operation
* -@tprior 1
* -@tauto TRUE
* -@tremark
*/

VOID ItFsFat066(VOID)
{
    TEST_ADD_CASE("IT_FS_FAT_066", TestCase, TEST_VFS, TEST_VFAT, TEST_LEVEL2, TEST_FUNCTION);
}
