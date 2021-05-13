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

#include "It_posix_mutex.h"
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cpluscplus */
#endif /* __cpluscplus */


static pthread_mutex_t g_mutex044 = PTHREAD_MUTEX_INITIALIZER;
static int g_t1Start = 0;
static int g_t1Pause = 1;

static void *TaskF01(void *parm)
{
    int rc;
    if ((rc = pthread_mutex_lock(&g_mutex044)) != 0) {
        ICUNIT_GOTO_EQUAL(1, 0, rc, EXIT);
    }

    g_t1Start = 1;

    g_testCount++;

    while (g_t1Pause)
        sleep(1);

    g_testCount++;

    if ((rc = pthread_mutex_unlock(&g_mutex044)) != 0) {
        ICUNIT_GOTO_EQUAL(1, 0, rc, EXIT);
    }
    g_testCount++;

EXIT:
    pthread_exit(0);
    return (void *)(LOS_OK);
}

static UINT32 Testcase(VOID)
{
    int i, rc;
    pthread_t t1;
    g_testCount = 0;

    /* Create a secondary thread and wait until it has locked the mutex */
    rc = pthread_create(&t1, NULL, TaskF01, NULL);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    while (!g_t1Start) {
        sleep(1);
    }

    ICUNIT_ASSERT_EQUAL(g_testCount, 1, g_testCount);

    /* Trylock the mutex and expect it returns EBUSY */
    rc = pthread_mutex_trylock(&g_mutex044);
    if (rc != EBUSY) {
        ICUNIT_ASSERT_EQUAL(1, 0, rc);
    }

    /* Allow the secondary thread to go ahead */
    g_t1Pause = 0;
    /* Trylock the mutex for N times */
    for (i = 0; i < 5; i++) { // 5, The loop frequency.
        rc = pthread_mutex_trylock(&g_mutex044);
        if (rc == 0) {
            pthread_mutex_unlock(&g_mutex044);
            break;
        } else if (rc == EBUSY) {
            sleep(1);
            continue;
        } else {
            ICUNIT_ASSERT_EQUAL(1, 0, rc);
        }
    }

    /* Clean up */
    pthread_join(t1, NULL);
    pthread_mutex_destroy(&g_mutex044);

    if (i >= 5) {  // 5, The loop frequency.
        ICUNIT_ASSERT_EQUAL(1, 0, LOS_NOK);
    }

    g_t1Pause = 1;
    g_t1Start = 0;
    return LOS_OK;
}


VOID ItPosixMux044(void)
{
    TEST_ADD_CASE("ItPosixMux044", Testcase, TEST_POSIX, TEST_MUX, TEST_LEVEL2, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
