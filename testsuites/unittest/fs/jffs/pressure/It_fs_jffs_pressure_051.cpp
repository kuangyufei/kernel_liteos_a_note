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
    INT32 ret, len, i;
    INT32 fd = -1;
    INT64 total = 0;
    INT64 perTime;
    INT64 totalSize = 0;
    INT64 totalTime = 0;
    INT32 cycleCount = 0;
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR writebuf[JFFS_STANDARD_NAME_LENGTH] = "0123456789";
    CHAR *writeBuf = NULL;
    UINT32 writeSize = JFFS_PRESSURE_W_R_SIZE1;
    DOUBLE testSpeed;
    CHAR *pid = NULL;
    struct timeval testTime1;
    struct timeval testTime2;

    writeBuf = (CHAR *)malloc(writeSize);
    ICUNIT_GOTO_NOT_EQUAL(writeBuf, NULL, writeBuf, EXIT);

    gettimeofday(&testTime1, 0);

    fd = open(pathname1, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd, JFFS_IS_ERROR, fd, EXIT1);

    gettimeofday(&testTime2, 0);
    perTime = (testTime2.tv_sec - testTime1.tv_sec) * USECS_PER_SEC + (testTime2.tv_usec - testTime1.tv_usec);
    dprintf("\n open %s,task %lld ms,\n", pathname1, perTime / MSECS_PER_SEC);

    gettimeofday(&testTime1, 0);

    while (1) {
        len = write(fd, writeBuf, writeSize);
        if (len <= 0) {
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

            dprintf("%d time write, write %lldbytes, cost %lld usecs, speed: %0.3lfMB/s,\n", cycleCount++, total,
                perTime, testSpeed);

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
    dprintf("\n total write=%lld, time=%lld,arv speed =%0.3lfMB/s,\n", totalSize, totalTime, testSpeed);

    len = write(fd, writebuf, strlen(writebuf));
    ICUNIT_GOTO_EQUAL(len, JFFS_IS_ERROR, len, EXIT1);
    ICUNIT_GOTO_EQUAL(errno, ENOSPC, errno, EXIT1);

    gettimeofday(&testTime1, 0);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    gettimeofday(&testTime2, 0);
    perTime = (testTime2.tv_sec - testTime1.tv_sec) * USECS_PER_SEC + (testTime2.tv_usec - testTime1.tv_usec);

    dprintf("\n sucess to fclose the %s,task:%lld ms,\n", pathname1, perTime / MSECS_PER_SEC);

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
    INT32 ret;
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    pthread_t newTh1;
    pthread_attr_t attr1;

    g_TestCnt = 0;

    ret = access(pathname1, F_OK);
    ICUNIT_GOTO_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT);

    ret = PosixPthreadInit(&attr1, TASK_PRIO_TEST2);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    ret = pthread_create(&newTh1, &attr1, PthreadF01, NULL);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    ret = pthread_join(newTh1, NULL);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    ret = pthread_attr_destroy(&attr1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    ret = remove(pathname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    return JFFS_NO_ERROR;
EXIT1:
    PosixPthreadDestroy(&attr1, newTh1);
EXIT:
    remove(pathname1);
    return JFFS_NO_ERROR;
}

VOID ItFsJffsPressure051(VOID)
{
    TEST_ADD_CASE("ItFsJffsPressure051", TestCase, TEST_VFS, TEST_JFFS, TEST_LEVEL3, TEST_PRESSURE);
}
