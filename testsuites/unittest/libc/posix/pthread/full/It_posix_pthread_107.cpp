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

static VOID *pthread_f01(void *t)
{
    long myId = (long)t;
    int rc;

    rc = pthread_mutex_lock(&g_pthreadMutexTest1);
    ICUNIT_GOTO_EQUAL(rc, 0, rc, EXIT);

    g_testCount++;
    LosTaskDelay(1);
    ICUNIT_GOTO_EQUAL(g_testCount, 2, g_testCount, EXIT);
    g_testCount++;

    rc = pthread_cond_wait(&g_pthreadCondTest1, &g_pthreadMutexTest1);
    ICUNIT_GOTO_EQUAL(rc, 0, rc, EXIT);

    ICUNIT_GOTO_EQUAL(g_testCount, 5, g_testCount, EXIT);
    rc = pthread_mutex_unlock(&g_pthreadMutexTest1);
    ICUNIT_GOTO_EQUAL(rc, 0, rc, EXIT);


EXIT:
    return NULL;
}

static VOID *pthread_f02(void *t)
{
    int i;
    long myId = (long)t;
    int rc;

    ICUNIT_GOTO_EQUAL(g_testCount, 1, g_testCount, EXIT);
    g_testCount++;
    rc = pthread_mutex_lock(&g_pthreadMutexTest1);
    ICUNIT_GOTO_EQUAL(rc, 0, rc, EXIT);

    ICUNIT_GOTO_EQUAL(g_testCount, 3, g_testCount, EXIT);
    g_testCount++;
    rc = pthread_cond_signal(&g_pthreadCondTest1);
    ICUNIT_GOTO_EQUAL(rc, 0, rc, EXIT);

    ICUNIT_GOTO_EQUAL(g_testCount, 4, g_testCount, EXIT);
    g_testCount++;

    rc = pthread_mutex_unlock(&g_pthreadMutexTest1); /* 为线程轮询互斥锁增加延时 */
    ICUNIT_GOTO_EQUAL(rc, 0, rc, EXIT);
    LosTaskDelay(2);

EXIT:
    pthread_exit(NULL);
}
static UINT32 Testcase(VOID)
{
    int i;
    long t1 = 1, t2 = 2, t3 = 3;
    int rc;
    pthread_t threads[3];
    pthread_attr_t attr; /* 初始化互斥量和条件变量对象 */

    g_testCount = 0;
    pthread_mutex_init(&g_pthreadMutexTest1, NULL);
    pthread_cond_init(&g_pthreadCondTest1, NULL); /* 创建线程时设为可连接状态，便于移植 */
    pthread_attr_init(&attr);

    rc = pthread_create(&threads[0], &attr, pthread_f01, (void *)t1);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    rc = pthread_create(&threads[1], &attr, pthread_f02, (void *)t2);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);


    /* 等待所有线程完成 */
    for (i = 0; i < 2; i++) {
        rc = pthread_join(threads[i], NULL);
        ICUNIT_ASSERT_EQUAL(rc, 0, rc);
    }

    /* 清除并退出 */
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
    TEST_ADD_CASE("IT_POSIX_PTHREAD_107", Testcase, TEST_POSIX, TEST_PTHREAD, TEST_LEVEL2, TEST_FUNCTION);
}
