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
    INT32 i, j, k, ret, len;
    INT32 fd = -1;
    CHAR filebuf[260] = "01234567890123456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123"
        "456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfgh"
        "ij9876543210abcdeabcde0123456789abcedfghij9876543210lalalalalalalala";
    CHAR *bufWrite = nullptr;
    off_t off;
    CHAR readbuf[2000] = "";
    CHAR pathname[FAT_STANDARD_NAME_LENGTH] = FAT_PATH_NAME;
    CHAR pathname1[FAT_STANDARD_NAME_LENGTH] = FAT_PATH_NAME;
    struct stat buf1 = { 0 };

    ret = chdir("/");
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT0);

    ret = umount(FAT_MOUNT_DIR);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT_MOUNT);

    ret = format(FAT_DEV_PATH, 0, 0x02);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT_MOUNT);

    ret = mount(FAT_DEV_PATH1, FAT_MOUNT_DIR, FAT_FILESYS_TYPE, 0, NULL);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT_MOUNT);

    bufWrite = (CHAR *)malloc(BYTES_PER_MBYTES + 1); // BYTES_PER_MBYTES = 1MB
    ICUNIT_ASSERT_NOT_EQUAL_NULL(bufWrite, NULL, NULL);
    (void)memset_s(bufWrite, BYTES_PER_MBYTES + 1, 0,
        BYTES_PER_MBYTES + 1); // BYTES_PER_MBYTES = 1MB

    for (i = 0; i < BYTES_PER_KBYTES * 4; i++) { // 1M * BYTES_PER_KBYTES * 4 = 4GB
        (void)strcat_s(bufWrite, BYTES_PER_MBYTES, filebuf);
    }

    ret = mkdir(pathname, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    (void)strcat_s(pathname, FAT_STANDARD_NAME_LENGTH, "testfile.txt");
    fd = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_NOT_EQUAL(fd, FAT_IS_ERROR, fd, EXIT2);

    for (j = 0; j < 100; j++) { // Ð´100M
        ret = write(fd, bufWrite, strlen(bufWrite));
        printf("\n write times = : %d\n", j + 1);             // j+1µÈÓÚ10×óÓÒÊ±£¬²å°ÎSD¿¨
        ICUNIT_GOTO_EQUAL(ret, BYTES_PER_MBYTES, ret, EXIT2); // not equal to 1M

        off = lseek(fd, 0, SEEK_CUR);
        ICUNIT_GOTO_EQUAL(off, (BYTES_PER_MBYTES * (j + 1)), off, EXIT2); // from 1M to 4G
    }

    off = lseek(fd, 64, SEEK_SET);          // 64 byte
    ICUNIT_GOTO_EQUAL(off, 64, off, EXIT2); // 64 byte

    for (k = 0; k < 500; k++) {                    // ¶Á500´Î
        len = read(fd, readbuf, BYTES_PER_KBYTES); // read BYTES_PER_KBYTES bytes
        printf("\n read times = : %d\n", k + 1);
        ICUNIT_GOTO_EQUAL(len, BYTES_PER_KBYTES, len, EXIT2); // make sure BYTES_PER_KBYTES bytes
    }

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

    ret = stat(pathname1, &buf1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);
    FatStatPrintf(buf1);

    ret = unlink(pathname);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = rmdir(pathname1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    free(bufWrite);

    return FAT_NO_ERROR;

    free(bufWrite);
EXIT2:
    close(fd);
EXIT1:
    unlink(pathname);
EXIT:
    rmdir(pathname1);
EXIT_MOUNT:
    mount(FAT_DEV_PATH1, FAT_MOUNT_DIR, FAT_FILESYS_TYPE, 0, NULL);
    free(bufWrite);
EXIT0:
    return FAT_NO_ERROR;
}

/* *
* - @test IT_FS_FAT_909
* - @tspec function test
* - @ttitle Insert and remove SD card during reading and writing
* - @tbrief
1. Create a new file and open it
2. Unplug the SD card when writing 10M
3. View serial port printing status and file status.
* - @ tprior 1
* - @ tauto TRUE
* - @ tremark
*/

VOID ItFsFat909(VOID)
{
    TEST_ADD_CASE("IT_FS_FAT_909", TestCase, TEST_VFS, TEST_VFAT, TEST_LEVEL2, TEST_FUNCTION);
}
