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
    INT32 i, j, ret, len, index, index1;
    INT32 fd[JFFS_MIDDLE_CYCLES] = {};
    INT32 fd1[JFFS_MIDDLE_CYCLES] = {};
    INT32 flag = 0;
    INT32 readLen = 10;
    CHAR pathname[JFFS_STANDARD_NAME_LENGTH] = "";
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = "";
    CHAR readbuf[JFFS_STANDARD_NAME_LENGTH] = "";
    CHAR filebuf[FILE_BUF_SIZE] =
        "01234567890123456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123"
        "456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfgh"
        "ij9876543210abcdeabcde0123456789abcedfghij9876543210lalalalalalalala";
    CHAR *bufW = NULL;
    INT32 bufWLen = BYTES_PER_MBYTE;

    bufW = (CHAR *)malloc(bufWLen + 1);
    ICUNIT_ASSERT_NOT_EQUAL_NULL(bufW, NULL, NULL);
    memset_s(bufW, bufWLen + 1, 0, bufWLen + 1);

    for (i = 0; i < bufWLen / strlen(filebuf); i++) {
        strcat_s(bufW, bufWLen + 1, filebuf);
    }

    index = 0;
    for (i = 0; i < JFFS_MIDDLE_CYCLES; i++) {
        snprintf_s(pathname, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "/storage/test/file%d.txt",
            index);
        fd[index] = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY);
        if (fd[index] == -1) {
            break;
        }

        for (j = 0; j < JFFS_MIDDLE_CYCLES; j++) {
            len = write(fd[index], bufW, strlen(bufW));
            if (len <= 0) {
                flag = 1;
                break;
            }
        }

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
        snprintf_s(pathname, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "/storage/test/file%d.txt", i);
        ret = unlink(pathname);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);
    }

    index1 = 0;
    for (i = 0; i < JFFS_MIDDLE_CYCLES; i++) {
        snprintf_s(pathname1, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1,
            "/storage/test/test00/file%d.txt", index1);
        fd1[index1] = open(pathname1, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY);
        if (fd1[index1] == -1) {
            break;
        }

        for (j = 0; j < JFFS_MIDDLE_CYCLES; j++) {
            len = write(fd1[index1], bufW, strlen(bufW));
            if (len <= 0) {
                flag = 1;
                break;
            }
        }
        ret = lseek(fd1[index1], 0, SEEK_SET);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);
        len = read(fd1[index1], readbuf, readLen);
        ICUNIT_GOTO_EQUAL(len, readLen, len, EXIT1);
        ICUNIT_GOTO_STRING_EQUAL(readbuf, "0123456789", readbuf, EXIT1);
        ret = close(fd1[index1]);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

        LosTaskDelay(1); // delay time: 1
        dprintf("PthreadF01 is excecuting\n");

        if (flag == 1) {
            break;
        }
        index1++;
    }

    if (flag == 0) {
        index1--;
    }

    for (i = index1; i >= 0; i--) {
        snprintf_s(pathname1, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1,
            "/storage/test/test00/file%d.txt", i);
        ret = unlink(pathname1);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);
    }

    g_TestCnt++;

    free(bufW);

    return NULL;

EXIT1:
    for (i = index1; i >= 0; i--) {
        close(fd1[i]);
    }

    for (i = index; i >= 0; i--) {
        close(fd[i]);
    }
EXIT:

    for (i = index1; i >= 0; i--) {
        snprintf_s(pathname1, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1,
            "/storage/test/test00/file%d.txt", i);
        unlink(pathname1);
    }

    for (i = index; i >= 0; i--) {
        snprintf_s(pathname, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "/storage/test/file%d.txt", i);
        unlink(pathname);
    }
    free(bufW);
    return NULL;
}

static VOID *PthreadF02(void *arg)
{
    INT32 i, j, ret, len, index, index1;
    INT32 fd[JFFS_MIDDLE_CYCLES] = {};
    INT32 fd1[JFFS_MIDDLE_CYCLES] = {};
    INT32 flag = 0;
    CHAR pathname[JFFS_STANDARD_NAME_LENGTH] = "";
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = "";
    CHAR readbuf[JFFS_STANDARD_NAME_LENGTH] = "";
    CHAR filebuf[FILE_BUF_SIZE] =
        "01234567890123456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123"
        "456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfgh"
        "ij9876543210abcdeabcde0123456789abcedfghij9876543210lalalalalalalala";
    CHAR *bufW = NULL;
    INT32 bufWLen = BYTES_PER_MBYTE;

    bufW = (CHAR *)malloc(bufWLen + 1);
    ICUNIT_ASSERT_NOT_EQUAL_NULL(bufW, NULL, NULL);
    memset_s(bufW, bufWLen + 1, 0, bufWLen + 1);

    for (i = 0; i < bufWLen / strlen(filebuf); i++) {
        strcat_s(bufW, bufWLen + 1, filebuf);
    }

    index = 0;
    for (i = 0; i < JFFS_MIDDLE_CYCLES; i++) {
        snprintf_s(pathname, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "/storage/test1/file%d.txt",
            index);
        fd[index] = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY);
        if (fd[index] == -1) {
            break;
        }

        for (j = 0; j < JFFS_MIDDLE_CYCLES; j++) {
            len = write(fd[index], bufW, strlen(bufW));
            if (len <= 0) {
                flag = 1;
                break;
            }
        }

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

    LosTaskDelay(3); // delay time: 3
    dprintf("PthreadF02 is excecuting\n");

    for (i = index; i >= 0; i--) {
        snprintf_s(pathname, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "/storage/test1/file%d.txt", i);
        ret = unlink(pathname);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);
    }

    index1 = 0;
    for (i = 0; i < JFFS_MIDDLE_CYCLES; i++) {
        snprintf_s(pathname1, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1,
            "/storage/test1/test11/file%d.txt", index1);
        fd1[index1] = open(pathname1, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY);
        if (fd1[index1] == -1) {
            break;
        }

        for (j = 0; j < JFFS_MIDDLE_CYCLES; j++) {
            len = write(fd1[index1], bufW, strlen(bufW));
            if (len <= 0) {
                flag = 1;
                break;
            }
        }
        ret = lseek(fd1[index1], 0, SEEK_SET);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);
        len = read(fd1[index1], readbuf, 10); // read length: 10
        ICUNIT_GOTO_EQUAL(len, 10, len, EXIT1);
        ICUNIT_GOTO_STRING_EQUAL(readbuf, "0123456789", readbuf, EXIT1);
        ret = close(fd1[index1]);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

        if (flag == 1) {
            break;
        }
        index1++;
    }

    if (flag == 0) {
        index1--;
    }

    for (i = index1; i >= 0; i--) {
        snprintf_s(pathname1, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1,
            "/storage/test1/test11/file%d.txt", i);
        ret = unlink(pathname1);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);
    }

    g_TestCnt++;

    free(bufW);

    return NULL;

EXIT2:
    free(bufW);
EXIT1:
    for (i = index1; i >= 0; i--) {
        close(fd1[i]);
    }

    for (i = index; i >= 0; i--) {
        close(fd[i]);
    }
EXIT:

    for (i = index1; i >= 0; i--) {
        snprintf_s(pathname1, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1,
            "/storage/test1/test11/file%d.txt", i);
        unlink(pathname1);
    }

    for (i = index; i >= 0; i--) {
        snprintf_s(pathname, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "/storage/test1/file%d.txt", i);
        unlink(pathname);
    }
    free(bufW);
    return NULL;
}

static VOID *PthreadF03(void *arg)
{
    INT32 i, j, ret, len, index, index1;
    INT32 fd[JFFS_MIDDLE_CYCLES] = {};
    INT32 fd1[JFFS_MIDDLE_CYCLES] = {};
    INT32 flag = 0;
    CHAR pathname[JFFS_STANDARD_NAME_LENGTH] = "";
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = "";
    CHAR readbuf[JFFS_STANDARD_NAME_LENGTH] = "";
    CHAR filebuf[FILE_BUF_SIZE] =
        "01234567890123456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123"
        "456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfgh"
        "ij9876543210abcdeabcde0123456789abcedfghij9876543210lalalalalalalala";
    CHAR *bufW = NULL;
    INT32 bufWLen = BYTES_PER_MBYTE;

    bufW = (CHAR *)malloc(bufWLen + 1);
    ICUNIT_ASSERT_NOT_EQUAL_NULL(bufW, NULL, NULL);
    memset_s(bufW, bufWLen + 1, 0, bufWLen + 1);

    for (i = 0; i < bufWLen / strlen(filebuf); i++) {
        strcat_s(bufW, bufWLen + 1, filebuf);
    }

    index = 0;
    for (i = 0; i < JFFS_MIDDLE_CYCLES; i++) {
        snprintf_s(pathname, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "/storage/test2/file%d.txt",
            index);
        fd[index] = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY);
        if (fd[index] == -1) {
            break;
        }

        for (j = 0; j < JFFS_MIDDLE_CYCLES; j++) {
            len = write(fd[index], bufW, strlen(bufW));
            if (len <= 0) {
                flag = 1;
                break;
            }
        }

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
        snprintf_s(pathname, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "/storage/test2/file%d.txt", i);
        ret = unlink(pathname);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);
    }

    index1 = 0;
    for (i = 0; i < JFFS_MIDDLE_CYCLES; i++) {
        snprintf_s(pathname1, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1,
            "/storage/test2/test22/file%d.txt", index1);
        fd1[index1] = open(pathname1, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY);
        if (fd1[index1] == -1) {
            break;
        }

        for (j = 0; j < JFFS_MIDDLE_CYCLES; j++) {
            len = write(fd1[index1], bufW, strlen(bufW));
            if (len <= 0) {
                flag = 1;
                break;
            }
        }
        ret = lseek(fd1[index1], 0, SEEK_SET);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);
        len = read(fd1[index1], readbuf, 10); // read length: 10
        ICUNIT_GOTO_EQUAL(len, 10, len, EXIT1);
        ICUNIT_GOTO_STRING_EQUAL(readbuf, "0123456789", readbuf, EXIT1);
        ret = close(fd1[index1]);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

        LosTaskDelay(1); // delay time: 1
        dprintf("PthreadF03 is excecuting\n");

        if (flag == 1) {
            break;
        }
        index1++;
    }

    if (flag == 0) {
        index1--;
    }

    for (i = index1; i >= 0; i--) {
        snprintf_s(pathname1, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1,
            "/storage/test2/test22/file%d.txt", i);
        ret = unlink(pathname1);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);
    }

    g_TestCnt++;

    free(bufW);

    return NULL;

EXIT2:
    free(bufW);
EXIT1:
    for (i = index1; i >= 0; i--) {
        close(fd1[i]);
    }

    for (i = index; i >= 0; i--) {
        close(fd[i]);
    }
EXIT:

    for (i = index1; i >= 0; i--) {
        snprintf_s(pathname1, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1,
            "/storage/test2/test22/file%d.txt", i);
        unlink(pathname1);
    }

    for (i = index; i >= 0; i--) {
        snprintf_s(pathname, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "/storage/test2/file%d.txt", i);
        unlink(pathname);
    }
    free(bufW);
    return NULL;
}

static UINT32 TestCase(VOID)
{
    INT32 ret, i;
    CHAR bufname1[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR bufname2[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME00 };
    CHAR bufname3[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME01 };
    CHAR bufname4[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME11 };
    CHAR bufname5[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME02 };
    CHAR bufname6[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME22 };
    pthread_attr_t attr[JFFS_MAX_THREADS];
    pthread_t threadId[JFFS_MAX_THREADS];

    g_TestCnt = 0;
    ret = mkdir(bufname1, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = mkdir(bufname2, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = mkdir(bufname3, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = mkdir(bufname4, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = mkdir(bufname5, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = mkdir(bufname6, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    i = 0;
    ret = PosixPthreadInit(&attr[i], JFFS2NUM_JFFS2_GC_THREAD_PRIORITY2);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = pthread_create(&threadId[i], &attr[i], PthreadF01, (void *)i);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    i++;
    ret = PosixPthreadInit(&attr[i], JFFS2NUM_JFFS2_GC_THREAD_PRIORITY2);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = pthread_create(&threadId[i], &attr[i], PthreadF02, (void *)i);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    i++;
    ret = PosixPthreadInit(&attr[i], JFFS2NUM_JFFS2_GC_THREAD_PRIORITY2);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = pthread_create(&threadId[i], &attr[i], PthreadF03, (void *)i);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    for (i = 0; i < JFFS_MAX_THREADS; i++) {
        ret = pthread_join(threadId[i], NULL);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);
    }

    for (i = 0; i < JFFS_MAX_THREADS; i++) {
        ret = pthread_attr_destroy(&attr[i]);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);
    }

    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ICUNIT_ASSERT_EQUAL(g_TestCnt, JFFS_MAX_THREADS, g_TestCnt);

    ret = rmdir(bufname2);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = rmdir(bufname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = rmdir(bufname4);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = rmdir(bufname3);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = rmdir(bufname6);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = rmdir(bufname5);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    return JFFS_NO_ERROR;

EXIT:
    for (i = 0; i < JFFS_MAX_THREADS; i++) {
        pthread_join(threadId[i], NULL);
    }
    for (i = 0; i < JFFS_MAX_THREADS; i++) {
        pthread_attr_destroy(&attr[i]);
    }

    return JFFS_NO_ERROR;
}

VOID ItFsJffsMultipthread003(VOID)
{
    TEST_ADD_CASE("ItFsJffsMultipthread003", TestCase, TEST_VFS, TEST_JFFS, TEST_LEVEL3, TEST_PRESSURE);
}
