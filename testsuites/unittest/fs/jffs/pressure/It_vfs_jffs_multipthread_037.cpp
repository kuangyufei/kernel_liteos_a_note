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
    INT32 i, j, ret, len, flag, fd;
    CHAR pathname[JFFS_STANDARD_NAME_LENGTH] = "";
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = "";
    CHAR filebuf[FILE_BUF_SIZE] =
        "01234567890123456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123"
        "456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfgh"
        "ij9876543210abcdeabcde0123456789abcedfghij9876543210lalalalalalalala";
    DIR *dir = NULL;

    for (i = 0; i < JFFS_MIDDLE_CYCLES; i++) {
        snprintf_s(pathname, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "%s/test1%d", JFFS_PATH_NAME0,
            i);
        ret = mkdir(pathname, HIGHEST_AUTHORITY);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

        snprintf_s(pathname1, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "%s/test1_%d", JFFS_PATH_NAME0,
            i);
        ret = rename(pathname, pathname1);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

        LosTaskDelay(3); // delay time: 3
        dprintf("PthreadF01 is executing\n");
    }

    for (j = i - 1; j >= 0; j--) {
        snprintf_s(pathname, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "%s/test1_%d", JFFS_PATH_NAME0,
            j);
        ret = rmdir(pathname);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);
    }

    g_TestCnt++;

    return NULL;

EXIT1:
    snprintf_s(pathname, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "%s/test1%d", JFFS_PATH_NAME0, i);
    rmdir(pathname);
EXIT:
    for (j = i - 1; j >= 0; j--) {
        snprintf_s(pathname, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "%s/test1_%d", JFFS_PATH_NAME0,
            j);
        rmdir(pathname);
    }
    g_TestCnt = 0;
    return NULL;
}

static VOID *PthreadF02(void *arg)
{
    INT32 i, j, ret, len, flag, fd;
    CHAR pathname[JFFS_STANDARD_NAME_LENGTH] = "";
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = "";
    CHAR filebuf[FILE_BUF_SIZE] =
        "01234567890123456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123"
        "456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfgh"
        "ij9876543210abcdeabcde0123456789abcedfghij9876543210lalalalalalalala";
    DIR *dir = NULL;

    for (i = 0; i < JFFS_MIDDLE_CYCLES; i++) {
        snprintf_s(pathname, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "%s/test2%d", JFFS_PATH_NAME0,
            i);
        ret = mkdir(pathname, HIGHEST_AUTHORITY);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

        snprintf_s(pathname1, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "%s/test2_%d", JFFS_PATH_NAME0,
            i);
        ret = rename(pathname, pathname1);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

        LosTaskDelay(5); // delay time: 5
        dprintf("PthreadF02 is executing\n");
    }

    for (j = i - 1; j >= 0; j--) {
        snprintf_s(pathname, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "%s/test2_%d", JFFS_PATH_NAME0,
            j);
        ret = rmdir(pathname);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);
    }

    g_TestCnt++;

    return NULL;

EXIT1:
    snprintf_s(pathname, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "%s/test2%d", JFFS_PATH_NAME0, i);
    rmdir(pathname);
EXIT:
    for (j = i - 1; j >= 0; j--) {
        snprintf_s(pathname, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "%s/test2_%d", JFFS_PATH_NAME0,
            j);
        rmdir(pathname);
    }
    g_TestCnt = 0;
    return NULL;
}

static VOID *PthreadF03(void *arg)
{
    INT32 i, j, ret, len, flag, fd;
    CHAR pathname[JFFS_STANDARD_NAME_LENGTH] = "";
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = "";
    CHAR filebuf[FILE_BUF_SIZE] =
        "01234567890123456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123"
        "456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfgh"
        "ij9876543210abcdeabcde0123456789abcedfghij9876543210lalalalalalalala";
    DIR *dir = NULL;

    for (i = 0; i < JFFS_MIDDLE_CYCLES; i++) {
        snprintf_s(pathname, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "%s/test3%d", JFFS_PATH_NAME0,
            i);
        ret = mkdir(pathname, HIGHEST_AUTHORITY);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

        snprintf_s(pathname1, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "%s/test3_%d", JFFS_PATH_NAME0,
            i);
        ret = rename(pathname, pathname1);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

        LosTaskDelay(7); // delay time: 7
        dprintf("PthreadF03 is executing\n");
    }

    for (j = i - 1; j >= 0; j--) {
        snprintf_s(pathname, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "%s/test3_%d", JFFS_PATH_NAME0,
            j);
        ret = rmdir(pathname);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);
    }

    g_TestCnt++;

    return NULL;

EXIT1:
    snprintf_s(pathname, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "%s/test3%d", JFFS_PATH_NAME0, i);
    rmdir(pathname);
EXIT:
    for (j = i - 1; j >= 0; j--) {
        snprintf_s(pathname, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "%s/test3_%d", JFFS_PATH_NAME0,
            j);
        rmdir(pathname);
    }
    g_TestCnt = 0;
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
    ret = PosixPthreadInit(&attr[i], JFFS2NUM_JFFS2_GC_THREAD_PRIORITY5);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    ret = pthread_create(&threadId[i], &attr[i], PthreadF01, (void *)i);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    i++;
    ret = PosixPthreadInit(&attr[i], JFFS2NUM_JFFS2_GC_THREAD_PRIORITY4);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    ret = pthread_create(&threadId[i], &attr[i], PthreadF02, (void *)i);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    i++;
    ret = PosixPthreadInit(&attr[i], JFFS2NUM_JFFS2_GC_THREAD_PRIORITY);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    ret = pthread_create(&threadId[i], &attr[i], PthreadF03, (void *)i);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

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

VOID ItFsJffsMultipthread037(VOID)
{
    TEST_ADD_CASE("ItFsJffsMultipthread037", TestCase, TEST_VFS, TEST_JFFS, TEST_LEVEL3, TEST_PRESSURE);
}
