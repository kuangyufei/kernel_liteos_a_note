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


static pthread_mutex_t g_mutex070;

static VOID *TaskF01(void *arg)
{
    int i;
    UINT32 ret;

    g_testCount = 0;

    ret = pthread_mutex_lock(&g_mutex070);
    ICUNIT_TRACK_EQUAL(ret, 0, ret);

    for (i = 1; i <= 10; i++) { // 10, The loop frequency.
        g_testCount++;
    }

    ret = pthread_mutex_unlock(&g_mutex070);
    ICUNIT_TRACK_EQUAL(ret, 0, ret);

    ICUNIT_TRACK_EQUAL(g_testCount, 10, g_testCount); // 10, Here, assert that g_testCount is equal to 30.

    ret = LOS_EventWrite(&g_eventCB, 0x1);
    ICUNIT_TRACK_EQUAL(ret, 0, ret);

    return NULL;
}

static VOID *TaskF02(void *arg)
{
    int i;
    UINT32 ret;

    ret = pthread_mutex_lock(&g_mutex070);
    ICUNIT_TRACK_EQUAL(ret, 0, ret);

    for (i = 1; i <= 10; i++) { // 10, The loop frequency.
        g_testCount += 2; // 2, Set g_testCount.
    }

    ret = pthread_mutex_unlock(&g_mutex070);
    ICUNIT_TRACK_EQUAL(ret, 0, ret);

    ICUNIT_TRACK_EQUAL(g_testCount, 30, g_testCount); // 30, Here, assert that g_testCount is equal to 30.

    ret = LOS_EventWrite(&g_eventCB, 0x10);
    ICUNIT_TRACK_EQUAL(ret, 0, ret);

    return NULL;
}

static UINT32 Testcase(VOID)
{
    UINT32 ret;
    pthread_t newTh;
    pthread_attr_t attr;
    pthread_t newTh2;
    pthread_attr_t attr2;

    ret = pthread_mutex_init(&g_mutex070, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = LOS_EventInit(&g_eventCB);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    LOS_TaskLock();

    ret = PosixPthreadInit(&attr, 4); // 4, Set thread priority.
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT1);

    ret = pthread_create(&newTh, &attr, TaskF01, NULL);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);

    ret = PosixPthreadInit(&attr2, 4); // 4, Set thread priority.
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);

    ret = pthread_create(&newTh2, &attr2, TaskF02, NULL);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT3);

    LOS_TaskUnlock();

    ret = LOS_EventRead(&g_eventCB, 0x11, LOS_WAITMODE_AND | LOS_WAITMODE_CLR, LOS_WAIT_FOREVER);
    ICUNIT_GOTO_EQUAL(ret, 0x11, ret, EXIT3);

    ret = PosixPthreadDestroy(&attr2, newTh2);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT3);

    ret = PosixPthreadDestroy(&attr, newTh);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);

    ret = LOS_EventDestroy(&g_eventCB);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);

    ret = pthread_mutex_destroy(&g_mutex070);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    return 0;

EXIT3:
    PosixPthreadDestroy(&attr, newTh2);

EXIT2:
    PosixPthreadDestroy(&attr, newTh);

EXIT1:
    LOS_EventDestroy(&g_eventCB);

EXIT:
    pthread_mutex_destroy(&g_mutex070);
    return 0;
}


VOID ItPosixMux070(void)
{
    TEST_ADD_CASE("ItPosixMux070", Testcase, TEST_POSIX, TEST_MUX, TEST_LEVEL2, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */