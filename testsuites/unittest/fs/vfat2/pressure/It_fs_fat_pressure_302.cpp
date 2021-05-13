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
    INT32 fd = -1;
    INT32 ret, len;
    CHAR pathname1[FAT_STANDARD_NAME_LENGTH] = FAT_PATH_NAME;
    CHAR buffile[FAT_STANDARD_NAME_LENGTH] = FAT_PATH_NAME;
    struct stat buf1 = { 0 };
    struct stat buf2 = { 0 };
    INT32 n = 103;

    ret = mkdir(pathname1, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    (void)strcat_s(pathname1, FAT_STANDARD_NAME_LENGTH, "/dir");
    ret = mkdir(pathname1, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    (void)strcat_s(buffile, FAT_STANDARD_NAME_LENGTH, "/dir/files");
    fd = open(buffile, O_NONBLOCK | O_CREAT | O_RDWR, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_NOT_EQUAL(fd, FAT_IS_ERROR, fd, EXIT);

    while (n--) {
        len = write(fd, "0123456789012345678901234567890123456789", 40); // write 40 bytes
        ICUNIT_GOTO_EQUAL(len, 40, len, EXIT);                           // len must be 40
    }

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    ret = stat(buffile, &buf1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    fd = open(buffile, O_NONBLOCK | O_RDWR, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_NOT_EQUAL(fd, FAT_IS_ERROR, fd, EXIT);

    ret = fstat(fd, &buf2);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    ICUNIT_GOTO_EQUAL(buf1.st_blocks, buf2.st_blocks, buf1.st_blocks, EXIT);
    ICUNIT_GOTO_EQUAL(buf1.st_size, buf2.st_size, buf1.st_size, EXIT);
    ICUNIT_GOTO_EQUAL(buf1.st_blksize, buf2.st_blksize, buf1.st_blksize, EXIT);
    ICUNIT_GOTO_EQUAL(buf1.st_ino, buf2.st_ino, buf1.st_ino, EXIT);
    ICUNIT_GOTO_EQUAL(buf1.st_mode, buf2.st_mode, buf1.st_mode, EXIT);
    ICUNIT_GOTO_EQUAL(buf1.st_mtim.tv_nsec, buf2.st_mtim.tv_nsec, buf1.st_mtim.tv_nsec, EXIT);

    FatDeleteFile(fd, buffile);

    ret = rmdir(pathname1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    ret = rmdir(FAT_PATH_NAME);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    return FAT_NO_ERROR;
EXIT:
    format(FAT_DEV_PATH, 8, 0); // cluster size 8, 0 for auto
    return FAT_NO_ERROR;
}

VOID ItFsFatPressure302(VOID)
{
    TEST_ADD_CASE("IT_FS_FAT_PRESSURE_302", TestCase, TEST_VFS, TEST_VFAT, TEST_LEVEL3, TEST_PRESSURE);
}
