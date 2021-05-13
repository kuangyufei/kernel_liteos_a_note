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

static VOID *PthreadF01(void *argument)
{
    g_testCount++;
    pthread_exit((void *)8); // 8, here set value about the exit status.

    return argument;
}

static VOID HwiF01(int irq, void *dev)
{
    UINT32 ret;
    int policy;
    struct sched_param param = { 31 };
    struct sched_param param2 = { 2 };
    TEST_HwiClear(HWI_NUM_TEST);
    g_testCount += 1;

    ret = pthread_setschedparam(g_testNewTh, SCHED_RR, &param);
    ICUNIT_ASSERT_EQUAL_VOID(ret, EINTR, ret);

    g_testCount += 1;
    ret = pthread_getschedparam(g_testNewTh, &policy, &param2);
    ICUNIT_ASSERT_EQUAL_VOID(ret, EINTR, ret);
    g_testCount += 1;

    return;
}
static UINT32 Testcase(VOID)
{
    UINT32 ret;
    unsigned long flags;
    UINTPTR temp;
    g_testCount = 0;

    ret = pthread_create(&g_testNewTh, NULL, PthreadF01, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    LOS_TaskDelay(1);
    ICUNIT_ASSERT_EQUAL(g_testCount, 1, g_testCount);

    ret = LOS_HwiCreate(HWI_NUM_TEST, 1, 0, (HWI_PROC_FUNC)HwiF01, 0);
    ICUNIT_GOTO_EQUAL(ret, PTHREAD_NO_ERROR, ret, EXIT);

    TestHwiTrigger(HWI_NUM_TEST);
    ICUNIT_GOTO_EQUAL(g_testCount, 4, g_testCount, EXIT); // 4, here assert the result.

    ret = pthread_join(g_testNewTh, (void *)&temp);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ICUNIT_ASSERT_EQUAL(temp, 8, temp); // 8, here assert the result.

    TEST_HwiDelete(HWI_NUM_TEST);

    return PTHREAD_NO_ERROR;

EXIT:
    return LOS_NOK;
}

VOID ItPosixPthread103(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("ItPosixPthread103", Testcase, TEST_POSIX, TEST_PTHREAD, TEST_LEVEL2, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
