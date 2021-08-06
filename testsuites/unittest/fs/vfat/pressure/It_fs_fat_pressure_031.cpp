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


#include "It_vfs_fat.h"

static VOID *PthreadF01(void *arg)
{
    INT32 ret;
    INT32 len = 0;
    INT32 i = 0;
    INT32 j = 0;
    CHAR pathname[FAT_STANDARD_NAME_LENGTH] = FAT_PATH_NAME;
    CHAR filebuf[260] = "abcdeabcde0123456789abcedfghij9876550210abcdeabcde0123456789abcedfghij9876550210abcdeabcde0123"
        "456789abcedfghij9876550210abcdeabcde0123456789abcedfghij9876550210abcdeabcde0123456789abcedfgh"
        "ij9876550210abcdeabcde0123456789abcedfghij9876550210lalalalalalalala";
    CHAR *bufWrite = nullptr;
    CHAR *bufWrite1 = nullptr;
    CHAR *bufWrite2 = nullptr;
    off64_t off64Num1, off64Num2;
    struct stat64 statbuf1;

    bufWrite = (CHAR *)malloc(8 * BYTES_PER_MBYTES + 1); // 8 * BYTES_PER_MBYTES = 8MB
    ICUNIT_GOTO_NOT_EQUAL(bufWrite, NULL, 0, EXIT1);
    (void)memset_s(bufWrite, 8 * BYTES_PER_MBYTES + 1, 0, // 8 * BYTES_PER_MBYTES = 8MB
        8 * BYTES_PER_MBYTES + 1);                        // 8 * BYTES_PER_MBYTES = 8MB

    bufWrite1 = (CHAR *)malloc(BYTES_PER_MBYTES + 1); // BYTES_PER_MBYTES = 1MB
    ICUNIT_GOTO_NOT_EQUAL(bufWrite1, NULL, 0, EXIT2);
    (void)memset_s(bufWrite1, BYTES_PER_MBYTES + 1, 0, BYTES_PER_MBYTES + 1); // BYTES_PER_MBYTES = 1MB

    bufWrite2 = (CHAR *)malloc(16 * BYTES_PER_KBYTES + 1); // 16 * BYTES_PER_KBYTES = 16KB
    ICUNIT_GOTO_NOT_EQUAL(bufWrite2, NULL, 0, EXIT3);
    (void)memset_s(bufWrite2, 16 * BYTES_PER_KBYTES + 1, 0, 16 * BYTES_PER_KBYTES + 1); // 16 kb

    for (j = 0; j < 16; j++) { // loop 16 times
        (void)strcat_s(bufWrite1, BYTES_PER_MBYTES + 1, filebuf);
        (void)strcat_s(bufWrite1, BYTES_PER_MBYTES + 1, filebuf);
        (void)strcat_s(bufWrite1, BYTES_PER_MBYTES + 1, filebuf);
        (void)strcat_s(bufWrite1, BYTES_PER_MBYTES + 1, filebuf);
    }

    for (j = 0; j < 16; j++) { // loop 16 times
        (void)strcat_s(bufWrite1, BYTES_PER_MBYTES + 1, bufWrite2);
        (void)strcat_s(bufWrite1, BYTES_PER_MBYTES + 1, bufWrite2);
        (void)strcat_s(bufWrite1, BYTES_PER_MBYTES + 1, bufWrite2);
        (void)strcat_s(bufWrite1, BYTES_PER_MBYTES + 1, bufWrite2);
    }

    for (i = 0; i < 4; i++) {                                      // loop 4 times
        (void)strcat_s(bufWrite, 8 * BYTES_PER_MBYTES, bufWrite1); // 8 * BYTES_PER_MBYTES = 8MB
        (void)strcat_s(bufWrite, 8 * BYTES_PER_MBYTES, bufWrite1); // 8 * BYTES_PER_MBYTES = 8MB
    }

    free(bufWrite1);
    free(bufWrite2);

    g_testCount++;
    g_fatFlagF01++;

    (void)memset_s(g_fatPathname1, FAT_STANDARD_NAME_LENGTH, 0, FAT_STANDARD_NAME_LENGTH);
    (void)strcat_s(g_fatPathname1, FAT_STANDARD_NAME_LENGTH, pathname);
    (void)strcat_s(g_fatPathname1, FAT_STANDARD_NAME_LENGTH, "/031.txt");

    g_fatFd = open64(g_fatPathname1, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_NOT_EQUAL(g_fatFd, FAT_IS_ERROR, g_fatFd, EXIT2);

    while (1) {
        ret = write(g_fatFd, bufWrite, strlen(bufWrite));
        if (ret <= 0) {
            if (g_testCount < (4 * BYTES_PER_KBYTES / 8)) { // 8 MB per write, write 4 * BYTES_PER_KBYTES MB size
                printf("The biggest file size is smaller than the 4GB");
                goto EXIT;
            }
            printf("The cycle count = :%d,the file size = :%dMB,= :%0.3lfGB\n", g_testCount,
                g_testCount * 8,                             // 8MB per write
                (g_testCount * 8) * 1.0 / BYTES_PER_KBYTES); // 8 MB per write, BYTES_PER_KBYTES MB/GB
            break;
        }
        g_testCount++;
    }

    ret = stat64(g_fatPathname1, &statbuf1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

    off64Num1 = lseek64(g_fatFd, 0, SEEK_END);
    ICUNIT_GOTO_EQUAL(off64Num1, statbuf1.st_size, off64Num1, EXIT2);

    len = write(g_fatFd, bufWrite, strlen(bufWrite));
    printf("len=:%d,errno=:%d\t", len, errno);

    free(bufWrite);

    off64Num2 = lseek64(g_fatFd, 0, SEEK_END);
    ICUNIT_GOTO_EQUAL(off64Num2, off64Num1, off64Num2, EXIT1);

    ret = close(g_fatFd);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    g_fatFlag++;

    return NULL;
EXIT3:
    free(bufWrite1);
EXIT2:
    free(bufWrite);
EXIT1:
    close(g_fatFd);
EXIT:
    errno = 0;
    ret = remove(g_fatPathname1);
    g_testCount = 0;
    return NULL;
}

static UINT32 TestCase(VOID)
{
    INT32 ret, len, fd;
    off64_t off64;
    CHAR pathname[FAT_STANDARD_NAME_LENGTH] = FAT_PATH_NAME;
    CHAR readbuf[FAT_STANDARD_NAME_LENGTH] = "";
    DIR *dir = nullptr;
    struct dirent *ptr = nullptr;
    pthread_t newThread1;
    pthread_attr_t attr1;
    struct stat64 statbuf1;

    g_fatFlag = 0;
    g_fatFlagF01 = 0;

    ret = mkdir(pathname, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    while ((g_fatFlag < FAT_PRESSURE_CYCLES) && (g_fatFlag == g_fatFlagF01)) {
        g_testCount = 0;

        ret = PosixPthreadInit(&attr1, TASK_PRIO_TEST - 5); // level 5 lower
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

        ret = pthread_create(&newThread1, &attr1, PthreadF01, NULL);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

        ret = pthread_join(newThread1, NULL);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

        ret = pthread_attr_destroy(&attr1);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

        fd = open64(g_fatPathname1, O_NONBLOCK | O_CREAT | O_RDWR, S_IRWXU | S_IRWXG | S_IRWXO);
        ICUNIT_GOTO_NOT_EQUAL(fd, FAT_IS_ERROR, fd, EXIT3);

        ret = stat64(g_fatPathname1, &statbuf1);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT3);

        off64 = lseek64(fd, -FAT_SHORT_ARRAY_LENGTH, SEEK_END);
        ICUNIT_GOTO_EQUAL(off64, statbuf1.st_size - FAT_SHORT_ARRAY_LENGTH, off64, EXIT3);

        (void)memset_s(readbuf, FAT_STANDARD_NAME_LENGTH, 0, FAT_STANDARD_NAME_LENGTH);
        len = read(fd, readbuf, FAT_STANDARD_NAME_LENGTH - 1);
        ICUNIT_GOTO_EQUAL(len, FAT_SHORT_ARRAY_LENGTH, len, EXIT3);
        ICUNIT_GOTO_STRING_EQUAL(readbuf, "alalalalal", readbuf, EXIT3);

        ret = close(fd);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT3);

        ret = remove(g_fatPathname1);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT3);

        ret = pthread_attr_destroy(&attr1);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

        printf("\tg_fatFlag=:%d\t", g_fatFlag);
    }
    printf("\n");
    ICUNIT_GOTO_EQUAL(g_fatFlag, FAT_PRESSURE_CYCLES, g_fatFlag, EXIT2);

    dir = opendir(pathname);
    ICUNIT_GOTO_NOT_EQUAL(dir, NULL, dir, EXIT2);

    ptr = readdir(dir);
    ICUNIT_GOTO_EQUAL(ptr, NULL, ptr, EXIT2);

    ret = closedir(dir);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

    ret = rmdir(pathname);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    return FAT_NO_ERROR;
EXIT3:
    close(fd);
    remove(g_fatPathname1);
    goto EXIT;
EXIT2:
    closedir(dir);
    goto EXIT;
EXIT1:
    pthread_join(newThread1, NULL);
    pthread_attr_destroy(&attr1);
EXIT:
    rmdir(FAT_PATH_NAME);
    return FAT_NO_ERROR;
}

/* *
* -@test IT_FS_FAT_PRESSURE_031
* -@tspec The Pressure test
* -@ttitle write the file to 4GB
* -@tprecon The filesystem module is open
* -@tbrief
1. create two tasks;
2. the first task write the file to 4GB;
3. the second task write one byte to the file once more;
4. delete the files;
* -@texpect
1. Return successed
2. Return successed
3. Return failure
4. Sucessful operation
* -@tprior 1
* -@tauto TRUE
* -@tremark
*/

VOID ItFsFatPressure031(VOID)
{
    TEST_ADD_CASE("IT_FS_FAT_PRESSURE_031", TestCase, TEST_VFS, TEST_VFAT, TEST_LEVEL3, TEST_PRESSURE);
}
