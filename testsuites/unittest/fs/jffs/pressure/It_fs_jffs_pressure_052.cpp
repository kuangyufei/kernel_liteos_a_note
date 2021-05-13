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

static CHAR *g_writeBuf = NULL;

static VOID *PthreadF01(void *arg)
{
    INT32 len, taskId;
    INT64 total = 0;
    INT64 perTime;
    INT64 totalSize = 0;
    INT64 totalTime = 0;
    INT32 cycleCount = 0;
    UINT32 writeSize = JFFS_PRESSURE_W_R_SIZE1;
    DOUBLE testSpeed;
    struct timeval testTime1;
    struct timeval testTime2;
    INT32 testNum = 4 * 1024 / 5; // 4 * 1024 / 5: test number

    taskId = (UINT32)(UINTPTR)arg;

    gettimeofday(&testTime1, 0);

    while (1) {
        len = write(g_jffsFd, g_writeBuf, writeSize);
        if (len <= 0) {
            if (g_TestCnt < testNum) {
                dprintf("\n TaskID:%3d, The biggest file size is smaller than the 4GB", taskId);
                goto EXIT;
            }
            dprintf("\n TaskID:%3d, The cycle count = :%d,the file size = :%dMB,= :%0.3lfGB", taskId, g_TestCnt,
                g_TestCnt * 5, (g_TestCnt * 5) * 1.0 / BYTES_PER_KBYTE); // file size per test: 5 MB

            break;
        }
        g_TestCnt++;

        total += len;
        totalSize += len;
        if (total >= JFFS_WR_CAP_SIZE_TEST) {
            gettimeofday(&testTime2, 0);

            perTime = (testTime2.tv_sec - testTime1.tv_sec) * USECS_PER_SEC + (testTime2.tv_usec - testTime1.tv_usec);

            totalTime += perTime;

            testSpeed = total * 1.0;
            testSpeed = testSpeed * USECS_PER_SEC / perTime;
            testSpeed = testSpeed / BYTES_PER_MBYTE;

            dprintf("TaskID:%3d, %d time write, write %lldbytes, cost %lld usecs, speed: %0.3lfMB/s,\n", taskId,
                cycleCount++, total, perTime, testSpeed);

            total = 0;
            gettimeofday(&testTime1, 0);
        }
    }

    gettimeofday(&testTime2, 0);
    perTime = (testTime2.tv_sec - testTime1.tv_sec) * USECS_PER_SEC + (testTime2.tv_usec - testTime1.tv_usec);

    totalTime += perTime;

    testSpeed = totalSize * 1.0;
    testSpeed = testSpeed * USECS_PER_SEC / totalTime;
    testSpeed = testSpeed / BYTES_PER_MBYTE;
    dprintf("\n TaskID:%3d,total write=%lld, time=%lld,arv speed =%0.3lfMB/s,\n", taskId, totalSize, totalTime,
        testSpeed);

    return NULL;
EXIT:
    close(g_jffsFd);
    return NULL;
}

static UINT32 TestCase(VOID)
{
    INT32 ret, len, i, j;
    UINT64 toatlSize1, toatlSize2;
    UINT64 freeSize1, freeSize2;
    UINT32 writeSize = JFFS_PRESSURE_W_R_SIZE1;
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR pathname2[JFFS_STANDARD_NAME_LENGTH] = { JFFS_MAIN_DIR0 };
    CHAR readbuf[JFFS_STANDARD_NAME_LENGTH] = "";
    off64_t off64;
    pthread_t threadId[JFFS_THREAD_NUM_TEST];
    pthread_attr_t attr;
    struct statfs statfsbuf1;
    struct statfs statfsbuf2;
    struct stat64 statbuf;

    g_TestCnt = 0;

    g_writeBuf = (CHAR *)malloc(writeSize);
    ICUNIT_GOTO_NOT_EQUAL(g_writeBuf, NULL, g_writeBuf, EXIT);

    ret = access(pathname1, F_OK);
    ICUNIT_GOTO_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT1);

    g_jffsFd = open(pathname1, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(g_jffsFd, JFFS_IS_ERROR, g_jffsFd, EXIT1);

    ret = statfs(pathname1, &statfsbuf1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

    toatlSize1 = (UINT64)statfsbuf1.f_bsize * statfsbuf1.f_blocks;
    freeSize1 = (UINT64)statfsbuf1.f_bsize * statfsbuf1.f_bfree;

    ret = PosixPthreadInit(&attr, TASK_PRIO_TEST2);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

    for (i = 0; i < JFFS_THREAD_NUM_TEST; i++) {
        ret = pthread_create(&threadId[i], &attr, PthreadF01, (void *)i);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);
    }

    for (j = 0; j < JFFS_THREAD_NUM_TEST; j++) {
        pthread_join(threadId[j], NULL);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);
    }

    ret = pthread_attr_destroy(&attr);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    ret = stat64(pathname1, &statbuf);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    off64 = lseek64(g_jffsFd, 0, SEEK_CUR);
    ICUNIT_GOTO_EQUAL(off64, statbuf.st_size, off64, EXIT1);

    ret = close(g_jffsFd);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    ret = statfs(pathname1, &statfsbuf2);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

    ret = remove(pathname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    toatlSize2 = (UINT64)statfsbuf2.f_bsize * statfsbuf2.f_blocks;
    freeSize2 = (UINT64)statfsbuf2.f_bsize * statfsbuf2.f_bfree;

    ICUNIT_GOTO_EQUAL(toatlSize1, toatlSize2, toatlSize1, EXIT);
    ICUNIT_GOTO_EQUAL(((statfsbuf1.f_bfree - statfsbuf2.f_bfree) >= 0), TRUE, (statfsbuf1.f_bfree - statfsbuf2.f_bfree),
        EXIT);

    free(g_writeBuf);
    g_writeBuf = NULL;

    return JFFS_NO_ERROR;
EXIT2:
    for (j = 0; j < JFFS_THREAD_NUM_TEST; j++) {
        pthread_join(threadId[i], NULL);
    }
    pthread_attr_destroy(&attr);
EXIT1:
    close(g_jffsFd);
    remove(pathname1);
EXIT:
    free(g_writeBuf);
    g_writeBuf = NULL;
    return JFFS_NO_ERROR;
}

VOID ItFsJffsPressure052(VOID)
{
    TEST_ADD_CASE("ItFsJffsPressure052", TestCase, TEST_VFS, TEST_JFFS, TEST_LEVEL3, TEST_PRESSURE);
}
