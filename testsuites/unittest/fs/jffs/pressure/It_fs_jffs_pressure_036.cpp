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

static CHAR g_jffs1647Pathname4[JFFS_SHORT_ARRAY_LENGTH][JFFS_MAXIMUM_SIZES][JFFS_STANDARD_NAME_LENGTH] = {0};
static CHAR g_jffs1647Pathname5[JFFS_SHORT_ARRAY_LENGTH][JFFS_MAXIMUM_SIZES][JFFS_STANDARD_NAME_LENGTH] = {0};

static VOID *PthreadF01(void *arg)
{
    INT32 ret, len;
    INT32 i = 0;
    INT32 j = 0;
    INT32 k = 0;
    off_t off;
    CHAR bufname[JFFS_SHORT_ARRAY_LENGTH] = "123456";
    CHAR filebuf[FILE_BUF_SIZE] =
        "abcdeabcde0123456789abcedfghij9876547210abcdeabcde0123456789abcedfghij9876547210abcdeabcde0123"
        "456789abcedfghij9876547210abcdeabcde0123456789abcedfghij9876547210abcdeabcde0123456789abcedfgh"
        "ij9876547210abcdeabcde0123456789abcedfghij9876547210lalalalalalalala";
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR pathname2[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR *bufR = NULL;
    CHAR *bufW = NULL;
    INT32 bufLen = BYTES_PER_KBYTE;

    ret = pthread_mutex_lock(&g_jffs2GlobalLock1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    g_TestCnt++;
    g_jffsFlagF01++;

    for (i = 0; i < JFFS_MAX_CYCLES; i++) {
        (void)memset_s(g_jffsPathname1, JFFS_STANDARD_NAME_LENGTH, 0, JFFS_STANDARD_NAME_LENGTH);
        (void)memset_s(g_jffsPathname11[i], JFFS_NAME_LIMITTED_SIZE, 0, JFFS_NAME_LIMITTED_SIZE);
        (void)memset_s(bufname, JFFS_SHORT_ARRAY_LENGTH, 0, strlen(bufname));

        snprintf_s(bufname, JFFS_SHORT_ARRAY_LENGTH, JFFS_SHORT_ARRAY_LENGTH - 1, "/test%d", i);
        strcat_s(g_jffsPathname1, JFFS_STANDARD_NAME_LENGTH, pathname1);
        strcat_s(g_jffsPathname1, JFFS_STANDARD_NAME_LENGTH, bufname);
        (void)strcpy_s(g_jffsPathname11[i], JFFS_NAME_LIMITTED_SIZE, g_jffsPathname1);

        ret = mkdir(g_jffsPathname11[i], HIGHEST_AUTHORITY);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);
    }

    (void)pthread_mutex_unlock(&g_jffs2GlobalLock1);

    LosTaskDelay(4); // delay time: 4

    ret = pthread_mutex_lock(&g_jffs2GlobalLock1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    bufW = (CHAR *)malloc(bufLen + 1);
    ICUNIT_GOTO_NOT_EQUAL(bufW, NULL, 0, EXIT3);
    (void)memset_s(bufW, bufLen + 1, 0, bufLen + 1);

    for (i = 0; i < bufLen / strlen(filebuf); i++) {
        strcat_s(bufW, bufLen + 1, filebuf);
    }

    for (i = 0; i < JFFS_MAX_CYCLES; i++) {
        for (j = 0; j < JFFS_MAXIMUM_SIZES; j++) {
            g_jffsFd12[i][j] = open(g_jffs1647Pathname4[i][j], O_NONBLOCK | O_RDWR, HIGHEST_AUTHORITY);
            ICUNIT_GOTO_NOT_EQUAL(g_jffsFd12[i][j], -1, g_jffsFd12[i][j], EXIT4);

            len = write(g_jffsFd12[i][j], bufW, strlen(bufW));
            ICUNIT_GOTO_EQUAL(len, strlen(bufW), len, EXIT4);

            ret = close(g_jffsFd12[i][j]);
            ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT4);
        }
    }

    free(bufW);
    (void)pthread_mutex_unlock(&g_jffs2GlobalLock1);

    LosTaskDelay(5); // delay time: 5

    ret = pthread_mutex_lock(&g_jffs2GlobalLock1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    bufR = (CHAR *)malloc(bufLen + 1);
    ICUNIT_GOTO_NOT_EQUAL(bufR, NULL, 0, EXIT3);
    (void)memset_s(bufR, bufLen + 1, 0, bufLen + 1);

    for (i = 0; i < JFFS_MAX_CYCLES; i++) {
        for (j = 0; j < JFFS_MAXIMUM_SIZES; j++) {
            g_jffsFd12[i][j] = open(g_jffs1647Pathname5[i][j], O_NONBLOCK | O_RDWR, HIGHEST_AUTHORITY);
            ICUNIT_GOTO_NOT_EQUAL(g_jffsFd12[i][j], -1, g_jffsFd12[i][j], EXIT5);

            off = lseek(g_jffsFd12[i][j], 0, SEEK_SET);
            ICUNIT_GOTO_EQUAL(off, 0, off, EXIT5);

            len = read(g_jffsFd12[i][j], bufR, bufLen + 1);
            ICUNIT_GOTO_EQUAL(len, bufLen, len, EXIT5);

            ret = close(g_jffsFd12[i][k]);
            ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT5);
        }
    }

    free(bufR);

    (void)pthread_mutex_unlock(&g_jffs2GlobalLock1);

    LosTaskDelay(4); // delay time: 4

    ret = pthread_mutex_lock(&g_jffs2GlobalLock1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    for (i = 0; i < JFFS_MAX_CYCLES; i++) {
        ret = remove(g_jffsPathname11[i]);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);
    }
    g_jffsFlag++;
    (void)pthread_mutex_unlock(&g_jffs2GlobalLock1);

    return NULL;
EXIT5:
    free(bufR);
    goto EXIT3;
EXIT4:
    free(bufW);
EXIT3:
    for (i = 0; i < JFFS_MAX_CYCLES; i++) {
        for (j = 0; j < JFFS_MAXIMUM_SIZES; j++) {
            close(g_jffsFd12[i][j]);
        }
    }
EXIT2:
    for (i = 0; i < JFFS_MAX_CYCLES; i++) {
        for (j = 0; j < JFFS_MAXIMUM_SIZES; j++) {
            unlink(g_jffs1647Pathname5[i][j]);
        }
    }
EXIT1:
    for (i = 0; i < JFFS_MAX_CYCLES; i++) {
        for (j = 0; j < JFFS_MAXIMUM_SIZES; j++) {
            unlink(g_jffs1647Pathname4[i][j]);
        }
    }
EXIT:
    for (i = 0; i < JFFS_MAX_CYCLES; i++) {
        remove(g_jffsPathname11[i]);
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
    INT32 k = 0;
    UINT32 len;
    INT32 fileCount2 = 0;
    CHAR bufname[JFFS_SHORT_ARRAY_LENGTH] = "123456";
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR pathname2[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };

    ret = pthread_mutex_lock(&g_jffs2GlobalLock1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    g_TestCnt++;

    for (i = 0; i < JFFS_MAX_CYCLES; i++) {
        for (j = 0; j < JFFS_MAXIMUM_SIZES; j++) {
            (void)memset_s(g_jffsPathname2, JFFS_STANDARD_NAME_LENGTH, 0, JFFS_STANDARD_NAME_LENGTH);
            (void)memset_s(g_jffs1647Pathname4[i][j], JFFS_STANDARD_NAME_LENGTH, 0, JFFS_STANDARD_NAME_LENGTH);
            (void)memset_s(g_jffs1647Pathname5[i][j], JFFS_STANDARD_NAME_LENGTH, 0, JFFS_STANDARD_NAME_LENGTH);
            (void)memset_s(bufname, JFFS_SHORT_ARRAY_LENGTH, 0, strlen(bufname));

            snprintf_s(bufname, JFFS_SHORT_ARRAY_LENGTH, JFFS_SHORT_ARRAY_LENGTH - 1, "/test%d", j);
            strcat_s(g_jffsPathname2, JFFS_NAME_LIMITTED_SIZE, g_jffsPathname11[i]);
            strcat_s(g_jffsPathname2, JFFS_STANDARD_NAME_LENGTH, bufname);
            strcat_s(g_jffsPathname2, JFFS_STANDARD_NAME_LENGTH, ".txt");
            (void)strcpy_s(g_jffs1647Pathname4[i][j], JFFS_STANDARD_NAME_LENGTH, g_jffsPathname2);

            (void)memset_s(g_jffsPathname2, JFFS_STANDARD_NAME_LENGTH, 0, JFFS_STANDARD_NAME_LENGTH);
            strcat_s(g_jffsPathname2, JFFS_NAME_LIMITTED_SIZE, g_jffsPathname11[i]);
            strcat_s(g_jffsPathname2, JFFS_STANDARD_NAME_LENGTH, bufname);
            strcat_s(g_jffsPathname2, JFFS_STANDARD_NAME_LENGTH, ".cpp");
            (void)strcpy_s(g_jffs1647Pathname5[i][j], JFFS_STANDARD_NAME_LENGTH, g_jffsPathname2);

            g_jffsFd12[i][j] = open(g_jffs1647Pathname4[i][j], O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL,
                HIGHEST_AUTHORITY);
            ICUNIT_GOTO_NOT_EQUAL(g_jffsFd12[i][j], -1, g_jffsFd12[i][j], EXIT1);

            ret = close(g_jffsFd12[i][j]);
            ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);
        }
    }

    (void)pthread_mutex_unlock(&g_jffs2GlobalLock1);

    LosTaskDelay(4); // delay time: 4

    ret = pthread_mutex_lock(&g_jffs2GlobalLock1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    for (i = 0; i < JFFS_MAX_CYCLES; i++) {
        for (j = 0; j < JFFS_MAXIMUM_SIZES; j++) {
            ret = rename(g_jffs1647Pathname4[i][j], g_jffs1647Pathname5[i][j]);
            ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);
        }
    }

    (void)pthread_mutex_unlock(&g_jffs2GlobalLock1);

    LosTaskDelay(4); // delay time: 4

    ret = pthread_mutex_lock(&g_jffs2GlobalLock1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

    for (i = 0; i < JFFS_MAX_CYCLES; i++) {
        for (j = 0; j < JFFS_MAXIMUM_SIZES; j++) {
            ret = unlink(g_jffs1647Pathname5[i][j]);
            ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);
        }
    }
    g_jffsFlagF02++;

    (void)pthread_mutex_unlock(&g_jffs2GlobalLock1);

    return NULL;
EXIT3:
    for (i = 0; i < JFFS_MAX_CYCLES; i++) {
        for (j = 0; j < JFFS_MAXIMUM_SIZES; j++) {
            close(g_jffsFd12[i][j]);
        }
    }
EXIT2:
    for (i = 0; i < JFFS_MAX_CYCLES; i++) {
        for (j = 0; j < JFFS_MAXIMUM_SIZES; j++) {
            unlink(g_jffs1647Pathname5[i][j]);
        }
    }
EXIT1:
    for (i = 0; i < JFFS_MAX_CYCLES; i++) {
        for (j = 0; j < JFFS_MAXIMUM_SIZES; j++) {
            unlink(g_jffs1647Pathname4[i][j]);
        }
    }
EXIT:
    for (i = 0; i < JFFS_MAX_CYCLES; i++) {
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
    INT32 fd[JFFS_SHORT_ARRAY_LENGTH];
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

        dprintf("\tjffs_flag=:%d\t", g_jffsFlag);
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

VOID ItFsJffsPressure036(VOID)
{
    TEST_ADD_CASE("ItFsJffsPressure036", TestCase, TEST_VFS, TEST_JFFS, TEST_LEVEL3, TEST_PRESSURE);
}
