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
    INT32 fd = -1;
    INT32 ret, len;
    INT32 i = 0;
    INT32 j = 0;
    INT32 k = JFFS_SHORT_ARRAY_LENGTH;
    CHAR filebuf[FILE_BUF_SIZE] =
        "abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123"
        "456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfgh"
        "ij9876543210abcdeabcde0123456789abcedfghij9876543210lalalalalalalala";
    CHAR readbuf[70] = "liteos";
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR buffile[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR *bufW = NULL;
    CHAR *bufW1 = NULL;
    CHAR *bufW2 = NULL;
    off_t off;
    struct stat statfile;

    INT32 bufWLen = BYTES_PER_MBYTE;
    INT32 bufW1Len = 512 * BYTES_PER_KBYTE; // 512 KB
    INT32 bufW2Len = 16 * BYTES_PER_KBYTE;  // 16 KB

    bufW = (CHAR *)malloc(bufWLen + 1);
    ICUNIT_ASSERT_NOT_EQUAL_NULL(bufW, NULL, 0);
    (void)memset_s(bufW, bufWLen + 1, 0, bufWLen + 1);

    bufW1 = (CHAR *)malloc(bufW1Len + 1);
    ICUNIT_GOTO_NOT_EQUAL(bufW1, NULL, 0, EXIT2);
    (void)memset_s(bufW1, bufW1Len + 1, 0, bufW1Len + 1);

    bufW2 = (CHAR *)malloc(bufW2Len + 1);
    ICUNIT_GOTO_NOT_EQUAL(bufW2, NULL, 0, EXIT3);
    (void)memset_s(bufW2, bufW2Len + 1, 0, bufW2Len + 1);

    for (j = 0; j < bufW2Len / strlen(filebuf); j++) {
        strcat_s(bufW2, bufW2Len + 1, filebuf);
    }

    for (j = 0; j < bufW1Len / bufW2Len; j++) {
        strcat_s(bufW1, bufW1Len + 1, bufW2);
    }

    for (i = 0; i < bufWLen / bufW1Len; i++) {
        strcat_s(bufW, bufWLen + 1, bufW1);
    }
    free(bufW1);
    free(bufW2);

    strcat_s(pathname1, JFFS_STANDARD_NAME_LENGTH, "/001.txt");
    fd = open(pathname1, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd, JFFS_IS_ERROR, fd, EXIT2);

    off = lseek(fd, 0, SEEK_SET);
    ICUNIT_GOTO_EQUAL(off, JFFS_NO_ERROR, off, EXIT2);

    for (i = 0; i < JFFS_MIDDLE_CYCLES; i++) {
        ret = write(fd, bufW, strlen(bufW));
        ICUNIT_GOTO_EQUAL(ret, BYTES_PER_MBYTE, ret, EXIT2);

        off = lseek(fd, 0, SEEK_CUR);
        ICUNIT_GOTO_EQUAL(off, (BYTES_PER_MBYTE * (i + 1)), off, EXIT2);

        LosTaskDelay(5); // delay time: 5
        dprintf("PthreadF01 is excecuting\n");
    }

    off = lseek(fd, 64, SEEK_SET); // file position: 64
    ICUNIT_GOTO_EQUAL(off, 64, off, EXIT2);

    len = read(fd, readbuf, 64); // read length: 64
    ICUNIT_GOTO_EQUAL(len, 64, len, EXIT2);

    ret = stat(pathname1, &statfile);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);
    // 3 * 1024 * 1024: filesize
    ICUNIT_GOTO_EQUAL(statfile.st_size, 3 * 1024 * 1024, statfile.st_size, EXIT2);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

    free(bufW);

    ret = unlink(pathname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    g_TestCnt++;
    return NULL;
EXIT3:
    free(bufW1);
EXIT2:
    close(fd);
EXIT1:
    unlink(pathname1);
EXIT:
    free(bufW);
    return NULL;
}

static VOID *PthreadF02(void *arg)
{
    INT32 fd = -1;
    INT32 ret, len;
    INT32 i = 0;
    INT32 j = 0;
    INT32 k = JFFS_SHORT_ARRAY_LENGTH;
    CHAR filebuf[FILE_BUF_SIZE] =
        "abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123"
        "456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfgh"
        "ij9876543210abcdeabcde0123456789abcedfghij9876543210lalalalalalalala";
    CHAR readbuf[70] = "liteos";
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR buffile[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR *bufW = NULL;
    CHAR *bufW1 = NULL;
    CHAR *bufW2 = NULL;
    off_t off;
    struct stat statfile;

    INT32 bufWLen = BYTES_PER_MBYTE;
    INT32 bufW1Len = 512 * BYTES_PER_KBYTE; // 512 KB
    INT32 bufW2Len = 16 * BYTES_PER_KBYTE;  // 16 KB

    bufW = (CHAR *)malloc(bufWLen + 1);
    ICUNIT_ASSERT_NOT_EQUAL_NULL(bufW, NULL, 0);
    (void)memset_s(bufW, bufWLen + 1, 0, bufWLen + 1);

    bufW1 = (CHAR *)malloc(bufW1Len + 1);
    ICUNIT_GOTO_NOT_EQUAL(bufW1, NULL, 0, EXIT2);
    (void)memset_s(bufW1, bufW1Len + 1, 0, bufW1Len + 1);

    bufW2 = (CHAR *)malloc(bufW2Len + 1);
    ICUNIT_GOTO_NOT_EQUAL(bufW2, NULL, 0, EXIT3);
    (void)memset_s(bufW2, bufW2Len + 1, 0, bufW2Len + 1);

    for (j = 0; j < bufW2Len / strlen(filebuf); j++) {
        strcat_s(bufW2, bufW2Len + 1, filebuf);
    }

    for (j = 0; j < bufW1Len / bufW2Len; j++) {
        strcat_s(bufW1, bufW1Len + 1, bufW2);
    }

    for (i = 0; i < bufWLen / bufW1Len; i++) {
        strcat_s(bufW, bufWLen + 1, bufW1);
    }
    free(bufW1);
    free(bufW2);

    strcat_s(pathname1, JFFS_STANDARD_NAME_LENGTH, "/002.txt");
    fd = open(pathname1, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd, JFFS_IS_ERROR, fd, EXIT2);

    off = lseek(fd, 0, SEEK_SET);
    ICUNIT_GOTO_EQUAL(off, JFFS_NO_ERROR, off, EXIT2);

    for (i = 0; i < JFFS_MIDDLE_CYCLES; i++) {
        ret = write(fd, bufW, strlen(bufW));
        ICUNIT_GOTO_EQUAL(ret, BYTES_PER_MBYTE, ret, EXIT2);

        off = lseek(fd, 0, SEEK_CUR);
        ICUNIT_GOTO_EQUAL(off, (BYTES_PER_MBYTE * (i + 1)), off, EXIT2);

        LosTaskDelay(10); // delay time: 10
        dprintf("PthreadF02 is excecuting\n");
    }

    off = lseek(fd, 64, SEEK_SET); // file position: 64
    ICUNIT_GOTO_EQUAL(off, 64, off, EXIT2);

    len = read(fd, readbuf, 64); // read length: 64
    ICUNIT_GOTO_EQUAL(len, 64, len, EXIT2);

    ret = stat(pathname1, &statfile);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);
    // 3 * 1024 * 1024: filesize
    ICUNIT_GOTO_EQUAL(statfile.st_size, 3 * 1024 * 1024, statfile.st_size, EXIT2);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

    free(bufW);

    ret = unlink(pathname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    g_TestCnt++;

    return NULL;
EXIT3:
    free(bufW1);
EXIT2:
    close(fd);
EXIT1:
    unlink(pathname1);
EXIT:
    free(bufW);
    return NULL;
}

static VOID *PthreadF03(void *arg)
{
    INT32 fd = -1;
    INT32 ret, len;
    INT32 i = 0;
    INT32 j = 0;
    INT32 k = JFFS_SHORT_ARRAY_LENGTH;
    CHAR filebuf[FILE_BUF_SIZE] =
        "abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123"
        "456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfgh"
        "ij9876543210abcdeabcde0123456789abcedfghij9876543210lalalalalalalala";
    CHAR readbuf[70] = "liteos";
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR buffile[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR *bufW = NULL;
    CHAR *bufW1 = NULL;
    CHAR *bufW2 = NULL;
    off_t off;
    struct stat statfile;

    INT32 bufWLen = BYTES_PER_MBYTE;
    INT32 bufW1Len = 512 * BYTES_PER_KBYTE; // 512 KB
    INT32 bufW2Len = 16 * BYTES_PER_KBYTE;  // 16 KB

    bufW = (CHAR *)malloc(bufWLen + 1);
    ICUNIT_ASSERT_NOT_EQUAL_NULL(bufW, NULL, 0);
    (void)memset_s(bufW, bufWLen + 1, 0, bufWLen + 1);

    bufW1 = (CHAR *)malloc(bufW1Len + 1);
    ICUNIT_GOTO_NOT_EQUAL(bufW1, NULL, 0, EXIT2);
    (void)memset_s(bufW1, bufW1Len + 1, 0, bufW1Len + 1);

    bufW2 = (CHAR *)malloc(bufW2Len + 1);
    ICUNIT_GOTO_NOT_EQUAL(bufW2, NULL, 0, EXIT3);
    (void)memset_s(bufW2, bufW2Len + 1, 0, bufW2Len + 1);

    for (j = 0; j < bufW2Len / strlen(filebuf); j++) {
        strcat_s(bufW2, bufW2Len + 1, filebuf);
    }

    for (j = 0; j < bufW1Len / bufW2Len; j++) {
        strcat_s(bufW1, bufW1Len + 1, bufW2);
    }

    for (i = 0; i < bufWLen / bufW1Len; i++) {
        strcat_s(bufW, bufWLen + 1, bufW1);
    }
    free(bufW1);
    free(bufW2);

    strcat_s(pathname1, JFFS_STANDARD_NAME_LENGTH, "/003.txt");
    fd = open(pathname1, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd, JFFS_IS_ERROR, fd, EXIT2);

    off = lseek(fd, 0, SEEK_SET);
    ICUNIT_GOTO_EQUAL(off, JFFS_NO_ERROR, off, EXIT2);

    for (i = 0; i < JFFS_MIDDLE_CYCLES; i++) {
        ret = write(fd, bufW, strlen(bufW));
        ICUNIT_GOTO_EQUAL(ret, BYTES_PER_MBYTE, ret, EXIT2);

        off = lseek(fd, 0, SEEK_CUR);
        ICUNIT_GOTO_EQUAL(off, (BYTES_PER_MBYTE * (i + 1)), off, EXIT2);

        LosTaskDelay(5); // delay time: 5
        dprintf("PthreadF03 is excecuting\n");
    }

    off = lseek(fd, 64, SEEK_SET); // seek offset: 64
    ICUNIT_GOTO_EQUAL(off, 64, off, EXIT2);

    len = read(fd, readbuf, 64); // read length: 64
    ICUNIT_GOTO_EQUAL(len, 64, len, EXIT2);

    ret = stat(pathname1, &statfile);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);
    // 3 * 1024 * 1024: filesize
    ICUNIT_GOTO_EQUAL(statfile.st_size, 3 * 1024 * 1024, statfile.st_size, EXIT2);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

    free(bufW);

    ret = unlink(pathname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    g_TestCnt++;

    return NULL;
EXIT3:
    free(bufW1);
EXIT2:
    close(fd);
EXIT1:
    unlink(pathname1);
EXIT:
    free(bufW);
    return NULL;
}

static UINT32 TestCase(VOID)
{
    INT32 ret, i;
    CHAR pathname[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    pthread_attr_t attr[JFFS_MAX_THREADS];
    pthread_t threadId[JFFS_MAX_THREADS];

    g_TestCnt = 0;

    ret = mkdir(pathname, HIGHEST_AUTHORITY);
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

    ICUNIT_ASSERT_EQUAL(g_TestCnt, JFFS_MAX_THREADS, g_TestCnt);

    ret = rmdir(pathname);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    return JFFS_NO_ERROR;

EXIT:
    for (i = 0; i < JFFS_MAX_THREADS; i++) {
        pthread_join(threadId[i], NULL);
    }
    for (i = 0; i < JFFS_MAX_THREADS; i++) {
        pthread_attr_destroy(&attr[i]);
    }
    rmdir(pathname);
    return JFFS_NO_ERROR;
}

VOID ItFsJffsMultipthread001(VOID)
{
    TEST_ADD_CASE("ItFsJffsMultipthread001", TestCase, TEST_VFS, TEST_JFFS, TEST_LEVEL3, TEST_PRESSURE);
}
