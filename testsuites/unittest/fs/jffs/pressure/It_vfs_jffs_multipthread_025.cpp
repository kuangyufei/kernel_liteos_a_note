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
    INT32 fd = -1;
    INT32 ret, len, i;
    CHAR pathname[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR writebuf[JFFS_STANDARD_NAME_LENGTH] = "0123456789";
    CHAR readbuf[JFFS_STANDARD_NAME_LENGTH] = "";
    CHAR filebuf[FILE_BUF_SIZE] =
        "abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123"
        "456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfgh"
        "ij9876543210abcdeabcde0123456789abcedfghij9876543210lalalalalalalala";
    CHAR *bufW = NULL;
    INT32 bufWLen = 256 * BYTES_PER_KBYTE; // 256 KB

    bufW = (CHAR *)malloc(bufWLen + 1);
    ICUNIT_ASSERT_NOT_EQUAL_NULL(bufW, NULL, NULL);
    (void)memset_s(bufW, bufWLen + 1, 0, bufWLen + 1);

    for (i = 0; i < bufWLen / strlen(filebuf); i++) {
        strcat_s(bufW, bufWLen + 1, filebuf);
    }

    fd = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd, JFFS_IS_ERROR, fd, EXIT);

    for (i = 0; i < JFFS_MAX_CYCLES; i++) {
        len = write(fd, bufW, strlen(bufW));
        ICUNIT_GOTO_EQUAL(len, strlen(bufW), len, EXIT1);
        LosTaskDelay(3); // delay time: 3
        dprintf("PthreadF01 is executing\n");
    }

    len = write(fd, writebuf, strlen(writebuf));
    ICUNIT_GOTO_EQUAL(len, strlen(writebuf), len, EXIT1);

    ret = lseek(fd, -10, SEEK_CUR); // seek offset: -10
    ICUNIT_GOTO_NOT_EQUAL(ret, -1, ret, EXIT1);

    len = read(fd, readbuf, strlen(writebuf)); // read length: 10
    ICUNIT_GOTO_EQUAL(len, strlen(writebuf), len, EXIT1);
    ICUNIT_GOTO_STRING_EQUAL(readbuf, "0123456789", readbuf, EXIT1);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    ret = unlink(pathname);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    g_TestCnt++;

    free(bufW);

    return NULL;
EXIT1:
    close(fd);
EXIT:
    unlink(pathname);
    free(bufW);
    g_TestCnt = 0;
    return NULL;
}

static VOID *PthreadF02(void *arg)
{
    LosTaskDelay(1); // delay time: 1
    INT32 fd = -1;
    INT32 ret, len, i;
    CHAR pathname[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME01 };
    CHAR writebuf[JFFS_STANDARD_NAME_LENGTH] = "0123456789";
    CHAR readbuf[JFFS_STANDARD_NAME_LENGTH] = "";
    CHAR filebuf[FILE_BUF_SIZE] =
        "abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123"
        "456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfgh"
        "ij9876543210abcdeabcde0123456789abcedfghij9876543210lalalalalalalala";
    CHAR *bufW = NULL;
    INT32 bufWLen = 256 * BYTES_PER_KBYTE; // 256 KB

    bufW = (CHAR *)malloc(bufWLen + 1);
    ICUNIT_ASSERT_NOT_EQUAL_NULL(bufW, NULL, NULL);
    (void)memset_s(bufW, bufWLen + 1, 0, bufWLen + 1);

    for (i = 0; i < bufWLen / strlen(filebuf); i++) {
        strcat_s(bufW, bufWLen + 1, filebuf);
    }

    fd = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd, JFFS_IS_ERROR, fd, EXIT);

    for (i = 0; i < JFFS_MAX_CYCLES; i++) {
        len = write(fd, bufW, strlen(bufW));
        ICUNIT_GOTO_EQUAL(len, strlen(bufW), len, EXIT1);
        LosTaskDelay(5); // delay time: 5
        dprintf("PthreadF02 is executing\n");
    }

    len = write(fd, writebuf, strlen(writebuf));
    ICUNIT_GOTO_EQUAL(len, strlen(writebuf), len, EXIT1);

    ret = lseek(fd, -10, SEEK_CUR); // seek offset: -10
    ICUNIT_GOTO_NOT_EQUAL(ret, -1, ret, EXIT1);

    len = read(fd, readbuf, strlen(writebuf)); // read length: 10
    ICUNIT_GOTO_EQUAL(len, strlen(writebuf), len, EXIT1);
    ICUNIT_GOTO_STRING_EQUAL(readbuf, "0123456789", readbuf, EXIT1);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    ret = unlink(pathname);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    g_TestCnt++;

    free(bufW);

    return NULL;
EXIT1:
    close(fd);
EXIT:
    unlink(pathname);
    free(bufW);
    g_TestCnt = 0;
    return NULL;
}

static VOID *PthreadF03(void *arg)
{
    LosTaskDelay(1); // delay time: 1
    INT32 fd = -1;
    INT32 ret, len, i;
    CHAR pathname[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME02 };
    CHAR writebuf[JFFS_STANDARD_NAME_LENGTH] = "0123456789";
    CHAR readbuf[JFFS_STANDARD_NAME_LENGTH] = "";
    CHAR filebuf[FILE_BUF_SIZE] =
        "abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123"
        "456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfgh"
        "ij9876543210abcdeabcde0123456789abcedfghij9876543210lalalalalalalala";
    CHAR *bufW = NULL;
    INT32 bufWLen = 256 * BYTES_PER_KBYTE; // 256 KB

    bufW = (CHAR *)malloc(bufWLen + 1);
    ICUNIT_ASSERT_NOT_EQUAL_NULL(bufW, NULL, NULL);
    (void)memset_s(bufW, bufWLen + 1, 0, bufWLen + 1);

    for (i = 0; i < bufWLen / strlen(filebuf); i++) {
        strcat_s(bufW, bufWLen + 1, filebuf);
    }

    fd = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd, JFFS_IS_ERROR, fd, EXIT);

    for (i = 0; i < JFFS_MAX_CYCLES; i++) {
        len = write(fd, bufW, strlen(bufW));
        ICUNIT_GOTO_EQUAL(len, strlen(bufW), len, EXIT1);
        LosTaskDelay(7); // delay time: 7
        dprintf("PthreadF03 is executing\n");
    }

    len = write(fd, writebuf, strlen(writebuf));
    ICUNIT_GOTO_EQUAL(len, strlen(writebuf), len, EXIT1);

    ret = lseek(fd, -10, SEEK_CUR); // seek offset: -10
    ICUNIT_GOTO_NOT_EQUAL(ret, -1, ret, EXIT1);

    len = read(fd, readbuf, strlen(writebuf)); // read length: 10
    ICUNIT_GOTO_EQUAL(len, strlen(writebuf), len, EXIT1);
    ICUNIT_GOTO_STRING_EQUAL(readbuf, "0123456789", readbuf, EXIT1);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    ret = unlink(pathname);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    g_TestCnt++;

    free(bufW);

    return NULL;
EXIT1:
    close(fd);
EXIT:
    unlink(pathname);
    free(bufW);
    g_TestCnt = 0;
    return NULL;
}

static UINT32 TestCase(VOID)
{
    INT32 fd, ret, i;
    pthread_attr_t attr[JFFS_MAX_THREADS];
    pthread_t threadId[JFFS_MAX_THREADS];
    struct stat st;

    g_TestCnt = 0;

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

    LosTaskDelay(1); // delay time: 1

    for (i = 0; i < JFFS_MAX_THREADS; i++) {
        ret = pthread_join(threadId[i], NULL);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);
    }

    for (i = 0; i < JFFS_MAX_THREADS; i++) {
        ret = pthread_attr_destroy(&attr[i]);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);
    }

    ICUNIT_ASSERT_EQUAL(g_TestCnt, JFFS_MAX_THREADS, g_TestCnt);

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

VOID ItFsJffsMultipthread025(VOID)
{
    TEST_ADD_CASE("ItFsJffsMultipthread025", TestCase, TEST_VFS, TEST_JFFS, TEST_LEVEL3, TEST_PRESSURE);
}
