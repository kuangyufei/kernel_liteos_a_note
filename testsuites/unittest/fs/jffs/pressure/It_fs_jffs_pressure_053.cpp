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
static CHAR* g_readBuf[3] = {};

static VOID *PthreadF01(void *arg)
{
    INT32 ret, len, taskId;
    INT32 fd = -1;
    INT64 total = 0;
    INT64 perTime;
    INT64 totalSize = 0;
    INT64 totalTime = 0;
    INT32 cycleCount = 0;
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = "";
    UINT32 writeSize = JFFS_PRESSURE_W_R_SIZE1;
    UINT32 readSize = JFFS_PRESSURE_W_R_SIZE1;
    DOUBLE testSpeed;
    CHAR *pid = NULL;
    struct timeval testTime1;
    struct timeval testTime2;
    time_t tTime;
    struct tm *pstTM = NULL;
    INT32 testNum = 4 * 1024; // 4 * 1024: test number

    g_TestCnt++;

    time(&tTime);
    pstTM = localtime(&tTime);
    (void)memset_s(g_jffsPathname4, JFFS_STANDARD_NAME_LENGTH, 0, JFFS_STANDARD_NAME_LENGTH);
    strftime(g_jffsPathname4, JFFS_STANDARD_NAME_LENGTH - 1, "%Y-%m-%d_%H.%M.%S", pstTM);
    snprintf_s(pathname1, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "/storage/%s_#%d", g_jffsPathname4,
        (INT32)(INTPTR)arg);

    snprintf_s(g_jffsPathname4, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "testfile#%d",
        (INT32)(INTPTR)arg);
    prctl(PR_SET_NAME, (unsigned long)g_jffsPathname4, 0, 0, 0);

    taskId = strlen(pathname1);
    pid = pathname1 + taskId - 1;
    taskId = atoi(pid);

    gettimeofday(&testTime1, 0);

    fd = open(pathname1, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        dprintf("\n Task_%d fail to open %s,\n", taskId, pathname1);
        goto EXIT1;
    }

    LosTaskDelay(5); // delay time: 5

    gettimeofday(&testTime2, 0);
    perTime = (testTime2.tv_sec - testTime1.tv_sec) * USECS_PER_SEC + (testTime2.tv_usec - testTime1.tv_usec);
    dprintf("\n TaskID:%3d,open %s,task %lld ms,\n", taskId, pathname1, perTime / MSECS_PER_SEC);

    gettimeofday(&testTime1, 0);

    while (1) {
        len = write(fd, g_writeBuf, writeSize);
        if (len <= 0) {
            if (g_TestCnt < testNum) {
                dprintf("\n TaskID:%3d, The biggest file size is smaller than the 4GB", taskId);
                goto EXIT1;
            }
            dprintf("\n TaskID:%3d, The cycle count = :%d,the file size = :%dMB,= :%0.3lfGB", taskId, g_TestCnt,
                g_TestCnt, g_TestCnt / BYTES_PER_MBYTE);

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

    gettimeofday(&testTime1, 0);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    gettimeofday(&testTime2, 0);
    perTime = (testTime2.tv_sec - testTime1.tv_sec) * USECS_PER_SEC + (testTime2.tv_usec - testTime1.tv_usec);

    dprintf("\n TaskID:%3d,sucess to fclose the %s,task:%lld ms,\n", taskId, pathname1, perTime / MSECS_PER_SEC);

    gettimeofday(&testTime1, 0);

    fd = open(pathname1, O_RDWR, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        dprintf("Task_%d fail to open %s,\n", taskId, pathname1);
        goto EXIT1;
    }

    gettimeofday(&testTime2, 0);

    perTime = (testTime2.tv_sec - testTime1.tv_sec) * USECS_PER_SEC + (testTime2.tv_usec - testTime1.tv_usec);

    dprintf(" fix_Read TaskID:%3d,open %s, task:%lld ms,\n", taskId, pathname1, perTime / MSECS_PER_SEC);

    gettimeofday(&testTime1, 0);

    while (1) {
        ret = read(fd, g_readBuf[(INT32)(INTPTR)arg], readSize);
        if (ret < 0) {
            dprintf("ret = %d,%s read fail!-->(X),\n", ret, pathname1);
            goto EXIT1;
        }
        if (!ret) {
            dprintf("warning: read ret = %d, errno=:%d\n", ret, errno);
            goto EXIT1;
        }
        total += ret;
        totalSize += ret;

        if (total >= JFFS_WR_CAP_SIZE_TEST) {
            gettimeofday(&testTime2, 0);

            perTime = (testTime2.tv_sec - testTime1.tv_sec) * USECS_PER_SEC + (testTime2.tv_usec - testTime1.tv_usec);

            totalTime += perTime;

            testSpeed = total * 1.0;
            testSpeed = testSpeed * USECS_PER_SEC / perTime;
            testSpeed = testSpeed / BYTES_PER_MBYTE;

            dprintf("fix_Read TaskID:%3d,times %d, read %lldbytes, cost %lld usecs, speed: %0.3lfMB/s,\n", taskId,
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
    dprintf("\n fix_Read TaskID:%3d,total read=%lld, time=%lld,arv speed =%0.3lfMB/s,\n", taskId, totalSize, totalTime,
        testSpeed);

    gettimeofday(&testTime1, 0);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    gettimeofday(&testTime2, 0);
    perTime = (testTime2.tv_sec - testTime1.tv_sec) * USECS_PER_SEC + (testTime2.tv_usec - testTime1.tv_usec);

    dprintf("fix_Read TaskID:%3d, fclose %s!,task:%lld ms,\n", taskId, pathname1, perTime / MSECS_PER_SEC);

    return NULL;

EXIT1:
    close(fd);
    remove(pathname1);
    return NULL;
}

static UINT32 TestCase(VOID)
{
    INT32 ret, i, j;
    pthread_t threadId[JFFS_THREAD_NUM_TEST];
    pthread_attr_t attr;
    UINT32 writeSize = JFFS_PRESSURE_W_R_SIZE1;
    UINT32 readSize = JFFS_PRESSURE_W_R_SIZE1;
    CHAR bufname[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    INT32 priority = 4;

    g_TestCnt = 0;

    g_writeBuf = (CHAR *)malloc(writeSize + 1);
    ICUNIT_GOTO_NOT_EQUAL(g_writeBuf, NULL, g_writeBuf, EXIT0);

    for (i = 0; i < JFFS_MIDDLE_CYCLES; i++) {
        g_readBuf[i] = (CHAR *)malloc(readSize + 1);
        ICUNIT_GOTO_NOT_EQUAL(g_readBuf[i], NULL, g_readBuf[i], EXIT0);
    }
    ret = PosixPthreadInit(&attr, priority);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    for (i = 0; i < JFFS_THREAD_NUM_TEST; i++) {
        ret = pthread_create(&threadId[i], &attr, PthreadF01, (void *)i);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);
    }

    while (g_TestCnt < JFFS_THREAD_NUM_TEST * 2) { // thread cnts: JFFS_THREAD_NUM_TEST * 2
        sleep(1);
    }

    for (i = 0; i < JFFS_THREAD_NUM_TEST; i++) {
        pthread_join(threadId[i], NULL);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);
    }

    ret = pthread_attr_destroy(&attr);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    return JFFS_NO_ERROR;
EXIT1:
    for (i = 0; i < JFFS_THREAD_NUM_TEST; i++) {
        pthread_join(threadId[i], NULL);
    }
EXIT:
    pthread_attr_destroy(&attr);
EXIT0:
    free(g_writeBuf);
    g_writeBuf = NULL;
    for (i = 0; i < JFFS_MIDDLE_CYCLES; i++) {
        free(g_readBuf[i]);
        g_readBuf[i] = NULL;
    }
    return JFFS_NO_ERROR;
}

VOID ItFsJffsPressure053(VOID)
{
    TEST_ADD_CASE("ItFsJffsPressure053", TestCase, TEST_VFS, TEST_JFFS, TEST_LEVEL3, TEST_PRESSURE);
}
