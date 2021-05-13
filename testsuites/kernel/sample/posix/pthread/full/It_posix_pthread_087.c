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
    int rc;
    struct timespec timeout;
    struct timeval curtime;

    rc = pthread_mutex_lock(&g_td.mutex);
    ICUNIT_GOTO_EQUAL(rc, 0, rc, EXIT);

    g_t1Start = 1; /* let main thread continue */

    timeout.tv_sec = PTHREAD_TIMEOUT;
    timeout.tv_nsec = 0;

    rc = pthread_cond_timedwait(&g_td.cond, &g_td.mutex, &timeout);
    ICUNIT_GOTO_EQUAL(rc, ETIMEDOUT, rc, EXIT);

    rc = pthread_mutex_trylock(&g_td.mutex);
    ICUNIT_GOTO_NOT_EQUAL(rc, 0, rc, EXIT);
    ICUNIT_GOTO_EQUAL(g_signaled, 1, g_signaled, EXIT);

    rc = pthread_mutex_unlock(&g_td.mutex);
    ICUNIT_GOTO_EQUAL(rc, 0, rc, EXIT);
EXIT:
    pthread_exit((void *)5); // 5, here set value of the exit status.
}

static UINT32 Testcase(VOID)
{
    struct sigaction act;
    int rc;
    pthread_t thread1;
    void *thRet = NULL;

    g_t1Start = 0;

    rc = pthread_mutex_init(&g_td.mutex, NULL);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    rc = pthread_cond_init(&g_td.cond, NULL);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    rc = pthread_create(&thread1, NULL, PthreadF01, NULL);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    while (!g_t1Start) { /* wait for thread1 started */
        usleep(100); // 100, delay for Timing control.
    }

    /* acquire the mutex released by pthread_cond_wait() within thread 1 */
    rc = pthread_mutex_lock(&g_td.mutex);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    rc = pthread_mutex_unlock(&g_td.mutex);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);
    sleep(1);
    g_signaled = 1;
    rc = pthread_join(thread1, &thRet);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);
    ICUNIT_ASSERT_EQUAL((long)(intptr_t)thRet, 5, (long)(intptr_t)thRet); // 5, here assert the result.
    g_signaled = 0;

    rc = pthread_cond_destroy(&g_td.cond);
    ICUNIT_ASSERT_EQUAL(rc, ENOERR, rc);
    return PTHREAD_NO_ERROR;
}

VOID ItPosixPthread087(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("ItPosixPthread087", Testcase, TEST_POSIX, TEST_PTHREAD, TEST_LEVEL2, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */