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

static VOID *PthreadF02(void *argument)
{
    _pthread_data *info = NULL;

    TestExtraTaskDelay(2); // 2, delay for Timing control.

    ICUNIT_GOTO_EQUAL(g_testCount, 1, g_testCount, EXIT);
    g_testCount++;

    info = pthread_get_self_data();

    // 3, here assert the result.
    ICUNIT_GOTO_EQUAL(info->attr.schedparam.sched_priority, 3, info->attr.schedparam.sched_priority, EXIT);
    ICUNIT_GOTO_EQUAL(info->attr.schedpolicy, SCHED_RR, info->attr.schedpolicy, EXIT);
    ICUNIT_GOTO_EQUAL(info->attr.scope, PTHREAD_SCOPE_PROCESS, info->attr.scope, EXIT);
    ICUNIT_GOTO_EQUAL(info->task->priority, 3, info->task->priority, EXIT); // 3, here assert the result.
EXIT:
    return (void *)9; // 9, here set value about the return status.
}

static VOID *PthreadF01(void *argument)
{
    pthread_t newTh;
    UINT32 ret;
    pthread_attr_t attr;
    struct sched_param param;
    _pthread_data *info = NULL;

    g_testCount++;

    info = pthread_get_self_data();

    // 4, here assert the result.
    ICUNIT_GOTO_EQUAL(info->attr.schedparam.sched_priority, 4, info->attr.schedparam.sched_priority, EXIT);
    ICUNIT_GOTO_EQUAL(info->attr.schedpolicy, SCHED_RR, info->attr.schedpolicy, EXIT);
    ICUNIT_GOTO_EQUAL(info->attr.scope, PTHREAD_SCOPE_PROCESS, info->attr.scope, EXIT);
    ICUNIT_GOTO_EQUAL(info->task->priority, 4, info->task->priority, EXIT); // 4, here assert the result.

    ret = pthread_attr_init(&attr);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = pthread_attr_setinheritsched(&attr, PTHREAD_INHERIT_SCHED);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = pthread_create(&newTh, &attr, PthreadF02, NULL);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ICUNIT_GOTO_EQUAL(g_testCount, 1, g_testCount, EXIT);
    param.sched_priority = 3; // 3, set priority.
    ret = pthread_setschedparam(newTh, SCHED_RR, &param);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = pthread_detach(newTh);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    TestExtraTaskDelay(4); // 4, delay for Timing control.

    ICUNIT_GOTO_EQUAL(g_testCount, 2, g_testCount, EXIT); // 2, here assert the result.
    g_testCount++;
    ret = pthread_attr_destroy(&attr);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);
EXIT:
    return (void *)9; // 9, here set value about the return status.
}
static UINT32 Testcase(VOID)
{
    pthread_t newTh;
    UINT32 ret;
    pthread_attr_t attr;
    int inherit;
    struct sched_param param;

    g_testCount = 0;

    ret = pthread_attr_init(&attr);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_attr_setschedpolicy(&attr, SCHED_RR); //
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    param.sched_priority = 4; // 4, set priority.
    ret = pthread_attr_setschedparam(&attr, &param);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_attr_setscope(&attr, PTHREAD_SCOPE_PROCESS);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_create(&newTh, &attr, PthreadF01, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    TestExtraTaskDelay(6); // 6, delay for Timing control.

    ICUNIT_ASSERT_EQUAL(g_testCount, 3, g_testCount); // 3, here assert the result.

    ret = pthread_join(newTh, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_attr_getinheritsched(&attr, &inherit);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ICUNIT_ASSERT_EQUAL(inherit, PTHREAD_EXPLICIT_SCHED, inherit);

    ret = pthread_attr_destroy(&attr);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    return PTHREAD_NO_ERROR;
}

VOID ItPosixPthread044(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("ItPosixPthread044", Testcase, TEST_POSIX, TEST_PTHREAD, TEST_LEVEL2, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
