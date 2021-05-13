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

static pthread_mutex_t g_pthreadMuxTest1;
static pthread_cond_t g_pthrdCondTest1;
static unsigned int g_pthreadExit = 0;

static void *PthreadF01(void *t)
{
    int rc;
    unsigned int count = 0;
    const int testLoop = 2000;
    while (count < testLoop) {
        rc = pthread_mutex_lock(&g_pthreadMuxTest1);
        ICUNIT_GOTO_EQUAL(rc, 0, rc, EXIT);

        rc = pthread_cond_wait(&g_pthrdCondTest1, &g_pthreadMuxTest1);
        ICUNIT_GOTO_EQUAL(rc, 0, rc, EXIT);

        rc = pthread_mutex_unlock(&g_pthreadMuxTest1);
        ICUNIT_GOTO_EQUAL(rc, 0, rc, EXIT);
        count++;
    }

    g_pthreadExit = 1;
EXIT:
    return NULL;
}

static void *PthreadF02(void *t)
{
    int i;
    int rc;

    while (g_pthreadExit != 1) {
        rc = pthread_mutex_lock(&g_pthreadMuxTest1);
        ICUNIT_GOTO_EQUAL(rc, 0, rc, EXIT);

        rc = pthread_cond_signal(&g_pthrdCondTest1);
        ICUNIT_GOTO_EQUAL(rc, 0, rc, EXIT);

        rc = pthread_mutex_unlock(&g_pthreadMuxTest1);
        ICUNIT_GOTO_EQUAL(rc, 0, rc, EXIT);
    }

EXIT:
    pthread_exit(NULL);
}

static unsigned int TestCase(void)
{
    int i;
    long t1 = 1;
    long t2 = 2;
    int rc;
    pthread_t threads[3];
    pthread_attr_t attr;
    const int loopNum = 2;

    g_pthreadExit = 0;
    g_testCount = 0;
    pthread_mutex_init(&g_pthreadMuxTest1, NULL);
    pthread_cond_init(&g_pthrdCondTest1, NULL);

    rc = pthread_create(&threads[0], NULL, PthreadF01, (void *)t1);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    rc = pthread_create(&threads[1], NULL, PthreadF02, (void *)t2);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    for (i = 0; i < loopNum; i++) {
        rc = pthread_join(threads[i], NULL);
        ICUNIT_ASSERT_EQUAL(rc, 0, rc);
    }

    rc = pthread_attr_destroy(&attr);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    rc = pthread_mutex_destroy(&g_pthreadMuxTest1);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);
    rc = pthread_cond_destroy(&g_pthrdCondTest1);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    return 0;
}

void ItTestPthreadCond002(void)
{
    TEST_ADD_CASE("IT_POSIX_PTHREAD_COND_002", TestCase, TEST_POSIX, TEST_PTHREAD, TEST_LEVEL2, TEST_FUNCTION);
}
