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

static pthread_t g_th1;
static pthread_mutex_t g_mutx = PTHREAD_MUTEX_INITIALIZER;
static int g_cnt = 0;
#define PRI 9

static VOID *pthread_f01(VOID *arg)
{
    struct sched_param schParam;
    UINT32 priority = 0;
    INT32 sched = SCHED_RR;
    int ret;

    g_cnt++;
    ICUNIT_GOTO_EQUAL(g_cnt, 1, g_cnt, EXIT);
    pthread_mutex_lock(&g_mutx);
    LosTaskDelay(10);

    pthread_getschedparam(g_th1, &sched, &schParam);
    ICUNIT_GOTO_EQUAL(schParam.sched_priority, PRI, schParam.sched_priority, EXIT);

    priority = schParam.sched_priority - 4;
    schParam.sched_priority = priority;
    ret = pthread_setschedparam(g_th1, sched, &schParam);
    g_cnt++;
    ICUNIT_GOTO_EQUAL(g_cnt, 3, g_cnt, EXIT);
    pthread_mutex_unlock(&g_mutx);
    pthread_getschedparam(g_th1, &sched, &schParam);
    ICUNIT_GOTO_EQUAL(schParam.sched_priority, priority, schParam.sched_priority, EXIT);
EXIT:
    return NULL;
}

static VOID *pthread_f02(VOID *arg)
{
    g_cnt++;
    ICUNIT_GOTO_EQUAL(g_cnt, 2, g_cnt, EXIT);
    pthread_mutex_lock(&g_mutx);

    g_cnt++;
    ICUNIT_GOTO_EQUAL(g_cnt, 4, g_cnt, EXIT);
    pthread_mutex_unlock(&g_mutx);

    return NULL;
EXIT:
    pthread_mutex_unlock(&g_mutx);
    return NULL;
}

static UINT32 Testcase(VOID)
{
    pthread_t newTh;
    UINT32 ret;
    pthread_attr_t attr;
    struct sched_param schParam;
    UINT32 priority = 0;
    INT32 sched = SCHED_RR;
    g_cnt = 0;
    struct sched_param schedparam;

    pthread_attr_init(&attr);
    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    schedparam.sched_priority = PRI;
    pthread_attr_setschedparam(&attr, &schedparam);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    ret = pthread_create(&g_th1, &attr, pthread_f01, NULL);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    pthread_attr_init(&attr);
    schedparam.sched_priority = PRI - 1;
    pthread_attr_setschedparam(&attr, &schedparam);
    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);

    ret = pthread_create(&newTh, &attr, pthread_f02, NULL);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);
    pthread_getschedparam(g_th1, &sched, &schParam);

    ret = pthread_join(newTh, NULL);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);
    ICUNIT_GOTO_EQUAL(g_cnt, 4, g_cnt, EXIT);

    return PTHREAD_NO_ERROR;

EXIT:
    pthread_detach(g_th1);
    pthread_detach(newTh);
    return LOS_NOK;
}

VOID ItPosixPthread116(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("IT_POSIX_PTHREAD_116", Testcase, TEST_POSIX, TEST_PTHREAD, TEST_LEVEL2, TEST_FUNCTION);
}
