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

static VOID *PthreadF01(void *arg)
{
    INT32 ret;
    INT32 j = 0;
    CHAR bufname[FAT_SHORT_ARRAY_LENGTH] = "123456";
    CHAR pathname[FAT_STANDARD_NAME_LENGTH] = FAT_PATH_NAME;
    CHAR pathname2[FAT_STANDARD_NAME_LENGTH] = "/vs/sd/fat_1655";
    off_t off;

    ret = pthread_mutex_lock(&g_vfatGlobalLock1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    g_testCount++;
    g_fatFlagF01++;

    for (j = 0; j < 4; j++) { // loop 4 times
        (void)memset_s(g_fatPathname11[j], FAT_NAME_LIMITTED_SIZE, 0, FAT_NAME_LIMITTED_SIZE);
        (void)memset_s(g_fatPathname12[j], FAT_NAME_LIMITTED_SIZE, 0, FAT_NAME_LIMITTED_SIZE);
        (void)memset_s(bufname, FAT_SHORT_ARRAY_LENGTH, 0, FAT_SHORT_ARRAY_LENGTH);

        (void)snprintf_s(bufname, FAT_SHORT_ARRAY_LENGTH, FAT_SHORT_ARRAY_LENGTH, "/test%d", j);
        (void)strcat_s(g_fatPathname11[j], FAT_NAME_LIMITTED_SIZE, pathname);
        (void)strcat_s(g_fatPathname12[j], FAT_NAME_LIMITTED_SIZE, pathname2);

        (void)strcat_s(g_fatPathname11[j], FAT_NAME_LIMITTED_SIZE, bufname);
        (void)strcat_s(g_fatPathname12[j], FAT_NAME_LIMITTED_SIZE, bufname);

        (void)strcat_s(g_fatPathname11[j], FAT_NAME_LIMITTED_SIZE, ".txt");
        (void)strcat_s(g_fatPathname12[j], FAT_NAME_LIMITTED_SIZE, ".txt");

        g_fatFd11[j] = open(g_fatPathname11[j], O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, S_IRWXU | S_IRWXG | S_IRWXO);
        ICUNIT_GOTO_NOT_EQUAL(g_fatFd11[j], -1, g_fatFd11[j], EXIT2);
    }

    (void)pthread_mutex_unlock(&g_vfatGlobalLock1);

    LosTaskDelay(1);

    ret = pthread_mutex_lock(&g_vfatGlobalLock1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

    off = lseek(g_fatFd11[0], 0, SEEK_END);
    ICUNIT_GOTO_EQUAL(off, 100 * BYTES_PER_MBYTES, off, EXIT2); // BYTES_PER_MBYTES = 100 MB

    off = lseek(g_fatFd11[1], 0, SEEK_END);
    ICUNIT_GOTO_EQUAL(off, FAT_SHORT_ARRAY_LENGTH * BYTES_PER_MBYTES, off, EXIT2); // BYTES_PER_MBYTES = 1MB

    off = lseek(g_fatFd11[2], 0, SEEK_END);               // the 2 nd fd
    ICUNIT_GOTO_EQUAL(off, BYTES_PER_MBYTES, off, EXIT2); // BYTES_PER_MBYTES = 1MB

    off = lseek(g_fatFd11[3], 0, SEEK_END);                     // fd 3
    ICUNIT_GOTO_EQUAL(off, 256 * BYTES_PER_KBYTES, off, EXIT2); // 256 * BYTES_PER_KBYTES = 256KB

    for (j = 0; j < 4; j++) { // loop 4 times
        ret = close(g_fatFd11[j]);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);
    }

    for (j = 0; j < FAT_MAXIMUM_OPERATIONS; j++) {
        ret = umount(FAT_MOUNT_DIR);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

        ret = mount(FAT_DEV_PATH, FAT_MOUNT_DIR, FAT_FILESYS_TYPE, 0, NULL);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT3);
    }

    (void)pthread_mutex_unlock(&g_vfatGlobalLock1);

    LosTaskDelay(1);

    ret = pthread_mutex_lock(&g_vfatGlobalLock1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

    for (j = 0; j < FAT_MAXIMUM_OPERATIONS; j++) {
        ret = umount(FAT_MOUNT_DIR);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

        ret = mount(FAT_DEV_PATH, FAT_MOUNT_DIR, FAT_FILESYS_TYPE, 0, NULL);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT3);
    }

    for (j = 0; j < 4; j++) { // loop 4 times
        ret = access(g_fatPathname12[j], F_OK);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT3);
    }

    g_fatFlag++;

    (void)pthread_mutex_unlock(&g_vfatGlobalLock1);

    return NULL;
EXIT3:
    mount(FAT_DEV_PATH, FAT_MOUNT_DIR, FAT_FILESYS_TYPE, 0, NULL);
EXIT2:
    for (j = 0; j < 4; j++) { // loop 4 times
        close(g_fatFd11[j]);
    }

    for (j = 0; j < 4; j++) { // loop 4 times
        remove(g_fatPathname11[j]);
        remove(g_fatPathname12[j]);
    }
EXIT:
    g_testCount = 0;
    remove(pathname2);
    (void)pthread_mutex_unlock(&g_vfatGlobalLock1);
    return NULL;
}

static VOID *PthreadF02(void *arg)
{
    INT32 ret;
    INT32 j = 0;
    CHAR pathname[FAT_STANDARD_NAME_LENGTH] = FAT_PATH_NAME;
    CHAR pathname2[FAT_STANDARD_NAME_LENGTH] = "/vs/sd/fat_1655";
    CHAR filebuf[260] = "abcdeabcde0123456789abcedfghij9876550210abcdeabcde0123456789abcedfghij9876550210abcdeabcde0123"
        "456789abcedfghij9876550210abcdeabcde0123456789abcedfghij9876550210abcdeabcde0123456789abcedfgh"
        "ij9876550210abcdeabcde0123456789abcedfghij9876550210lalalalalalalala";
    CHAR *bufWrite1 = NULL;
    CHAR *bufWrite2 = NULL;
    CHAR *bufWrite3 = NULL;
    CHAR *bufWrite4 = NULL;
    off_t off;

    ret = pthread_mutex_lock(&g_vfatGlobalLock1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

    bufWrite1 = (CHAR *)malloc(10 * BYTES_PER_MBYTES + 1); // 10 * BYTES_PER_MBYTES = 10MB
    ICUNIT_GOTO_NOT_EQUAL(bufWrite1, NULL, 0, EXIT2);
    (void)memset_s(bufWrite1, 10 * BYTES_PER_MBYTES + 1, 0, 10 * BYTES_PER_MBYTES + 1); // 10 * BYTES_PER_MBYTES = 10MB

    bufWrite2 = (CHAR *)malloc(BYTES_PER_MBYTES + 1); // BYTES_PER_MBYTES = 1MB
    ICUNIT_GOTO_NOT_EQUAL(bufWrite2, NULL, 0, EXIT3);
    (void)memset_s(bufWrite2, BYTES_PER_MBYTES + 1, 0, BYTES_PER_MBYTES + 1); // BYTES_PER_MBYTES = 1MB

    bufWrite3 = (CHAR *)malloc(256 * BYTES_PER_KBYTES + 1); // 256 * BYTES_PER_KBYTES = 256KB
    ICUNIT_GOTO_NOT_EQUAL(bufWrite3, NULL, 0, EXIT4);
    (void)memset_s(bufWrite3, 256 * BYTES_PER_KBYTES + 1, 0, 256 * BYTES_PER_KBYTES + 1); // 256 kb

    bufWrite4 = (CHAR *)malloc(16 * BYTES_PER_KBYTES + 1); // 16 * BYTES_PER_KBYTES = 16KB
    ICUNIT_GOTO_NOT_EQUAL(bufWrite4, NULL, 0, EXIT5);
    (void)memset_s(bufWrite4, 16 * BYTES_PER_KBYTES + 1, 0, 16 * BYTES_PER_KBYTES + 1); // 16 kb

    for (j = 0; j < 16; j++) {                                         // loop 16 times
        (void)strcat_s(bufWrite4, 16 * BYTES_PER_KBYTES + 1, filebuf); // 16 kb
        (void)strcat_s(bufWrite4, 16 * BYTES_PER_KBYTES + 1, filebuf); // 16 kb
        (void)strcat_s(bufWrite4, 16 * BYTES_PER_KBYTES + 1, filebuf); // 16 kb
        (void)strcat_s(bufWrite4, 16 * BYTES_PER_KBYTES + 1, filebuf); // 16 kb
    }

    for (j = 0; j < 4; j++) {                                             // loop 4 times
        (void)strcat_s(bufWrite3, 256 * BYTES_PER_KBYTES + 1, bufWrite4); // 256 kb
        (void)strcat_s(bufWrite3, 256 * BYTES_PER_KBYTES + 1, bufWrite4); // 256 kb
        (void)strcat_s(bufWrite3, 256 * BYTES_PER_KBYTES + 1, bufWrite4); // 256 kb
        (void)strcat_s(bufWrite3, 256 * BYTES_PER_KBYTES + 1, bufWrite4); // 256 kb
    }

    for (j = 0; j < 2; j++) { // loop 2 times
        (void)strcat_s(bufWrite2, BYTES_PER_MBYTES + 1, bufWrite3);
        (void)strcat_s(bufWrite2, BYTES_PER_MBYTES + 1, bufWrite3);
    }

    for (j = 0; j < 2; j++) {                                            // loop 2 times
        (void)strcat_s(bufWrite1, 10 * BYTES_PER_MBYTES + 1, bufWrite2); // 10 mb
        (void)strcat_s(bufWrite1, 10 * BYTES_PER_MBYTES + 1, bufWrite2); // 10 mb
        (void)strcat_s(bufWrite1, 10 * BYTES_PER_MBYTES + 1, bufWrite2); // 10 mb
        (void)strcat_s(bufWrite1, 10 * BYTES_PER_MBYTES + 1, bufWrite2); // 10 mb
        (void)strcat_s(bufWrite1, 10 * BYTES_PER_MBYTES + 1, bufWrite2); // 10 mb
    }

    free(bufWrite4);

    g_testCount++;

    for (j = 0; j < 10; j++) { // loop 10 times
        ret = write(g_fatFd11[0], bufWrite1, strlen(bufWrite1));
        ICUNIT_GOTO_EQUAL(ret, 10 * BYTES_PER_MBYTES, ret, EXIT5); // 10 * BYTES_PER_MBYTES = 10MB
    }

    ret = write(g_fatFd11[1], bufWrite1, strlen(bufWrite1));
    ICUNIT_GOTO_EQUAL(ret, 10 * BYTES_PER_MBYTES, ret, EXIT5); // 10 * BYTES_PER_MBYTES = 10MB

    ret = write(g_fatFd11[2], bufWrite2, strlen(bufWrite2)); // fd 2
    ICUNIT_GOTO_EQUAL(ret, BYTES_PER_MBYTES, ret, EXIT5);    // BYTES_PER_MBYTES = 1MB

    ret = write(g_fatFd11[3], bufWrite3, strlen(bufWrite3));    // fd 3
    ICUNIT_GOTO_EQUAL(ret, 256 * BYTES_PER_KBYTES, ret, EXIT5); // 256 * BYTES_PER_KBYTES = 256KB

    free(bufWrite1);
    free(bufWrite2);
    free(bufWrite3);

    (void)pthread_mutex_unlock(&g_vfatGlobalLock1);

    LosTaskDelay(1);

    ret = pthread_mutex_lock(&g_vfatGlobalLock1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

    ret = rename(pathname, pathname2);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

    (void)pthread_mutex_unlock(&g_vfatGlobalLock1);

    LosTaskDelay(2); // delay 2 s

    ret = pthread_mutex_lock(&g_vfatGlobalLock1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

    for (j = 0; j < 4; j++) { // loop 4 times
        g_fatFd11[j] = open(g_fatPathname12[j], O_NONBLOCK | O_RDWR, S_IRWXU | S_IRWXG | S_IRWXO);
        ICUNIT_GOTO_NOT_EQUAL(g_fatFd11[j], -1, g_fatFd11[j], EXIT2);
    }

    off = lseek(g_fatFd11[0], 0, SEEK_END);
    ICUNIT_GOTO_EQUAL(off, 100 * BYTES_PER_MBYTES, off, EXIT2); // BYTES_PER_MBYTES = 100 MB

    off = lseek(g_fatFd11[1], 0, SEEK_END);
    ICUNIT_GOTO_EQUAL(off, 10 * BYTES_PER_MBYTES, off, EXIT2); // 10 mb

    off = lseek(g_fatFd11[2], 0, SEEK_END);               // the 2 nd fd
    ICUNIT_GOTO_EQUAL(off, BYTES_PER_MBYTES, off, EXIT2); // BYTES_PER_MBYTES = 1MB

    off = lseek(g_fatFd11[3], 0, SEEK_END);                     // fd 3
    ICUNIT_GOTO_EQUAL(off, 256 * BYTES_PER_KBYTES, off, EXIT2); // 256 * BYTES_PER_KBYTES = 256KB

    for (j = 0; j < 4; j++) { // loop 4 times
        ret = close(g_fatFd11[j]);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

        ret = unlink(g_fatPathname12[j]);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);
    }
    ret = remove(pathname2);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = mkdir(pathname, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    g_fatFlagF02++;

    (void)pthread_mutex_unlock(&g_vfatGlobalLock1);

    LosTaskDelay(2); // delay 2 s

    return NULL;
EXIT5:
    free(bufWrite3);
EXIT4:
    free(bufWrite2);
EXIT3:
    free(bufWrite1);
EXIT2:
    for (j = 0; j < 4; j++) { // loop 4 times
        close(g_fatFd11[j]);
    }
EXIT1:
    for (j = 0; j < 4; j++) { // loop 4 times
        remove(g_fatPathname11[j]);
        remove(g_fatPathname12[j]);
    }

    g_testCount = 0;
    remove(pathname2);
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

    ret = mkdir(pathname, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    ret = chdir("/");
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

        sleep(5); // sleep 5s

        ret = PosixPthreadDestroy(&attr2, newThread2);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

        ret = PosixPthreadDestroy(&attr1, newThread1);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

        printf("\tg_fatFlag=:%d\t", g_fatFlag);
    }
    printf("\n");

    dir = opendir(FAT_PATH_NAME);
    ICUNIT_GOTO_NOT_EQUAL(dir, NULL, dir, EXIT3);

    ptr = readdir(dir);
    ICUNIT_GOTO_EQUAL(ptr, NULL, ptr, EXIT3);

    ret = closedir(dir);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT3);

    ICUNIT_GOTO_EQUAL(g_fatFlag, FAT_PRESSURE_CYCLES, g_fatFlag, EXIT);

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
* -@test IT_FS_FAT_PRESSURE_041
* -@tspec The Pressure test
* -@ttitle After the creation of four documents 1000 umount / mount 1,000 months after renaming the directory
* -@tprecon The filesystem module is open
* -@tbrief
1. create two tasks;
2. the first task create 4 files ;
3. the another task rename the directory and mount/umount the filesysterm;
4. delete the files;
* -@texpect
1. Return successed
2. Return successed
3. Return successed
4. Successful operation
* -@tprior 1
* -@tauto TRUE
* -@tremark
*/

VOID ItFsFatPressure041(VOID)
{
    TEST_ADD_CASE("IT_FS_FAT_PRESSURE_041", TestCase, TEST_VFS, TEST_VFAT, TEST_LEVEL3, TEST_PRESSURE);
}
