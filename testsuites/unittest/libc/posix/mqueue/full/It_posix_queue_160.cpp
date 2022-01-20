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
#include "It_posix_queue.h"

static VOID *PthreadF01(VOID *arg)
{
    INT32 i, ret;
    INT32 count = MQUEUE_MAX_NUM_TEST;
    struct mq_attr attr = { 0 };

    g_testCount = 0;

    attr.mq_msgsize = MQUEUE_STANDARD_NAME_LENGTH;
    attr.mq_maxmsg = 5; // 5, queue max message size.

    for (i = 0; i < count; i++, g_testCount++) {
        snprintf(g_mqueueName[i], MQUEUE_STANDARD_NAME_LENGTH, "/mq160_%d_%d", LosCurTaskIDGet(), i);
        g_mqueueId[i] = mq_open(g_mqueueName[i], O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, &attr);
        ICUNIT_GOTO_NOT_EQUAL(g_mqueueId[i], (mqd_t)-1, g_mqueueId[i], EXIT);
    }

#ifndef LOSCFG_DEBUG_QUEUE
    snprintf(g_mqueueName[i], MQUEUE_STANDARD_NAME_LENGTH, "/mq160_%d_%d", LosCurTaskIDGet(), i);
    g_mqueueId[i] = mq_open(g_mqueueName[i], O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, &attr);
    ICUNIT_GOTO_EQUAL(g_mqueueId[i], (mqd_t)-1, g_mqueueId[i], EXIT);
#endif

    return NULL;
EXIT:
    for (i = 0; i < g_testCount; i++) {
        mq_close(g_mqueueId[i]);
        mq_unlink(g_mqueueName[i]);
    }
    return NULL;
}

static VOID *PthreadF02(VOID *arg)
{
    INT32 ret;
    UINT32 i;
    const CHAR *msgptr = MQUEUE_SEND_STRING_TEST;
    CHAR msgrcd[MQUEUE_STANDARD_NAME_LENGTH] = {0};
    struct timespec ts = { 0 };

    ts.tv_sec = 0xffff;

    for (i = 0; i < g_testCount; i++) {
        ret = mq_timedsend(g_mqueueId[i % g_testCount], msgptr, strlen(msgptr), 0, &ts);
        ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT);

        ret = mq_timedreceive(g_mqueueId[i % g_testCount], msgrcd, MQUEUE_STANDARD_NAME_LENGTH, 0, &ts);
        ICUNIT_GOTO_EQUAL(ret, MQUEUE_SHORT_ARRAY_LENGTH, ret, EXIT);
        ICUNIT_GOTO_STRING_EQUAL(msgrcd, MQUEUE_SEND_STRING_TEST, msgrcd, EXIT);
    }
    return NULL;
EXIT:
    for (i = 0; i < g_testCount; i++) {
        mq_close(g_mqueueId[i]);
        mq_unlink(g_mqueueName[i]);
    }
    return NULL;
}

static VOID *PthreadF03(VOID *arg)
{
    INT32 ret, i;

    for (i = 0; i < g_testCount; i++) {
        ret = mq_close(g_mqueueId[i]);
        ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT);

        ret = mq_unlink(g_mqueueName[i]);
        ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT);
    }

    return NULL;
EXIT:
    for (i = 0; i < g_testCount; i++) {
        mq_close(g_mqueueId[i]);
        mq_unlink(g_mqueueName[i]);
    }
    return NULL;
}


static UINT32 Testcase(VOID)
{
    INT32 ret, i;
    pthread_t pthread1, pthread2, pthread3;
    pthread_attr_t attr1;
    struct sched_param schedparam;

    g_testCount = 0;

    ret = pthread_attr_init(&attr1);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT);

    schedparam.sched_priority = MQUEUE_PTHREAD_PRIORITY_TEST2;
    ret = pthread_attr_setschedparam(&attr1, &schedparam);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT);

    ret = pthread_attr_init(&attr1);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT1);

    for (i = 0; i < MQUEUE_STANDARD_NAME_LENGTH; i++) {
        ret = pthread_create(&pthread1, &attr1, PthreadF01, NULL);
        ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT1);

        ret = pthread_join(pthread1, NULL);
        ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT1);

        ret = pthread_create(&pthread2, &attr1, PthreadF02, NULL);
        ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT2);

        ret = pthread_join(pthread2, NULL);
        ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT2);

        ret = pthread_create(&pthread3, &attr1, PthreadF03, NULL);
        ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT3);

        ret = pthread_join(pthread3, NULL);
        ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT3);
    }

    ret = pthread_attr_destroy(&attr1);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT);

    return MQUEUE_NO_ERROR;
EXIT3:
    pthread_join(pthread3, NULL);
EXIT2:
    pthread_join(pthread2, NULL);
EXIT1:
    pthread_join(pthread1, NULL);
EXIT:
    pthread_attr_destroy(&attr1);
    return MQUEUE_NO_ERROR;
}

VOID ItPosixQueue160(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("IT_POSIX_QUEUE_160", Testcase, TEST_POSIX, TEST_QUE, TEST_LEVEL2, TEST_FUNCTION);
}
