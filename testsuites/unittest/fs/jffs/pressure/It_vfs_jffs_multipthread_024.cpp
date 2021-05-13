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

#include "It_vfs_jffs.h"

static VOID *PthreadF01(void *arg)
{
    LosTaskDelay(1); // delay time: 1
    INT32 i, j, ret, len, flag, index;
    INT32 fd[10] = {};
    CHAR pathname[JFFS_STANDARD_NAME_LENGTH] = "";
    CHAR readbuf[JFFS_STANDARD_NAME_LENGTH] = "";
    CHAR filebuf[FILE_BUF_SIZE] =
        "01234567890123456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123"
        "456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfgh"
        "ij9876543210abcdeabcde0123456789abcedfghij9876543210lalalalalalalala";
    CHAR writebuf[20] = "0123456789";
    CHAR *bufW = NULL;
    struct stat statbuf;
    INT32 bufWLen = BYTES_PER_MBYTE;

    flag = 0;
    bufW = (CHAR *)malloc(bufWLen + 1);
    ICUNIT_ASSERT_NOT_EQUAL_NULL(bufW, NULL, NULL);
    memset_s(bufW, bufWLen + 1, 0, bufWLen + 1);

    for (i = 0; i < bufWLen / strlen(filebuf); i++) {
        strcat_s(bufW, bufWLen + 1, filebuf);
    }

    index = 0;
    for (i = 0; i < JFFS_MAX_CYCLES; i++) {
        snprintf_s(pathname, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "/storage/test/file1%d.txt",
            index);
        fd[index] = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY);
        if (fd[index] == -1) {
            break;
        }
        switch (index % 3) { // max file num: 3
            case 0:
                for (j = 0; j < JFFS_MAX_CYCLES; j++) {
                    len = write(fd[index], bufW, strlen(bufW));
                    if (len <= 0) {
                        flag = 1;
                        break;
                    }
                }
                break;
            case 1:
                for (j = 0; j < JFFS_MIDDLE_CYCLES; j++) {
                    len = write(fd[index], bufW, strlen(bufW));
                    if (len <= 0) {
                        flag = 1;
                        break;
                    }
                }
                break;
            case 2: // fd[2]
                len = write(fd[index], writebuf, strlen(writebuf));
                if (len <= 0) {
                    flag = 1;
                    break;
                }
                LosTaskDelay(7); // delay time: 7
                dprintf("PthreadF01 is excecuting\n");
            default:
                break;
        }

        ret = lseek(fd[index], 0, SEEK_SET);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);
        INT32 readLen = 10;
        len = read(fd[index], readbuf, readLen);
        ICUNIT_GOTO_EQUAL(len, readLen, len, EXIT1);
        ICUNIT_GOTO_STRING_EQUAL(readbuf, "0123456789", readbuf, EXIT1);
        ret = close(fd[index]);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

        if (flag == 1) {
            break;
        }
        index++;
    }

    if (flag == 0) {
        index--;
    }

    for (i = index; i >= 0; i--) {
        snprintf_s(pathname, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "/storage/test/file1%d.txt", i);
        ret = unlink(pathname);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);
    }

    g_TestCnt++;

    free(bufW);

    return NULL;
EXIT1:
    for (i = index; i >= 0; i--) {
        close(fd[i]);
    }
EXIT:
    for (i = index; i >= 0; i--) {
        snprintf_s(pathname, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "/storage/test/file1%d.txt", i);
        unlink(pathname);
    }
    g_TestCnt = 0;
    free(bufW);
    return NULL;
}

static VOID *PthreadF02(void *arg)
{
    LosTaskDelay(1); // delay time: 1
    INT32 i, j, ret, len, flag, index;
    INT32 fd[10] = {};
    CHAR pathname[JFFS_STANDARD_NAME_LENGTH] = "";
    CHAR readbuf[JFFS_STANDARD_NAME_LENGTH] = "";
    CHAR filebuf[FILE_BUF_SIZE] =
        "01234567890123456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123"
        "456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfgh"
        "ij9876543210abcdeabcde0123456789abcedfghij9876543210lalalalalalalala";
    CHAR writebuf[20] = "0000";
    CHAR *bufW = NULL;
    struct stat statbuf;
    INT32 bufWLen = BYTES_PER_MBYTE;

    flag = 0;
    bufW = (CHAR *)malloc(bufWLen + 1);
    ICUNIT_ASSERT_NOT_EQUAL_NULL(bufW, NULL, NULL);
    memset_s(bufW, bufWLen + 1, 0, bufWLen + 1);

    for (i = 0; i < bufWLen / strlen(filebuf); i++) {
        strcat_s(bufW, bufWLen + 1, filebuf);
    }

    index = 0;
    for (i = 0; i < JFFS_MAX_CYCLES; i++) {
        snprintf_s(pathname, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "/storage/test/file2%d.txt",
            index);
        fd[index] = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY);
        if (fd[index] == -1) {
            break;
        }
        switch (index % 3) { // max file num: 3
            case 0:
                for (j = 0; j < JFFS_MAX_CYCLES; j++) {
                    len = write(fd[index], bufW, strlen(bufW));
                    if (len <= 0) {
                        flag = 1;
                        break;
                    }
                }
                break;
            case 1:
                for (j = 0; j < JFFS_MIDDLE_CYCLES; j++) {
                    len = write(fd[index], bufW, strlen(bufW));
                    if (len <= 0) {
                        flag = 1;
                        break;
                    }
                }
                LosTaskDelay(5); // delay time: 5
                dprintf("PthreadF02 is excecuting\n");
                break;
            case 2: // fd[2]
                len = write(fd[index], writebuf, strlen(writebuf));
                if (len <= 0) {
                    flag = 1;
                    break;
                }
            default:
                break;
        }

        ret = close(fd[index]);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

        ret = stat(pathname, &statbuf);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);
        JffsStatPrintf(statbuf);

        if (flag == 1) {
            break;
        }
        index++;
    }

    if (flag == 0) {
        index--;
    }

    for (i = index; i >= 0; i--) {
        snprintf_s(pathname, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "/storage/test/file2%d.txt", i);
        ret = unlink(pathname);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);
    }

    g_TestCnt++;

    free(bufW);

    return NULL;
EXIT1:
    for (i = index; i >= 0; i--) {
        close(fd[i]);
    }
EXIT:
    for (i = index; i >= 0; i--) {
        snprintf_s(pathname, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "/storage/test/file2%d.txt", i);
        unlink(pathname);
    }
    g_TestCnt = 0;
    free(bufW);
    return NULL;
}

static VOID *PthreadF03(void *arg)
{
    LosTaskDelay(1); // delay time: 1
    INT32 i, j, ret, len, flag, index;
    INT32 fd[10] = {};
    CHAR pathname[JFFS_STANDARD_NAME_LENGTH] = "";
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = "";
    CHAR readbuf[JFFS_STANDARD_NAME_LENGTH] = "";
    CHAR filebuf[FILE_BUF_SIZE] =
        "01234567890123456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123"
        "456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfgh"
        "ij9876543210abcdeabcde0123456789abcedfghij9876543210lalalalalalalala";
    CHAR writebuf[20] = "0000";
    CHAR *bufW = NULL;
    struct stat statbuf;
    INT32 bufWLen = BYTES_PER_MBYTE;

    flag = 0;
    bufW = (CHAR *)malloc(bufWLen + 1);
    ICUNIT_ASSERT_NOT_EQUAL_NULL(bufW, NULL, NULL);
    memset_s(bufW, bufWLen + 1, 0, bufWLen + 1);

    for (i = 0; i < bufWLen / strlen(filebuf); i++) {
        strcat_s(bufW, bufWLen + 1, filebuf);
    }

    index = 0;
    for (i = 0; i < JFFS_MAX_CYCLES; i++) {
        snprintf_s(pathname, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "/storage/test/file3%d.txt",
            index);
        fd[index] = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY);
        if (fd[index] == -1) {
            break;
        }
        switch (index % 3) { // max file num: 3
            case 0:
                for (j = 0; j < JFFS_MAX_CYCLES; j++) {
                    len = write(fd[index], bufW, strlen(bufW));
                    if (len <= 0) {
                        flag = 1;
                        break;
                    }
                }
                LosTaskDelay(3); // delay time: 3
                dprintf("PthreadF03 is excecuting\n");
                break;
            case 1:
                for (j = 0; j < JFFS_MIDDLE_CYCLES; j++) {
                    len = write(fd[index], bufW, strlen(bufW));
                    if (len <= 0) {
                        flag = 1;
                        break;
                    }
                }
                break;
            case 2: // fd[2]
                len = write(fd[index], writebuf, strlen(writebuf));
                if (len <= 0) {
                    flag = 1;
                    break;
                }
            default:
                break;
        }

        ret = close(fd[index]);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

        snprintf_s(pathname1, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "/storage/test/file3_%d.txt",
            index);
        ret = rename(pathname, pathname1);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

        if (flag == 1) {
            break;
        }
        index++;
    }

    if (flag == 0) {
        index--;
    }

    for (i = index; i >= 0; i--) {
        snprintf_s(pathname, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "/storage/test/file3_%d.txt", i);
        ret = unlink(pathname);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);
    }

    g_TestCnt++;

    free(bufW);

    return NULL;
EXIT1:
    for (i = index; i >= 0; i--) {
        close(fd[i]);
    }
    unlink(pathname);
EXIT:
    for (i = index; i >= 0; i--) {
        snprintf_s(pathname, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "/storage/test/file3_%d.txt", i);
        unlink(pathname);
    }
    g_TestCnt = 0;
    free(bufW);
    return NULL;
}


static UINT32 TestCase(VOID)
{
    INT32 fd, ret, i;
    CHAR bufname[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    pthread_attr_t attr[JFFS_MAX_THREADS];
    pthread_t threadId[JFFS_MAX_THREADS];
    struct stat st;

    g_TestCnt = 0;

    ret = mkdir(bufname, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    i = 0;
    ret = PosixPthreadInit(&attr[i], JFFS2NUM_JFFS2_GC_THREAD_PRIORITY1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    ret = pthread_create(&threadId[i], &attr[i], PthreadF01, (void *)i);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    i++;
    ret = PosixPthreadInit(&attr[i], JFFS2NUM_JFFS2_GC_THREAD_PRIORITY5);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    ret = pthread_create(&threadId[i], &attr[i], PthreadF02, (void *)i);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    i++;
    ret = PosixPthreadInit(&attr[i], JFFS2NUM_JFFS2_GC_THREAD_PRIORITY);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    ret = pthread_create(&threadId[i], &attr[i], PthreadF03, (void *)i);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    LosTaskDelay(1); // delay time: 1

    for (i = 0; i < JFFS_MAX_THREADS; i++) {
        ret = pthread_join(threadId[i], NULL);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);
    }

    for (i = 0; i < JFFS_MAX_THREADS; i++) {
        ret = pthread_attr_destroy(&attr[i]);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);
    }

    ICUNIT_ASSERT_EQUAL(g_TestCnt, JFFS_MAX_THREADS, g_TestCnt);

    ret = rmdir(bufname);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    return JFFS_NO_ERROR;

EXIT1:
    for (i = 0; i < JFFS_MAX_THREADS; i++) {
        pthread_join(threadId[i], NULL);
    }
    for (i = 0; i < JFFS_MAX_THREADS; i++) {
        pthread_attr_destroy(&attr[i]);
    }
EXIT:
    rmdir(bufname);

    return JFFS_NO_ERROR;
}

VOID ItFsJffsMultipthread024(VOID)
{
    TEST_ADD_CASE("ItFsJffsMultipthread024", TestCase, TEST_VFS, TEST_JFFS, TEST_LEVEL3, TEST_PRESSURE);
}
