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
    CHAR pathname[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR readbuf[JFFS_STANDARD_NAME_LENGTH] = "a";
    CHAR writebuf[JFFS_STANDARD_NAME_LENGTH] = "LLLLLLLLLL";
    off_t off = 0;

    g_TestCnt++;

    len = write(g_jffsFd, writebuf, strlen(writebuf));
    ICUNIT_GOTO_EQUAL(len, strlen(writebuf), len, EXIT);

    off = lseek(g_jffsFd, -10, SEEK_END); // seek offset: -10
    ICUNIT_GOTO_EQUAL(off, 10, off, EXIT); // file position: 10

    (void)memset_s(readbuf, JFFS_STANDARD_NAME_LENGTH, 0, JFFS_STANDARD_NAME_LENGTH);
    len = read(g_jffsFd, readbuf, JFFS_STANDARD_NAME_LENGTH - 1);
    ICUNIT_GOTO_EQUAL(len, strlen(writebuf), len, EXIT);
    ICUNIT_GOTO_STRING_EQUAL(readbuf, writebuf, readbuf, EXIT);

    g_TestCnt++;

    return NULL;

EXIT:
    close(g_jffsFd);
    g_TestCnt++;
    return NULL;
}

static VOID *PthreadF02(void *arg)
{
    INT32 ret, len;
    CHAR pathname[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR readbuf[JFFS_STANDARD_NAME_LENGTH] = "a";
    CHAR writebuf[JFFS_STANDARD_NAME_LENGTH] = "AAAAAAAAAA";
    off_t off = 0;

    g_TestCnt++;

    off = lseek(g_jffsFd, -10, SEEK_END);    // seek offset: -10
    ICUNIT_GOTO_EQUAL(off, 10, off, EXIT); // file position: 10

    (void)memset_s(readbuf, JFFS_STANDARD_NAME_LENGTH, 0, JFFS_STANDARD_NAME_LENGTH);
    len = read(g_jffsFd, readbuf, JFFS_STANDARD_NAME_LENGTH - 1);
    ICUNIT_GOTO_EQUAL(len, 10, len, EXIT); // file size: 10
    ICUNIT_GOTO_STRING_EQUAL(readbuf, "LLLLLLLLLL", readbuf, EXIT);

    len = write(g_jffsFd, writebuf, strlen(writebuf));
    ICUNIT_GOTO_EQUAL(len, strlen(writebuf), len, EXIT);

    off = lseek(g_jffsFd, -10, SEEK_END);    // seek offset: -10
    ICUNIT_GOTO_EQUAL(off, 20, off, EXIT); // file position: 20

    (void)memset_s(readbuf, JFFS_STANDARD_NAME_LENGTH, 0, JFFS_STANDARD_NAME_LENGTH);
    len = read(g_jffsFd, readbuf, JFFS_STANDARD_NAME_LENGTH - 1);
    ICUNIT_GOTO_EQUAL(len, 10, len, EXIT); // file size: 10
    ICUNIT_GOTO_STRING_EQUAL(readbuf, writebuf, readbuf, EXIT);

    g_TestCnt++;

    return NULL;

EXIT:
    close(g_jffsFd);
    g_TestCnt++;
    return NULL;
}

static UINT32 TestCase(VOID)
{
    INT32 ret, len;
    CHAR pathname[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR readbuf[JFFS_STANDARD_NAME_LENGTH] = "a";
    CHAR writebuf[JFFS_STANDARD_NAME_LENGTH] = "0123456789";
    pthread_t newTh1, newTh2;
    pthread_attr_t attr1, attr2;
    INT32 priority = 20;
    INT32 testNum = 4;

    g_jffsFlag = 0;

    ret = access(pathname, F_OK);
    ICUNIT_GOTO_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT);

    while (g_jffsFlag < JFFS_PRESSURE_CYCLES) {
        g_TestCnt = 0;

        g_jffsFd = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY);
        ICUNIT_GOTO_NOT_EQUAL(g_jffsFd, JFFS_IS_ERROR, g_jffsFd, EXIT3);

        len = write(g_jffsFd, writebuf, strlen(writebuf));
        ICUNIT_GOTO_EQUAL(len, strlen(writebuf), len, EXIT3);

        (void)memset_s(readbuf, JFFS_STANDARD_NAME_LENGTH, 0, JFFS_STANDARD_NAME_LENGTH);
        len = read(g_jffsFd, readbuf, JFFS_STANDARD_NAME_LENGTH - 1);
        ICUNIT_GOTO_EQUAL(len, 0, len, EXIT3);

        ret = PosixPthreadInit(&attr1, priority);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

        ret = pthread_create(&newTh1, &attr1, PthreadF01, NULL);
        ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);

        ret = PosixPthreadInit(&attr2, priority);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

        ret = pthread_create(&newTh2, &attr2, PthreadF02, NULL);
        ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);

        while (g_TestCnt != testNum) {
            LosTaskDelay(1); // delay time: 1
        }

        g_jffsFlag++;

        ret = close(g_jffsFd);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT3);

        ret = remove(JFFS_PATH_NAME0);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

        ret = PosixPthreadDestroy(&attr2, newTh2);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

        ret = PosixPthreadDestroy(&attr1, newTh1);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);
        dprintf("\tjffs_flag=:%d\t", g_jffsFlag);
    }
    dprintf("\n");

    ICUNIT_GOTO_EQUAL(g_jffsFlag, JFFS_PRESSURE_CYCLES, g_jffsFlag, EXIT2);

    return JFFS_NO_ERROR;
EXIT3:
    close(g_jffsFd);
    goto EXIT;
EXIT2:
    PosixPthreadDestroy(&attr2, newTh2);
EXIT1:
    PosixPthreadDestroy(&attr1, newTh1);
EXIT:
    remove(JFFS_PATH_NAME0);
    return JFFS_NO_ERROR;
}

VOID ItFsJffsPressure046(VOID)
{
    TEST_ADD_CASE("ItFsJffsPressure046", TestCase, TEST_VFS, TEST_JFFS, TEST_LEVEL3, TEST_PRESSURE);
}
