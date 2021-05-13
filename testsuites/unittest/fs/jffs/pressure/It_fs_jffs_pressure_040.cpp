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
    INT32 fd1 = -1;
    INT32 ret, len;
    INT32 i = 0;
    INT32 j = 0;
    INT32 k = 0;
    INT32 dirCount = 0;
    INT32 dirCount1 = 0;
    CHAR bufname[JFFS_SHORT_ARRAY_LENGTH] = "123456";
    CHAR pathname[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR filebuf[FILE_BUF_SIZE] =
        "abcdeabcde0123456789abcedfghij9876550210abcdeabcde0123456789abcedfghij9876550210abcdeabcde0123"
        "456789abcedfghij9876550210abcdeabcde0123456789abcedfghij9876550210abcdeabcde0123456789abcedfgh"
        "ij9876550210abcdeabcde0123456789abcedfghij9876550210lalalalalalalala";
    CHAR *bufR = NULL;
    CHAR *bufW = NULL;
    INT32 bufLen = 2 * BYTES_PER_KBYTE; // 2 KB

    ret = pthread_mutex_lock(&g_jffs2GlobalLock1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    g_TestCnt++;
    g_jffsFlagF01++;

    for (i = 0; i < JFFS_SHORT_ARRAY_LENGTH; i++) {
        memset_s(g_jffsPathname11[i], JFFS_NAME_LIMITTED_SIZE, 0, JFFS_NAME_LIMITTED_SIZE);
        memset_s(g_jffsPathname12[i], JFFS_NAME_LIMITTED_SIZE, 0, JFFS_NAME_LIMITTED_SIZE);
        strcat_s(g_jffsPathname11[i], JFFS_NAME_LIMITTED_SIZE, pathname);
        strcat_s(g_jffsPathname12[i], JFFS_NAME_LIMITTED_SIZE, pathname);
        strcat_s(g_jffsPathname11[i], JFFS_NAME_LIMITTED_SIZE, "/");
        strcat_s(g_jffsPathname12[i], JFFS_NAME_LIMITTED_SIZE, "/");

        memset_s(bufname, JFFS_SHORT_ARRAY_LENGTH, 0, strlen(bufname));
        snprintf_s(bufname, JFFS_SHORT_ARRAY_LENGTH, JFFS_SHORT_ARRAY_LENGTH - 1, "AAAA%d", i);
        strcat_s(g_jffsPathname11[i], JFFS_NAME_LIMITTED_SIZE, bufname);

        memset_s(bufname, JFFS_SHORT_ARRAY_LENGTH, 0, strlen(bufname));
        snprintf_s(bufname, JFFS_SHORT_ARRAY_LENGTH, JFFS_SHORT_ARRAY_LENGTH - 1, "BBBB%d", i);
        strcat_s(g_jffsPathname12[i], JFFS_NAME_LIMITTED_SIZE, bufname);

        memset_s(bufname, JFFS_SHORT_ARRAY_LENGTH, 0, strlen(bufname));
        snprintf_s(bufname, JFFS_SHORT_ARRAY_LENGTH, JFFS_SHORT_ARRAY_LENGTH - 1, "%d", i);
        strcat_s(g_jffsPathname11[i], JFFS_NAME_LIMITTED_SIZE, bufname);
        strcat_s(g_jffsPathname12[i], JFFS_NAME_LIMITTED_SIZE, bufname);

        g_jffsFd11[i] = open(g_jffsPathname11[i], O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY);
        ICUNIT_GOTO_NOT_EQUAL(g_jffsFd11[i], -1, g_jffsFd11[i], EXIT);

        ret = close(g_jffsFd11[i]);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);
    }

    (void)pthread_mutex_unlock(&g_jffs2GlobalLock1);

    LosTaskDelay(1); // delay time: 1

    ret = pthread_mutex_lock(&g_jffs2GlobalLock1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    bufR = (CHAR *)malloc(bufLen + 1);
    ICUNIT_GOTO_NOT_EQUAL(bufR, NULL, 0, EXIT2);
    memset_s(bufR, bufLen + 1, 0, bufLen + 1);

    for (i = 0; i < JFFS_SHORT_ARRAY_LENGTH; i++) {
        g_jffsFd11[i] = open(g_jffsPathname11[i], O_NONBLOCK | O_RDWR, HIGHEST_AUTHORITY);
        ICUNIT_GOTO_NOT_EQUAL(g_jffsFd11[i], -1, g_jffsFd11[i], EXIT3);

        ret = read(g_jffsFd11[i], bufR, bufLen + 1);
        ICUNIT_GOTO_EQUAL(ret, bufLen, ret, EXIT3);

        ret = close(g_jffsFd11[i]);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT3);
    }
    free(bufR);

    (void)pthread_mutex_unlock(&g_jffs2GlobalLock1);

    LosTaskDelay(1); // delay time: 1

    ret = pthread_mutex_lock(&g_jffs2GlobalLock1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    bufR = (CHAR *)malloc(bufLen + 1);
    ICUNIT_GOTO_NOT_EQUAL(bufR, NULL, 0, EXIT2);
    memset_s(bufR, bufLen + 1, 0, bufLen + 1);

    for (i = 0; i < JFFS_SHORT_ARRAY_LENGTH; i++) {
        g_jffsFd11[i] = open(g_jffsPathname11[i], O_NONBLOCK | O_RDWR, HIGHEST_AUTHORITY);
        ICUNIT_GOTO_NOT_EQUAL(g_jffsFd11[i], -1, g_jffsFd11[i], EXIT3);

        ret = read(g_jffsFd11[i], bufR, bufLen);
        ICUNIT_GOTO_EQUAL(ret, bufLen, ret, EXIT3);

        ret = close(g_jffsFd11[i]);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT3);
    }

    free(bufR);
    g_jffsFlag++;

    (void)pthread_mutex_unlock(&g_jffs2GlobalLock1);

    LosTaskDelay(1); // delay time: 1

    return NULL;
EXIT3:
    free(bufR);
EXIT2:
    for (i = 0; i < JFFS_MAXIMUM_SIZES; i++) {
        close(g_jffsFd11[i]);
    }
EXIT1:
    for (i = 0; i < JFFS_MAXIMUM_SIZES; i++) {
        remove(g_jffsPathname12[i]);
    }
EXIT:
    for (i = 0; i < JFFS_MAXIMUM_SIZES; i++) {
        remove(g_jffsPathname11[i]);
    }
    g_TestCnt = 0;
    (void)pthread_mutex_unlock(&g_jffs2GlobalLock1);
    return NULL;
}

static VOID *PthreadF02(void *arg)
{
    INT32 fd = -1;
    INT32 fd1 = -1;
    INT32 ret, len;
    INT32 i = 0;
    INT32 j = 0;
    INT32 dirCount = 0;
    CHAR bufname[JFFS_SHORT_ARRAY_LENGTH] = "123456";
    CHAR filebuf[FILE_BUF_SIZE] =
        "abcdeabcde0123456789abcedfghij9876550210abcdeabcde0123456789abcedfghij9876550210abcdeabcde0123"
        "456789abcedfghij9876550210abcdeabcde0123456789abcedfghij9876550210abcdeabcde0123456789abcedfgh"
        "ij9876550210abcdeabcde0123456789abcedfghij9876550210lalalalalalalala";
    CHAR *bufW = NULL;
    INT32 bufWLen = 2 * BYTES_PER_KBYTE; // 2 KB

    g_TestCnt++;
    g_jffsFlagF02++;

    ret = pthread_mutex_lock(&g_jffs2GlobalLock1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    bufW = (CHAR *)malloc(bufWLen + 1);
    ICUNIT_GOTO_NOT_EQUAL(bufW, NULL, 0, EXIT2);
    memset_s(bufW, bufWLen + 1, 0, bufWLen + 1);

    for (i = 0; i < bufWLen / strlen(filebuf); i++) {
        strcat_s(bufW, bufWLen + 1, filebuf);
    }

    for (i = 0; i < JFFS_SHORT_ARRAY_LENGTH; i++) {
        g_jffsFd11[i] = open(g_jffsPathname11[i], O_NONBLOCK | O_RDWR, HIGHEST_AUTHORITY);
        ICUNIT_GOTO_NOT_EQUAL(g_jffsFd11[i], -1, g_jffsFd11[i], EXIT3);

        len = write(g_jffsFd11[i], bufW, strlen(bufW));
        ICUNIT_GOTO_EQUAL(len, strlen(bufW), len, EXIT3);

        ret = close(g_jffsFd11[i]);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT3);
    }
    free(bufW);

    (void)pthread_mutex_unlock(&g_jffs2GlobalLock1);

    LosTaskDelay(4); // delay time: 4

    ret = pthread_mutex_lock(&g_jffs2GlobalLock1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    for (i = 0; i < JFFS_SHORT_ARRAY_LENGTH; i++) {
        for (j = 0; j < JFFS_SHORT_ARRAY_LENGTH; j++) {
            if (j % 2 == 0) { // even number: 2
                ret = rename(g_jffsPathname11[i], g_jffsPathname12[i]);
                ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);
            } else {
                ret = rename(g_jffsPathname12[i], g_jffsPathname11[i]);
                ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);
            }
        }
    }

    (void)pthread_mutex_unlock(&g_jffs2GlobalLock1);

    LosTaskDelay(2); // delay time: 2

    ret = pthread_mutex_lock(&g_jffs2GlobalLock1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    for (i = 0; i < JFFS_SHORT_ARRAY_LENGTH; i++) {
        ret = remove(g_jffsPathname11[i]);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);
    }

    (void)pthread_mutex_unlock(&g_jffs2GlobalLock1);

    LosTaskDelay(4); // delay time: 4

    return NULL;
EXIT3:
    free(bufW);
EXIT2:
    for (i = 0; i < JFFS_MAXIMUM_SIZES; i++) {
        close(g_jffsFd11[i]);
    }
EXIT1:
    for (i = 0; i < JFFS_MAXIMUM_SIZES; i++) {
        remove(g_jffsPathname12[i]);
    }
EXIT:
    for (i = 0; i < JFFS_MAXIMUM_SIZES; i++) {
        remove(g_jffsPathname11[i]);
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
    CHAR pathname[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR buffile[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    struct stat buf1 = { 0 };
    struct stat buf2 = { 0 };
    DIR *dir = NULL;
    struct dirent *ptr = NULL;
    pthread_t newTh1, newTh2;
    pthread_attr_t attr1, attr2;
    INT32 priority = 20;

    g_jffsFlag = 0;
    g_jffsFlagF01 = 0;
    g_jffsFlagF02 = 0;

    ret = mkdir(pathname, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = chdir(pathname);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    while ((g_jffsFlag < JFFS_PRESSURE_CYCLES) && (g_jffsFlag == g_jffsFlagF01)) {
        g_TestCnt = 0;

        ret = PosixPthreadInit(&attr1, priority);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

        ret = pthread_create(&newTh1, &attr1, PthreadF01, NULL);
        ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);

        ret = PosixPthreadInit(&attr2, priority);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

        ret = pthread_create(&newTh2, &attr2, PthreadF02, NULL);
        ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);

        LosTaskDelay(1); // delay time: 1

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

VOID ItFsJffsPressure040(VOID)
{
    TEST_ADD_CASE("ItFsJffsPressure040", TestCase, TEST_VFS, TEST_JFFS, TEST_LEVEL3, TEST_PRESSURE);
}
