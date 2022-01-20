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

static UINT32 g_index = 0;

static VOID *PthreadF01(VOID *arg)
{
    INT32 i, ret;
    UINT32 uret = 0;
    INT32 count = (INT32)arg;
    struct mq_attr attr = { 0 };

    attr.mq_msgsize = MQUEUE_STANDARD_NAME_LENGTH;
    attr.mq_maxmsg = 5; // 5, queue max message size.

    uret = g_index % count;

    snprintf(g_mqueueName[uret], MQUEUE_STANDARD_NAME_LENGTH, "/mqueue_161_%d_%d", LosCurTaskIDGet(), (uret));
    g_mqueueId[uret] = mq_open(g_mqueueName[uret], O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, &attr);
    ICUNIT_GOTO_NOT_EQUAL(g_mqueueId[uret], (mqd_t)-1, g_mqueueId[uret], EXIT);

    return NULL;

EXIT:
    for (i = 0; i < count; i++) {
        mq_close(g_mqueueId[i]);
        mq_unlink(g_mqueueName[i]);
    }
    return NULL;
}

static VOID *PthreadF02(VOID *arg)
{
    INT32 ret;
    INT32 queueCount = MQUEUE_MAX_NUM_TEST;
    UINT32 i, j, count;
    const CHAR *msgptr = MQUEUE_SEND_STRING_TEST;
    CHAR msgrcd[MQUEUE_STANDARD_NAME_LENGTH] = {0};
    struct timespec ts = { 0 };

    ts.tv_sec = 0xffff;

    for (i = 0; i < 10; i++) { // 10, The loop frequency.
        count = g_index % queueCount;

        for (j = 0; j <= count; j++) {
            ret = mq_timedsend(g_mqueueId[j], msgptr, strlen(msgptr), 0, &ts);
            ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT);

            ret = mq_timedreceive(g_mqueueId[j], msgrcd, MQUEUE_STANDARD_NAME_LENGTH, 0, &ts);
            ICUNIT_GOTO_EQUAL(ret, MQUEUE_SHORT_ARRAY_LENGTH, ret, EXIT);
            ICUNIT_GOTO_STRING_EQUAL(msgrcd, MQUEUE_SEND_STRING_TEST, msgrcd, EXIT);
        }
    }
    return NULL;
EXIT:
    for (i = 0; i < queueCount; i++) {
        mq_close(g_mqueueId[i]);
        mq_unlink(g_mqueueName[i]);
    }
    return NULL;
}

static VOID *PthreadF03(VOID *arg)
{
    INT32 ret, i;
    INT32 count = (INT32)arg;

    for (i = 0; i < count; i++) {
        ret = mq_close(g_mqueueId[i]);
        ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT);

        ret = mq_unlink(g_mqueueName[i]);
        ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT);
    }

    return NULL;
EXIT:
    for (i = 0; i < count; i++) {
        mq_close(g_mqueueId[i]);
        mq_unlink(g_mqueueName[i]);
    }
    return NULL;
}

static UINT32 Testcase(VOID)
{
    UINT32 ret;
    INT32 count = MQUEUE_MAX_NUM_TEST;
    pthread_t threadA;
    pthread_attr_t attrA;
    pthread_t threadB;
    pthread_attr_t attrB;
    pthread_t threadC;
    pthread_attr_t attrC;
    struct sched_param sp;

    ret = pthread_attr_init(&attrA);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT);

    sp.sched_priority = MQUEUE_PTHREAD_PRIORITY_TEST2;
    ret = pthread_attr_setschedparam(&attrA, &sp);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT1);

    ret = pthread_attr_init(&attrB);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT1);

    sp.sched_priority = MQUEUE_PTHREAD_PRIORITY_TEST2;
    ret = pthread_attr_setschedparam(&attrB, &sp);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT2);

    ret = pthread_attr_init(&attrC);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT2);

    sp.sched_priority = MQUEUE_PTHREAD_PRIORITY_TEST2;
    ret = pthread_attr_setschedparam(&attrC, &sp);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT2);

    for (g_index = 0; g_index < count; g_index++) {
        ret = pthread_create(&threadA, &attrA, PthreadF01, (void *)count);
        ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT2);

        ret = pthread_join(threadA, NULL);
        ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT2);

        ret = pthread_create(&threadB, &attrB, PthreadF02, (void *)count);
        ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT2);

        ret = pthread_join(threadB, NULL);
        ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT2);

        if (g_index % count == (count - 1)) {
            ret = pthread_create(&threadC, &attrC, PthreadF03, (void *)count);
            ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT2);

            ret = pthread_join(threadC, NULL);
            ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT2);
        }
    }

    ret = pthread_attr_destroy(&attrC);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT2);

    ret = pthread_attr_destroy(&attrB);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT1);

    ret = pthread_attr_destroy(&attrA);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT);

    return MQUEUE_NO_ERROR;

EXIT2:
    pthread_attr_destroy(&attrC);
EXIT1:
    pthread_attr_destroy(&attrB);
EXIT:
    pthread_attr_destroy(&attrA);
    return MQUEUE_NO_ERROR;
}

VOID ItPosixQueue161(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("IT_POSIX_QUEUE_161", Testcase, TEST_POSIX, TEST_QUE, TEST_LEVEL2, TEST_FUNCTION);
}
