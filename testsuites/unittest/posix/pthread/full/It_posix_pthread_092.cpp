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

#define THREAD_COUNT 15
static int g_testCnt;

static void *pthread_f01(void *arg)
{
    int rc;
    pthread_t self = pthread_self();

    rc = pthread_mutex_lock(&g_td.mutex);
    ICUNIT_GOTO_EQUAL(rc, 0, rc, EXIT);

    g_startNum++;
    if (g_startNum > 5)
        usleep(1000 * 10 * 2);

    printf("pthread start_num: %d \n", g_startNum);
    rc = pthread_cond_wait(&g_td.cond, &g_td.mutex);
    ICUNIT_GOTO_EQUAL(rc, 0, rc, EXIT);

    g_wakenNum++;

    printf("pthread waken_num: %d \n", g_wakenNum);
    rc = pthread_mutex_unlock(&g_td.mutex);
    ICUNIT_GOTO_EQUAL(rc, 0, rc, EXIT);

EXIT:
    return NULL;
}

static void *pthread_f02(void *arg)
{
    int rc;
    pthread_t self = pthread_self();

    g_testCnt++;
    rc = pthread_mutex_lock(&g_td.mutex);
    ICUNIT_GOTO_EQUAL(rc, 0, rc, EXIT);

    rc = pthread_mutex_unlock(&g_td.mutex);
    ICUNIT_GOTO_EQUAL(rc, 0, rc, EXIT);

EXIT:
    return NULL;
}

static UINT32 Testcase(VOID)
{
    int i, rc;
    g_startNum = 0;
    g_wakenNum = 0;
    pthread_t thread[THREAD_COUNT];
    pthread_t thread2[THREAD_COUNT];

    rc = pthread_mutex_init(&g_td.mutex, NULL);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    rc = pthread_cond_init(&g_td.cond, NULL);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    rc = pthread_mutex_lock(&g_td.mutex);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    g_testCnt = 0;
    for (i = 0; i < THREAD_COUNT; i++) {
        rc = pthread_create(&thread[i], NULL, pthread_f02, NULL);
        ICUNIT_ASSERT_EQUAL(rc, 0, rc);
        usleep(1000 * 10 * 2);
    }

    ICUNIT_ASSERT_EQUAL(g_testCnt, THREAD_COUNT, g_testCnt);

    for (i = 0; i < THREAD_COUNT; i++) {
        rc = pthread_create(&thread2[i], NULL, pthread_f01, NULL);
        ICUNIT_ASSERT_EQUAL(rc, 0, rc);
        usleep(1000 * 10 * 2);
    }

    rc = pthread_mutex_unlock(&g_td.mutex);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    usleep(1000 * 10 * 1);

    printf("%s %d pthread_cond_broadcast \n", __FUNCTION__, __LINE__);

    /* broadcast and check if all waiters are wakened */
    rc = pthread_cond_broadcast(&g_td.cond);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    sleep(1);

    printf("%s %d pthread_cond_broadcast two\n", __FUNCTION__, __LINE__);
    rc = pthread_cond_broadcast(&g_td.cond);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    for (i = 0; i < THREAD_COUNT; i++) {
        rc = pthread_join(thread[i], NULL);
        ICUNIT_ASSERT_EQUAL(rc, 0, rc);
    }

    /* join all secondary threads */
    for (i = 0; i < THREAD_COUNT; i++) {
        rc = pthread_join(thread2[i], NULL);
        ICUNIT_ASSERT_EQUAL(rc, 0, rc);
    }

    printf("start_num: %d", g_startNum);
    rc = pthread_cond_destroy(&g_td.cond);
    ICUNIT_ASSERT_EQUAL(rc, ENOERR, rc);

    return PTHREAD_NO_ERROR;
}


VOID ItPosixPthread092(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("IT_POSIX_PTHREAD_092", Testcase, TEST_POSIX, TEST_PTHREAD, TEST_LEVEL2, TEST_FUNCTION);
}
