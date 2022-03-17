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
    INT32 dirCount1 = 0;
    CHAR bufname[JFFS_SHORT_ARRAY_LENGTH] = "123456";
    CHAR pathname[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR filebuf[FILE_BUF_SIZE] =
        "abcdeabcde0123456789abcedfghij9876550210abcdeabcde0123456789abcedfghij9876550210abcdeabcde0123"
        "456789abcedfghij9876550210abcdeabcde0123456789abcedfghij9876550210abcdeabcde0123456789abcedfgh"
        "ij9876550210abcdeabcde0123456789abcedfghij9876550210lalalalalalalala";
    off_t off;
    INT32 fileNum = 4;

    ret = pthread_mutex_lock(&g_jffs2GlobalLock1);
    printf("[%d] Thread1 first get lock\n", __LINE__);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    g_TestCnt++;
    g_jffsFlagF01++;

    for (j = 0; j < fileNum; j++) {
        (void)memset_s(g_jffsPathname11[j], JFFS_NAME_LIMITTED_SIZE, 0, JFFS_NAME_LIMITTED_SIZE);
        (void)memset_s(g_jffsPathname12[j], JFFS_NAME_LIMITTED_SIZE, 0, JFFS_NAME_LIMITTED_SIZE);
        (void)memset_s(g_jffsPathname13[j], JFFS_STANDARD_NAME_LENGTH, 0, JFFS_STANDARD_NAME_LENGTH);
        (void)memset_s(bufname, JFFS_SHORT_ARRAY_LENGTH, 0, JFFS_SHORT_ARRAY_LENGTH);
        snprintf_s(bufname, JFFS_SHORT_ARRAY_LENGTH, JFFS_SHORT_ARRAY_LENGTH - 1, "/test%d", j);
        strcat_s(g_jffsPathname11[j], JFFS_NAME_LIMITTED_SIZE, pathname);
        strcat_s(g_jffsPathname12[j], JFFS_NAME_LIMITTED_SIZE, pathname);
        strcat_s(g_jffsPathname13[j], JFFS_NAME_LIMITTED_SIZE, pathname);

        strcat_s(g_jffsPathname11[j], JFFS_NAME_LIMITTED_SIZE, bufname);
        strcat_s(g_jffsPathname12[j], JFFS_NAME_LIMITTED_SIZE, bufname);
        strcat_s(g_jffsPathname13[j], JFFS_NAME_LIMITTED_SIZE, bufname);

        strcat_s(g_jffsPathname11[j], JFFS_NAME_LIMITTED_SIZE, ".txt");
        strcat_s(g_jffsPathname12[j], JFFS_NAME_LIMITTED_SIZE, ".cpp");
        strcat_s(g_jffsPathname13[j], JFFS_NAME_LIMITTED_SIZE, ".jpg");

        g_jffsFd11[j] = open(g_jffsPathname11[j], O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY);
        ICUNIT_GOTO_NOT_EQUAL(g_jffsFd11[j], -1, g_jffsFd11[j], EXIT2);
    }

    (void)pthread_mutex_unlock(&g_jffs2GlobalLock1);

    LosTaskDelay(3); // delay time: 3

    ret = pthread_mutex_lock(&g_jffs2GlobalLock1);
    printf("[%d] Thread1 second get lock\n", __LINE__);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT2);

    for (j = 0; j < fileNum; j++) {
        errno = 0;
        off = lseek(g_jffsFd11[j], 0, SEEK_END);
        ICUNIT_GOTO_EQUAL(off, 1 * 1024, off, EXIT2); // file size: 1 * 1024
    }

    for (j = 0; j < fileNum; j++) {
        ret = close(g_jffsFd11[j]);
        ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT2);
    }

    ret = chdir("/");
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

    for (j = 0; j < JFFS_MAXIMUM_OPERATIONS; j++) {
        ret = umount(JFFS_MAIN_DIR0);
        printf("[%d] umount:%s,\n", __LINE__, JFFS_MAIN_DIR0);
        ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT2);

        ret = mount(JFFS_DEV_PATH0, JFFS_MAIN_DIR0, JFFS_FILESYS_TYPE, 0, NULL);
        printf("[%d] mount:%s,\n", __LINE__, JFFS_MAIN_DIR0);
        ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT3);
    }
    ret = chdir(JFFS_PATH_NAME0);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

    (void)pthread_mutex_unlock(&g_jffs2GlobalLock1);

    LosTaskDelay(3); // delay time: 3
    printf("[%d] Thread1 Start third get lock\n", __LINE__);
    ret = pthread_mutex_lock(&g_jffs2GlobalLock1);
    printf("[%d] Thread1 third get lock\n", __LINE__);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT2);

    ret = chdir("/");
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

    for (j = 0; j < JFFS_MAXIMUM_OPERATIONS; j++) {
        ret = umount(JFFS_MAIN_DIR0);
        ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT2);

        ret = mount(JFFS_DEV_PATH0, JFFS_MAIN_DIR0, JFFS_FILESYS_TYPE, 0, NULL);
        ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT3);
    }

    ret = chdir(JFFS_PATH_NAME0);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

    for (j = 0; j < fileNum; j++) {
        ret = access(g_jffsPathname13[j], F_OK);
        ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT3);
    }

    g_jffsFlag++;

    (void)pthread_mutex_unlock(&g_jffs2GlobalLock1);

    return NULL;
EXIT3:
    mount(JFFS_DEV_PATH0, JFFS_MAIN_DIR0, JFFS_FILESYS_TYPE, 0, NULL);
EXIT2:
    for (j = 0; j < fileNum; j++) {
        close(g_jffsFd11[j]);
    }
EXIT1:
    for (j = 0; j < fileNum; j++) {
        remove(g_jffsPathname11[j]);
        remove(g_jffsPathname12[j]);
        remove(g_jffsPathname13[j]);
    }
EXIT:
    g_TestCnt = 0;
    (void)pthread_mutex_unlock(&g_jffs2GlobalLock1);
    return NULL;
}

static VOID *PthreadF02(void *arg)
{
    INT32 ret;
    INT32 i = 0;
    INT32 j = 0;
    INT32 fileCount = 0;
    UINT64 len;
    CHAR bufname[15] = "01234567890";
    CHAR pathname[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR bufR[JFFS_SHORT_ARRAY_LENGTH] = "abcde";
    CHAR writebuf[JFFS_SHORT_ARRAY_LENGTH] = "a";
    CHAR filebuf[FILE_BUF_SIZE] =
        "abcdeabcde0123456789abcedfghij9876550210abcdeabcde0123456789abcedfghij9876550210abcdeabcde0123"
        "456789abcedfghij9876550210abcdeabcde0123456789abcedfghij9876550210abcdeabcde0123456789abcedfgh"
        "ij9876550210abcdeabcde0123456789abcedfghij9876550210lalalalalalalala";
    CHAR *bufW = NULL;
    off_t off;
    INT32 fileNum = 4;
    INT32 bufWLen = BYTES_PER_KBYTE;

    ret = pthread_mutex_lock(&g_jffs2GlobalLock1);
    printf("[%d] Thread2 first get lock\n", __LINE__);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT2);

    bufW = (CHAR *)malloc(bufWLen + 1);
    ICUNIT_GOTO_NOT_EQUAL(bufW, NULL, 0, EXIT2);
    (void)memset_s(bufW, bufWLen + 1, 0, bufWLen + 1);

    for (j = 0; j < fileNum; j++) {
        strcat_s(bufW, bufWLen + 1, filebuf);
    }

    g_TestCnt++;

    for (j = 0; j < fileNum; j++) {
        len = write(g_jffsFd11[j], bufW, strlen(bufW));
        ICUNIT_GOTO_EQUAL(len, BYTES_PER_KBYTE, len, EXIT3);
    }

    free(bufW);

    (void)pthread_mutex_unlock(&g_jffs2GlobalLock1);

    LosTaskDelay(4); // delay time: 4

    ret = pthread_mutex_lock(&g_jffs2GlobalLock1);
    printf("[%d] Thread2 second get lock\n", __LINE__);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT2);

    for (j = 0; j < fileNum; j++) {
        errno = 0;
        ret = rename(g_jffsPathname11[j], g_jffsPathname12[j]);
        ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT2);
    }

    for (j = 0; j < fileNum; j++) {
        ret = rename(g_jffsPathname12[j], g_jffsPathname13[j]);
        ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT2);
    }

    (void)pthread_mutex_unlock(&g_jffs2GlobalLock1);

    LosTaskDelay(4); // delay time: 4

    ret = pthread_mutex_lock(&g_jffs2GlobalLock1);
    printf("[%d] Thread2 third get lock\n", __LINE__);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT2);

    for (j = 0; j < fileNum; j++) {
        g_jffsFd11[j] = open(g_jffsPathname13[j], O_NONBLOCK | O_RDWR, HIGHEST_AUTHORITY);
        ICUNIT_GOTO_NOT_EQUAL(g_jffsFd11[j], -1, g_jffsFd11[j], EXIT2);
    }

    for (j = 0; j < fileNum; j++) {
        off = lseek(g_jffsFd11[j], 0, SEEK_END);
        ICUNIT_GOTO_EQUAL(off, 1 * 1024, off, EXIT2); // file size: 1 * 1024
    }

    for (j = 0; j < fileNum; j++) {
        ret = close(g_jffsFd11[j]);
        ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT2);

        ret = unlink(g_jffsPathname13[j]);
        ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT1);
    }

    g_jffsFlagF02++;

    (void)pthread_mutex_unlock(&g_jffs2GlobalLock1);

    LosTaskDelay(4); // delay time: 4

    return NULL;
EXIT3:
    free(bufW);
EXIT2:
    for (j = 0; j < fileNum; j++) {
        close(g_jffsFd11[j]);
    }
EXIT1:
    for (j = 0; j < fileNum; j++) {
        remove(g_jffsPathname11[j]);
        remove(g_jffsPathname12[j]);
        remove(g_jffsPathname13[j]);
    }
EXIT:
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
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    ret = chdir(pathname);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    while ((g_jffsFlag < JFFS_PRESSURE_CYCLES) && (g_jffsFlag == g_jffsFlagF01)) {
        g_TestCnt = 0;

        ret = PosixPthreadInit(&attr1, priority);
        ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT1);

        ret = pthread_create(&newTh1, &attr1, PthreadF01, NULL);
        ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);

        ret = PosixPthreadInit(&attr2, priority);
        ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT2);

        ret = pthread_create(&newTh2, &attr2, PthreadF02, NULL);
        ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);

        LosTaskDelay(50); // delay time: 50

        ret = PosixPthreadDestroy(&attr2, newTh2);
        ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT2);

        ret = PosixPthreadDestroy(&attr1, newTh1);
        ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT2);

        dprintf("\tjffs_flag=:%d\t", g_jffsFlag);
    }
    dprintf("\n");
    ICUNIT_GOTO_EQUAL(g_jffsFlag, JFFS_PRESSURE_CYCLES, g_jffsFlag, EXIT2);

    dir = opendir(JFFS_PATH_NAME0);
    ICUNIT_GOTO_NOT_EQUAL(dir, NULL, dir, EXIT3);

    ptr = readdir(dir);
    ICUNIT_GOTO_EQUAL(ptr, NULL, ptr, EXIT3);

    ret = closedir(dir);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT3);

    ret = chdir(JFFS_MAIN_DIR0);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = rmdir(JFFS_PATH_NAME0);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    return LOS_OK;
EXIT3:
    closedir(dir);
    goto EXIT;
EXIT2:
    PosixPthreadDestroy(&attr2, newTh2);
EXIT1:
    PosixPthreadDestroy(&attr1, newTh1);
EXIT:
    rmdir(JFFS_PATH_NAME0);
    return LOS_OK;
}

VOID ItFsJffsPressure041(VOID)
{
    TEST_ADD_CASE("ItFsJffsPressure041", TestCase, TEST_VFS, TEST_JFFS, TEST_LEVEL3, TEST_PRESSURE);
}
