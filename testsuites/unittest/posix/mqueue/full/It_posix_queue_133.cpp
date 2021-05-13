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
    INT32 ret;
    CHAR msgRcd[MQUEUE_STANDARD_NAME_LENGTH] = {0};
    struct timespec rcdTimeout = { 0 };
    rcdTimeout.tv_sec = 0xffff;

    LOS_AtomicInc(&g_testCount);

    ret = mq_timedreceive(g_messageQId, msgRcd, MQUEUE_STANDARD_NAME_LENGTH, NULL, &rcdTimeout);
    ICUNIT_GOTO_EQUAL(ret, MSGLEN, ret, NOK);
    ICUNIT_GOTO_STRING_EQUAL(msgRcd, g_mqueueMsessage[2], msgRcd, NOK); // 2, g_mqueueMsessage buffer index.

    LOS_AtomicInc(&g_testCount);

NOK:
    return NULL;
}

static VOID *PthreadF02(VOID *arg)
{
    INT32 ret;
    CHAR msgRcd[MQUEUE_STANDARD_NAME_LENGTH] = {0};
    struct timespec rcdTimeout = { 0 };
    rcdTimeout.tv_sec = 0xffff;

    LOS_AtomicInc(&g_testCount);

    ret = mq_timedreceive(g_messageQId, msgRcd, MQUEUE_STANDARD_NAME_LENGTH, NULL, &rcdTimeout);
    ICUNIT_GOTO_EQUAL(ret, MSGLEN, ret, NOK);
    ICUNIT_GOTO_STRING_EQUAL(msgRcd, g_mqueueMsessage[1], msgRcd, NOK);

    LOS_AtomicInc(&g_testCount);

NOK:
    return NULL;
}

static UINT32 Testcase(VOID)
{
    UINT32 ret;

    pthread_t threadA;
    pthread_attr_t attrA;
    struct sched_param spA;

    pthread_t threadB;
    pthread_attr_t attrB;
    struct sched_param spB;

    struct mq_attr msgAttr = { 0 };
    CHAR qName[MQUEUE_STANDARD_NAME_LENGTH] = "";

    msgAttr.mq_msgsize = MQUEUE_STANDARD_NAME_LENGTH;
    msgAttr.mq_maxmsg = MQUEUE_STANDARD_NAME_LENGTH;

    g_testCount = 0;

    snprintf(qName, MQUEUE_STANDARD_NAME_LENGTH, "/mq170_%d", LosCurTaskIDGet());

    g_messageQId = mq_open(qName, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, &msgAttr);
    ICUNIT_GOTO_NOT_EQUAL(g_messageQId, (mqd_t)-1, g_messageQId, EXIT1);

    ret = pthread_attr_init(&attrA);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT1);

    spA.sched_priority = MQUEUE_PTHREAD_PRIORITY_TEST2;
    ret = pthread_attr_setschedparam(&attrA, &spA);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT2);

    ret = pthread_create(&threadA, &attrA, PthreadF02, NULL);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT2);

    ret = pthread_attr_init(&attrB);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT3);

    spB.sched_priority = 8; // 8, Queue pthread priority.
    ret = pthread_attr_setschedparam(&attrB, &spB);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT3);

    ret = pthread_create(&threadB, &attrB, PthreadF01, NULL);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT3);

    ret = mq_send(g_messageQId, g_mqueueMsessage[1], MSGLEN, 0);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT3);

    ret = mq_send(g_messageQId, g_mqueueMsessage[2], MSGLEN, 0); // 2, g_mqueueMsessage buffer index.
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT3);

    TestExtraTaskDelay(2); // 2, Set delay time.
    ICUNIT_GOTO_EQUAL(g_testCount, 4, g_testCount, EXIT3); // 4, Here, assert the g_testCount.

    ret = pthread_join(threadB, NULL);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT3);

    ret = pthread_attr_destroy(&attrB);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT3);

    ret = pthread_join(threadA, NULL);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT2);

    ret = pthread_attr_destroy(&attrA);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT2);

    ret = mq_close(g_messageQId);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT1);

    ret = mq_unlink(qName);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT1);

    return MQUEUE_NO_ERROR;
EXIT3:
    PosixPthreadDestroy(&attrB, threadB);
EXIT2:
    PosixPthreadDestroy(&attrA, threadA);
EXIT1:
    mq_close(g_messageQId);
    mq_unlink(qName);
EXIT:
    return MQUEUE_NO_ERROR;
}

VOID ItPosixQueue133(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("IT_POSIX_QUEUE_133", Testcase, TEST_POSIX, TEST_QUE, TEST_LEVEL2, TEST_FUNCTION);
}
