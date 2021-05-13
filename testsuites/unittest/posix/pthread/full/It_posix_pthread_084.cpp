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
    pthread_t self = pthread_self();

    rc = pthread_mutex_lock(&g_td.mutex);
    ICUNIT_GOTO_EQUAL(rc, 0, rc, EXIT);

    g_startNum++;

    rc = pthread_cond_wait(&g_td.cond, &g_td.mutex);
    ICUNIT_GOTO_EQUAL(rc, 0, rc, EXIT);

    rc = pthread_mutex_trylock(&g_td.mutex);
    ICUNIT_GOTO_NOT_EQUAL(rc, 0, rc, EXIT);

    g_wakenNum++;

    rc = pthread_mutex_unlock(&g_td.mutex);
    ICUNIT_GOTO_EQUAL(rc, 0, rc, EXIT);

EXIT:
    return NULL;
}
static UINT32 Testcase(VOID)
{
    int i, rc;
    pthread_t thread[THREAD_NUM];

    g_startNum = 0;
    g_wakenNum = 0;

    rc = pthread_mutex_init(&g_td.mutex, NULL);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    rc = pthread_cond_init(&g_td.cond, NULL);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    for (i = 0; i < THREAD_NUM; i++) {
        rc = pthread_create(&thread[i], NULL, pthread_f01, NULL);
        ICUNIT_ASSERT_EQUAL(rc, 0, rc);
    }

    while (g_startNum < THREAD_NUM)
        usleep(1000 * 10 * 2);

    /*
     * Acquire the mutex to make sure that all waiters are currently
     * blocked on pthread_cond_wait
     */
    rc = pthread_mutex_lock(&g_td.mutex);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    rc = pthread_mutex_unlock(&g_td.mutex);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    /* broadcast and check if all waiters are wakened */
    rc = pthread_cond_signal(&g_td.cond);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);
    sleep(1);

    ICUNIT_ASSERT_EQUAL(g_wakenNum, 1, g_wakenNum);

    rc = pthread_cond_signal(&g_td.cond);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    rc = pthread_cond_signal(&g_td.cond);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    rc = pthread_cond_signal(&g_td.cond);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);
    sleep(1);

    ICUNIT_ASSERT_EQUAL(g_wakenNum, THREAD_NUM, g_wakenNum);

    /* join all secondary threads */
    for (i = 0; i < THREAD_NUM; i++) {
        rc = pthread_join(thread[i], NULL);
        ICUNIT_ASSERT_EQUAL(rc, 0, rc);
    }

    rc = pthread_cond_destroy(&g_td.cond);
    ICUNIT_ASSERT_EQUAL(rc, ENOERR, rc);
    return PTHREAD_NO_ERROR;
}

VOID ItPosixPthread084(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("IT_POSIX_PTHREAD_084", Testcase, TEST_POSIX, TEST_PTHREAD, TEST_LEVEL2, TEST_FUNCTION);
}
