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

static void *PthreadF01(void *arg)
{
    INT32 ret;
    INT32 policy;
    struct sched_param schedparam;
    struct sched_param schedparam1;

    g_pthreadSchedPolicy = SCHED_RR;

    schedparam.sched_priority = sched_get_priority_min(g_pthreadSchedPolicy);

    ret = pthread_setschedparam(pthread_self(), g_pthreadSchedPolicy, &schedparam);
    ICUNIT_TRACK_EQUAL(ret, PTHREAD_NO_ERROR, ret);

    policy = SCHED_RR;
    ret = pthread_getschedparam(pthread_self(), &policy, &schedparam1);
    ICUNIT_TRACK_EQUAL(ret, PTHREAD_NO_ERROR, ret);
    ICUNIT_TRACK_EQUAL(policy, g_pthreadSchedPolicy, policy);
    ICUNIT_TRACK_EQUAL(schedparam1.sched_priority, schedparam.sched_priority, schedparam1.sched_priority);

    /* Now set the priority to an invalid value. */
    schedparam.sched_priority++;

    ret = pthread_setschedparam(pthread_self(), SCHED_RR, &schedparam);
    ICUNIT_TRACK_EQUAL(ret, PTHREAD_NO_ERROR, ret);

    policy = SCHED_RR;
    ret = pthread_getschedparam(pthread_self(), &policy, &schedparam1);
    ICUNIT_TRACK_EQUAL(ret, PTHREAD_NO_ERROR, ret);
    ICUNIT_TRACK_EQUAL(policy, g_pthreadSchedPolicy, policy);
    ICUNIT_TRACK_EQUAL(schedparam1.sched_priority, schedparam.sched_priority, schedparam1.sched_priority);

    return NULL;
}

static UINT32 Testcase(VOID)
{
    INT32 ret;
    pthread_attr_t attr;
    pthread_t newTh1;

    ret = pthread_attr_init(&attr);
    ICUNIT_ASSERT_EQUAL(ret, PTHREAD_NO_ERROR, ret);

    g_pthreadSchedInherit = PTHREAD_EXPLICIT_SCHED;
    ret = pthread_attr_setinheritsched(&attr, g_pthreadSchedInherit);
    ICUNIT_ASSERT_EQUAL(ret, PTHREAD_NO_ERROR, ret);

    ret = pthread_create(&newTh1, &attr, PthreadF01, NULL);
    ICUNIT_ASSERT_EQUAL(ret, PTHREAD_NO_ERROR, ret);

    ret = pthread_join(newTh1, NULL);
    ICUNIT_ASSERT_EQUAL(ret, PTHREAD_NO_ERROR, ret);

    ret = pthread_attr_destroy(&attr);
    ICUNIT_ASSERT_EQUAL(ret, PTHREAD_NO_ERROR, ret);

    return PTHREAD_NO_ERROR;
}

VOID ItPosixPthread206(VOID)
{
    TEST_ADD_CASE("ItPosixPthread206", Testcase, TEST_POSIX, TEST_PTHREAD, TEST_LEVEL2, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
