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
    INT32 i, j, ret, len, flag, index, offset;
    INT32 fd = -1;
    CHAR pathname[JFFS_STANDARD_NAME_LENGTH] = "";
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = "";
    CHAR pathname2[JFFS_STANDARD_NAME_LENGTH] = "";
    CHAR filebuf[FILE_BUF_SIZE] =
        "01234567890123456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123"
        "456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfgh"
        "ij9876543210abcdeabcde0123456789abcedfghij9876543210lalalalalalalala";
    CHAR *bufW = NULL;
    INT32 bufWLen = 256 * BYTES_PER_KBYTE; // 256 KB

    flag = 0;
    bufW = (CHAR *)malloc(bufWLen + 1);
    ICUNIT_ASSERT_NOT_EQUAL_NULL(bufW, NULL, NULL);
    (void)memset_s(bufW, bufWLen + 1, 0, bufWLen + 1);

    for (i = 0; i < bufWLen / strlen(filebuf); i++) {
        strcat_s(bufW, bufWLen + 1, filebuf);
    }

    index = 0;
    for (i = 0; i < JFFS_MIDDLE_CYCLES; i++) {
        snprintf_s(pathname, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "%s/test1%d", JFFS_PATH_NAME0,
            index);
        ret = mkdir(pathname, HIGHEST_AUTHORITY);
        if (ret == -1) {
            break;
        }
        strcpy_s(pathname1, JFFS_STANDARD_NAME_LENGTH, pathname);
        strcat_s(pathname1, JFFS_STANDARD_NAME_LENGTH, "/file.txt");
        fd = open(pathname1, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY);
        if (fd == -1) {
            break;
        }

        for (j = 0; j < JFFS_MAX_CYCLES; j++) {
            len = write(fd, bufW, strlen(bufW));
            if (len <= 0) {
                flag = 1;
                break;
            }
        }

        LosTaskDelay(3); // delay time: 3
        dprintf("PthreadF01 is executing\n");

        ret = close(fd);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

        snprintf_s(pathname2, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "%s/test1_%d", JFFS_PATH_NAME0,
            index);
        ret = rename(pathname, pathname2);
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
        snprintf_s(pathname, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "%s/test1_%d", JFFS_PATH_NAME0,
            i);
        strcpy_s(pathname1, JFFS_STANDARD_NAME_LENGTH, pathname);
        strcat_s(pathname1, JFFS_STANDARD_NAME_LENGTH, "/file.txt");
        ret = unlink(pathname1);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);
        ret = rmdir(pathname);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);
    }

    g_TestCnt++;

    free(bufW);

    return NULL;
EXIT1:
    close(fd);
    snprintf_s(pathname, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "%s/test1%d", JFFS_PATH_NAME0, i);
    strcpy_s(pathname1, JFFS_STANDARD_NAME_LENGTH, pathname);
    strcat_s(pathname1, JFFS_STANDARD_NAME_LENGTH, "/file.txt");
    unlink(pathname1);
    rmdir(pathname);

EXIT:
    for (i = index; i >= 0; i--) {
        snprintf_s(pathname, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "%s/test1_%d", JFFS_PATH_NAME0,
            i);
        strcpy_s(pathname1, JFFS_STANDARD_NAME_LENGTH, pathname);
        strcat_s(pathname1, JFFS_STANDARD_NAME_LENGTH, "/file.txt");
        unlink(pathname1);
        rmdir(pathname);
    }
    g_TestCnt = 0;
    free(bufW);
    return NULL;
}

static VOID *PthreadF02(void *arg)
{
    INT32 i, j, ret, len, flag, index, offset;
    INT32 fd = -1;
    CHAR pathname[JFFS_STANDARD_NAME_LENGTH] = "";
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = "";
    CHAR pathname2[JFFS_STANDARD_NAME_LENGTH] = "";
    CHAR filebuf[FILE_BUF_SIZE] =
        "01234567890123456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123"
        "456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfgh"
        "ij9876543210abcdeabcde0123456789abcedfghij9876543210lalalalalalalala";
    CHAR *bufW = NULL;
    INT32 bufWLen = 256 * BYTES_PER_KBYTE; // 256 KB

    flag = 0;
    bufW = (CHAR *)malloc(bufWLen + 1);
    ICUNIT_ASSERT_NOT_EQUAL_NULL(bufW, NULL, NULL);
    (void)memset_s(bufW, bufWLen + 1, 0, bufWLen + 1);

    for (i = 0; i < bufWLen / strlen(filebuf); i++) {
        strcat_s(bufW, bufWLen + 1, filebuf);
    }

    index = 0;
    for (i = 0; i < JFFS_MIDDLE_CYCLES; i++) {
        snprintf_s(pathname, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "%s/test2%d", JFFS_PATH_NAME_0,
            index);
        ret = mkdir(pathname, HIGHEST_AUTHORITY);
        if (ret == -1) {
            break;
        }
        strcpy_s(pathname1, JFFS_STANDARD_NAME_LENGTH, pathname);
        strcat_s(pathname1, JFFS_STANDARD_NAME_LENGTH, "/file.txt");
        fd = open(pathname1, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY);
        if (fd == -1) {
            break;
        }

        for (j = 0; j < JFFS_MAX_CYCLES; j++) {
            len = write(fd, bufW, strlen(bufW));
            if (len <= 0) {
                flag = 1;
                break;
            }
        }

        LosTaskDelay(5); // delay time: 5
        dprintf("PthreadF02 is executing\n");

        ret = close(fd);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

        snprintf_s(pathname2, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "%s/test2_%d", JFFS_PATH_NAME_0,
            index);
        ret = rename(pathname, pathname2);
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
        snprintf_s(pathname, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "%s/test2_%d", JFFS_PATH_NAME_0,
            i);
        strcpy_s(pathname1, JFFS_STANDARD_NAME_LENGTH, pathname);
        strcat_s(pathname1, JFFS_STANDARD_NAME_LENGTH, "/file.txt");
        ret = unlink(pathname1);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);
        ret = rmdir(pathname);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);
    }

    g_TestCnt++;

    free(bufW);

    return NULL;
EXIT1:
    close(fd);
    snprintf_s(pathname, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "%s/test2%d", JFFS_PATH_NAME_0, i);
    strcpy_s(pathname1, JFFS_STANDARD_NAME_LENGTH, pathname);
    strcat_s(pathname1, JFFS_STANDARD_NAME_LENGTH, "/file.txt");
    unlink(pathname1);
    rmdir(pathname);

EXIT:
    for (i = index; i >= 0; i--) {
        snprintf_s(pathname, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "%s/test2_%d", JFFS_PATH_NAME_0,
            i);
        strcpy_s(pathname1, JFFS_STANDARD_NAME_LENGTH, pathname);
        strcat_s(pathname1, JFFS_STANDARD_NAME_LENGTH, "/file.txt");
        unlink(pathname1);
        rmdir(pathname);
    }
    g_TestCnt = 0;
    free(bufW);
    return NULL;
}

static VOID *PthreadF03(void *arg)
{
    INT32 i, j, ret, len, flag, index, offset;
    INT32 fd = -1;
    CHAR pathname[JFFS_STANDARD_NAME_LENGTH] = "";
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = "";
    CHAR pathname2[JFFS_STANDARD_NAME_LENGTH] = "";
    CHAR filebuf[FILE_BUF_SIZE] =
        "01234567890123456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123"
        "456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfgh"
        "ij9876543210abcdeabcde0123456789abcedfghij9876543210lalalalalalalala";
    CHAR *bufW = NULL;
    INT32 bufWLen = 256 * BYTES_PER_KBYTE; // 256 KB

    flag = 0;
    bufW = (CHAR *)malloc(bufWLen + 1);
    ICUNIT_ASSERT_NOT_EQUAL_NULL(bufW, NULL, NULL);
    (void)memset_s(bufW, bufWLen + 1, 0, bufWLen + 1);

    for (i = 0; i < bufWLen / strlen(filebuf); i++) {
        strcat_s(bufW, bufWLen + 1, filebuf);
    }

    index = 0;
    for (i = 0; i < JFFS_MIDDLE_CYCLES; i++) {
        snprintf_s(pathname, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "%s/test3%d", JFFS_PATH_NAME_01,
            index);
        ret = mkdir(pathname, HIGHEST_AUTHORITY);
        if (ret == -1) {
            break;
        }
        strcpy_s(pathname1, JFFS_STANDARD_NAME_LENGTH, pathname);
        strcat_s(pathname1, JFFS_STANDARD_NAME_LENGTH, "/file.txt");
        fd = open(pathname1, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY);
        if (fd == -1) {
            break;
        }

        for (j = 0; j < JFFS_MAX_CYCLES; j++) {
            len = write(fd, bufW, strlen(bufW));
            if (len <= 0) {
                flag = 1;
                break;
            }
        }

        LosTaskDelay(7); // delay time: 7
        dprintf("PthreadF03 is executing\n");

        ret = close(fd);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

        snprintf_s(pathname2, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "%s/test3_%d",
            JFFS_PATH_NAME_01, index);
        ret = rename(pathname, pathname2);
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
        snprintf_s(pathname, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "%s/test3_%d", JFFS_PATH_NAME_01,
            i);
        strcpy_s(pathname1, JFFS_STANDARD_NAME_LENGTH, pathname);
        strcat_s(pathname1, JFFS_STANDARD_NAME_LENGTH, "/file.txt");
        ret = unlink(pathname1);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);
        ret = rmdir(pathname);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);
    }

    g_TestCnt++;

    free(bufW);

    return NULL;
EXIT1:
    close(fd);
    snprintf_s(pathname, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "%s/test3%d", JFFS_PATH_NAME_01, i);
    strcpy_s(pathname1, JFFS_STANDARD_NAME_LENGTH, pathname);
    strcat_s(pathname1, JFFS_STANDARD_NAME_LENGTH, "/file.txt");
    unlink(pathname1);
    rmdir(pathname);

EXIT:
    for (i = index; i >= 0; i--) {
        snprintf_s(pathname, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "%s/test3_%d", JFFS_PATH_NAME_01,
            i);
        strcpy_s(pathname1, JFFS_STANDARD_NAME_LENGTH, pathname);
        strcat_s(pathname1, JFFS_STANDARD_NAME_LENGTH, "/file.txt");
        unlink(pathname1);
        rmdir(pathname);
    }
    g_TestCnt = 0;
    free(bufW);
    return NULL;
}


static UINT32 TestCase(VOID)
{
    INT32 ret, i;
    CHAR bufname1[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR bufname2[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME_0 };
    CHAR bufname3[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME_01 };
    pthread_attr_t attr[JFFS_MAX_THREADS];
    pthread_t threadId[JFFS_MAX_THREADS];

    g_TestCnt = 0;

    ret = mkdir(bufname1, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = mkdir(bufname2, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = mkdir(bufname3, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    i = 0;
    ret = PosixPthreadInit(&attr[i], JFFS2NUM_JFFS2_GC_THREAD_PRIORITY2);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    ret = pthread_create(&threadId[i], &attr[i], PthreadF01, (void *)i);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    i++;
    ret = PosixPthreadInit(&attr[i], JFFS2NUM_JFFS2_GC_THREAD_PRIORITY2);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    ret = pthread_create(&threadId[i], &attr[i], PthreadF02, (void *)i);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    i++;
    ret = PosixPthreadInit(&attr[i], JFFS2NUM_JFFS2_GC_THREAD_PRIORITY2);
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

    ret = rmdir(bufname3);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = rmdir(bufname2);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = rmdir(bufname1);
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
    rmdir(bufname3);
    rmdir(bufname2);
    rmdir(bufname1);

    return JFFS_NO_ERROR;
}

VOID ItFsJffsMultipthread038(VOID)
{
    TEST_ADD_CASE("ItFsJffsMultipthread038", TestCase, TEST_VFS, TEST_JFFS, TEST_LEVEL3, TEST_PRESSURE);
}
