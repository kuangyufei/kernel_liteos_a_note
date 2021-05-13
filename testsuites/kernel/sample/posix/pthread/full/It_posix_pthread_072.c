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

static void *PthreadF01(void *tmp)
{
    int rc;
    g_testCount++;

    /* acquire the mutex */
    rc = pthread_mutex_lock(&g_pthreadMutexTest1);
    ICUNIT_GOTO_EQUAL(rc, 0, rc, EXIT);

    g_testCount++;
    /* Wait on the cond var. This will not return, as nobody signals */
    rc = pthread_cond_wait(&g_pthreadCondTest1, &g_pthreadMutexTest1);
    ICUNIT_GOTO_EQUAL(rc, 0, rc, EXIT);

    g_testCount++;

    rc = pthread_mutex_unlock(&g_pthreadMutexTest1);
    ICUNIT_GOTO_EQUAL(rc, 0, rc, EXIT);

    g_testCount++;
EXIT:
    return NULL;
}
static UINT32 Testcase(VOID)
{
    pthread_t lowId;
    int rc;

    g_pthreadMutexTest1 = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
    g_pthreadCondTest1 = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
    g_testCount = 0;

    /* Create a new thread with default attributes */
    rc = pthread_create(&lowId, NULL, PthreadF01, NULL);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    /* Let the other thread run */
    LOS_TaskDelay(2); // 2, delay for Timing control.
    ICUNIT_ASSERT_EQUAL(g_testCount, 2, g_testCount); // 2, here assert the result.

    /* Try to destroy the cond var. This should return an error */
    rc = pthread_cond_destroy(&g_pthreadCondTest1);
    ICUNIT_ASSERT_EQUAL(rc, EBUSY, rc);

    rc = pthread_cond_broadcast(&g_pthreadCondTest1);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    LOS_TaskDelay(2); // 2, delay for Timing control.
    ICUNIT_ASSERT_EQUAL(g_testCount, 4, g_testCount); // 4, here assert the result.

    rc = pthread_cond_destroy(&g_pthreadCondTest1);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    rc = pthread_join(lowId, NULL);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);
    return PTHREAD_NO_ERROR;
}

VOID ItPosixPthread072(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("ItPosixPthread072", Testcase, TEST_POSIX, TEST_PTHREAD, TEST_LEVEL2, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */