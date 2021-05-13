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
    UINT32 ret;
    struct sched_param param;
    int policy;

    g_testCount++;

    ret = pthread_getschedparam(g_newTh, &policy, &param);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);
    ICUNIT_GOTO_EQUAL(policy, SCHED_RR, policy, EXIT);
    ICUNIT_GOTO_EQUAL(param.sched_priority, 4, param.sched_priority, EXIT); // 4, here assert the result.

    ICUNIT_GOTO_EQUAL(g_testCount, 1, g_testCount, EXIT);

    param.sched_priority = 26; // 26, set priority.
    ret = pthread_setschedparam(g_newTh, SCHED_RR, &param);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ICUNIT_GOTO_EQUAL(g_testCount, 2, g_testCount, EXIT); // 2, here assert the result.
    g_testCount++;

    ret = pthread_getschedparam(g_newTh, &policy, &param);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);
    ICUNIT_GOTO_EQUAL(policy, SCHED_RR, policy, EXIT);
    ICUNIT_GOTO_EQUAL(param.sched_priority, 26, param.sched_priority, EXIT); // 26, here assert the result.
EXIT:
    return (void *)9; // 9, here set value about the return status.
}
static UINT32 Testcase(VOID)
{
    UINT32 ret;
    pthread_attr_t attr;
    struct sched_param param;
    int policy;

    g_testCount = 0;

    ret = pthread_attr_init(&attr);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_attr_setschedpolicy(&attr, SCHED_RR);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    param.sched_priority = 4; // 4, set priority.
    ret = pthread_attr_setschedparam(&attr, &param);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_create(&g_newTh, &attr, PthreadF01, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_getschedparam(g_newTh, &policy, &param);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ICUNIT_ASSERT_EQUAL(policy, SCHED_RR, policy);
    ICUNIT_ASSERT_EQUAL(param.sched_priority, 26, param.sched_priority); // 26, here assert the result.

    ICUNIT_ASSERT_EQUAL(g_testCount, 1, g_testCount);
    g_testCount++;
    LOS_TaskDelay(1);
    ICUNIT_ASSERT_EQUAL(g_testCount, 3, g_testCount); // 3, here assert the result.

    ret = pthread_join(g_newTh, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_attr_destroy(&attr);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    return PTHREAD_NO_ERROR;
}

VOID ItPosixPthread047(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("ItPosixPthread047", Testcase, TEST_POSIX, TEST_PTHREAD, TEST_LEVEL2, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */