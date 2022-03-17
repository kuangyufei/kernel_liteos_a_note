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
    UINT32 writeSize = JFFS_PRESSURE_W_R_SIZE1;
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR writebuf[JFFS_STANDARD_NAME_LENGTH] = "0123456789";
    CHAR *writeBuf = NULL;
    struct stat64 statbuf1;
    off64_t off64;

    writeBuf = (CHAR *)malloc(writeSize);
    ICUNIT_GOTO_NOT_EQUAL(writeBuf, NULL, writeBuf, EXIT);
    (void)memset_s(writeBuf, writeSize, 0x61, writeSize);

    g_jffsFd = open64(pathname1, O_RDWR, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(g_jffsFd, JFFS_IS_ERROR, g_jffsFd, EXIT1);

    while (1) {
        ret = write(g_jffsFd, writeBuf, writeSize);
        printf("[%d]write_size: %d write ret:%d Fd:%d \n", __LINE__, writeSize, ret, g_jffsFd);
        if (ret <= 0) {
            break;
        }
        g_TestCnt++;
        dprintf("write 5M date %d times\n", g_TestCnt);
    }

    ret = stat64(pathname1, &statbuf1);
    printf("[%d]stat64 ret:%d Fd:%d \n", __LINE__, ret, g_jffsFd);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    off64 = lseek64(g_jffsFd, 0, SEEK_END);
    printf("[%d]lseek64 off64:%lld Fd:%d \n", __LINE__, off64, g_jffsFd);
    ICUNIT_GOTO_EQUAL(statbuf1.st_size, off64, statbuf1.st_size, EXIT1);

    len = write(g_jffsFd, writeBuf, strlen(writeBuf));
    printf("[%d]lseek64 len:%d Fd:%d, errno:%d\n", __LINE__, len, g_jffsFd, errno);
    ICUNIT_GOTO_EQUAL(len, JFFS_IS_ERROR, len, EXIT1);
    ICUNIT_GOTO_EQUAL(errno, ENOSPC, errno, EXIT1);

    off64 = lseek64(g_jffsFd, 0, SEEK_END);
    ICUNIT_GOTO_EQUAL(statbuf1.st_size, off64, statbuf1.st_size, EXIT1);

    off64 = lseek64(g_jffsFd, -10, SEEK_END); // seek offset: -10
    ICUNIT_GOTO_EQUAL(off64, statbuf1.st_size - 10, off64, EXIT1);

    ret = close(g_jffsFd);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    free(writeBuf);

    return NULL;
EXIT1:
    close(g_jffsFd);
EXIT:
    free(writeBuf);
    return NULL;
}

static UINT32 TestCase(VOID)
{
    INT32 ret, len, i;
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR readbuf[JFFS_STANDARD_NAME_LENGTH] = "";
    off64_t off64;
    pthread_t newTh1;
    pthread_attr_t attr1;
    struct stat64 statbuf1;
    struct stat64 statbuf2;

    g_TestCnt = 0;

    ret = access(pathname1, F_OK);
    ICUNIT_GOTO_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT);

    g_jffsFd = open64(pathname1, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(g_jffsFd, JFFS_IS_ERROR, g_jffsFd, EXIT2);

    len = read(g_jffsFd, readbuf, JFFS_STANDARD_NAME_LENGTH - 1);
    ICUNIT_GOTO_EQUAL(len, 0, len, EXIT2);

    ret = close(g_jffsFd);
    ICUNIT_GOTO_EQUAL(len, JFFS_NO_ERROR, len, EXIT2);

    ret = PosixPthreadInit(&attr1, TASK_PRIO_TEST2);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    ret = pthread_create(&newTh1, &attr1, PthreadF01, NULL);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    ret = pthread_join(newTh1, NULL);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    ret = close(g_jffsFd);
    ICUNIT_GOTO_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT2);

    ret = pthread_attr_destroy(&attr1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    ret = stat64(pathname1, &statbuf1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    for (i = 0; i < JFFS_PRESSURE_CYCLES; i++) {
        ret = chdir("/");
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT3);

        errno = 0;
        ret = umount(JFFS_MOUNT_DIR0);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT3);

        ret = mount(JFFS_DEV_PATH0, JFFS_MOUNT_DIR0, JFFS_FILESYS_TYPE, 0, NULL);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT3);

        ret = chdir(JFFS_MAIN_DIR0);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

        ret = access(pathname1, F_OK);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

        ret = stat64(pathname1, &statbuf2);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);
        ICUNIT_GOTO_EQUAL(statbuf2.st_size, statbuf1.st_size, statbuf2.st_size, EXIT);
        dprintf("\tcycle=:%d\t", i);
    }

    g_jffsFd = open64(pathname1, O_RDONLY | O_APPEND, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(g_jffsFd, JFFS_IS_ERROR, g_jffsFd, EXIT2);

    off64 = lseek64(g_jffsFd, -10, SEEK_END); // seek offset: -10
    ICUNIT_GOTO_EQUAL(off64, statbuf1.st_size - 10, off64, EXIT);

    len = read(g_jffsFd, readbuf, 10); // read length: 10
    ICUNIT_GOTO_EQUAL(len, 10, len, EXIT2);

    ret = close(g_jffsFd);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

    ret = remove(pathname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    return JFFS_NO_ERROR;
EXIT3:
    mount(JFFS_DEV_PATH0, JFFS_MOUNT_DIR0, JFFS_FILESYS_TYPE, 0, NULL);
    goto EXIT;
EXIT2:
    close(g_jffsFd);
    goto EXIT;
EXIT1:
    pthread_join(newTh1, NULL);
    pthread_attr_destroy(&attr1);
EXIT:
    remove(pathname1);
    return JFFS_NO_ERROR;
}

VOID ItFsJffsPressure047(VOID)
{
    TEST_ADD_CASE("ItFsJffsPressure047", TestCase, TEST_VFS, TEST_JFFS, TEST_LEVEL3, TEST_PRESSURE);
}
