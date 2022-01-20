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

static VOID *pthread_f01(void *argument)
{
    UINT32 ret;
    int oldstate;
    int i = 0;

    ICUNIT_ASSERT_EQUAL_NULL(g_testCount, 1, (void *)g_testCount);
    g_testCount++; // 2
    printf("enter pthread_01\n");
    printf("count: %d\n", g_testCount);
    ret = pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &oldstate);
    ICUNIT_ASSERT_EQUAL_NULL(ret, 0, (void *)ret);
    ICUNIT_ASSERT_EQUAL_NULL(oldstate, PTHREAD_CANCEL_DEFERRED, (void *)oldstate);
    pthread_testcancel();
    while (1) {
        if (++i == 5) {
            sleep(20);
        }
    }
}
static VOID *pthread_f02(void *argument)
{
    UINT32 ret;
    int oldstate;

    ICUNIT_ASSERT_EQUAL_NULL(g_testCount, 3, (void *)g_testCount);
    g_testCount++; // 4
    ret = pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &oldstate);
    ICUNIT_ASSERT_EQUAL_NULL(ret, 0, (void *)ret);
    ICUNIT_ASSERT_EQUAL_NULL(oldstate, PTHREAD_CANCEL_DEFERRED, (void *)oldstate);
    pthread_testcancel();
    LosTaskDelay(20);
    g_testCount++; // 7
    ICUNIT_ASSERT_EQUAL_NULL(g_testCount, 7, (void *)g_testCount);
}
static VOID *PthreadF03(void *argument)
{
    UINT32 ret;
    UINTPTR uwtemp = 1;
    int oldstate;

    ICUNIT_ASSERT_EQUAL_NULL(g_testCount, 5, (void *)g_testCount);
    g_testCount++; // 6
    printf("thread01: %#x", (UINT32)g_newTh);
    pthread_cancel(g_newTh);
    LosTaskDelay(12);

    ret = pthread_join(g_newTh, (void **)&uwtemp);
    ICUNIT_ASSERT_EQUAL_NULL(ret, 0, (void *)ret);
    ICUNIT_ASSERT_EQUAL_NULL(uwtemp, (UINTPTR)PTHREAD_CANCELED, (void *)uwtemp);

    ret = pthread_join(g_newTh2, (void **)&uwtemp);
    ICUNIT_ASSERT_EQUAL_NULL(ret, 0, (void *)ret);
    ICUNIT_ASSERT_EQUAL_NULL(g_testCount, 7, (void *)g_testCount);
    ICUNIT_ASSERT_NOT_EQUAL_NULL(uwtemp, (UINTPTR)PTHREAD_CANCELED, (void *)uwtemp);
}
static UINT32 Testcase(VOID)
{
    pthread_t newTh;
    g_testCount = 0;
    struct sched_param param = { 0 };
    int ret;
    void *res = NULL;
    pthread_attr_t a = { 0 };
    int curThreadPri, curThreadPolicy;

    g_testCount++; // 1

    curThreadPri = param.sched_priority;
    ret = pthread_attr_init(&a);
    param.sched_priority = curThreadPri - 1;

    ret = pthread_attr_setinheritsched(&a, PTHREAD_EXPLICIT_SCHED);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    pthread_attr_setschedparam(&a, &param);
    ret = pthread_create(&g_newTh, &a, pthread_f01, 0);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    LosTaskDelay(10);
    ICUNIT_ASSERT_EQUAL(g_testCount, 2, g_testCount);
    g_testCount++; // 3

    ret = pthread_create(&g_newTh2, &a, pthread_f02, 0);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    LosTaskDelay(10);
    ICUNIT_ASSERT_EQUAL(g_testCount, 4, g_testCount);
    g_testCount++; // 5

    ret = pthread_create(&newTh, &a, PthreadF03, 0);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    LosTaskDelay(20);
    ICUNIT_ASSERT_EQUAL(g_testCount, 7, g_testCount);

    ret = pthread_join(newTh, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    return 0;
}

VOID ItPosixPthread057(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("IT_POSIX_PTHREAD_057", Testcase, TEST_POSIX, TEST_PTHREAD, TEST_LEVEL2, TEST_FUNCTION);
}
