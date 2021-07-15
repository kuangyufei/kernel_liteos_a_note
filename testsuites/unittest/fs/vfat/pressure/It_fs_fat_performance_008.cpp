/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2021 Huawei
 * Device Co., Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source
 * code must retain the above copyright notice, this list of
 * conditions and the following disclaimer.
 *
 * 2.
 * Redistributions in binary form must reproduce the above copyright notice, this list
 * of conditions and the
 * following disclaimer in the documentation and/or other materials
 * provided with the distribution.
 *
 * 3.
 * Neither the name of the copyright holder nor the names of its contributors may be used
 * to endorse or promote
 * products derived from this software without specific prior written
 * permission.
 *
 * THIS SOFTWARE IS PROVIDED
 * BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "It_vfs_fat.h"

static VOID *PthreadF01(void *arg)
{
    INT32 ret;
    CHAR fileName[FAT_STANDARD_NAME_LENGTH] = "";
    time_t tTime;
    struct tm stTime;

    g_testCount++;

    time(&tTime);
    localtime_r(&tTime, &stTime);
    (void)memset_s(g_fatPathname1, FAT_STANDARD_NAME_LENGTH, 0, FAT_STANDARD_NAME_LENGTH);
    strftime(g_fatPathname1, FAT_STANDARD_NAME_LENGTH - 1, "%Y-%m-%d_%H.%M.%S", &stTime);
    (void)snprintf_s(fileName, FAT_STANDARD_NAME_LENGTH - 1, FAT_STANDARD_NAME_LENGTH - 1, "/vs/sd/%s_#%d",
        g_fatPathname1, (INT32)arg);

    (void)snprintf_s(g_fatPathname1, FAT_STANDARD_NAME_LENGTH, FAT_STANDARD_NAME_LENGTH - 1, "testfile#%d", (INT32)arg);
    prctl(PR_SET_NAME, (unsigned long)g_fatPathname1, 0, 0, 0);

    ret = FixWrite(fileName, FILE_SIZE, FIX_DATA_LEN, 1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    ret = FixRead(fileName, FILE_SIZE, FIX_DATA_LEN, 1);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    g_testCount++;

    return FAT_NO_ERROR;

EXIT:
    return FAT_NO_ERROR;
    g_testCount = 0;
}
static UINT32 TestCase(VOID)
{
    INT32 ret, i;
    pthread_t threadId[3];
    pthread_attr_t attr;

    g_testCount = 0;

    ret = PosixPthreadInit(&attr, 4); // level 4
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);

    for (i = 0; i < 3; i++) { // create 3 child threads
        ret = pthread_create(&threadId[i], &attr, PthreadF01, (void *)i);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);
    }

    while (g_testCount < 6) { // 6 threads each add 2 at g_testCount
        sleep(1);
    }

    for (i = 0; i < 3; i++) { // join all 3 child threads
        pthread_join(threadId[i], NULL);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);
    }

    ret = pthread_attr_destroy(&attr);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    return FAT_NO_ERROR;
EXIT1:
    for (i = 0; i < 3; i++) { // join all 3 child threads
        pthread_join(threadId[i], NULL);
    }
EXIT:
    pthread_attr_destroy(&attr);
    return FAT_NO_ERROR;
}

VOID ItFsFatPerformance008(VOID)
{
    TEST_ADD_CASE("IT_FS_FAT_PERFORMANCE_008", TestCase, TEST_VFS, TEST_VFAT, TEST_LEVEL3, TEST_PERFORMANCE);
}
