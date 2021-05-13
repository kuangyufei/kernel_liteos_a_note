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

static VOID *PthreadF01(VOID *argument)
{
    TestExtraTaskDelay(1);
    LOS_TaskDelay(1);

    g_testCount = 1;

    pthread_exit(PTHREAD_NO_ERROR);

    return NULL;
}

static UINT32 Testcase(VOID)
{
    pthread_t thread;
    pthread_attr_t attr;
    INT32 ret;
    struct sched_param param;
    INT32 priority;
    _pthread_data *pthreadData = NULL;

    g_testCount = 0;

    ret = pthread_attr_init(&attr);
    ICUNIT_ASSERT_EQUAL(ret, PTHREAD_NO_ERROR, ret);

    g_pthreadSchedPolicy = SCHED_FIFO;
    ret = pthread_attr_setschedpolicy(&attr, g_pthreadSchedPolicy);
    ICUNIT_ASSERT_EQUAL(ret, EINVAL, ret);

    priority = sched_get_priority_max(g_pthreadSchedPolicy);
    ICUNIT_ASSERT_EQUAL(priority, -1, priority);
    ICUNIT_ASSERT_EQUAL(errno, EINVAL, errno);

    param.sched_priority = priority;
    ret = pthread_attr_setschedparam(&attr, &param);
    ICUNIT_ASSERT_EQUAL(ret, ENOTSUP, ret);

    ret = pthread_create(&thread, &attr, PthreadF01, NULL);
    ICUNIT_ASSERT_EQUAL(ret, PTHREAD_NO_ERROR, ret);

    pthreadData = pthread_get_data(thread);
    ICUNIT_ASSERT_EQUAL(pthreadData->state, PTHREAD_STATE_RUNNING, pthreadData->state);
    ICUNIT_ASSERT_EQUAL(pthreadData->attr.schedparam.sched_priority, TASK_PRIO_TEST,
        pthreadData->attr.schedparam.sched_priority);

    ret = pthread_join(thread, NULL);
    ICUNIT_ASSERT_EQUAL(ret, PTHREAD_NO_ERROR, ret);

    ret = pthread_attr_destroy(&attr);
    ICUNIT_ASSERT_EQUAL(ret, PTHREAD_NO_ERROR, ret);
    ICUNIT_ASSERT_EQUAL(g_testCount, 1, g_testCount);

    return PTHREAD_NO_ERROR;
}

VOID ItPosixPthread193(VOID)
{
    TEST_ADD_CASE("ItPosixPthread193", Testcase, TEST_POSIX, TEST_PTHREAD, TEST_LEVEL2, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
