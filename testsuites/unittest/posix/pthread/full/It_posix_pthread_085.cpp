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

static void *pthread_f01(void *arg)
{
    int rc;

    rc = pthread_mutex_lock(&g_td.mutex);
    ICUNIT_GOTO_EQUAL(rc, 0, rc, EXIT);

    g_t1Start = 1; /* let main thread continue */

    rc = pthread_cond_wait(&g_td.cond, &g_td.mutex);
    ICUNIT_GOTO_EQUAL(rc, 0, rc, EXIT);

    ICUNIT_GOTO_NOT_EQUAL(g_signaled, 0, g_signaled, EXIT);

    rc = pthread_mutex_unlock(&g_td.mutex);
    ICUNIT_GOTO_EQUAL(rc, 0, rc, EXIT);

    g_t1Start = 2;
EXIT:
    return NULL;
}
static UINT32 Testcase(VOID)
{
    struct sigaction act;
    int rc;
    pthread_t thread1;

    g_t1Start = 0;

    rc = pthread_mutex_init(&g_td.mutex, NULL);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    rc = pthread_cond_init(&g_td.cond, NULL);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    rc = pthread_create(&thread1, NULL, pthread_f01, NULL);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    while (!g_t1Start) /* wait for thread1 started */
        usleep(1000 * 10 * 2);

    /* acquire the mutex released by pthread_cond_wait() within thread 1 */
    rc = pthread_mutex_lock(&g_td.mutex);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    rc = pthread_mutex_unlock(&g_td.mutex);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);
    sleep(1);

    fprintf(stderr, "Time to wake up thread1 by signaling a condition\n");
    g_signaled = 1;
    rc = pthread_cond_signal(&g_td.cond);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    rc = pthread_join(thread1, NULL);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    ICUNIT_ASSERT_EQUAL(g_t1Start, 2, g_t1Start);
    g_signaled = 0; // add by d00346846

    rc = pthread_cond_destroy(&g_td.cond);
    ICUNIT_ASSERT_EQUAL(rc, ENOERR, rc);
    return PTHREAD_NO_ERROR;
}

VOID ItPosixPthread085(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("IT_POSIX_PTHREAD_085", Testcase, TEST_POSIX, TEST_PTHREAD, TEST_LEVEL2, TEST_FUNCTION);
}
