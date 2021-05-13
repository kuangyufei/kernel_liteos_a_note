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

static VOID *PthreadF01(void *argument)
{
    INT32 len;
    INT32 n = 1000;

    g_TestCnt++;
    while (n--) {
        len = write(g_jffsFd,
            "1234567890123456789012345678901234567890abcdefghjk12345678901234567890123456789012345678901234567890123456"
            "7890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890",
            200); // write length: 200
        ICUNIT_GOTO_EQUAL(len, 200, len, EXIT);
        if (g_TestCnt == 2) { // test number: 2
            break;
        }
    }
    g_TestCnt++;

    return (void *)0;

EXIT:
    g_TestCnt = 0;
    return (void *)0;
}

static UINT32 TestCase(VOID)
{
    INT32 ret, len;
    CHAR pathname[50] = { JFFS_PATH_NAME0 };
    pthread_t newTh;
    pthread_attr_t attr;
    CHAR readbuf[201] = {0};
    off_t off;
    INT32 priority = 4;
    INT32 testNum = 2;

    g_TestCnt = 0;
    g_jffsFd = open(pathname, O_CREAT | O_RDWR, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(g_jffsFd, -1, g_jffsFd, EXIT1);

    ret = PosixPthreadInit(&attr, priority);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

    ret = pthread_create(&newTh, &attr, PthreadF01, NULL);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);
    while (g_TestCnt < testNum) {
        LosTaskDelay(1); // delay time: 1
    }
    g_TestCnt++;

    off = lseek(g_jffsFd, 0, SEEK_SET);
    ICUNIT_GOTO_NOT_EQUAL(off, -1, off, EXIT2);

    len = read(g_jffsFd, readbuf, 200); // read length: 200
    ICUNIT_GOTO_EQUAL(len, 200, len, EXIT2);
    ICUNIT_GOTO_STRING_EQUAL(readbuf,
        "1234567890123456789012345678901234567890abcdefghjk123456789012345678901234567890123456789012345678901234567890"
        "123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890",
        readbuf, EXIT2);

    ICUNIT_GOTO_EQUAL(g_TestCnt, 3, g_TestCnt, EXIT2); // 3: test num

    ret = PosixPthreadDestroy(&attr, newTh);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

    ret = close(g_jffsFd);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    ret = unlink(pathname);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    return JFFS_NO_ERROR;
EXIT2:
    PosixPthreadDestroy(&attr, newTh);
EXIT1:
    close(g_jffsFd);
EXIT:
    remove(pathname);
    return JFFS_NO_ERROR;
}

VOID ItFsJffsPressure304(VOID)
{
    TEST_ADD_CASE("ItFsJffsPressure304", TestCase, TEST_VFS, TEST_JFFS, TEST_LEVEL3, TEST_PRESSURE);
}
