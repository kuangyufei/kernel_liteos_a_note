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
    INT32 fd = -1;
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR bufname[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR writebuf[JFFS_STANDARD_NAME_LENGTH] = "0123456789";
    CHAR *writeBuf = NULL;
    UINT32 writeSize = JFFS_PRESSURE_W_R_SIZE1;

    writeBuf = (CHAR *)malloc(writeSize);
    ICUNIT_GOTO_NOT_EQUAL(writeBuf, NULL, writeBuf, EXIT);
    memset_s(writeBuf, writeSize, (0x61 + (INT32)(INTPTR)arg), writeSize);

    fd = open64(g_jffsPathname11[(INT32)(INTPTR)arg], O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY);
    dprintf("(INT32)(INTPTR)arg=:%d,jffs_pathname11=:%s\n", (INT32)(INTPTR)arg, g_jffsPathname11[(INT32)(INTPTR)arg]);
    ICUNIT_GOTO_NOT_EQUAL(fd, JFFS_IS_ERROR, fd, EXIT1);

    LosTaskDelay(3); // delay time: 3

    while (1) {
        ret = write(fd, writeBuf, writeSize);
        if (ret <= 0) {
            break;
        }
    }

    len = write(fd, writebuf, strlen(writebuf));
    ICUNIT_GOTO_EQUAL(len, JFFS_IS_ERROR, len, EXIT1);
    ICUNIT_GOTO_EQUAL(errno, ENOSPC, errno, EXIT1);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    free(writeBuf);

    return NULL;
EXIT1:
    close(fd);
EXIT:
    free(writeBuf);
    return NULL;
}

static UINT32 TestCase(VOID)
{
    INT32 ret, len, i;
    INT32 fd = -1;
    UINT64 toatlSize1, toatlSize2;
    UINT64 freeSize1, freeSize2;
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = { JFFS_MAIN_DIR0 };
    CHAR pathname2[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR bufname[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR readbuf[JFFS_STANDARD_NAME_LENGTH] = "";
    off64_t off64;
    pthread_attr_t attr;
    pthread_t threadId[JFFS_THREAD_NUM_TEST];
    struct statfs statfsbuf1;
    struct statfs statfsbuf2;
    struct stat64 statbuf;

    for (i = 0; i < JFFS_THREAD_NUM_TEST; i++) {
        memset_s(g_jffsPathname11[i], JFFS_NAME_LIMITTED_SIZE, 0, JFFS_NAME_LIMITTED_SIZE);
        memset_s(bufname, JFFS_STANDARD_NAME_LENGTH, 0, JFFS_STANDARD_NAME_LENGTH);
        snprintf_s(bufname, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "_%d", i);
        strcat_s(g_jffsPathname11[i], JFFS_NAME_LIMITTED_SIZE, pathname2);
        strcat_s(g_jffsPathname11[i], JFFS_NAME_LIMITTED_SIZE, bufname);
    }

    g_TestCnt = 0;

    ret = access(pathname1, F_OK);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = statfs(pathname1, &statfsbuf1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    toatlSize1 = (UINT64)statfsbuf1.f_bsize * statfsbuf1.f_blocks;
    freeSize1 = (UINT64)statfsbuf1.f_bsize * statfsbuf1.f_bfree;

    ret = PosixPthreadInit(&attr, TASK_PRIO_TEST2);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    for (i = 0; i < JFFS_THREAD_NUM_TEST; i++) {
        ret = pthread_create(&threadId[i], &attr, PthreadF01, (void *)i);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);
    }

    for (i = 0; i < JFFS_THREAD_NUM_TEST; i++) {
        pthread_join(threadId[i], NULL);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);
    }

    ret = pthread_attr_destroy(&attr);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    for (i = 0; i < JFFS_MAXIMUM_OPERATIONS; i++) {
        ret = chdir("/");
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT3);

        ret = umount(JFFS_MOUNT_DIR0);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT3);

        ret = mount(JFFS_DEV_PATH0, JFFS_MOUNT_DIR0, JFFS_FILESYS_TYPE, 0, NULL);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT3);

        ret = chdir(JFFS_MAIN_DIR0);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

        ret = access(pathname1, F_OK);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);
    }

    for (i = 0; i < JFFS_THREAD_NUM_TEST; i++) {
        fd = open64(g_jffsPathname11[i], O_RDONLY | O_APPEND, HIGHEST_AUTHORITY);
        ICUNIT_GOTO_NOT_EQUAL(fd, JFFS_IS_ERROR, fd, EXIT2);

        ret = stat64(g_jffsPathname11[i], &statbuf);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

        off64 = lseek64(fd, -10, SEEK_END); // seek offset: -10
        ICUNIT_GOTO_EQUAL(off64, statbuf.st_size - 10, off64, EXIT);

        memset_s(readbuf, JFFS_STANDARD_NAME_LENGTH, 0, JFFS_STANDARD_NAME_LENGTH);
        len = read(fd, readbuf, 10); // read length: 10
        ICUNIT_GOTO_EQUAL(len, 10, len, EXIT2);

        ret = close(fd);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

        ret = remove(g_jffsPathname11[i]);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);
    }

    ret = statfs(pathname1, &statfsbuf2);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    toatlSize2 = (UINT64)statfsbuf2.f_bsize * statfsbuf2.f_blocks;
    freeSize2 = (UINT64)statfsbuf2.f_bsize * statfsbuf2.f_bfree;
    ICUNIT_GOTO_EQUAL(toatlSize1, toatlSize2, toatlSize1, EXIT);
    ICUNIT_GOTO_NOT_EQUAL(freeSize1, freeSize2, freeSize1, EXIT);

    return JFFS_NO_ERROR;
EXIT3:
    mount(JFFS_DEV_PATH0, JFFS_MOUNT_DIR0, JFFS_FILESYS_TYPE, 0, NULL);
    goto EXIT;
EXIT2:
    close(fd);
    goto EXIT;
EXIT1:
    for (i = 0; i < JFFS_THREAD_NUM_TEST; i++) {
        pthread_join(threadId[i], NULL);
    }
    pthread_attr_destroy(&attr);
EXIT:
    for (i = 0; i < JFFS_THREAD_NUM_TEST; i++) {
        remove(g_jffsPathname11[i]);
    }
    return JFFS_NO_ERROR;
}

VOID ItFsJffsPressure050(VOID)
{
    TEST_ADD_CASE("ItFsJffsPressure050", TestCase, TEST_VFS, TEST_JFFS, TEST_LEVEL3, TEST_PRESSURE);
}
