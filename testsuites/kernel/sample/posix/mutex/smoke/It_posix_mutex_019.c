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


static pthread_mutex_t *g_mtx;
static sem_t g_semA, g_semB;
static pthread_mutex_t g_mtxNull, g_mtxDef;

static void *TaskF01(void *arg)
{
    int ret;

    TestBusyTaskDelay(20); // 20, Set the timeout of runtime.

    g_testCount++;

    if ((ret = pthread_mutex_lock(g_mtx))) {
        ICUNIT_GOTO_EQUAL(1, 0, ret, EXIT);
    }

    if ((ret = sem_post(&g_semA))) {
        ICUNIT_GOTO_EQUAL(1, 0, ret, EXIT);
    }

    if ((ret = sem_wait(&g_semB))) {
        ICUNIT_GOTO_EQUAL(1, 0, ret, EXIT);
    }

    if (g_retval != 0) { /* parent thread failed to unlock the mutex) */
        if ((ret = pthread_mutex_unlock(g_mtx))) {
            ICUNIT_GOTO_EQUAL(1, 0, ret, EXIT);
        }
    }

    g_testCount++;

EXIT:
    return NULL;
}

static UINT32 Testcase(VOID)
{
    pthread_mutexattr_t mattr;
    pthread_t thr;

    pthread_mutex_t *tabMutex[2];
    int tabRes[2][3] = { {0, 0, 0}, {0, 0, 0} };

    int ret;
    void *thRet = NULL;

    int i;

    g_retval = 0;

    g_testCount = 0;

    /* We first initialize the two mutexes. */
    if ((ret = pthread_mutex_init(&g_mtxNull, NULL))) {
        ICUNIT_ASSERT_EQUAL(1, 0, ret);
    }

    if ((ret = pthread_mutexattr_init(&mattr))) {
        ICUNIT_ASSERT_EQUAL(1, 0, ret);
    }

    if ((ret = pthread_mutex_init(&g_mtxDef, &mattr))) {
        ICUNIT_ASSERT_EQUAL(1, 0, ret);
    }

    if ((ret = pthread_mutexattr_destroy(&mattr))) {
        ICUNIT_ASSERT_EQUAL(1, 0, ret);
    }

    tabMutex[0] = &g_mtxNull;
    tabMutex[1] = &g_mtxDef;

    if ((ret = sem_init(&g_semA, 0, 0))) {
        ICUNIT_ASSERT_EQUAL(1, 0, ret);
    }

    if ((ret = sem_init(&g_semB, 0, 0))) {
        ICUNIT_ASSERT_EQUAL(1, 0, ret);
    }

    /* OK let's go for the first part of the test : abnormals unlocking */

    /* We first check if unlocking an unlocked mutex returns an uwErr. */
    ret = pthread_mutex_unlock(tabMutex[0]);
    ICUNIT_ASSERT_NOT_EQUAL(ret, ENOERR, ret);

    ret = pthread_mutex_unlock(tabMutex[1]);
    ICUNIT_ASSERT_NOT_EQUAL(ret, ENOERR, ret);

    /* Now we focus on unlocking a mutex lock by another thread */
    for (i = 0; i < 2; i++) { // 2, Set the timeout of runtime
        g_mtx = tabMutex[i];
        tabRes[i][0] = 0;
        tabRes[i][1] = 0;
        tabRes[i][2] = 0; // 2, buffer index.

        if ((ret = pthread_create(&thr, NULL, TaskF01, NULL))) {
            ICUNIT_ASSERT_EQUAL(1, 0, ret);
        }

        if (i == 0) {
            ICUNIT_ASSERT_EQUAL(g_testCount, 0, g_testCount);
        }
        if (i == 1) {
            ICUNIT_ASSERT_EQUAL(g_testCount, 2, g_testCount); // 2, Here, assert the g_testCount.
        }

        if ((ret = sem_wait(&g_semA))) {
            ICUNIT_ASSERT_EQUAL(1, 0, ret);
        }

        g_retval = pthread_mutex_unlock(g_mtx);
        ICUNIT_ASSERT_EQUAL(g_retval, EPERM, g_retval);

        if (i == 0) {
            ICUNIT_ASSERT_EQUAL(g_testCount, 1, g_testCount);
        }
        if (i == 1) {
            ICUNIT_ASSERT_EQUAL(g_testCount, 3, g_testCount); // 3, Here, assert the g_testCount.
        }

        if ((ret = sem_post(&g_semB))) {
            ICUNIT_ASSERT_EQUAL(1, 0, ret);
        }

        if ((ret = pthread_join(thr, &thRet))) {
            ICUNIT_ASSERT_EQUAL(1, 0, ret);
        }

        if (i == 0) {
            ICUNIT_ASSERT_EQUAL(g_testCount, 2, g_testCount); // 2, Here, assert the g_testCount.
        }
        if (i == 1) {
            ICUNIT_ASSERT_EQUAL(g_testCount, 4, g_testCount); // 4, Here, assert the g_testCount.
        }

        tabRes[i][0] = g_retval;
    }

    if (tabRes[0][0] != tabRes[1][0]) {
        ICUNIT_ASSERT_EQUAL(1, 0, LOS_NOK);
    }

    /* We start with testing the NULL mutex features */
    (void)pthread_mutexattr_destroy(&mattr);
    ret = pthread_mutex_destroy(&g_mtxNull);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_mutex_destroy(&g_mtxDef);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = sem_destroy(&g_semA);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = sem_destroy(&g_semB);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    return LOS_OK;
}


VOID ItPosixMux019(void)
{
    TEST_ADD_CASE("ItPosixMux019", Testcase, TEST_POSIX, TEST_MUX, TEST_LEVEL0, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
