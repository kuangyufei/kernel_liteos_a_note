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
    INT32 ret;
    CHAR fileName[JFFS_STANDARD_NAME_LENGTH] = "";
    time_t tTime;
    struct tm *pstTM = NULL;

    g_TestCnt++;

    time(&tTime);
    pstTM = localtime(&tTime);
    (void)memset_s(g_jffsPathname1, JFFS_STANDARD_NAME_LENGTH, 0, JFFS_STANDARD_NAME_LENGTH);
    strftime(g_jffsPathname1, JFFS_STANDARD_NAME_LENGTH - 1, "%Y-%m-%d_%H.%M.%S", pstTM);
    snprintf_s(fileName, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "/storage/%s_#%d", g_jffsPathname1,
        (INT32)(INTPTR)arg);

    snprintf_s(g_jffsPathname1, JFFS_STANDARD_NAME_LENGTH, JFFS_STANDARD_NAME_LENGTH - 1, "performance_004_%d",
        (INT32)(INTPTR)arg);
    prctl(PR_SET_NAME, (unsigned long)g_jffsPathname1, 0, 0, 0);

    ret = JffsFixWrite(fileName, (INT64)JFFS_PERFORMANCE_W_R_SIZE, JFFS_PRESSURE_W_R_SIZE1, JFFS_WR_TYPE_TEST2);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = JffsFixRead(fileName, (INT64)JFFS_PERFORMANCE_W_R_SIZE, JFFS_PRESSURE_W_R_SIZE1, JFFS_WR_TYPE_TEST2);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    g_TestCnt++;

    return JFFS_NO_ERROR;

EXIT:
    return JFFS_NO_ERROR;
    g_TestCnt = 0;
}

static UINT32 TestCase(VOID)
{
    INT32 ret;
    pthread_t threadId;
    pthread_attr_t attr;
    INT32 priority = 14;
    INT32 testNum = 2;

    g_TestCnt = 0;

    ret = PosixPthreadInit(&attr, priority);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = pthread_create(&threadId, &attr, PthreadF01, NULL);
    ICUNIT_ASSERT_EQUAL(ret, JFFS_NO_ERROR, ret);

    while (g_TestCnt < testNum) {
        sleep(1);
    }

    ret = pthread_join(threadId, NULL);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = pthread_attr_destroy(&attr);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    return JFFS_NO_ERROR;
EXIT1:
    pthread_join(threadId, NULL);
    pthread_attr_destroy(&attr);

EXIT:
    return JFFS_NO_ERROR;
}

VOID ItFsJffsPerformance004(VOID)
{
    TEST_ADD_CASE("ItFsJffsPerformance004", TestCase, TEST_VFS, TEST_JFFS, TEST_LEVEL3, TEST_PERFORMANCE);
}
