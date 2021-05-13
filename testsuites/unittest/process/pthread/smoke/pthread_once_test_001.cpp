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
#include "it_pthread_test.h"

static int g_number = 0;
static int g_okStatus = 777; // 777, a special number indicate the status is ok.
static pthread_once_t g_onceCtrl = PTHREAD_ONCE_INIT;

static void InitRoutine(void)
{
    g_number++;
}

static void *Threadfunc(void *parm)
{
    int err;
    err = pthread_once(&g_onceCtrl, InitRoutine);
    ICUNIT_GOTO_EQUAL(err, 0, err, EXIT);
    return (void *)g_okStatus;
EXIT:
    return NULL;
}

static void *ThreadFuncTest(void *arg)
{
    pthread_t thread[3];
    int rc = 0;
    int i = 3;
    void *status;
    const int threadsNum = 3;

    g_number = 0;

    for (i = 0; i < threadsNum; ++i) {
        rc = pthread_create(&thread[i], NULL, Threadfunc, NULL);
        ICUNIT_GOTO_EQUAL(rc, 0, rc, EXIT);
    }

    for (i = 0; i < threadsNum; ++i) {
        rc = pthread_join(thread[i], &status);
        ICUNIT_GOTO_EQUAL(rc, 0, rc, EXIT);
        ICUNIT_GOTO_EQUAL((unsigned int)status, (unsigned int)g_okStatus, status, EXIT);
    }

    ICUNIT_GOTO_EQUAL(g_number, 1, g_number, EXIT);

EXIT:
    return NULL;
}

static int Testcase(void)
{
    int ret;
    pthread_t newPthread;
    int curThreadPri, curThreadPolicy;
    pthread_attr_t a = { 0 };
    struct sched_param param = { 0 };

    g_onceCtrl = PTHREAD_ONCE_INIT;

    ret = pthread_getschedparam(pthread_self(), &curThreadPolicy, &param);
    ICUNIT_ASSERT_EQUAL(ret, 0, -ret);

    curThreadPri = param.sched_priority;

    ret = pthread_attr_init(&a);
    pthread_attr_setinheritsched(&a, PTHREAD_EXPLICIT_SCHED);
    param.sched_priority = curThreadPri + 2; // 2, adjust the priority.
    pthread_attr_setschedparam(&a, &param);
    ret = pthread_create(&newPthread, &a, ThreadFuncTest, 0);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_join(newPthread, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    return 0;
}

void ItTestPthreadOnce001(void)
{
    TEST_ADD_CASE("IT_PTHREAD_ONCE_001", Testcase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
