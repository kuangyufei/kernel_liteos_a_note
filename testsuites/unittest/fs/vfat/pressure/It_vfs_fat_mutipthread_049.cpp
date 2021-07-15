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
    INT32 i, j, ret, len, flag, index;
    INT32 fd[100] = {};
    CHAR pathname[FAT_STANDARD_NAME_LENGTH] = "";
    CHAR readbuf[FAT_STANDARD_NAME_LENGTH] = "";
    CHAR filebuf[260] = "01234567890123456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123"
        "456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfgh"
        "ij9876543210abcdeabcde0123456789abcedfghij9876543210lalalalalalalala";
    CHAR writebuf[20] = "0123456789";
    CHAR *bufWrite = nullptr;

    flag = 0;
    bufWrite = (CHAR *)malloc(BYTES_PER_MBYTES + 1); // BYTES_PER_MBYTES = 1MB
    ICUNIT_ASSERT_NOT_EQUAL_NULL(bufWrite, NULL, NULL);
    (void)memset_s(bufWrite, BYTES_PER_MBYTES + 1, 0, BYTES_PER_MBYTES + 1); // BYTES_PER_MBYTES = 1MB

    for (i = 0; i < BYTES_PER_KBYTES * 4; i++) { // loop in 4 Kb
        (void)strcat_s(bufWrite, BYTES_PER_MBYTES + 1, filebuf);
    }

    index = 0;
    for (i = 0; i < FAT_MAX_CYCLES; i++) { // 100个文件
        (void)snprintf_s(pathname, FAT_STANDARD_NAME_LENGTH, FAT_STANDARD_NAME_LENGTH, "%s/file1%d.txt", FAT_PATH_NAME,
            index);
        fd[index] = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, S_IRWXU | S_IRWXG | S_IRWXO);
        if (fd[index] == -1) {
            break;
        }
        switch (index % 3) { // mod 3
            case 0:
                for (j = 0; j < FAT_MAX_CYCLES; j++) { // 写满100M
                    len = write(fd[index], bufWrite, strlen(bufWrite));
                    if (len <= 0) {
                        flag = 1;
                        break;
                    }
                }
                LosTaskDelay(3); // delay 3 s
                break;
            case 1:
                for (j = 0; j < FAT_MIDDLE_CYCLES; j++) { // 写满10M
                    len = write(fd[index], bufWrite, strlen(bufWrite));
                    if (len <= 0) {
                        flag = 1;
                        break;
                    }
                }
                LosTaskDelay(5); // delay 5 s
                break;
            case 2: // the 2 nd case
                len = write(fd[index], writebuf, strlen(writebuf));
                if (len <= 0) {
                    flag = 1;
                    break;
                }
                LosTaskDelay(7); // delay 7 s
                break;
            default:
                break;
        }

        printf("pthread_f01 is executing\n");

        ret = lseek(fd[index], 0, SEEK_SET);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);
        len = read(fd[index], readbuf, 10);     // read 10 bytes
        ICUNIT_GOTO_EQUAL(len, 10, len, EXIT1); // len must be 10
        ICUNIT_ASSERT_STRING_EQUAL_RET(readbuf, "0123456789", readbuf, NULL);
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

    for (i = index; i >= 0; i--) {
        (void)snprintf_s(pathname, FAT_STANDARD_NAME_LENGTH, FAT_STANDARD_NAME_LENGTH, "%s/file1%d.txt", FAT_PATH_NAME,
            i);
        ret = unlink(pathname);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);
    }

    g_testCount++;

    free(bufWrite);

    return NULL;
EXIT1:
    for (i = index; i >= 0; i--) {
        close(fd[i]);
    }
EXIT:
    for (i = index; i >= 0; i--) {
        (void)snprintf_s(pathname, FAT_STANDARD_NAME_LENGTH, FAT_STANDARD_NAME_LENGTH, "%s/file1%d.txt", FAT_PATH_NAME,
            i);
        unlink(pathname);
    }
    g_testCount = 0;
    free(bufWrite);
    return NULL;
}

static VOID *PthreadF02(void *arg)
{
    INT32 i, j, ret, len, flag, index;
    INT32 fd[100] = {};
    CHAR pathname[FAT_STANDARD_NAME_LENGTH] = "";
    CHAR filebuf[260] = "01234567890123456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123"
        "456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfgh"
        "ij9876543210abcdeabcde0123456789abcedfghij9876543210lalalalalalalala";
    CHAR writebuf[20] = "0123456789";
    CHAR *bufWrite = nullptr;
    struct stat statbuf;

    flag = 0;
    bufWrite = (CHAR *)malloc(BYTES_PER_MBYTES + 1); // BYTES_PER_MBYTES = 1MB
    ICUNIT_ASSERT_NOT_EQUAL_NULL(bufWrite, NULL, NULL);
    (void)memset_s(bufWrite, BYTES_PER_MBYTES + 1, 0, BYTES_PER_MBYTES + 1); // BYTES_PER_MBYTES = 1MB

    for (i = 0; i < BYTES_PER_KBYTES * 4; i++) { // loop in 4 Kb
        (void)strcat_s(bufWrite, BYTES_PER_MBYTES + 1, filebuf);
    }

    index = 0;
    for (i = 0; i < FAT_MAX_CYCLES; i++) { // 100个文件
        (void)snprintf_s(pathname, FAT_STANDARD_NAME_LENGTH, FAT_STANDARD_NAME_LENGTH, "%s/file2%d.txt", FAT_PATH_NAME,
            index);
        fd[index] = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, S_IRWXU | S_IRWXG | S_IRWXO);
        if (fd[index] == -1) {
            break;
        }
        switch (index % 3) { // mod 3
            case 0:
                for (j = 0; j < FAT_MAX_CYCLES; j++) { // 写满100M
                    len = write(fd[index], bufWrite, strlen(bufWrite));
                    if (len <= 0) {
                        flag = 1;
                        break;
                    }
                }
                LosTaskDelay(3); // delay 3 s
                break;
            case 1:
                for (j = 0; j < FAT_MIDDLE_CYCLES; j++) { // 写满10M
                    len = write(fd[index], bufWrite, strlen(bufWrite));
                    if (len <= 0) {
                        flag = 1;
                        break;
                    }
                }
                LosTaskDelay(5); // delay 5 s
                break;
            case 2: // the 2 nd case
                len = write(fd[index], writebuf, strlen(writebuf));
                if (len <= 0) {
                    flag = 1;
                    break;
                }
                LosTaskDelay(7); // delay 7 s
                break;
            default:
                break;
        }

        printf("pthread_f02 is executing\n");

        ret = close(fd[index]);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

        ret = stat(pathname, &statbuf);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);
        FatStatPrintf(statbuf);

        if (flag == 1) {
            break;
        }
        index++;
    }

    if (flag == 0) {
        index--;
    }

    for (i = index; i >= 0; i--) {
        (void)snprintf_s(pathname, FAT_STANDARD_NAME_LENGTH, FAT_STANDARD_NAME_LENGTH, "%s/file2%d.txt", FAT_PATH_NAME,
            i);
        ret = unlink(pathname);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);
    }

    g_testCount++;

    free(bufWrite);

    return NULL;
EXIT1:
    for (i = index; i >= 0; i--) {
        close(fd[i]);
    }
EXIT:
    for (i = index; i >= 0; i--) {
        (void)snprintf_s(pathname, FAT_STANDARD_NAME_LENGTH, FAT_STANDARD_NAME_LENGTH, "%s/file2%d.txt", FAT_PATH_NAME,
            i);
        unlink(pathname);
    }
    g_testCount = 0;
    free(bufWrite);
    return NULL;
}

static VOID *PthreadF03(void *arg)
{
    INT32 i, j, ret, len, flag, index;
    INT32 fd[100] = {};
    CHAR pathname[FAT_STANDARD_NAME_LENGTH] = "";
    CHAR pathname1[FAT_STANDARD_NAME_LENGTH] = "";
    CHAR filebuf[260] = "01234567890123456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123"
        "456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfgh"
        "ij9876543210abcdeabcde0123456789abcedfghij9876543210lalalalalalalala";
    CHAR writebuf[20] = "0123456789";
    CHAR *bufWrite = nullptr;

    flag = 0;
    bufWrite = (CHAR *)malloc(BYTES_PER_MBYTES + 1); // BYTES_PER_MBYTES = 1MB
    ICUNIT_ASSERT_NOT_EQUAL_NULL(bufWrite, NULL, NULL);
    (void)memset_s(bufWrite, BYTES_PER_MBYTES + 1, 0, BYTES_PER_MBYTES + 1); // BYTES_PER_MBYTES = 1MB

    for (i = 0; i < BYTES_PER_KBYTES * 4; i++) { // loop in 4 Kb
        (void)strcat_s(bufWrite, BYTES_PER_MBYTES + 1, filebuf);
    }

    index = 0;
    for (i = 0; i < FAT_MAX_CYCLES; i++) { // 100个文件
        (void)snprintf_s(pathname, FAT_STANDARD_NAME_LENGTH, FAT_STANDARD_NAME_LENGTH, "%s/file3%d.txt", FAT_PATH_NAME,
            index);
        fd[index] = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, S_IRWXU | S_IRWXG | S_IRWXO);
        if (fd[index] == -1) {
            break;
        }
        switch (index % 3) { // mod 3
            case 0:
                for (j = 0; j < FAT_MAX_CYCLES; j++) { // 写满100M
                    len = write(fd[index], bufWrite, strlen(bufWrite));
                    if (len <= 0) {
                        flag = 1;
                        break;
                    }
                }
                LosTaskDelay(3); // delay 3 s
                break;
            case 1:
                for (j = 0; j < FAT_MIDDLE_CYCLES; j++) { // 写满10M
                    len = write(fd[index], bufWrite, strlen(bufWrite));
                    if (len <= 0) {
                        flag = 1;
                        break;
                    }
                }
                LosTaskDelay(5); // delay 5 s
                break;
            case 2: // the 2 nd case
                len = write(fd[index], writebuf, strlen(writebuf));
                if (len <= 0) {
                    flag = 1;
                    break;
                }
                LosTaskDelay(7); // delay 7 s
                break;
            default:
                break;
        }

        printf("pthread_f03 is executing\n");

        ret = close(fd[index]);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

        (void)snprintf_s(pathname1, FAT_STANDARD_NAME_LENGTH, FAT_STANDARD_NAME_LENGTH, "%s/file3_%d.txt",
            FAT_PATH_NAME, index);
        ret = rename(pathname, pathname1);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

        if (flag == 1) {
            break;
        }
        index++;
    }

    if (flag == 0) {
        index--;
    }

    for (i = index; i >= 0; i--) {
        (void)snprintf_s(pathname, FAT_STANDARD_NAME_LENGTH, FAT_STANDARD_NAME_LENGTH, "%s/file3_%d.txt", FAT_PATH_NAME,
            i);
        ret = unlink(pathname);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);
    }

    g_testCount++;

    free(bufWrite);

    return NULL;
EXIT1:
    for (i = index; i >= 0; i--) {
        close(fd[i]);
    }
    unlink(pathname);
EXIT:
    for (i = index; i >= 0; i--) {
        (void)snprintf_s(pathname, FAT_STANDARD_NAME_LENGTH, FAT_STANDARD_NAME_LENGTH, "%s/file3_%d.txt", FAT_PATH_NAME,
            i);
        unlink(pathname);
    }
    g_testCount = 0;
    free(bufWrite);
    return NULL;
}
static UINT32 TestCase(VOID)
{
    INT32 ret, i;
    CHAR bufname[FAT_STANDARD_NAME_LENGTH] = FAT_PATH_NAME;
    pthread_attr_t attr[FAT_MAX_THREADS];
    pthread_t threadId[FAT_MAX_THREADS];

    g_testCount = 0;

    ret = mkdir(bufname, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);
    ret = PosixPthreadInit(&attr[0], TASK_PRIO_TEST - 1); // level less 1
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = pthread_create(&threadId[0], &attr[0], PthreadF01, (void *)0);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    ret = PosixPthreadInit(&attr[1], TASK_PRIO_TEST - 3); // level less 3
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

    ICUNIT_ASSERT_EQUAL(g_testCount, 3, g_testCount); // there 3 threads

    ret = rmdir(bufname);
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
    rmdir(bufname);

    return FAT_NO_ERROR;
}

/* *
* - @test IT_FS_FAT_MUTIPTHREAD_049
* - @tspec pressure test
* - @ttitle multithreaded operating the files
* - @tbrief
1. In the same directory
2. Three threads
3. different priority
       pthread_f01 low
       pthread_f02 high
       pthread_f03 middle
4. pthread_f01 open, write, read, offset, close files
5. pthread_f02 open, search, close files
6. pthread_f03 create, rename, remove files
7. 100 files per thread include 100M, 10M, 10b
* - @ tprior 4
* - @ tauto TRUE
* - @ tremark
*/

VOID ItFsFatMutipthread049(VOID)
{
    TEST_ADD_CASE("IT_FS_FAT_MUTIPTHREAD_049", TestCase, TEST_VFS, TEST_VFAT, TEST_LEVEL4, TEST_PRESSURE);
}
