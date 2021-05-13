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

static void PthreadOnceF01(void)
{
    g_testCount++;
}

static void PthreadOnceF02(void)
{
    g_testCount++;
}
static VOID *PthreadF01(void *argument)
{
    UINT32 ret;

    g_testCount++;

    ret = pthread_once(&g_onceControl, PthreadOnceF01);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

EXIT:
    return (void *)9; // 9, here set value about the return status.
}
static UINT32 Testcase(VOID)
{
    pthread_t newTh;
    UINT32 ret;

    g_testCount = 0;
    g_onceControl = PTHREAD_ONCE_INIT;

    ret = pthread_create(&newTh, NULL, PthreadF01, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    LOS_TaskDelay(1);
    ICUNIT_ASSERT_EQUAL(g_testCount, 2, g_testCount); // 2, here assert the result.

    ret = pthread_once(&g_onceControl, PthreadOnceF02);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ICUNIT_ASSERT_EQUAL(g_testCount, 2, g_testCount); // 2, here assert the result.

    ret = pthread_once(&g_onceControl, PthreadOnceF01);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ICUNIT_ASSERT_EQUAL(g_testCount, 2, g_testCount); // 2, here assert the result.

    ret = pthread_join(newTh, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    return PTHREAD_NO_ERROR;
}

VOID ItPosixPthread049(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("ItPosixPthread049", Testcase, TEST_POSIX, TEST_PTHREAD, TEST_LEVEL2, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */