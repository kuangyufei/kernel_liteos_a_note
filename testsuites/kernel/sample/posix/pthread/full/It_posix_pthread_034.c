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

#include "It_posix_pthread.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cpluscplus */
#endif /* __cpluscplus */


#define LOOP_NUM 2000

static VOID *PthreadF01(void *argument)
{
    UINT32 i, j;
    ICUNIT_GOTO_EQUAL(g_testCount, 0, g_testCount, EXIT);

    for (i = 0, j = 0; i < LOOP_NUM; i++) {
        j++;
    }

    g_testCount++;
    ICUNIT_GOTO_EQUAL(g_testCount, 1, g_testCount, EXIT);
EXIT:
    return (void *)9; // 9, here set value about the return status.
}

static VOID *PthreadF02(void *argument)
{
    UINT32 i, j;
    ICUNIT_GOTO_EQUAL(g_testCount, 1, g_testCount, EXIT);

    for (i = 0, j = 0; i < LOOP_NUM; i++) {
        j++;
    }

    g_testCount++;
    ICUNIT_GOTO_EQUAL(g_testCount, 2, g_testCount, EXIT); // 2, here assert the result.
EXIT:
    return (void *)8; // 8, here set value about the return status.
}

static VOID *PthreadF03(void *argument)
{
    UINT32 i, j;
    ICUNIT_GOTO_EQUAL(g_testCount, 2, g_testCount, EXIT); // 2, here assert the result.

    for (i = 0, j = 0; i < LOOP_NUM; i++) {
        j++;
    }

    g_testCount++;
    ICUNIT_GOTO_EQUAL(g_testCount, 3, g_testCount, EXIT); // 3, here assert the result.

EXIT:
    return (void *)7; // 7, here set value about the return status.
}

static UINT32 Testcase(VOID)
{
    pthread_t newTh, newTh2, newTh3;

    UINT32 ret;
    UINTPTR temp = 1;

    g_testCount = 0;

    ret = pthread_create(&newTh, NULL, PthreadF01, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_create(&newTh2, NULL, PthreadF02, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_create(&newTh3, NULL, PthreadF03, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    LOS_TaskDelay(1);
    ICUNIT_ASSERT_EQUAL(g_testCount, 3, g_testCount); // 3, here assert the result.

    ret = pthread_join(newTh, (void *)&temp);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_join(newTh2, (void *)&temp);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_join(newTh3, (void *)&temp);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    return PTHREAD_NO_ERROR;
}

VOID ItPosixPthread034(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("ItPosixPthread034", Testcase, TEST_POSIX, TEST_PTHREAD, TEST_LEVEL2, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */