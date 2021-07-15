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

#include "It_vfs_fat.h"

static UINT32 TestCase(VOID)
{
    INT32 fd = 0;
    INT32 ret;
    INT32 len;
    INT32 fd1 = 0;
    INT32 fd2 = 0;
    INT32 fd3 = 0;
    CHAR filebuf[20] = "abcdeabcde";
    CHAR pathname1[FAT_STANDARD_NAME_LENGTH] = FAT_PATH_NAME;
    struct stat buf1 = { 0 };

    ret = chdir("/");
    ICUNIT_GOTO_NOT_EQUAL(fd, FAT_IS_ERROR, fd, EXIT0);

    ret = format(FAT_DEV_PATH, 0, 2); // cluster size 0, 2 for FAT32, will fail
    if (ret != 0) {
        perror("format sd card");
    }
    (void)strcat_s(pathname1, FAT_STANDARD_NAME_LENGTH, "test1");
    fd = open(pathname1, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_NOT_EQUAL(fd, FAT_IS_ERROR, fd, EXIT);

    ret = fallocate(fd, 1, 0, 1 << 10); // fallocate 1 << 10 for 1KB
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    len = write(fd, filebuf, strlen(filebuf));
    ICUNIT_GOTO_EQUAL(len, FAT_SHORT_ARRAY_LENGTH, len, EXIT1);

    close(fd);

    fd = open(pathname1, O_RDWR, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_NOT_EQUAL(fd, FAT_IS_ERROR, fd, EXIT);

    ret = fallocate(fd, 1, 0, 1 << 20); // fallocate 1 << 20 for 1MB
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    len = write(fd, filebuf, strlen(filebuf));
    ICUNIT_GOTO_EQUAL(len, FAT_SHORT_ARRAY_LENGTH, len, EXIT);

    ret = ftruncate(fd, 0xffff);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    ret = stat(pathname1, &buf1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);
    FatStatPrintf(buf1);

    (void)strcat_s(pathname1, FAT_STANDARD_NAME_LENGTH, "test2");
    fd1 = open(pathname1, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_NOT_EQUAL(fd1, FAT_IS_ERROR, fd1, EXIT2);

    len = write(fd1, filebuf, strlen(filebuf));
    ICUNIT_GOTO_EQUAL(len, FAT_SHORT_ARRAY_LENGTH, len, EXIT1);

    ret = ftruncate(fd1, 1 << 20); // ftruncate 1 << 20 to 1MB
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    ret = stat(pathname1, &buf1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);
    FatStatPrintf(buf1);

    (void)strcat_s(pathname1, FAT_STANDARD_NAME_LENGTH, "test3");
    fd2 = open(pathname1, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_NOT_EQUAL(fd2, FAT_IS_ERROR, fd2, EXIT1);

    ret = fallocate(fd2, 1, 0, 10); // fallocate to 10 bytes
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = fallocate(fd2, 1, 0, 10); // fallocate to 10 bytes
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = fallocate(fd2, 1, 10, 20); // fallocate to 10 + 20 bytes
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    (void)strcat_s(pathname1, FAT_STANDARD_NAME_LENGTH, "test4");
    fd3 = open(pathname1, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_NOT_EQUAL(fd1, FAT_IS_ERROR, fd1, EXIT3);

    ret = ftruncate(fd3, 1 << 20); // ftruncate 1 << 20 for 1MB
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT3);

    ret = stat(pathname1, &buf1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT3);
    FatStatPrintf(buf1);
    ret = close(fd3);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

    ret = close(fd2);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = close(fd1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT_REMOVE);

    ret = remove(pathname1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT_REMOVE);

    ret = umount(FAT_MOUNT_DIR);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT_REMOVE);

    ret = format(FAT_DEV_PATH, 0, 2); // cluster size 0, 2 for FAT32
    if (ret != 0) {
        perror("format sd card");
    }

    ret = mount(FAT_DEV_PATH1, FAT_MOUNT_DIR, FAT_FILESYS_TYPE, 0, NULL);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT_MOUNT_REMOVE);

    (void)strncpy_s(pathname1, FAT_STANDARD_NAME_LENGTH, FAT_PATH_NAME, sizeof(FAT_PATH_NAME));
    (void)strcat_s(pathname1, FAT_STANDARD_NAME_LENGTH, "test1");
    fd = open(pathname1, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_NOT_EQUAL(fd, FAT_IS_ERROR, fd, EXIT);

    ret = fallocate(fd, 1, 0, 1 << 10); // fallocate 1 << 10 for 1KB
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    len = write(fd, filebuf, strlen(filebuf));
    ICUNIT_GOTO_EQUAL(len, FAT_SHORT_ARRAY_LENGTH, len, EXIT1);

    close(fd);

    fd = open(pathname1, O_RDWR, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_NOT_EQUAL(fd, FAT_IS_ERROR, fd, EXIT);

    ret = fallocate64(fd, 1, 0, 1 << 20); // fallocate 1 << 20 for 1MB
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    len = write(fd, filebuf, strlen(filebuf));
    ICUNIT_GOTO_EQUAL(len, FAT_SHORT_ARRAY_LENGTH, len, EXIT);

    ret = ftruncate64(fd, 0xffff);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    ret = stat(pathname1, &buf1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);
    FatStatPrintf(buf1);

    (void)strcat_s(pathname1, FAT_STANDARD_NAME_LENGTH, "test2");
    fd1 = open(pathname1, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_NOT_EQUAL(fd1, FAT_IS_ERROR, fd1, EXIT2);

    len = write(fd1, filebuf, strlen(filebuf));
    ICUNIT_GOTO_EQUAL(len, FAT_SHORT_ARRAY_LENGTH, len, EXIT1);

    ret = ftruncate64(fd1, 1 << 20); // ftruncate 1 << 20 for 1MB
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    ret = stat(pathname1, &buf1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);
    FatStatPrintf(buf1);

    (void)strcat_s(pathname1, FAT_STANDARD_NAME_LENGTH, "test3");
    fd2 = open(pathname1, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_NOT_EQUAL(fd2, FAT_IS_ERROR, fd2, EXIT1);

    ret = fallocate64(fd2, 1, 0, 10); // fallocate 10 bytes
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = fallocate64(fd2, 1, 0, 10); // fallocate 10 bytes
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = fallocate64(fd2, 1, 10, 20); // fallocate 10 + 20 bytes
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    (void)strcat_s(pathname1, FAT_STANDARD_NAME_LENGTH, "test4");
    fd3 = open(pathname1, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_NOT_EQUAL(fd1, FAT_IS_ERROR, fd1, EXIT3);

    ret = ftruncate64(fd3, 1 << 20); // fallocate 1 << 20 for 1MB
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT3);

    ret = stat(pathname1, &buf1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT3);
    FatStatPrintf(buf1);
    ret = close(fd3);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

    ret = close(fd2);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = close(fd1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT_REMOVE);

    ret = remove(pathname1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT_REMOVE);

    return FAT_NO_ERROR;

EXIT3:
    close(fd3);
EXIT1:
    close(fd2);
EXIT2:
    close(fd1);
EXIT:
    close(fd);
EXIT_REMOVE:
    remove(pathname1);

    return FAT_NO_ERROR;

EXIT_MOUNT_REMOVE:
    mount(FAT_DEV_PATH1, FAT_MOUNT_DIR, FAT_FILESYS_TYPE, 0, NULL);
    remove(pathname1);
EXIT0:
    return FAT_NO_ERROR;
}
VOID ItFsFat904(VOID)
{
    TEST_ADD_CASE("IT_FS_FAT_904", TestCase, TEST_VFS, TEST_VFAT, TEST_LEVEL2, TEST_FUNCTION);
}
