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

static VOID *pthread_f01(VOID *argument)
{
    struct sched_param sparam;
    INT32 priority, policy;
    INT32 ret;

    g_pthreadSchedPolicy = SCHED_RR;

#ifdef LOSCFG_KERNEL_TICKLESS
    priority = sched_get_priority_max(g_pthreadSchedPolicy) - 1;
#else
    priority = sched_get_priority_max(g_pthreadSchedPolicy);
#endif

    sparam.sched_priority = priority;

    ret = pthread_setschedparam(pthread_self(), g_pthreadSchedPolicy, &sparam);
    ICUNIT_TRACK_EQUAL(ret, PTHREAD_NO_ERROR, ret);

    ret = pthread_getschedparam(pthread_self(), &policy, &sparam);
    ICUNIT_TRACK_EQUAL(ret, PTHREAD_NO_ERROR, ret);
    ICUNIT_TRACK_EQUAL(policy, g_pthreadSchedPolicy, policy);
    ICUNIT_TRACK_EQUAL(sparam.sched_priority, priority, sparam.sched_priority);

    pthread_exit(0);

    return NULL;
}


static UINT32 Testcase(VOID)
{
    pthread_t newTh;
    INT32 ret = PTHREAD_NO_ERROR;

    ret = pthread_create(&newTh, NULL, pthread_f01, NULL);
    ICUNIT_ASSERT_EQUAL(ret, PTHREAD_NO_ERROR, ret);

    ret = pthread_join(newTh, NULL);
    ICUNIT_ASSERT_EQUAL(ret, PTHREAD_NO_ERROR, ret);

    return PTHREAD_NO_ERROR;
}



VOID ItPosixPthread204(VOID)
{
    TEST_ADD_CASE("IT_POSIX_PTHREAD_204", Testcase, TEST_POSIX, TEST_PTHREAD, TEST_LEVEL2, TEST_FUNCTION);
}
