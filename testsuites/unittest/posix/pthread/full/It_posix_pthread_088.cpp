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
static pthread_mutex_t g_pthreadMutexTest100;
static pthread_cond_t g_pthreadCondTest100;

static void PthreadSignalHandlerF01(int sig)
{
    int rc;

    rc = pthread_cond_signal(&g_pthreadCondTest100);
    ICUNIT_ASSERT_EQUAL_VOID(rc, 0, rc);
}

/* Utility function to find difference between two time values */
static float PthreadTimeF01(struct timespec t2, struct timespec t1)
{
    float diff = t2.tv_sec - t1.tv_sec;
    diff += (t2.tv_nsec - t1.tv_nsec) / 1000000000.0;
    return diff;
}

static void *pthread_f01(void *tmp)
{
    struct sched_param param;
    int policy;
    int rc = 0;

    param.sched_priority = HIGH_PRIORITY;

    rc = pthread_setschedparam(pthread_self(), SCHED_RR, &param);
    ICUNIT_GOTO_EQUAL(rc, 0, rc, EXIT);

    rc = pthread_getschedparam(pthread_self(), &policy, &param);
    ICUNIT_GOTO_EQUAL(rc, 0, rc, EXIT);

    ICUNIT_GOTO_EQUAL(policy, SCHED_RR, policy, EXIT);
    ICUNIT_GOTO_EQUAL(param.sched_priority, HIGH_PRIORITY, param.sched_priority, EXIT);

    rc = pthread_mutex_lock(&g_pthreadMutexTest100);
    ICUNIT_GOTO_EQUAL(rc, 0, rc, EXIT);

    /* Block, to be woken up by the signal handler */
    rc = pthread_cond_wait(&g_pthreadCondTest100, &g_pthreadMutexTest100);
    ICUNIT_GOTO_EQUAL(rc, 0, rc, EXIT);

    /* This variable is unprotected because the scheduling removes
     * the contention
     */
    if (g_lowDone != 1)
        g_wokenUp = 1;

    rc = pthread_mutex_unlock(&g_pthreadMutexTest100);
    ICUNIT_GOTO_EQUAL(rc, 0, rc, EXIT);
EXIT:
    return NULL;
}

static void *pthread_f02(void *tmp)
{
    struct timespec startTime, currentTime;
    struct sched_param param;
    int policy;
    int rc = 0;

    param.sched_priority = LOW_PRIORITY;

    rc = pthread_setschedparam(pthread_self(), SCHED_RR, &param);
    ICUNIT_GOTO_EQUAL(rc, 0, rc, EXIT);

    rc = pthread_getschedparam(pthread_self(), &policy, &param);
    ICUNIT_GOTO_EQUAL(rc, 0, rc, EXIT);

    ICUNIT_GOTO_EQUAL(policy, SCHED_RR, policy, EXIT);
    ICUNIT_GOTO_EQUAL(param.sched_priority, LOW_PRIORITY, param.sched_priority, EXIT);

    PthreadSignalHandlerF01(SIGALRM);

    /* grab the start time and busy loop for 5 seconds */
    clock_gettime(CLOCK_REALTIME, &startTime);
    while (1) {
        clock_gettime(CLOCK_REALTIME, &currentTime);
        if (PthreadTimeF01(currentTime, startTime) > RUNTIME)
            break;
    }
    g_lowDone = 1;
EXIT:
    return NULL;
}
static UINT32 Testcase(VOID)
{
    pthread_t highId, lowId;
    pthread_attr_t highAttr, lowAttr;
    struct sched_param param;
    int rc = 0;
    g_pthreadMutexTest100 = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
    g_pthreadCondTest100 = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
    /* Create the higher priority thread */
    rc = pthread_attr_init(&highAttr);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    rc = pthread_attr_setschedpolicy(&highAttr, SCHED_RR); //
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    pthread_attr_setinheritsched(&highAttr, PTHREAD_EXPLICIT_SCHED);

    param.sched_priority = HIGH_PRIORITY;
    rc = pthread_attr_setschedparam(&highAttr, &param);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    rc = pthread_create(&highId, &highAttr, pthread_f01, NULL);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    /* Create the low priority thread */
    rc = pthread_attr_init(&lowAttr);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    rc = pthread_attr_setschedpolicy(&lowAttr, SCHED_RR); //
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    pthread_attr_setinheritsched(&lowAttr, PTHREAD_EXPLICIT_SCHED);

    param.sched_priority = LOW_PRIORITY;
    rc = pthread_attr_setschedparam(&lowAttr, &param);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    rc = pthread_create(&lowId, &lowAttr, pthread_f02, NULL);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);


    /* Wait for the threads to exit */
    rc = pthread_join(highId, NULL);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    rc = pthread_join(lowId, NULL);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    rc = pthread_cond_destroy(&g_pthreadCondTest100);
    ICUNIT_ASSERT_EQUAL(rc, 0, rc);

    /* Check the result */
    ICUNIT_ASSERT_EQUAL(g_wokenUp, 1, g_wokenUp);
    ICUNIT_ASSERT_EQUAL(g_lowDone, 1, g_lowDone);

    g_iCunitErrLineNo = 0;
    g_iCunitErrCode = 0;

    return PTHREAD_NO_ERROR;
}

VOID ItPosixPthread088(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("IT_POSIX_PTHREAD_088", Testcase, TEST_POSIX, TEST_PTHREAD, TEST_LEVEL2, TEST_FUNCTION);
}
