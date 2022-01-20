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

static VOID *pthread_f02(void *argument)
{
    int oldstate;
    UINT32 ret;

    ICUNIT_GOTO_EQUAL(g_testCount, 1, g_testCount, EXIT);

    ret = pthread_cancel(g_newTh);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    g_testCount++;

    ICUNIT_GOTO_EQUAL(g_testCount, 2, g_testCount, EXIT);
EXIT:
    return (void *)9;
}

static VOID *pthread_f01(void *argument)
{
    int oldstate;
    UINT32 ret;
    pthread_attr_t attr;
    pthread_t newTh;
    struct sched_param schedparam;

    g_testCount++;

    ret = pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &oldstate);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);
    ICUNIT_GOTO_EQUAL(oldstate, PTHREAD_CANCEL_DEFERRED, oldstate, EXIT);

    ret = pthread_attr_init(&attr);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    schedparam.sched_priority = 8;
    pthread_attr_setschedparam(&attr, &schedparam);

    ret = pthread_create(&g_newTh2, &attr, pthread_f02, NULL);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

#ifdef LOSCFG_KERNEL_SMP
    while (g_testCount < 2) {
        TestBusyTaskDelay(1);
    }
    TestExtraTaskDelay(1);
#else
    g_testCount++;

    ret = pthread_join(g_newTh2, NULL);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);
    g_testCount++;

    ICUNIT_GOTO_EQUAL(g_testCount, 4, g_testCount, EXIT);
#endif
EXIT:
    return (void *)9;
}

static UINT32 Testcase(VOID)
{
    UINT32 ret;
    UINTPTR uwtemp = 1;
    pthread_attr_t attr;
    struct sched_param schedparam;

    g_testCount = 0;

    ret = pthread_attr_init(&attr);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    schedparam.sched_priority = 9;
    pthread_attr_setschedparam(&attr, &schedparam);

    ret = pthread_create(&g_newTh, &attr, pthread_f01, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    LosTaskDelay(10);

    ICUNIT_ASSERT_EQUAL(g_testCount, 2, g_testCount);

    ret = pthread_join(g_newTh, (void **)&uwtemp);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ICUNIT_ASSERT_EQUAL(uwtemp, (UINTPTR)PTHREAD_CANCELED, uwtemp);

    ret = pthread_join(g_newTh2, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    return PTHREAD_NO_ERROR;
}

VOID ItPosixPthread066(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("IT_POSIX_PTHREAD_066", Testcase, TEST_POSIX, TEST_PTHREAD, TEST_LEVEL2, TEST_FUNCTION);
}
