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
    INT32 ret, i, index, len;
    INT32 fd[FAT_FILE_LIMITTED_NUM] = {};
    INT32 flag = 0;
    CHAR pathname0[FAT_STANDARD_NAME_LENGTH] = FAT_PATH_NAME0;
    CHAR bufname0[FAT_STANDARD_NAME_LENGTH] = "";
    CHAR filebuf[260] = "01234567890123456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123"
        "456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfgh"
        "ij9876543210abcdeabcde0123456789abcedfghij9876543210lalalalalalalala";
    CHAR *bufWrite = nullptr;
    DIR *dir = NULL;
    struct timeval testTime1;
    struct timeval testTime2;

    bufWrite = (CHAR *)malloc(BYTES_PER_MBYTES + 1); // BYTES_PER_MBYTES = 1MB // 1M
    ICUNIT_ASSERT_NOT_EQUAL(bufWrite, NULL, bufWrite);
    (void)memset_s(bufWrite, BYTES_PER_MBYTES + 1, 0, BYTES_PER_MBYTES + 1); // BYTES_PER_MBYTES = 1MB // 1M

    for (i = 0; i < 200 * 4; i++) { // append 200 * 4 times to 1M
        (void)strcat_s(bufWrite, BYTES_PER_MBYTES, filebuf);
    }

    ret = mkdir(pathname0, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = chdir(pathname0);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    index = 0;
    for (i = 0; i < FAT_FILE_LIMITTED_NUM; i++) {
        (void)snprintf_s(bufname0, FAT_STANDARD_NAME_LENGTH, FAT_STANDARD_NAME_LENGTH, "/vs/sd/test0/file%d.txt",
            index);
        fd[index] = open(bufname0, O_CREAT | O_RDWR | O_EXCL, S_IRWXU | S_IRWXG | S_IRWXO);
        ICUNIT_GOTO_NOT_EQUAL(fd[index], FAT_IS_ERROR, fd[index], EXIT2);

        len = write(fd[index], bufWrite, strlen(bufWrite));
        if (len <= 0) {
            flag = 1;
            break;
        }

        ret = close(fd[index]);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

        index++;
    }
    if (flag == 0) {
        index--;
    }

    gettimeofday(&testTime1, 0);
    for (i = index; i >= 0; i--) {
        (void)snprintf_s(bufname0, FAT_STANDARD_NAME_LENGTH, FAT_STANDARD_NAME_LENGTH, "/vs/sd/test0/file%d.txt", i);
        ret = unlink(bufname0);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT3);
    }
    ret = chdir(SD_MOUNT_DIR);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    ret = rmdir(pathname0);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);
    gettimeofday(&testTime2, 0);
    printf("FF--%s:%d, time: %lld\n", __func__, __LINE__,
        (testTime2.tv_sec - testTime1.tv_sec) * US_PER_SEC + // US_PER_SEC
        (testTime2.tv_usec - testTime1.tv_usec));

    free(bufWrite);
    return FAT_NO_ERROR;
EXIT3:
    for (i = index; i >= 0; i--) {
        (void)snprintf_s(bufname0, FAT_STANDARD_NAME_LENGTH, FAT_STANDARD_NAME_LENGTH, "/vs/sd/test0/file%d.txt", i);
        unlink(bufname0);
    }
EXIT2:
    for (i = index; i >= 0; i--) {
        close(fd[i]);
    }
EXIT1:
    closedir(dir);
EXIT:
    chdir(SD_MOUNT_DIR);
    rmdir(FAT_PATH_NAME);
    return FAT_NO_ERROR;
}

/* *
* -@test IT_FS_FAT_PERFORMANCE_013
* -@tspec The function test for filesystem
* -@ttitle How long does it take to delete a directory
* -@tprecon The filesystem module is open
* -@tbrief
1. Create a directory with 200 files
2.200Kb per file
3.delete the file box directory
* -@texpect
1. Return successed
2. Return successed
3. Return successed
4. Sucessful operation
* -@tprior 1
* -@tauto TRUE
* -@tremark
*/

VOID ItFsFatPerformance013(VOID)
{
    TEST_ADD_CASE("IT_FS_FAT_PERFORMANCE_013", TestCase, TEST_VFS, TEST_VFAT, TEST_LEVEL3, TEST_PERFORMANCE);
}
