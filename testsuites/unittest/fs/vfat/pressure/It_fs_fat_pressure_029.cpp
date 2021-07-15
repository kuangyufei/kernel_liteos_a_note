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

static VOID *PthreadF01(void *arg)
{
    INT32 ret;
    INT32 i = 0;
    INT32 k;
    off_t off;
    CHAR bufname[FAT_SHORT_ARRAY_LENGTH] = "123456";
    CHAR pathname1[FAT_STANDARD_NAME_LENGTH] = FAT_PATH_NAME;

    ret = pthread_mutex_lock(&g_vfatGlobalLock1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    g_testCount++;
    g_fatFlagF01++;

    for (i = 0; i < FAT_MAXIMUM_SIZES; i++) {
        (void)memset_s(g_fatPathname2, FAT_STANDARD_NAME_LENGTH, 0, FAT_STANDARD_NAME_LENGTH);
        (void)memset_s(g_fatPathname12[i], FAT_STANDARD_NAME_LENGTH, 0, FAT_NAME_LIMITTED_SIZE);

        (void)snprintf_s(bufname, FAT_SHORT_ARRAY_LENGTH, FAT_SHORT_ARRAY_LENGTH, "/test%d", i);
        (void)strcat_s(g_fatPathname2, FAT_SHORT_ARRAY_LENGTH, pathname1);
        (void)strcat_s(g_fatPathname2, FAT_SHORT_ARRAY_LENGTH, bufname);
        (void)strcat_s(g_fatPathname2, FAT_SHORT_ARRAY_LENGTH, ".txt");
        (void)strcpy_s(g_fatPathname12[i], FAT_SHORT_ARRAY_LENGTH, g_fatPathname2);
        g_fatFd11[i] = open(g_fatPathname12[i], O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, S_IRWXU | S_IRWXG | S_IRWXO);
        ICUNIT_GOTO_NOT_EQUAL(g_fatFd11[i], -1, g_fatFd11[i], EXIT1);

        if (i % FAT_SHORT_ARRAY_LENGTH == 9) { // meet i = 9 close the fat fd
            for (k = (i / FAT_SHORT_ARRAY_LENGTH) * FAT_SHORT_ARRAY_LENGTH; k <= i; k++) {
                ret = close(g_fatFd11[k]);
                ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);
            }
        }
    }

    ICUNIT_GOTO_NOT_EQUAL((g_testCount % 2), 0, g_testCount, EXIT1); // 2 for even or not

    (void)pthread_mutex_unlock(&g_vfatGlobalLock1);

    LosTaskDelay(1);

    ret = pthread_mutex_lock(&g_vfatGlobalLock1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    for (i = 0; i < FAT_MAXIMUM_SIZES; i++) {
        g_fatFd11[i] = open(g_fatPathname12[i], O_NONBLOCK | O_RDWR, S_IRWXU | S_IRWXG | S_IRWXO);
        ICUNIT_GOTO_NOT_EQUAL(g_fatFd11[i], -1, g_fatFd11[i], EXIT1);

        off = lseek(g_fatFd11[i], 0, SEEK_END);
        ICUNIT_GOTO_EQUAL(off, 1 * BYTES_PER_MBYTES, off, EXIT1); // 1 * BYTES_PER_MBYTES = 1M

        if (i % FAT_SHORT_ARRAY_LENGTH == 9) { // meet i = 9 close the fat fd
            for (k = (i / FAT_SHORT_ARRAY_LENGTH) * FAT_SHORT_ARRAY_LENGTH; k <= i; k++) {
                ret = close(g_fatFd11[k]);
                ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);
            }
        }
    }

    for (i = 0; i < FAT_MAXIMUM_SIZES; i++) {
        ret = unlink(g_fatPathname12[i]);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);
    }

    g_fatFlag++;

    (void)pthread_mutex_unlock(&g_vfatGlobalLock1);

    return NULL;
EXIT1:
    for (i = 0; i < FAT_MAXIMUM_SIZES; i++) {
        close(g_fatFd11[i]);
    }
EXIT:
    for (i = 0; i < FAT_MAXIMUM_SIZES; i++) {
        unlink(g_fatPathname12[i]);
    }
    g_testCount = 0;
    (void)pthread_mutex_unlock(&g_vfatGlobalLock1);
    return NULL;
}

static VOID *PthreadF02(void *arg)
{
    INT32 ret;
    INT32 len;
    INT32 i = 0;
    INT32 j = 0;
    INT32 k = 0;
    CHAR filebuf[260] = "abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123"
        "456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfgh"
        "ij9876543210abcdeabcde0123456789abcedfghij9876543210lalalalalalalala";
    CHAR readbuf[FAT_STANDARD_NAME_LENGTH] = "liteos";
    CHAR *bufWrite = nullptr;
    off_t off;

    ret = pthread_mutex_lock(&g_vfatGlobalLock1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    g_testCount++;

    bufWrite = (CHAR *)malloc(1 * BYTES_PER_MBYTES + 1); // 1 * BYTES_PER_MBYTES = 1MB
    ICUNIT_GOTO_NOT_EQUAL(bufWrite, NULL, 0, EXIT1);
    (void)memset_s(bufWrite, BYTES_PER_MBYTES + 1, 0, BYTES_PER_MBYTES + 1); // BYTES_PER_MBYTES = 1MB

    for (i = 0; i < 1 * 32 * 4; i++) { // 4K * 2 * 32 * 4 = 1M
        for (j = 0; j < 4; j++) {      // 256 * 4 * 4 = 4K
            (void)strcat_s(bufWrite, 1 * BYTES_PER_MBYTES + 1, filebuf);
            (void)strcat_s(bufWrite, 1 * BYTES_PER_MBYTES + 1, filebuf);
            (void)strcat_s(bufWrite, 1 * BYTES_PER_MBYTES + 1, filebuf);
            (void)strcat_s(bufWrite, 1 * BYTES_PER_MBYTES + 1, filebuf);
        }

        for (j = 0; j < 4; j++) { // 256 * 4 * 4 = 4K
            (void)strcat_s(bufWrite, 1 * BYTES_PER_MBYTES + 1, filebuf);
            (void)strcat_s(bufWrite, 1 * BYTES_PER_MBYTES + 1, filebuf);
            (void)strcat_s(bufWrite, 1 * BYTES_PER_MBYTES + 1, filebuf);
            (void)strcat_s(bufWrite, 1 * BYTES_PER_MBYTES + 1, filebuf);
        }
    }

    for (i = 0; i < FAT_MAXIMUM_SIZES; i++) {
        g_fatFd11[i] = open(g_fatPathname12[i], O_NONBLOCK | O_RDWR, S_IRWXU | S_IRWXG | S_IRWXO);
        ICUNIT_GOTO_NOT_EQUAL(g_fatFd11[i], -1, g_fatFd11[i], EXIT2);

        len = read(g_fatFd11[i], readbuf, FAT_STANDARD_NAME_LENGTH);
        ICUNIT_GOTO_EQUAL(len, 0, len, EXIT2);

        off = lseek(g_fatFd11[i], 0, SEEK_SET);
        ICUNIT_GOTO_EQUAL(off, 0, off, EXIT2);

        len = write(g_fatFd11[i], bufWrite, strlen(bufWrite));
        ICUNIT_GOTO_EQUAL(len, 1 * BYTES_PER_MBYTES, len, EXIT2); // BYTES_PER_MBYTES = 1MB

        if (i % FAT_SHORT_ARRAY_LENGTH == 9) { // meet i = 9 close the fat fd
            for (k = (i / FAT_SHORT_ARRAY_LENGTH) * FAT_SHORT_ARRAY_LENGTH; k <= i; k++) {
                ret = close(g_fatFd11[k]);
                ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);
            }
        }
    }

    ICUNIT_GOTO_EQUAL((g_testCount % 2), 0, g_testCount, EXIT2); // 2 for even or not

    free(bufWrite);

    (void)pthread_mutex_unlock(&g_vfatGlobalLock1);

    LosTaskDelay(2); // delay 2 s

    return NULL;
EXIT2:
    free(bufWrite);
EXIT1:
    for (i = 0; i < FAT_MAXIMUM_SIZES; i++) {
        close(g_fatFd11[i]);
    }
EXIT:
    for (i = 0; i < FAT_MAXIMUM_SIZES; i++) {
        unlink(g_fatPathname12[i]);
    }
    g_testCount = 0;
    (void)pthread_mutex_unlock(&g_vfatGlobalLock1);
    return NULL;
}

static UINT32 TestCase(VOID)
{
    INT32 ret;
    CHAR pathname[FAT_STANDARD_NAME_LENGTH] = FAT_PATH_NAME;
    DIR *dir = nullptr;
    struct dirent *ptr = nullptr;
    pthread_t newThread1, newThread2;
    pthread_attr_t attr1, attr2;

    g_fatFlag = 0;
    g_fatFlagF01 = 0;
    g_fatFlagF02 = 0;

    g_testCount = 0;

    ret = mkdir(pathname, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    while ((g_fatFlag < FAT_PRESSURE_CYCLES) && (g_fatFlag == g_fatFlagF01)) {
        ret = PosixPthreadInit(&attr1, 20); // level 20
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

        ret = pthread_create(&newThread1, &attr1, PthreadF01, NULL);
        ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);

        ret = PosixPthreadInit(&attr2, 20); // level 20
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

        ret = pthread_create(&newThread2, &attr2, PthreadF02, NULL);
        ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);

        LosTaskDelay(1);

        ret = PosixPthreadDestroy(&attr2, newThread2);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

        ret = PosixPthreadDestroy(&attr1, newThread1);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);
        printf("\tg_fatFlag=:%d\t", g_fatFlag);
    }
    printf("\n");
    ICUNIT_GOTO_EQUAL(g_fatFlag, FAT_PRESSURE_CYCLES, g_fatFlag, EXIT2);

    dir = opendir(FAT_PATH_NAME);
    ICUNIT_GOTO_NOT_EQUAL(dir, NULL, dir, EXIT3);

    ptr = readdir(dir);
    ICUNIT_GOTO_EQUAL(ptr, NULL, ptr, EXIT3);

    ret = closedir(dir);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT3);

    ret = rmdir(FAT_PATH_NAME);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    return FAT_NO_ERROR;
EXIT3:
    closedir(dir);
    goto EXIT;
EXIT2:
    PosixPthreadDestroy(&attr2, newThread2);
EXIT1:
    PosixPthreadDestroy(&attr1, newThread1);
EXIT:
    rmdir(FAT_PATH_NAME);
    return FAT_NO_ERROR;
}

/* *
* -@test IT_FS_FAT_PRESSURE_029
* -@tspec The Pressure test
* -@ttitle Create, read ,write and delete the 1000 files
* -@tprecon The filesystem module is open
* -@tbrief
1. create two tasks;
2. the first task create 1000 files;
3. the second task read ,write these files;
4. delete all the directories;
* -@texpect
1. Return successed
2. Return successed
3. Return successed
4. Sucessful operation
* -@tprior 1
* -@tauto TRUE
* -@tremark
*/

VOID ItFsFatPressure029(VOID)
{
    TEST_ADD_CASE("IT_FS_FAT_PRESSURE_029", TestCase, TEST_VFS, TEST_VFAT, TEST_LEVEL3, TEST_PRESSURE);
}
