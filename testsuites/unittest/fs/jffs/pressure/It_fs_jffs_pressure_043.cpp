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
    INT32 ret, len;
    INT32 i = 0;
    INT32 j = 0;
    INT32 k = 0;
    CHAR bufname[JFFS_SHORT_ARRAY_LENGTH] = "123456";
    CHAR pathname[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = { JFFS_MAIN_DIR0 };
    CHAR pathname2[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR filebuf1[JFFS_STANDARD_NAME_LENGTH] = "0123456789LLLLLL";
    CHAR filebuf2[JFFS_STANDARD_NAME_LENGTH] = "aaaaaaaaaaaaaaaa";
    CHAR filebuf3[JFFS_STANDARD_NAME_LENGTH] = "BBBBBBBBBBBBBBBB";
    CHAR *bufR1 = NULL;
    CHAR *bufR2 = NULL;
    CHAR *bufR3 = NULL;
    off_t off;
    INT32 fileNum = 3;

    ret = pthread_mutex_lock(&g_jffs2GlobalLock1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    g_TestCnt++;
    g_jffsFlagF01++;

    for (j = 0; j < fileNum; j++) {
        memset_s(g_jffsPathname11[j], JFFS_NAME_LIMITTED_SIZE, 0, JFFS_NAME_LIMITTED_SIZE);
        memset_s(g_jffsPathname12[j], JFFS_NAME_LIMITTED_SIZE, 0, JFFS_NAME_LIMITTED_SIZE);
        memset_s(bufname, JFFS_SHORT_ARRAY_LENGTH, 0, JFFS_SHORT_ARRAY_LENGTH);

        snprintf_s(bufname, JFFS_SHORT_ARRAY_LENGTH, JFFS_SHORT_ARRAY_LENGTH - 1, "/test%d", j);
        strcat_s(g_jffsPathname11[j], JFFS_NAME_LIMITTED_SIZE, pathname);
        strcat_s(g_jffsPathname12[j], JFFS_NAME_LIMITTED_SIZE, pathname2);

        strcat_s(g_jffsPathname11[j], JFFS_NAME_LIMITTED_SIZE, bufname);
        strcat_s(g_jffsPathname12[j], JFFS_NAME_LIMITTED_SIZE, bufname);

        strcat_s(g_jffsPathname11[j], JFFS_NAME_LIMITTED_SIZE, ".txt");
        strcat_s(g_jffsPathname12[j], JFFS_NAME_LIMITTED_SIZE, ".cpp");

        g_jffsFd11[j] = open(g_jffsPathname11[j], O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY);
        ICUNIT_GOTO_NOT_EQUAL(g_jffsFd11[j], -1, g_jffsFd11[j], EXIT2);
    }

    (void)pthread_mutex_unlock(&g_jffs2GlobalLock1);

    LosTaskDelay(4); // delay time: 4

    ret = pthread_mutex_lock(&g_jffs2GlobalLock1);
    printf("[%d] Thread1 Second Get lock,ret:%d \n", __LINE__, ret);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

    bufR1 = (CHAR *)malloc(BYTES_PER_KBYTE + 1);
    ICUNIT_GOTO_NOT_EQUAL(bufR1, NULL, 0, EXIT2);
    memset_s(bufR1, BYTES_PER_KBYTE + 1, 0, BYTES_PER_KBYTE + 1);

    bufR2 = (CHAR *)malloc(BYTES_PER_KBYTE + 1);
    ICUNIT_GOTO_NOT_EQUAL(bufR2, NULL, 0, EXIT3);
    memset_s(bufR2, BYTES_PER_KBYTE + 1, 0, BYTES_PER_KBYTE + 1);

    bufR3 = (CHAR *)malloc(BYTES_PER_KBYTE + 1);
    ICUNIT_GOTO_NOT_EQUAL(bufR3, NULL, 0, EXIT4);
    memset_s(bufR3, BYTES_PER_KBYTE + 1, 0, BYTES_PER_KBYTE + 1);

    for (j = 0; j < fileNum; j++) {
        off = lseek(g_jffsFd11[j], 0, SEEK_END);
        ICUNIT_GOTO_EQUAL(off, BYTES_PER_KBYTE, off, EXIT5);
    }

    for (j = 0; j < fileNum; j++) {
        off = lseek(g_jffsFd11[j], 0, SEEK_SET);
        ICUNIT_GOTO_EQUAL(off, 0, off, EXIT5);
    }

    j = 0;
    len = read(g_jffsFd11[j], bufR1, BYTES_PER_KBYTE + 1);
    ICUNIT_GOTO_EQUAL(len, BYTES_PER_KBYTE, len, EXIT5);

    j++;
    len = read(g_jffsFd11[j], bufR2, BYTES_PER_KBYTE + 1);
    ICUNIT_GOTO_EQUAL(len, BYTES_PER_KBYTE, len, EXIT5);

    j++;
    len = read(g_jffsFd11[j], bufR3, BYTES_PER_KBYTE + 1);
    ICUNIT_GOTO_EQUAL(len, BYTES_PER_KBYTE, len, EXIT5);

    for (j = 0; j < fileNum; j++) {
        ret = close(g_jffsFd11[j]);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT5);
    }

    for (j = 0; j < fileNum; j++) {
        g_jffsFd11[j] = open(g_jffsPathname11[j], O_NONBLOCK | O_TRUNC | O_RDWR, HIGHEST_AUTHORITY);
        ICUNIT_GOTO_NOT_EQUAL(g_jffsFd11[j], -1, g_jffsFd11[j], EXIT5);
    }

    j = 0;
    len = write(g_jffsFd11[j], bufR2, strlen(bufR2));
    ICUNIT_GOTO_EQUAL(len, BYTES_PER_KBYTE, len, EXIT5);

    j++;
    len = write(g_jffsFd11[j], bufR3, strlen(bufR3));
    ICUNIT_GOTO_EQUAL(len, BYTES_PER_KBYTE, len, EXIT5);

    j++;
    len = write(g_jffsFd11[j], bufR1, strlen(bufR1));
    ICUNIT_GOTO_EQUAL(len, BYTES_PER_KBYTE, len, EXIT5);

    for (j = 0; j < fileNum; j++) {
        ret = close(g_jffsFd11[j]);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT5);
    }

    free(bufR1);
    free(bufR2);
    free(bufR3);

    g_jffsFlag++;

    (void)pthread_mutex_unlock(&g_jffs2GlobalLock1);

    LosTaskDelay(4); // delay time: 4

    return NULL;
EXIT5:
    free(bufR3);
EXIT4:
    free(bufR2);
EXIT3:
    free(bufR1);
EXIT2:
    for (j = 0; j < fileNum; j++) {
        close(g_jffsFd11[j]);
    }
EXIT1:
    for (j = 0; j < fileNum; j++) {
        remove(g_jffsPathname11[j]);
        remove(g_jffsPathname12[j]);
    }
EXIT:
    g_TestCnt = 0;
    remove(pathname2);
    (void)pthread_mutex_unlock(&g_jffs2GlobalLock1);
    return NULL;
}

static VOID *PthreadF02(void *arg)
{
    INT32 ret, len;
    INT32 i = 0;
    INT32 j = 0;
    CHAR bufname[15] = "01234567890";
    CHAR pathname[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR pathname2[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR filebuf1[JFFS_STANDARD_NAME_LENGTH] = "0123456789LLLLLL";
    CHAR filebuf2[JFFS_STANDARD_NAME_LENGTH] = "aaaaaaaaaaaaaaaa";
    CHAR filebuf3[JFFS_STANDARD_NAME_LENGTH] = "BBBBBBBBBBBBBBBB";
    CHAR *bufW1 = NULL;
    CHAR *bufW2 = NULL;
    CHAR *bufW3 = NULL;
    off_t off;
    INT32 bufWLen = BYTES_PER_KBYTE;
    INT32 fileNum = 3;

    ret = pthread_mutex_lock(&g_jffs2GlobalLock1);
    printf("[%d] Thread2 First Get lock,ret:%d \n", __LINE__, ret);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

    g_TestCnt++;

    bufW1 = (CHAR *)malloc(bufWLen + 1);
    ICUNIT_GOTO_NOT_EQUAL(bufW1, NULL, 0, EXIT2);
    memset_s(bufW1, bufWLen + 1, 0, bufWLen + 1);

    bufW2 = (CHAR *)malloc(bufWLen + 1);
    ICUNIT_GOTO_NOT_EQUAL(bufW2, NULL, 0, EXIT3);
    memset_s(bufW2, bufWLen + 1, 0, bufWLen + 1);

    bufW3 = (CHAR *)malloc(bufWLen + 1);
    ICUNIT_GOTO_NOT_EQUAL(bufW3, NULL, 0, EXIT4);
    memset_s(bufW3, bufWLen + 1, 0, bufWLen + 1);

    for (j = 0; j < bufWLen / strlen(filebuf1); j++) {
        strcat_s(bufW1, bufWLen + 1, filebuf1);
        strcat_s(bufW2, bufWLen + 1, filebuf2);
        strcat_s(bufW3, bufWLen + 1, filebuf3);
    }

    j = 0;
    ret = write(g_jffsFd11[j], bufW1, strlen(bufW1));
    ICUNIT_GOTO_EQUAL(ret, strlen(bufW1), ret, EXIT5);

    j++;
    ret = write(g_jffsFd11[j], bufW2, strlen(bufW2));
    ICUNIT_GOTO_EQUAL(ret, strlen(bufW2), ret, EXIT5);

    j++;
    ret = write(g_jffsFd11[j], bufW3, strlen(bufW3));
    ICUNIT_GOTO_EQUAL(ret, strlen(bufW3), ret, EXIT5);

    free(bufW1);
    free(bufW2);
    free(bufW3);

    (void)pthread_mutex_unlock(&g_jffs2GlobalLock1);

    LosTaskDelay(10); // delay time: 10

    ret = pthread_mutex_lock(&g_jffs2GlobalLock1);
    printf("[%d] Thread2 Second Get lock,ret:%d \n", __LINE__, ret);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

    for (j = 0; j < fileNum; j++) {
        ret = rename(g_jffsPathname11[j], g_jffsPathname12[j]);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);
    }
    ret = chdir("/");
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT7);

    for (j = 0; j < JFFS_MAXIMUM_OPERATIONS; j++) {
        ret = umount(JFFS_MAIN_DIR0);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT7);

        ret = mount(JFFS_DEV_PATH0, JFFS_MAIN_DIR0, JFFS_FILESYS_TYPE, 0, NULL);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT7);
    }
    ret = chdir(JFFS_MAIN_DIR0);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

    for (j = 0; j < fileNum; j++) {
        g_jffsFd11[j] = open(g_jffsPathname12[j], O_NONBLOCK | O_RDWR, HIGHEST_AUTHORITY);
        ICUNIT_GOTO_NOT_EQUAL(g_jffsFd11[j], -1, g_jffsFd11[j], EXIT2);
    }

    for (j = 0; j < fileNum; j++) {
        off = lseek(g_jffsFd11[j], 0, SEEK_END);
        ICUNIT_GOTO_EQUAL(off, 1 * 1024, off, EXIT2); // file size: 1 * 1024
    }

    for (j = 0; j < fileNum; j++) {
        ret = close(g_jffsFd11[j]);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

        ret = unlink(g_jffsPathname12[j]);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);
    }

    g_jffsFlagF02++;

    ret = rmdir(pathname2);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = mkdir(pathname, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    (void)pthread_mutex_unlock(&g_jffs2GlobalLock1);

    LosTaskDelay(2); // delay time: 2

    return NULL;
EXIT7:
    mount(JFFS_DEV_PATH0, JFFS_MAIN_DIR0, "vfat", 0, NULL);
    goto EXIT2;
EXIT5:
    free(bufW3);
EXIT4:
    free(bufW2);
EXIT3:
    free(bufW1);
EXIT2:
    for (j = 0; j < fileNum; j++) {
        close(g_jffsFd11[j]);
    }
EXIT1:
    for (j = 0; j < fileNum; j++) {
        remove(g_jffsPathname11[j]);
        remove(g_jffsPathname12[j]);
    }
EXIT:
    g_TestCnt = 0;
    remove(pathname2);
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
    CHAR filebuf[FILE_BUF_SIZE] =
        "abcdeabcde0123456789abcedfghij9876550210abcdeabcde0123456789abcedfghij9876550210abcdeabcde0123"
        "456789abcedfghij9876550210abcdeabcde0123456789abcedfghij9876550210abcdeabcde0123456789abcedfgh"
        "ij9876550210abcdeabcde0123456789abcedfghij9876550210lalalalalalalala";
    CHAR *bufW1 = NULL;
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

        LosTaskDelay(10); // delay time: 10

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

VOID ItFsJffsPressure043(VOID)
{
    TEST_ADD_CASE("ItFsJffsPressure043", TestCase, TEST_VFS, TEST_JFFS, TEST_LEVEL3, TEST_PRESSURE);
}
