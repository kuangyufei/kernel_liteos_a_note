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
    INT32 i, j, ret, len;
    INT32 index = 0;
    INT32 index1 = 0;
    INT32 fd[100] = {};
    INT32 flag = 0;
    CHAR pathname[FAT_STANDARD_NAME_LENGTH] = "";
    CHAR pathname1[FAT_STANDARD_NAME_LENGTH] = "";
    CHAR writebuf[FAT_STANDARD_NAME_LENGTH] = "1111";
    CHAR filebuf[260] = "01234567890123456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123"
        "456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfgh"
        "ij9876543210abcdeabcde0123456789abcedfghij9876543210lalalalalalalala";
    CHAR *bufWrite = nullptr;
    CHAR buffile[FAT_NAME_LIMITTED_SIZE] = FAT_PATH_NAME0;
    CHAR buffile2[FAT_STANDARD_NAME_LENGTH] = FAT_PATH_NAME1;
    struct stat buf1 = { 0 };
    struct stat buf2 = { 0 };

    bufWrite = (CHAR *)malloc(BYTES_PER_MBYTES + 1); // BYTES_PER_MBYTES = 1MB
    ICUNIT_ASSERT_NOT_EQUAL_NULL(bufWrite, NULL, NULL);
    (void)memset_s(bufWrite, BYTES_PER_MBYTES + 1, 0, BYTES_PER_MBYTES + 1); // BYTES_PER_MBYTES = 1MB

    for (i = 0; i < BYTES_PER_KBYTES * 4; i++) { // loop in 4 Kb
        (void)strcat_s(bufWrite, BYTES_PER_MBYTES, filebuf);
    }

    index = 0;
    for (i = 0; i < FAT_STANDARD_NAME_LENGTH; i++) { // 50 个文件
        (void)snprintf_s(pathname, FAT_STANDARD_NAME_LENGTH, FAT_STANDARD_NAME_LENGTH, "/vs/sd/test0/file%d.txt",
            index);
        fd[index] = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, S_IRWXU | S_IRWXG | S_IRWXO);
        if (fd[index] == -1) {
            break;
        }

        for (j = 0; j < FAT_MAX_CYCLES; j++) { // 写100M
            len = write(fd[index], bufWrite, strlen(bufWrite));
            if (len <= 0) {
                flag = 1;
                break;
            }
        }

        ret = close(fd[index]);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

        if (flag == 1) {
            break;
        }
        index++;
    }

    if (flag == 0) {
        index--;
    }

    ret = stat(buffile, &buf1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);
    FatStatPrintf(buf1);

    for (i = index; i >= 0; i--) {
        (void)snprintf_s(pathname, FAT_STANDARD_NAME_LENGTH, FAT_STANDARD_NAME_LENGTH, "/vs/sd/test0/file%d.txt", i);
        ret = unlink(pathname);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);
    }

    free(bufWrite);
    index1 = FAT_STANDARD_NAME_LENGTH;
    for (i = FAT_STANDARD_NAME_LENGTH; i < FAT_MAX_CYCLES; i++) { // 50 个文件
        (void)snprintf_s(pathname1, FAT_STANDARD_NAME_LENGTH, FAT_STANDARD_NAME_LENGTH, "/vs/sd/test1/file%d.txt",
            index1);
        fd[index1] = open(pathname1, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, S_IRWXU | S_IRWXG | S_IRWXO);
        if (fd[index1] == -1) {
            break;
        }
        len = write(fd[index1], writebuf, strlen(writebuf));
        ICUNIT_GOTO_EQUAL(len, 4, len, EXIT1); // write 4 bytes
        if (len <= 0) {
            flag = 1;
            break;
        }

        LosTaskDelay(7); // delay 7 s
        printf("pthread_f01 is excecuting\n");

        ret = close(fd[index1]);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

        if (flag == 1) {
            break;
        }
        index1++;
    }

    if (flag == 0) {
        index1--;
    }
    ret = stat(buffile2, &buf2);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);
    FatStatPrintf(buf2);

    for (i = index1; i >= FAT_STANDARD_NAME_LENGTH; i--) {
        (void)snprintf_s(pathname1, FAT_STANDARD_NAME_LENGTH, FAT_STANDARD_NAME_LENGTH, "/vs/sd/test1/file%d.txt", i);
        ret = unlink(pathname1);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);
    }

    g_testCount++;

    return NULL;

    free(bufWrite);
EXIT1:
    for (i = index1; i >= FAT_STANDARD_NAME_LENGTH; i--) {
        close(fd[i]);
    }

    for (i = index; i >= 0; i--) {
        close(fd[i]);
    }
EXIT:

    for (i = index1; i >= FAT_STANDARD_NAME_LENGTH; i--) {
        (void)snprintf_s(pathname1, FAT_STANDARD_NAME_LENGTH, FAT_STANDARD_NAME_LENGTH, "/vs/sd/test1/file%d.txt", i);
        unlink(pathname1);
    }

    for (i = index; i >= 0; i--) {
        (void)snprintf_s(pathname, FAT_STANDARD_NAME_LENGTH, FAT_STANDARD_NAME_LENGTH, "/vs/sd/test0/file%d.txt", i);
        unlink(pathname);
    }
    free(bufWrite);
    return NULL;
}

static VOID *PthreadF02(void *arg)
{
    INT32 i, j, ret, len;
    INT32 index = 0;
    INT32 index1 = 0;
    INT32 fd[100] = {};
    INT32 flag = 0;
    CHAR pathname[FAT_STANDARD_NAME_LENGTH] = "";
    CHAR pathname1[FAT_STANDARD_NAME_LENGTH] = "";
    CHAR writebuf[FAT_STANDARD_NAME_LENGTH] = "1111";
    CHAR filebuf[260] = "01234567890123456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123"
        "456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfgh"
        "ij9876543210abcdeabcde0123456789abcedfghij9876543210lalalalalalalala";
    CHAR *bufWrite = nullptr;
    CHAR buffile[FAT_NAME_LIMITTED_SIZE] = FAT_PATH_NAME1;
    CHAR buffile2[FAT_STANDARD_NAME_LENGTH] = FAT_PATH_NAME2;
    struct stat buf1 = { 0 };
    struct stat buf2 = { 0 };

    bufWrite = (CHAR *)malloc(BYTES_PER_MBYTES + 1); // BYTES_PER_MBYTES = 1MB
    ICUNIT_ASSERT_NOT_EQUAL_NULL(bufWrite, NULL, NULL);
    (void)memset_s(bufWrite, BYTES_PER_MBYTES + 1, 0, BYTES_PER_MBYTES + 1); // BYTES_PER_MBYTES = 1MB

    for (i = 0; i < BYTES_PER_KBYTES * 4; i++) { // loop in 4 Kb
        (void)strcat_s(bufWrite, BYTES_PER_MBYTES, filebuf);
    }

    index = 0;
    for (i = 0; i < FAT_STANDARD_NAME_LENGTH; i++) { // 50 个文件
        (void)snprintf_s(pathname, FAT_STANDARD_NAME_LENGTH, FAT_STANDARD_NAME_LENGTH, "/vs/sd/test1/file%d.txt",
            index);
        fd[index] = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, S_IRWXU | S_IRWXG | S_IRWXO);
        if (fd[index] == -1) {
            break;
        }

        for (j = 0; j < FAT_MAX_CYCLES; j++) { // 写100M
            len = write(fd[index], bufWrite, strlen(bufWrite));
            if (len <= 0) {
                flag = 1;
                break;
            }
        }

        LosTaskDelay(3); // delay 3s
        printf("pthread_f02 is excecuting\n");

        ret = close(fd[index]);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

        if (flag == 1) {
            break;
        }
        index++;
    }

    if (flag == 0) {
        index--;
    }

    ret = stat(buffile, &buf1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);
    FatStatPrintf(buf1);

    for (i = index; i >= 0; i--) {
        (void)snprintf_s(pathname, FAT_STANDARD_NAME_LENGTH, FAT_STANDARD_NAME_LENGTH, "/vs/sd/test1/file%d.txt", i);
        ret = unlink(pathname);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);
    }

    free(bufWrite);
    index1 = FAT_STANDARD_NAME_LENGTH;
    for (i = FAT_STANDARD_NAME_LENGTH; i < FAT_MAX_CYCLES; i++) { // 50 个文件
        (void)snprintf_s(pathname1, FAT_STANDARD_NAME_LENGTH, FAT_STANDARD_NAME_LENGTH, "/vs/sd/test2/file%d.txt",
            index1);
        fd[index1] = open(pathname1, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, S_IRWXU | S_IRWXG | S_IRWXO);
        if (fd[index1] == -1) {
            break;
        }
        len = write(fd[index1], writebuf, strlen(writebuf));
        ICUNIT_GOTO_EQUAL(len, 4, len, EXIT1); // write 4 bytes
        if (len <= 0) {
            flag = 1;
            break;
        }

        ret = close(fd[index1]);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

        if (flag == 1) {
            break;
        }
        index1++;
    }

    if (flag == 0) {
        index1--;
    }
    ret = stat(buffile2, &buf2);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);
    FatStatPrintf(buf2);

    for (i = index1; i >= FAT_STANDARD_NAME_LENGTH; i--) {
        (void)snprintf_s(pathname1, FAT_STANDARD_NAME_LENGTH, FAT_STANDARD_NAME_LENGTH, "/vs/sd/test2/file%d.txt", i);
        ret = unlink(pathname1);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);
    }

    g_testCount++;

    return NULL;

    free(bufWrite);
EXIT1:
    for (i = index1; i >= FAT_STANDARD_NAME_LENGTH; i--) {
        close(fd[i]);
    }

    for (i = index; i >= 0; i--) {
        close(fd[i]);
    }
EXIT:

    for (i = index1; i >= FAT_STANDARD_NAME_LENGTH; i--) {
        (void)snprintf_s(pathname1, FAT_STANDARD_NAME_LENGTH, FAT_STANDARD_NAME_LENGTH, "/vs/sd/test2/file%d.txt", i);
        unlink(pathname1);
    }

    for (i = index; i >= 0; i--) {
        (void)snprintf_s(pathname, FAT_STANDARD_NAME_LENGTH, FAT_STANDARD_NAME_LENGTH, "/vs/sd/test1/file%d.txt", i);
        unlink(pathname);
    }
    free(bufWrite);
    return NULL;
}

static VOID *PthreadF03(void *arg)
{
    INT32 i, j, ret, len;
    INT32 index = 0;
    INT32 index1 = 0;
    INT32 fd[100] = {};
    INT32 flag = 0;
    CHAR pathname[FAT_STANDARD_NAME_LENGTH] = "";
    CHAR pathname1[FAT_STANDARD_NAME_LENGTH] = "";
    CHAR writebuf[FAT_STANDARD_NAME_LENGTH] = "1111";
    CHAR filebuf[260] = "01234567890123456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123"
        "456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfgh"
        "ij9876543210abcdeabcde0123456789abcedfghij9876543210lalalalalalalala";
    CHAR *bufWrite = nullptr;
    CHAR buffile[FAT_NAME_LIMITTED_SIZE] = FAT_PATH_NAME0;
    CHAR buffile2[FAT_STANDARD_NAME_LENGTH] = FAT_PATH_NAME2;
    struct stat buf1 = { 0 };
    struct stat buf2 = { 0 };

    bufWrite = (CHAR *)malloc(BYTES_PER_MBYTES + 1); // BYTES_PER_MBYTES = 1MB
    ICUNIT_ASSERT_NOT_EQUAL_NULL(bufWrite, NULL, NULL);
    (void)memset_s(bufWrite, BYTES_PER_MBYTES + 1, 0, BYTES_PER_MBYTES + 1); // BYTES_PER_MBYTES = 1MB

    for (i = 0; i < BYTES_PER_KBYTES * 4; i++) { // loop in 4 Kb
        (void)strcat_s(bufWrite, BYTES_PER_MBYTES, filebuf);
    }

    index = 0;
    for (i = 0; i < FAT_STANDARD_NAME_LENGTH; i++) { // 50 个文件
        (void)snprintf_s(pathname, FAT_STANDARD_NAME_LENGTH, FAT_STANDARD_NAME_LENGTH, "/vs/sd/test0/file%d.txt",
            index);
        fd[index] = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, S_IRWXU | S_IRWXG | S_IRWXO);
        if (fd[index] == -1) {
            break;
        }

        for (j = 0; j < FAT_MAX_CYCLES; j++) { // 写100M
            len = write(fd[index], bufWrite, strlen(bufWrite));
            if (len <= 0) {
                flag = 1;
                break;
            }
        }

        LosTaskDelay(3); // 3s
        printf("pthread_f03 is excecuting\n");

        ret = close(fd[index]);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

        if (flag == 1) {
            break;
        }
        index++;
    }

    if (flag == 0) {
        index--;
    }

    ret = stat(buffile, &buf1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);
    FatStatPrintf(buf1);

    for (i = index; i >= 0; i--) {
        (void)snprintf_s(pathname, FAT_STANDARD_NAME_LENGTH, FAT_STANDARD_NAME_LENGTH, "/vs/sd/test0/file%d.txt", i);
        ret = unlink(pathname);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);
    }

    free(bufWrite);
    index1 = FAT_STANDARD_NAME_LENGTH;
    for (i = FAT_STANDARD_NAME_LENGTH; i < FAT_MAX_CYCLES; i++) { // 50 个文件
        (void)snprintf_s(pathname1, FAT_STANDARD_NAME_LENGTH, FAT_STANDARD_NAME_LENGTH, "/vs/sd/test2/file%d.txt",
            index1);
        fd[index1] = open(pathname1, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, S_IRWXU | S_IRWXG | S_IRWXO);
        if (fd[index1] == -1) {
            break;
        }
        len = write(fd[index1], writebuf, strlen(writebuf));
        ICUNIT_GOTO_EQUAL(len, 4, len, EXIT1); // write 4 bytes
        if (len <= 0) {
            flag = 1;
            break;
        }

        ret = close(fd[index1]);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

        if (flag == 1) {
            break;
        }
        index1++;
    }

    if (flag == 0) {
        index1--;
    }
    ret = stat(buffile2, &buf2);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);
    FatStatPrintf(buf2);

    for (i = index1; i >= FAT_STANDARD_NAME_LENGTH; i--) {
        (void)snprintf_s(pathname1, FAT_STANDARD_NAME_LENGTH, FAT_STANDARD_NAME_LENGTH, "/vs/sd/test2/file%d.txt", i);
        ret = unlink(pathname1);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);
    }
    g_testCount++;

    return NULL;

EXIT1:
    for (i = index1; i >= FAT_STANDARD_NAME_LENGTH; i--) {
        close(fd[i]);
    }

    for (i = index; i >= 0; i--) {
        close(fd[i]);
    }
EXIT:

    for (i = index1; i >= FAT_STANDARD_NAME_LENGTH; i--) {
        (void)snprintf_s(pathname1, FAT_STANDARD_NAME_LENGTH, FAT_STANDARD_NAME_LENGTH, "/vs/sd/test2/file%d.txt", i);
        unlink(pathname1);
    }

    for (i = index; i >= 0; i--) {
        (void)snprintf_s(pathname, FAT_STANDARD_NAME_LENGTH, FAT_STANDARD_NAME_LENGTH, "/vs/sd/test0/file%d.txt", i);
        unlink(pathname);
    }
    free(bufWrite);
    return NULL;
}

static UINT32 TestCase(VOID)
{
    INT32 ret, i;
    CHAR bufname1[FAT_STANDARD_NAME_LENGTH] = FAT_PATH_NAME0;
    CHAR bufname2[FAT_STANDARD_NAME_LENGTH] = FAT_PATH_NAME00;
    CHAR bufname3[FAT_STANDARD_NAME_LENGTH] = FAT_PATH_NAME1;
    CHAR bufname4[FAT_STANDARD_NAME_LENGTH] = FAT_PATH_NAME11;
    CHAR bufname5[FAT_STANDARD_NAME_LENGTH] = FAT_PATH_NAME2;
    CHAR bufname6[FAT_STANDARD_NAME_LENGTH] = FAT_PATH_NAME22;
    pthread_attr_t attr[FAT_MAX_THREADS];
    pthread_t threadId[FAT_MAX_THREADS];

    g_testCount = 0;
    ret = mkdir(bufname1, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    ret = mkdir(bufname2, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    ret = mkdir(bufname3, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    ret = mkdir(bufname4, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    ret = mkdir(bufname5, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    ret = mkdir(bufname6, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);
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

    ret = pthread_create(&threadId[2], &attr[2], PthreadF03, (void *)2); // the no 2 thread
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

    ICUNIT_ASSERT_EQUAL(g_testCount, 3, g_testCount); // there 3 threads

    ret = rmdir(bufname2);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    ret = rmdir(bufname1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    ret = rmdir(bufname4);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    ret = rmdir(bufname3);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    ret = rmdir(bufname6);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    ret = rmdir(bufname5);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    return FAT_NO_ERROR;

EXIT1:
    for (i = 0; i < FAT_MAX_THREADS; i++) {
        pthread_join(threadId[i], NULL);
    }
    for (i = 0; i < FAT_MAX_THREADS; i++) {
        pthread_attr_destroy(&attr[i]);
    }
EXIT:
    rmdir(bufname2);
    rmdir(bufname1);
    rmdir(bufname4);
    rmdir(bufname3);
    rmdir(bufname6);
    rmdir(bufname5);

    return FAT_NO_ERROR;
}

/* *
* - @test IT_FS_FAT_MUTIPTHREAD_008
* - @tspec function test
* - @ttitle multithreading reads the same file at the same time
* - @tbrief
1. Create a file to open
2. Create three threads to simultaneously read this file.
3. Delete all files and threads;
* - @ tprior 1
* - @ tauto TRUE
* - @ tremark
*/

VOID ItFsFatMutipthread008(VOID)
{
    TEST_ADD_CASE("IT_FS_FAT_MUTIPTHREAD_008", TestCase, TEST_VFS, TEST_VFAT, TEST_LEVEL4, TEST_PRESSURE);
}
