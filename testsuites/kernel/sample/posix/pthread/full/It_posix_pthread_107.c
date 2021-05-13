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

#define LOOP_NUM 2

static VOID *PthreadF01(void *t)
{
    int rc;

    rc = pthread_mutex_lock(&g_pthreadMutexTest1);
    ICUNIT_GOTO_EQUAL(rc, 0, rc, EXIT);

    g_testCount++;
    LOS_TaskDelay(1);
    ICUNIT_GOTO_EQUAL(g_testCount, 2, g_testCount, EXIT); // 2, here assert the result.
    g_testCount++;

    rc = pthread_cond_wait(&g_pthreadCondTest1, &g_pthreadMutexTest1);
    ICUNIT_GOTO_EQUAL(rc, 0, rc, EXIT);

    ICUNIT_GOTO_EQUAL(g_testCount, 5, g_testCount, EXIT); // 5, here assert the result.
    rc = pthread_mutex_unlock(&g_pthreadMutexTest1);
    ICUNIT_GOTO_EQUAL(rc, 0, rc, EXIT);

EXIT:
    return NULL;
}

static VOID *PthreadF02(void *t)
{
    int i;
    int rc;

    ICUNIT_GOTO_EQUAL(g_testCount, 1, g_testCount, EXIT);
    g_testCount++;
    rc = pthread_mutex_lock(&g_pthreadMutexTest1);
    ICUNIT_GOTO_EQUAL(rc, 0, rc, EXIT);

    ICUNIT_GOTO_EQUAL(g_testCount, 3, g_testCount, EXIT); // 3, here assert the result.
    g_testCount++;
    rc = pthread_cond_signal(&g_pthreadCondTest1);
    ICUNIT_GOTO_EQUAL(rc, 0, rc, EXIT);

    ICUNIT_GOTO_EQUAL(g_testCount, 4, g_testCount, EXIT); // 4, here assert the result.
    g_testCount++;

    rc = pthread_mutex_unlock(&g_pthreadMutexTest1); /* Increase latency for thread polling mutex */
    ICUNIT_GOTO_EQUAL(rc, 0, rc, EXIT);
    LOS_TaskDelay(2); // 2, delay for Timing control.

EXIT:
    pthread_exit(NULL);
}
static UINT32 Testcase(VOID)
{
    int i;
    long t1 = 1;
    long t2 = 2; // 2, init
    int rc;
    pthread_t threads[3]; // 3, need 3 pthread for test.
    pthread_attr_t attr; /* Initializes mutex and conditional variable objects */

    g_testCount = 0;
    pthread_mutex_init(&g_pthreadMutexTest1, NULL);
    /* When creating thread, it is set to connectable state, which is easy to transplant */
    pthread_cond_init(&g_pthreadCondTest1, NULL);
    pthread_attr_init(&attr);

    rc = pthread_create(&threads[0], &attr, PthreadF01, (void *)t1);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    rc = pthread_create(&threads[1], &attr, PthreadF02, (void *)t2);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    /* Wait for all threads to complete */
    for (i = 0; i < LOOP_NUM; i++) {
        rc = pthread_join(threads[i], NULL);
        ICUNIT_ASSERT_EQUAL(rc, 0, rc);
    }

    /* Clear and exit */
    rc = pthread_attr_destroy(&attr);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    rc = pthread_mutex_destroy(&g_pthreadMutexTest1);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);
    rc = pthread_cond_destroy(&g_pthreadCondTest1);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    return PTHREAD_NO_ERROR;
}

VOID ItPosixPthread107(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("ItPosixPthread107", Testcase, TEST_POSIX, TEST_PTHREAD, TEST_LEVEL2, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */