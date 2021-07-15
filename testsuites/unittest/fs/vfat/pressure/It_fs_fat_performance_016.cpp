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

static VOID *PthreadF01(void *arg)
{
    INT32 i, j, ret, len, index;
    INT32 fd[FAT_FILE_LIMITTED_NUM] = {};
    INT32 flag = 0;
    CHAR bufname[FAT_STANDARD_NAME_LENGTH] = FAT_PATH_NAME0;
    CHAR pathname[FAT_STANDARD_NAME_LENGTH] = "";
    CHAR filebuf[260] = "01234567890123456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123"
        "456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfgh"
        "ij9876543210abcdeabcde0123456789abcedfghij9876543210lalalalalalalala";
    CHAR *bufWrite = NULL;
    struct timeval testTime1;
    struct timeval testTime2;

    bufWrite = (CHAR *)malloc(BYTES_PER_MBYTES + 1); // BYTES_PER_MBYTES = 1MB
    ICUNIT_ASSERT_NOT_EQUAL_NULL(bufWrite, NULL, NULL);
    (void)memset_s(bufWrite, BYTES_PER_MBYTES + 1, 0, BYTES_PER_MBYTES + 1); // BYTES_PER_MBYTES = 1MB

    for (i = 0; i < 4; i++) { // append 4 times to 1M
        (void)strcat_s(bufWrite, BYTES_PER_MBYTES + 1, filebuf);
    }

    ret = mkdir(bufname, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    index = 0;
    for (i = 0; i < FAT_FILE_LIMITTED_NUM; i++) { // 200 个文件
        (void)snprintf_s(pathname, FAT_STANDARD_NAME_LENGTH, FAT_STANDARD_NAME_LENGTH, "/vs/sd/test0/file%d.txt",
            index);
        fd[index] = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, S_IRWXU | S_IRWXG | S_IRWXO);
        if (fd[index] == -1) {
            break;
        }

        for (j = 0; j < FAT_NAME_LIMITTED_SIZE; j++) { // 300K
            len = write(fd[index], bufWrite, strlen(bufWrite));
            if (len <= 0) {
                flag = 1;
                break;
            }
        }

        LosTaskDelay(3); // delay 3s
        printf("pthread_f01 is excecuting\n");

        ret = close(fd[index]);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

        if (flag == 1) {
            break;
        }
        index++;
    }

    if (flag == 0) {
        index--;
    }

    gettimeofday(&testTime1, 0);
    for (i = index; i >= 0; i--) {
        (void)snprintf_s(pathname, FAT_STANDARD_NAME_LENGTH, FAT_STANDARD_NAME_LENGTH, "/vs/sd/test0/file%d.txt", i);
        ret = unlink(pathname);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);
    }

    ret = rmdir(bufname);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);
    gettimeofday(&testTime2, 0);
    printf("FF--%s:%d, time: %lld\n", __func__, __LINE__,
        (testTime2.tv_sec - testTime1.tv_sec) * US_PER_SEC + // US_PER_SEC
        (testTime2.tv_usec - testTime1.tv_usec));

    g_testCount++;
    free(bufWrite);

    return NULL;
EXIT2:
    close(fd[index]);
EXIT1:
    for (; i >= 0; i--) {
        (void)snprintf_s(pathname, FAT_STANDARD_NAME_LENGTH, FAT_STANDARD_NAME_LENGTH, "/vs/sd/test0/file%d.txt", i);
        unlink(pathname);
    }
EXIT:
    free(bufWrite);
    rmdir(bufname);
    return NULL;
}

static VOID *PthreadF02(void *arg)
{
    INT32 i, j, ret, len, index;
    INT32 fd[FAT_FILE_LIMITTED_NUM] = {};
    INT32 flag = 0;
    CHAR bufname[FAT_STANDARD_NAME_LENGTH] = FAT_PATH_NAME1;
    CHAR pathname[FAT_STANDARD_NAME_LENGTH] = "";
    CHAR filebuf[260] = "01234567890123456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123"
        "456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfgh"
        "ij9876543210abcdeabcde0123456789abcedfghij9876543210lalalalalalalala";
    CHAR *bufWrite = NULL;
    struct timeval testTime1;
    struct timeval testTime2;

    bufWrite = (CHAR *)malloc(BYTES_PER_MBYTES + 1); // BYTES_PER_MBYTES = 1MB
    ICUNIT_ASSERT_NOT_EQUAL_NULL(bufWrite, NULL, NULL);
    (void)memset_s(bufWrite, BYTES_PER_MBYTES + 1, 0, BYTES_PER_MBYTES + 1); // BYTES_PER_MBYTES = 1MB

    for (i = 0; i < 4; i++) { // append 4 times to 1M
        (void)strcat_s(bufWrite, BYTES_PER_MBYTES + 1, filebuf);
    }

    ret = mkdir(bufname, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    index = 0;
    for (i = 0; i < FAT_FILE_LIMITTED_NUM; i++) { // 200 个文件
        (void)snprintf_s(pathname, FAT_STANDARD_NAME_LENGTH, FAT_STANDARD_NAME_LENGTH, "/vs/sd/test1/file%d.txt",
            index);
        fd[index] = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, S_IRWXU | S_IRWXG | S_IRWXO);
        if (fd[index] == -1) {
            break;
        }

        for (j = 0; j < FAT_NAME_LIMITTED_SIZE; j++) { // 300K
            len = write(fd[index], bufWrite, strlen(bufWrite));
            if (len <= 0) {
                flag = 1;
                break;
            }
        }

        LosTaskDelay(3); // delay 3s
        printf("pthread_f02 is excecuting\n");

        ret = close(fd[index]);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

        if (flag == 1) {
            break;
        }
        index++;
    }

    if (flag == 0) {
        index--;
    }

    gettimeofday(&testTime1, 0);
    for (i = index; i >= 0; i--) {
        (void)snprintf_s(pathname, FAT_STANDARD_NAME_LENGTH, FAT_STANDARD_NAME_LENGTH, "/vs/sd/test1/file%d.txt", i);
        ret = unlink(pathname);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);
    }

    ret = rmdir(bufname);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);
    gettimeofday(&testTime2, 0);
    printf("FF--%s:%d, time: %lld\n", __func__, __LINE__,
        (testTime2.tv_sec - testTime1.tv_sec) * US_PER_SEC + // US_PER_SEC
        (testTime2.tv_usec - testTime1.tv_usec));

    g_testCount++;
    free(bufWrite);

    return NULL;
EXIT2:
    close(fd[index]);
EXIT1:
    for (; i >= 0; i--) {
        (void)snprintf_s(pathname, FAT_STANDARD_NAME_LENGTH, FAT_STANDARD_NAME_LENGTH, "/vs/sd/test1/file%d.txt", i);
        unlink(pathname);
    }
EXIT:
    free(bufWrite);
    rmdir(bufname);
    return NULL;
}

static VOID *PthreadF03(void *arg)
{
    INT32 i, j, ret, len, index;
    INT32 fd[FAT_FILE_LIMITTED_NUM] = {};
    INT32 flag = 0;
    CHAR bufname[FAT_STANDARD_NAME_LENGTH] = FAT_PATH_NAME2;
    CHAR pathname[FAT_STANDARD_NAME_LENGTH] = "";
    CHAR filebuf[260] = "01234567890123456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123"
        "456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfgh"
        "ij9876543210abcdeabcde0123456789abcedfghij9876543210lalalalalalalala";
    CHAR *bufWrite = NULL;
    struct timeval testTime1;
    struct timeval testTime2;

    bufWrite = (CHAR *)malloc(BYTES_PER_MBYTES + 1); // BYTES_PER_MBYTES = 1MB
    ICUNIT_ASSERT_NOT_EQUAL_NULL(bufWrite, NULL, NULL);
    (void)memset_s(bufWrite, BYTES_PER_MBYTES + 1, 0, BYTES_PER_MBYTES + 1); // BYTES_PER_MBYTES = 1MB

    for (i = 0; i < 4; i++) { // append 4 times to 1M
        (void)strcat_s(bufWrite, BYTES_PER_MBYTES + 1, filebuf);
    }

    ret = mkdir(bufname, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    index = 0;
    for (i = 0; i < FAT_FILE_LIMITTED_NUM; i++) { // 200 个文件
        (void)snprintf_s(pathname, FAT_STANDARD_NAME_LENGTH, FAT_STANDARD_NAME_LENGTH, "/vs/sd/test2/file%d.txt",
            index);
        fd[index] = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, S_IRWXU | S_IRWXG | S_IRWXO);
        if (fd[index] == -1) {
            break;
        }

        for (j = 0; j < FAT_NAME_LIMITTED_SIZE; j++) { // 300K
            len = write(fd[index], bufWrite, strlen(bufWrite));
            if (len <= 0) {
                flag = 1;
                break;
            }
        }

        LosTaskDelay(3); // 3s
        printf("pthread_f03 is excecuting\n");

        ret = close(fd[index]);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT2);

        if (flag == 1) {
            break;
        }
        index++;
    }

    if (flag == 0) {
        index--;
    }

    gettimeofday(&testTime1, 0);
    for (i = index; i >= 0; i--) {
        (void)snprintf_s(pathname, FAT_STANDARD_NAME_LENGTH, FAT_STANDARD_NAME_LENGTH, "/vs/sd/test2/file%d.txt", i);
        ret = unlink(pathname);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);
    }

    ret = rmdir(bufname);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);
    gettimeofday(&testTime2, 0);
    printf("FF--%s:%d, time: %lld\n", __func__, __LINE__,
        (testTime2.tv_sec - testTime1.tv_sec) * US_PER_SEC + // US_PER_SEC
        (testTime2.tv_usec - testTime1.tv_usec));

    g_testCount++;
    free(bufWrite);

    return NULL;
EXIT2:
    close(fd[index]);
EXIT1:
    for (; i >= 0; i--) {
        (void)snprintf_s(pathname, FAT_STANDARD_NAME_LENGTH, FAT_STANDARD_NAME_LENGTH, "/vs/sd/test2/file%d.txt", i);
        unlink(pathname);
    }
EXIT:
    free(bufWrite);
    rmdir(bufname);
    return NULL;
}

static UINT32 TestCase(VOID)
{
    INT32 ret, i;
    pthread_attr_t attr[FAT_MAX_THREADS];
    pthread_t threadId[FAT_MAX_THREADS];

    g_testCount = 0;

    ret = chdir("/");
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    ret = umount(FAT_MOUNT_DIR);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT3);
    ret = mount(FAT_DEV_PATH, FAT_MOUNT_DIR, FAT_FILESYS_TYPE, 0, NULL);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT3);
    ret = PosixPthreadInit(&attr[0], TASK_PRIO_TEST - 2); // 2 less than TASK_PRIO_TEST
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = pthread_create(&threadId[0], &attr[0], PthreadF01, (void *)0);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = PosixPthreadInit(&attr[1], TASK_PRIO_TEST - 2); // 2 less than TASK_PRIO_TEST
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = pthread_create(&threadId[1], &attr[1], PthreadF02, (void *)1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = PosixPthreadInit(&attr[2], TASK_PRIO_TEST - 2); // 2 less than TASK_PRIO_TEST
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = pthread_create(&threadId[2], &attr[2], PthreadF03, (void *)2); // 2nd thread id
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);
    for (i = 0; i < FAT_MAX_THREADS; i++) {
        ret = pthread_join(threadId[i], NULL);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);
    }

    for (i = 0; i < FAT_MAX_THREADS; i++) {
        ret = pthread_attr_destroy(&attr[i]);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);
    }

    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ICUNIT_ASSERT_EQUAL(g_testCount, 3, g_testCount); // 3 threads

    return FAT_NO_ERROR;
EXIT3:
    mount(FAT_DEV_PATH, FAT_MOUNT_DIR, FAT_FILESYS_TYPE, 0, NULL);
    goto EXIT;

    chdir("/");
    umount(FAT_MOUNT_DIR);
EXIT1:
    for (i = 0; i < FAT_MAX_THREADS; i++) {
        pthread_join(threadId[i], NULL);
    }
    for (i = 0; i < FAT_MAX_THREADS; i++) {
        pthread_attr_destroy(&attr[i]);
    }
EXIT:
    return FAT_NO_ERROR;
}

/* *
* - @test IT_FS_FAT_PERFORMANCE_016
* - @tspec function test
* - @ttitle Multi-threaded takes time to delete directory files
* - @tbrief
1. Create 200 files per directory;
2. The size of each file is 300K;
3. Call the gettimeofday() to get the time;
4. Delete files and directories;
5. Call the gettimeofday() to get the time and View the time of serial port printing.
* - @ tprior 1
* - @ tauto TRUE
* - @ tremark
*/

VOID ItFsFatPerformance016(VOID)
{
    TEST_ADD_CASE("IT_FS_FAT_PERFORMANCE_016", TestCase, TEST_VFS, TEST_VFAT, TEST_LEVEL3, TEST_PERFORMANCE);
}
