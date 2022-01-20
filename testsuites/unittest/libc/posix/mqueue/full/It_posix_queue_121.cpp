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
    CHAR msgrcd[MQUEUE_STANDARD_NAME_LENGTH] = {0};
    struct timespec ts = { 0 };

    ts.tv_sec = 0xffff;

    LOS_AtomicInc(&g_testCount);

    ret = mq_timedreceive(g_gqueue, msgrcd, MQUEUE_STANDARD_NAME_LENGTH, NULL, &ts);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_SHORT_ARRAY_LENGTH, ret, EXIT);
    ICUNIT_GOTO_STRING_EQUAL(msgrcd, g_mqueueMsessage[4], msgrcd, EXIT); // 4, g_mqueueMsessage buffer index.

    LOS_AtomicInc(&g_testCount);
    return NULL;
EXIT:
    g_testCount = 0;
    return NULL;
}

static VOID *PthreadF02(VOID *arg)
{
    INT32 ret;
    CHAR msgrcd[MQUEUE_STANDARD_NAME_LENGTH] = {0};
    struct timespec ts = { 0 };

    ts.tv_sec = 0xffff;

    LOS_AtomicInc(&g_testCount);

    ret = mq_timedreceive(g_gqueue, msgrcd, MQUEUE_STANDARD_NAME_LENGTH, NULL, &ts);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_SHORT_ARRAY_LENGTH, ret, EXIT);
    ICUNIT_GOTO_STRING_EQUAL(msgrcd, g_mqueueMsessage[3], msgrcd, EXIT); // 3, g_mqueueMsessage buffer index.

    LOS_AtomicInc(&g_testCount);
    return NULL;
EXIT:
    g_testCount = 0;
    return NULL;
}

static VOID *PthreadF03(VOID *arg)
{
    INT32 ret;
    CHAR msgrcd[MQUEUE_STANDARD_NAME_LENGTH] = {0};

    struct timespec ts = { 0 };
    ts.tv_sec = 0xffff;

    LOS_AtomicInc(&g_testCount);

    ret = mq_timedreceive(g_gqueue, msgrcd, MQUEUE_STANDARD_NAME_LENGTH, NULL, &ts);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_SHORT_ARRAY_LENGTH, ret, EXIT);
    ICUNIT_GOTO_STRING_EQUAL(msgrcd, g_mqueueMsessage[2], msgrcd, EXIT); // 2, g_mqueueMsessage buffer index.

    LOS_AtomicInc(&g_testCount);
    return NULL;
EXIT:
    g_testCount = 0;
    return NULL;
}

static VOID *PthreadF04(VOID *arg)
{
    INT32 ret;
    CHAR msgrcd[MQUEUE_STANDARD_NAME_LENGTH] = {0};
    struct timespec ts = { 0 };

    ts.tv_sec = 0xffff;

    LOS_AtomicInc(&g_testCount);

    ret = mq_timedreceive(g_gqueue, msgrcd, MQUEUE_STANDARD_NAME_LENGTH, NULL, &ts);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_SHORT_ARRAY_LENGTH, ret, EXIT);
    ICUNIT_GOTO_STRING_EQUAL(msgrcd, g_mqueueMsessage[1], msgrcd, EXIT);

    LOS_AtomicInc(&g_testCount);

EXIT:
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

    pthread_t threadC;
    pthread_attr_t attrC;
    struct sched_param spC;

    pthread_t threadD;
    pthread_attr_t attrD;
    struct sched_param spD;

    struct mq_attr msgAttr = { 0 };
    CHAR qName[MQUEUE_STANDARD_NAME_LENGTH] = "";

    msgAttr.mq_msgsize = MQUEUE_STANDARD_NAME_LENGTH;
    msgAttr.mq_maxmsg = MQUEUE_STANDARD_NAME_LENGTH;

    g_testCount = 0;

    snprintf(qName, MQUEUE_STANDARD_NAME_LENGTH, "/mq121_%d", LosCurTaskIDGet());

    g_messageQId = mq_open(qName, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, &msgAttr);
    ICUNIT_GOTO_NOT_EQUAL(g_messageQId, (mqd_t)-1, g_messageQId, EXIT);

    ret = pthread_attr_init(&attrA);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    pthread_attr_setinheritsched(&attrA, PTHREAD_EXPLICIT_SCHED);

    spA.sched_priority = MQUEUE_PTHREAD_PRIORITY_TEST2;
    ret = pthread_attr_setschedparam(&attrA, &spA);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = pthread_create(&threadA, &attrA, PthreadF04, NULL);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);

    ret = pthread_attr_init(&attrB);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);
    pthread_attr_setinheritsched(&attrB, PTHREAD_EXPLICIT_SCHED);
    spB.sched_priority = 2; // 2, Queue pthread priority.
    ret = pthread_attr_setschedparam(&attrB, &spB);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);

    ret = pthread_create(&threadB, &attrB, PthreadF03, NULL);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);

    ret = pthread_attr_init(&attrC);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);

    pthread_attr_setinheritsched(&attrC, PTHREAD_EXPLICIT_SCHED);

    spC.sched_priority = 2; // 2, Queue pthread priority.
    ret = pthread_attr_setschedparam(&attrC, &spC);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT3);

    ret = pthread_create(&threadC, &attrC, PthreadF02, NULL);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT3);

    ret = pthread_attr_init(&attrD);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT3);

    pthread_attr_setinheritsched(&attrD, PTHREAD_EXPLICIT_SCHED);

    spD.sched_priority = 1;
    ret = pthread_attr_setschedparam(&attrD, &spD);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT4);

    ret = pthread_create(&threadD, &attrD, PthreadF01, NULL);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT4);

    ret = mq_send(g_messageQId, g_mqueueMsessage[1], MSGLEN, 0);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT4);

    ret = mq_send(g_messageQId, g_mqueueMsessage[2], MSGLEN, 0); // 2, g_mqueueMsessage buffer index.
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT4);

    ret = mq_send(g_messageQId, g_mqueueMsessage[3], MSGLEN, 0); // 3, g_mqueueMsessage buffer index.
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT4);

    ret = mq_send(g_messageQId, g_mqueueMsessage[4], MSGLEN, 0); // 4, g_mqueueMsessage buffer index.
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT4);

    TestExtraTaskDelay(3); // 3, Set delay time.
    ICUNIT_GOTO_EQUAL(g_testCount, 8, g_testCount, EXIT4); // 8, Here, assert the g_testCount.

    ret = pthread_join(threadD, NULL);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT4);
    ret = pthread_attr_destroy(&attrD);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT4);

    ret = pthread_join(threadC, NULL);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT3);
    ret = pthread_attr_destroy(&attrC);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT3);

    ret = pthread_join(threadB, NULL);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);
    ret = pthread_attr_destroy(&attrB);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);

    ret = pthread_join(threadA, NULL);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT4);
    ret = pthread_attr_destroy(&attrA);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT4);

    ret = mq_close(g_messageQId);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = mq_unlink(qName);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);
    return MQUEUE_NO_ERROR;

EXIT4:
    pthread_join(threadD, NULL);
    pthread_attr_destroy(&attrD);
EXIT3:
    pthread_join(threadC, NULL);
    pthread_attr_destroy(&attrC);
EXIT2:
    pthread_join(threadB, NULL);
    pthread_attr_destroy(&attrA);
EXIT1:
    pthread_join(threadB, NULL);
    pthread_attr_destroy(&attrA);
EXIT:
    mq_close(g_messageQId);
    mq_unlink(qName);
    return MQUEUE_NO_ERROR;
}

VOID ItPosixQueue121(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("IT_POSIX_QUEUE_121", Testcase, TEST_POSIX, TEST_QUE, TEST_LEVEL2, TEST_FUNCTION);
}
