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
static INT32 g_testNum = 100;

static VOID *PthreadF01(void *arg)
{
    INT32 ret, len;
    INT32 i = 0;
    INT32 j = 0;
    off_t off;
    CHAR bufname[JFFS_SHORT_ARRAY_LENGTH] = "123456";
    CHAR filebuf[FILE_BUF_SIZE] =
        "abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123"
        "456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfgh"
        "ij9876543210abcdeabcde0123456789abcedfghij9876543210lalalalalalalala";
    CHAR writebuf[101] = "liteos";
    CHAR readbuf[JFFS_STANDARD_NAME_LENGTH] = "liteos";
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR *bufW = NULL;
    INT32 bufWLen = BYTES_PER_KBYTE;

    ret = pthread_mutex_lock(&g_jffs2GlobalLock1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    g_TestCnt++;
    g_jffsFlagF01++;

    bufW = (CHAR *)malloc(bufWLen + 1);
    ICUNIT_GOTO_NOT_EQUAL(bufW, NULL, 0, EXIT1);
    (void)memset_s(bufW, bufWLen + 1, 0, bufWLen + 1);

    for (i = 0; i < bufWLen / strlen(filebuf); i++) {
        strcat_s(bufW, bufWLen + 1, filebuf);
    }

    for (i = 0; i < JFFS_MAXIMUM_SIZES; i++) {
        (void)memset_s(g_jffsPathname2, JFFS_STANDARD_NAME_LENGTH, 0, JFFS_STANDARD_NAME_LENGTH);
        (void)memset_s(g_jffsPathname12[i], JFFS_NAME_LIMITTED_SIZE, 0, JFFS_NAME_LIMITTED_SIZE);

        snprintf_s(bufname, JFFS_SHORT_ARRAY_LENGTH, JFFS_SHORT_ARRAY_LENGTH - 1, "/test%d", i);
        strcat_s(g_jffsPathname2, JFFS_STANDARD_NAME_LENGTH, pathname1);
        strcat_s(g_jffsPathname2, JFFS_STANDARD_NAME_LENGTH, bufname);
        strcat_s(g_jffsPathname2, JFFS_STANDARD_NAME_LENGTH, ".txt");
        (void)strcpy_s(g_jffsPathname12[i], JFFS_NAME_LIMITTED_SIZE, g_jffsPathname2);

        g_jffsFd11[i] = open(g_jffsPathname12[i], O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY);
        ICUNIT_GOTO_NOT_EQUAL(g_jffsFd11[i], -1, g_jffsFd11[i], EXIT2);
    }

    for (i = 0; i < JFFS_MAXIMUM_SIZES; i++) {
        len = write(g_jffsFd11[i], bufW, strlen(bufW));
        ICUNIT_GOTO_EQUAL(len, strlen(bufW), len, EXIT2);
    }

    ICUNIT_GOTO_NOT_EQUAL((g_TestCnt % 2), 0, g_TestCnt, EXIT2); // even number: 2

    free(bufW);

    (void)pthread_mutex_unlock(&g_jffs2GlobalLock1);

    LosTaskDelay(1); // delay time: 1

    ret = pthread_mutex_lock(&g_jffs2GlobalLock1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    (void)memset_s(writebuf, sizeof(writebuf), 0, strlen(writebuf));

    for (i = 0; i < g_testNum; i++) {
        strcat_s(writebuf, sizeof(writebuf), "a");
        for (j = 0; j < JFFS_MAXIMUM_SIZES; j++) {
            g_jffsFd11[j] = open(g_jffsPathname12[j], O_NONBLOCK | O_TRUNC | O_RDWR, HIGHEST_AUTHORITY);
            ICUNIT_GOTO_NOT_EQUAL(g_jffsFd11[j], -1, g_jffsFd11[j], EXIT1);

            len = write(g_jffsFd11[j], writebuf, strlen(writebuf));
            ICUNIT_GOTO_EQUAL(len, strlen(writebuf), len, EXIT1);

            ret = close(g_jffsFd11[j]);
            ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);
        }
    }

    (void)pthread_mutex_unlock(&g_jffs2GlobalLock1);

    LosTaskDelay(5); // delay time: 5

    ret = pthread_mutex_lock(&g_jffs2GlobalLock1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    for (i = 0; i < JFFS_MAXIMUM_SIZES; i++) {
        ret = unlink(g_jffsPathname12[i]);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);
    }
    g_jffsFlag++;

    (void)pthread_mutex_unlock(&g_jffs2GlobalLock1);

    return NULL;
EXIT2:
    free(bufW);
EXIT1:
    for (i = 0; i < JFFS_MAXIMUM_SIZES; i++) {
        close(g_jffsFd11[i]);
    }
EXIT:
    for (i = 0; i < JFFS_MAXIMUM_SIZES; i++) {
        unlink(g_jffsPathname12[i]);
    }
    g_TestCnt = 0;
    (void)pthread_mutex_unlock(&g_jffs2GlobalLock1);
    return NULL;
}

static VOID *PthreadF02(void *arg)
{
    INT32 fd1, ret;
    INT32 i = 0;
    INT32 j = 0;
    UINT32 len;
    INT32 fileCount2 = 0;
    CHAR bufname[JFFS_SHORT_ARRAY_LENGTH] = "123456";
    CHAR filebuf[FILE_BUF_SIZE] =
        "abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123"
        "456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfgh"
        "ij9876543210abcdeabcde0123456789abcedfghij9876543210lalalalalalalala";
    CHAR readbuf[101] = "liteos";
    CHAR *bufR = NULL;
    off_t off;

    ret = pthread_mutex_lock(&g_jffs2GlobalLock1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    g_TestCnt++;

    bufR = (CHAR *)malloc(BYTES_PER_KBYTE + 1);
    ICUNIT_GOTO_NOT_EQUAL(bufR, NULL, 0, EXIT2);
    (void)memset_s(bufR, BYTES_PER_KBYTE + 1, 0, BYTES_PER_KBYTE + 1);

    for (i = 0; i < JFFS_MAXIMUM_SIZES; i++) {
        len = read(g_jffsFd11[i], readbuf, JFFS_STANDARD_NAME_LENGTH);
        ICUNIT_GOTO_EQUAL(len, 0, len, EXIT2);

        off = lseek(g_jffsFd11[i], 0, SEEK_SET);
        ICUNIT_GOTO_EQUAL(off, 0, off, EXIT2);

        len = read(g_jffsFd11[i], bufR, BYTES_PER_KBYTE + 1);
        ICUNIT_GOTO_EQUAL(len, BYTES_PER_KBYTE, len, EXIT2);

        ret = close(g_jffsFd11[i]);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);
    }

    ICUNIT_GOTO_EQUAL((g_TestCnt % 2), 0, g_TestCnt, EXIT2); // even number: 2

    free(bufR);

    (void)pthread_mutex_unlock(&g_jffs2GlobalLock1);

    LosTaskDelay(4); // delay time: 4

    ret = pthread_mutex_lock(&g_jffs2GlobalLock1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    for (i = 0; i < g_testNum; i++) {
        for (j = 0; j < JFFS_MAXIMUM_SIZES; j++) {
            g_jffsFd11[j] = open(g_jffsPathname12[j], O_NONBLOCK | O_RDWR, HIGHEST_AUTHORITY);
            ICUNIT_GOTO_NOT_EQUAL(g_jffsFd11[j], -1, g_jffsFd11[j], EXIT1);

            off = lseek(g_jffsFd11[j], 0, SEEK_SET);
            ICUNIT_GOTO_EQUAL(off, 0, off, EXIT1);

            len = read(g_jffsFd11[j], readbuf, 101);   // read length: 101
            ICUNIT_GOTO_EQUAL(len, 100, len, EXIT1); // read length: 100
        }
        for (j = 0; j < JFFS_MAXIMUM_SIZES; j++) {
            ret = close(g_jffsFd11[j]);
            ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);
        }
    }

    g_jffsFlagF02++;

    ICUNIT_GOTO_EQUAL((g_TestCnt % 2), 0, g_TestCnt, EXIT1); // even number: 2

    (void)pthread_mutex_unlock(&g_jffs2GlobalLock1);

    LosTaskDelay(2); // delay time: 2

    return NULL;
EXIT2:
    free(bufR);
EXIT1:
    for (i = 0; i < JFFS_SHORT_ARRAY_LENGTH; i++) {
        close(g_jffsFd11[i]);
    }
EXIT:
    for (i = 0; i < JFFS_SHORT_ARRAY_LENGTH; i++) {
        unlink(g_jffsPathname12[i]);
    }
    g_TestCnt = 0;
    (void)pthread_mutex_unlock(&g_jffs2GlobalLock1);
    return NULL;
}

static UINT32 TestCase(VOID)
{
    INT32 ret, len;
    INT32 i = 0;
    INT32 j = 0;
    INT32 fd[JFFS_SHORT_ARRAY_LENGTH];
    CHAR pathname[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR buffile[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    struct stat buf1 = { 0 };
    struct stat buf2 = { 0 };
    DIR *dir = NULL;
    struct dirent *ptr = NULL;
    pthread_t newTh1, newTh2;
    pthread_attr_t attr1, attr2;
    INT32 priority1 = 18;
    INT32 priority2 = 20;

    g_jffsFlag = 0;
    g_jffsFlagF01 = 0;
    g_jffsFlagF02 = 0;

    ret = mkdir(pathname, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = chdir(pathname);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    while ((g_jffsFlag < JFFS_PRESSURE_CYCLES) && (g_jffsFlag == g_jffsFlagF01)) {
        g_TestCnt = 0;

        ret = PosixPthreadInit(&attr1, priority1);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

        ret = pthread_create(&newTh1, &attr1, PthreadF01, NULL);
        ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);

        ret = PosixPthreadInit(&attr2, priority2);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

        ret = pthread_create(&newTh2, &attr2, PthreadF02, NULL);
        ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);

        LosTaskDelay(10); // delay time: 10

        ret = PosixPthreadDestroy(&attr2, newTh2);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

        ret = PosixPthreadDestroy(&attr1, newTh1);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

        dprintf("\tjffs_flag:=%d\t", g_jffsFlag);
    }
    dprintf("\n");
    ICUNIT_GOTO_EQUAL(g_jffsFlag, JFFS_PRESSURE_CYCLES, g_jffsFlag, EXIT2);

    dir = opendir(JFFS_PATH_NAME0);
    ICUNIT_GOTO_NOT_EQUAL(dir, NULL, dir, EXIT3);

    ptr = readdir(dir);
    ICUNIT_GOTO_EQUAL(ptr, NULL, ptr, EXIT3);

    ret = closedir(dir);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT3);

    ret = chdir(JFFS_MAIN_DIR0);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = rmdir(JFFS_PATH_NAME0);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    return JFFS_NO_ERROR;
EXIT3:
    closedir(dir);
    goto EXIT;
EXIT2:
    PosixPthreadDestroy(&attr2, newTh2);
EXIT1:
    PosixPthreadDestroy(&attr1, newTh1);
EXIT:
    rmdir(JFFS_PATH_NAME0);
    return JFFS_NO_ERROR;
}

VOID ItFsJffsPressure034(VOID)
{
    TEST_ADD_CASE("ItFsJffsPressure034", TestCase, TEST_VFS, TEST_JFFS, TEST_LEVEL3, TEST_PRESSURE);
}
