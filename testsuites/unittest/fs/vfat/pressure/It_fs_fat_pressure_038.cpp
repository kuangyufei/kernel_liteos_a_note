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

static CHAR g_fat1650Pathname4[FAT_SHORT_ARRAY_LENGTH][FAT_SHORT_ARRAY_LENGTH][FAT_NAME_LIMITTED_SIZE] = {0, };
static CHAR g_fat1650Pathname5[FAT_SHORT_ARRAY_LENGTH][FAT_SHORT_ARRAY_LENGTH][FAT_NAME_LIMITTED_SIZE] = {0, };
static VOID *PthreadF01(void *arg)
{
    INT32 ret;
    INT32 len;
    INT32 i = 0;
    INT32 j = 0;
    CHAR bufname[FAT_SHORT_ARRAY_LENGTH] = "123456";
    CHAR pathname1[FAT_STANDARD_NAME_LENGTH] = FAT_PATH_NAME;
    CHAR *bufRead = nullptr;

    ret = pthread_mutex_lock(&g_vfatGlobalLock1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    (void)memset_s(g_fatPathname6, FAT_NAME_LIMITTED_SIZE, 0, FAT_NAME_LIMITTED_SIZE);
    (void)strcat_s(g_fatPathname6, FAT_NAME_LIMITTED_SIZE, pathname1);

    g_testCount++;
    g_fatFlagF01++;

    for (i = 0; i < FAT_SHORT_ARRAY_LENGTH; i++) {
        (void)memset_s(bufname, FAT_SHORT_ARRAY_LENGTH, 0, FAT_SHORT_ARRAY_LENGTH);

        (void)memset_s(g_fatPathname11[i], FAT_NAME_LIMITTED_SIZE, 0, FAT_NAME_LIMITTED_SIZE);

        (void)snprintf_s(bufname, FAT_SHORT_ARRAY_LENGTH, FAT_SHORT_ARRAY_LENGTH, "/test%d", i);
        (void)strcat_s(g_fatPathname6, FAT_NAME_LIMITTED_SIZE, bufname);
        (void)strcpy_s(g_fatPathname11[i], FAT_NAME_LIMITTED_SIZE, g_fatPathname6);

        ret = mkdir(g_fatPathname11[i], S_IRWXU | S_IRWXG | S_IRWXO);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

        for (j = 0; j < FAT_SHORT_ARRAY_LENGTH; j++) {
            (void)memset_s(g_fatPathname7, FAT_NAME_LIMITTED_SIZE, 0, FAT_NAME_LIMITTED_SIZE);
            (void)memset_s(bufname, FAT_SHORT_ARRAY_LENGTH, 0, strlen(bufname));
            (void)memset_s(g_fat1650Pathname4[i][j], FAT_NAME_LIMITTED_SIZE, 0, FAT_NAME_LIMITTED_SIZE);
            (void)memset_s(g_fat1650Pathname5[i][j], FAT_NAME_LIMITTED_SIZE, 0, FAT_NAME_LIMITTED_SIZE);

            (void)snprintf_s(bufname, FAT_SHORT_ARRAY_LENGTH, FAT_SHORT_ARRAY_LENGTH, "/test%d", j);
            (void)strcat_s(g_fatPathname7, FAT_NAME_LIMITTED_SIZE, g_fatPathname6);
            (void)strcat_s(g_fatPathname7, FAT_NAME_LIMITTED_SIZE, bufname);
            (void)strcat_s(g_fatPathname7, FAT_NAME_LIMITTED_SIZE, ".txt");
            (void)strcpy_s(g_fat1650Pathname4[i][j], FAT_NAME_LIMITTED_SIZE, g_fatPathname7);

            (void)memset_s(g_fatPathname7, FAT_NAME_LIMITTED_SIZE, 0, FAT_NAME_LIMITTED_SIZE);
            (void)strcat_s(g_fatPathname7, FAT_NAME_LIMITTED_SIZE, g_fatPathname6);
            (void)strcat_s(g_fatPathname7, FAT_NAME_LIMITTED_SIZE, bufname);
            (void)strcat_s(g_fatPathname7, FAT_NAME_LIMITTED_SIZE, ".cpp");
            (void)strcpy_s(g_fat1650Pathname5[i][j], FAT_NAME_LIMITTED_SIZE, g_fatPathname7);

            g_fatFd12[i][j] =
                open(g_fat1650Pathname4[i][j], O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, S_IRWXU | S_IRWXG | S_IRWXO);
            ICUNIT_GOTO_NOT_EQUAL(g_fatFd12[i][j], -1, g_fatFd12[i][j], EXIT1);

            ret = close(g_fatFd12[i][j]);
            ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);
        }
    }

    (void)pthread_mutex_unlock(&g_vfatGlobalLock1);

    LosTaskDelay(1);

    ret = pthread_mutex_lock(&g_vfatGlobalLock1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    bufRead = (CHAR *)malloc(10 * BYTES_PER_MBYTES + 1); // 10 * BYTES_PER_MBYTES = 10MB
    ICUNIT_GOTO_NOT_EQUAL(bufRead, NULL, 0, EXIT1);
    (void)memset_s(bufRead, 10 * BYTES_PER_MBYTES + 1, 0, 10 * BYTES_PER_MBYTES + 1); // 10 * BYTES_PER_MBYTES = 10MB

    for (i = 0; i < 10; i++) {     // loop 10 times
        for (j = 0; j < 10; j++) { // loop 10 times
            g_fatFd12[i][j] = open(g_fat1650Pathname4[i][j], O_NONBLOCK | O_RDWR, S_IRWXU | S_IRWXG | S_IRWXO);
            ICUNIT_GOTO_NOT_EQUAL(g_fatFd12[i][j], -1, g_fatFd12[i][j], EXIT2);

            len = read(g_fatFd12[i][j], bufRead, 10 * BYTES_PER_MBYTES + 1); // 10 * BYTES_PER_MBYTES = 10MB
            ICUNIT_GOTO_EQUAL(len, 10 * BYTES_PER_MBYTES, len, EXIT2);       // 10 * BYTES_PER_MBYTES = 10MB

            ret = close(g_fatFd12[i][j]);
            ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);
        }
    }

    free(bufRead);

    (void)pthread_mutex_unlock(&g_vfatGlobalLock1);

    LosTaskDelay(1);

    ret = pthread_mutex_lock(&g_vfatGlobalLock1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    g_fatFlag++;

    (void)pthread_mutex_unlock(&g_vfatGlobalLock1);

    LosTaskDelay(1);

    return NULL;
EXIT2:
    free(bufRead);
EXIT1:
    for (i = 0; i < 10; i++) {     // loop 10 times
        for (j = 0; j < 10; j++) { // loop 10 times
            close(g_fatFd12[i][j]);
        }
    }
EXIT:
    for (i = 9; i >= 0; i--) {     // loop 9 + 1 times
        for (j = 9; j >= 0; j--) { // loop 9 + 1 times
            remove(g_fat1650Pathname4[i][j]);
            remove(g_fat1650Pathname5[i][j]);
        }

        remove(g_fatPathname11[i]);
    }
    g_testCount = 0;
    (void)pthread_mutex_unlock(&g_vfatGlobalLock1);
    return NULL;
}

static VOID *PthreadF02(void *arg)
{
    INT32 ret;
    INT32 i = 0;
    INT32 j = 0;
    INT32 len;
    CHAR filebuf[260] = "abcdeabcde0123456789abcedfghij9876550210abcdeabcde0123456789abcedfghij9876550210abcdeabcde0123"
        "456789abcedfghij9876550210abcdeabcde0123456789abcedfghij9876550210abcdeabcde0123456789abcedfgh"
        "ij9876550210abcdeabcde0123456789abcedfghij9876550210lalalalalalalala";

    CHAR *bufWrite = nullptr;
    CHAR *bufWrite1 = nullptr;
    CHAR *bufWrite2 = nullptr;

    ret = pthread_mutex_lock(&g_vfatGlobalLock1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    g_testCount++;

    bufWrite = (CHAR *)malloc(10 * BYTES_PER_MBYTES + 1); // 10 * BYTES_PER_MBYTES = 10MB
    ICUNIT_GOTO_NOT_EQUAL(bufWrite, NULL, 0, EXIT1);
    (void)memset_s(bufWrite, 10 * BYTES_PER_MBYTES + 1, 0, 10 * BYTES_PER_MBYTES + 1); // 10 * BYTES_PER_MBYTES = 10MB

    bufWrite1 = (CHAR *)malloc(512 * BYTES_PER_KBYTES + 1); // 512 * BYTES_PER_KBYTES = 512KB
    ICUNIT_GOTO_NOT_EQUAL(bufWrite1, NULL, 0, EXIT2);
    (void)memset_s(bufWrite1, 512 * BYTES_PER_KBYTES + 1, 0, // 512 kb
        512 * BYTES_PER_KBYTES + 1);                         // 512 * BYTES_PER_KBYTES = 512KB

    bufWrite2 = (CHAR *)malloc(16 * BYTES_PER_KBYTES + 1); // 16 * BYTES_PER_KBYTES = 16KB
    ICUNIT_GOTO_NOT_EQUAL(bufWrite2, NULL, 0, EXIT3);
    (void)memset_s(bufWrite2, 16 * BYTES_PER_KBYTES + 1, 0, 16 * BYTES_PER_KBYTES + 1); // 16 kb

    for (j = 0; j < 16; j++) {                                         // loop 16 times
        (void)strcat_s(bufWrite2, 16 * BYTES_PER_KBYTES + 1, filebuf); // 16 kb
        (void)strcat_s(bufWrite2, 16 * BYTES_PER_KBYTES + 1, filebuf); // 16 kb
        (void)strcat_s(bufWrite2, 16 * BYTES_PER_KBYTES + 1, filebuf); // 16 kb
        (void)strcat_s(bufWrite2, 16 * BYTES_PER_KBYTES + 1, filebuf); // 16 kb
    }

    for (j = 0; j < 8; j++) {                                             // loop 8 times
        (void)strcat_s(bufWrite1, 512 * BYTES_PER_KBYTES + 1, bufWrite2); // 512 kb
        (void)strcat_s(bufWrite1, 512 * BYTES_PER_KBYTES + 1, bufWrite2); // 512 kb
        (void)strcat_s(bufWrite1, 512 * BYTES_PER_KBYTES + 1, bufWrite2); // 512 kb
        (void)strcat_s(bufWrite1, 512 * BYTES_PER_KBYTES + 1, bufWrite2); // 512 kb
    }

    for (i = 0; i < 10; i++) {                                      // loop 10 times
        (void)strcat_s(bufWrite, 10 * BYTES_PER_MBYTES + 1, bufWrite1); // 10 * BYTES_PER_MBYTES = 10MB
        (void)strcat_s(bufWrite, 10 * BYTES_PER_MBYTES + 1, bufWrite1); // 10 * BYTES_PER_MBYTES = 10MB
    }
    free(bufWrite1);
    free(bufWrite2);
    for (i = 0; i < 10; i++) {     // loop 10 times
        for (j = 0; j < 10; j++) { // loop 10 times
            g_fatFd12[i][j] = open(g_fat1650Pathname4[i][j], O_NONBLOCK | O_RDWR, S_IRWXU | S_IRWXG | S_IRWXO);
            ICUNIT_GOTO_NOT_EQUAL(g_fatFd12[i][j], -1, g_fatFd12[i][j], EXIT2);

            len = write(g_fatFd12[i][j], bufWrite, strlen(bufWrite));
            ICUNIT_GOTO_EQUAL(len, 10 * BYTES_PER_MBYTES, len, EXIT2); // 10 * BYTES_PER_MBYTES = 10MB

            ret = close(g_fatFd12[i][j]);
            ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);
        }
    }
    free(bufWrite);

    (void)pthread_mutex_unlock(&g_vfatGlobalLock1);

    LosTaskDelay(1);

    ret = pthread_mutex_lock(&g_vfatGlobalLock1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    for (i = 0; i < 10; i++) {     // loop 10 times
        for (j = 0; j < 10; j++) { // loop 10 times
            ret = rename(g_fat1650Pathname4[i][j], g_fat1650Pathname5[i][j]);
            ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);
        }
    }

    (void)pthread_mutex_unlock(&g_vfatGlobalLock1);

    LosTaskDelay(1);

    ret = pthread_mutex_lock(&g_vfatGlobalLock1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    for (i = 9; i >= 0; i--) {     // loop 9 + 1 times
        for (j = 9; j >= 0; j--) { // loop 9 + 1 times
            ret = remove(g_fat1650Pathname5[i][j]);
            ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);
        }

        ret = remove(g_fatPathname11[i]);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);
    }
    g_fatFlagF02++;

    (void)pthread_mutex_unlock(&g_vfatGlobalLock1);

    LosTaskDelay(2); // delay 2 s

    return NULL;
EXIT3:
    free(bufWrite1);
EXIT2:
    free(bufWrite);
EXIT1:
    for (i = 0; i < 10; i++) {     // loop 10 times
        for (j = 0; j < 10; j++) { // loop 10 times
            close(g_fatFd12[i][j]);
        }
    }
EXIT:
    for (i = 9; i >= 0; i--) {     // loop 9 + 1 times
        for (j = 9; j >= 0; j--) { // loop 9 + 1 times
            remove(g_fat1650Pathname4[i][j]);
            remove(g_fat1650Pathname5[i][j]);
        }
        remove(g_fatPathname11[i]);
    }

    (void)pthread_mutex_unlock(&g_vfatGlobalLock1);
    g_testCount = 0;
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

    ret = mkdir(pathname, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    while ((g_fatFlag < FAT_PRESSURE_CYCLES) && (g_fatFlag == g_fatFlagF01)) {
        g_testCount = 0;

        ret = PosixPthreadInit(&attr1, 20); // level 20
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

        ret = pthread_create(&newThread1, &attr1, PthreadF01, NULL);
        ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);

        ret = PosixPthreadInit(&attr2, 20); // level 20
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

        ret = pthread_create(&newThread2, &attr2, PthreadF02, NULL);
        ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);

        LosTaskDelay(FAT_SHORT_ARRAY_LENGTH);

        ret = PosixPthreadDestroy(&attr2, newThread2);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

        ret = PosixPthreadDestroy(&attr1, newThread1);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);
        printf("\tg_fatFlag=:%d\t", g_fatFlag);
    }
    printf("\n");
    ICUNIT_GOTO_EQUAL(g_fatFlag, FAT_PRESSURE_CYCLES, g_fatFlag, EXIT);

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
* -@test IT_FS_FAT_PRESSURE_038
* -@tspec The Pressure test
* -@ttitle Create the 10-level directory and create 10 subdirectories while creating the directory
* -@tprecon The filesystem module is open
* -@tbrief
1. create two tasks;
2. the first task create the 10-level directory ;
3. the another task create 10 subdirectories while creating the directory;
4. delete the directories and files;
* -@texpect
1. Return successed
2. Return successed
3. Return successed
4. Successful operation
* -@tprior 1
* -@tauto TRUE
* -@tremark
*/

VOID ItFsFatPressure038(VOID)
{
    TEST_ADD_CASE("IT_FS_FAT_PRESSURE_038", TestCase, TEST_VFS, TEST_VFAT, TEST_LEVEL3, TEST_PRESSURE);
}
