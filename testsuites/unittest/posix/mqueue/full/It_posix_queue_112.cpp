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

    LOS_AtomicInc(&g_testCount);

    ret = mq_receive(g_messageQId, msgRcd, MQUEUE_STANDARD_NAME_LENGTH, NULL);
    ICUNIT_GOTO_EQUAL(ret, MSGLEN, ret, EXIT);
    ICUNIT_GOTO_STRING_EQUAL(msgRcd, MQUEUE_SEND_STRING_TEST, msgRcd, EXIT);

    return NULL;
EXIT:
    g_testCount = 0;
    return NULL;
}
static UINT32 Testcase(VOID)
{
    pthread_t threadA;
    pthread_attr_t attrA;
    UINT32 ret;
    struct sched_param sp;

    CHAR qName[MQUEUE_STANDARD_NAME_LENGTH] = "";
    const CHAR *msgPtr = MQUEUE_SEND_STRING_TEST;
    struct mq_attr msgAttr = { 0 };
    msgAttr.mq_msgsize = MQUEUE_STANDARD_NAME_LENGTH;
    msgAttr.mq_maxmsg = MQUEUE_STANDARD_NAME_LENGTH;

    g_testCount = 0;

    snprintf(qName, MQUEUE_STANDARD_NAME_LENGTH, "/mq112_%d", LosCurTaskIDGet());

    g_messageQId = mq_open(qName, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, &msgAttr);
    ICUNIT_GOTO_NOT_EQUAL(g_messageQId, (mqd_t)-1, g_messageQId, EXIT);

    ret = mq_send(g_messageQId, msgPtr, strlen(msgPtr), 0);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT1);

    ret = pthread_attr_init(&attrA);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT2);

    pthread_attr_setinheritsched(&attrA, PTHREAD_EXPLICIT_SCHED);

    sp.sched_priority = MQUEUE_PTHREAD_PRIORITY_TEST2;
    ret = pthread_attr_setschedparam(&attrA, &sp);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT2);

    ret = pthread_create(&threadA, &attrA, PthreadF01, NULL);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT2);

    LosTaskDelay(2); // 2, Set delay time.
    ICUNIT_GOTO_EQUAL(g_testCount, 1, g_testCount, EXIT2);

    ret = pthread_join(threadA, NULL);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT2);

    ret = pthread_attr_destroy(&attrA);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT2);

    ret = mq_close(g_messageQId);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT1);

    ret = mq_unlink(qName);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT1);

    return MQUEUE_NO_ERROR;
EXIT2:
    PosixPthreadDestroy(&attrA, threadA);
EXIT1:
    mq_close(g_messageQId);
    mq_unlink(qName);
EXIT:
    return MQUEUE_NO_ERROR;
}

VOID ItPosixQueue112(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("IT_POSIX_QUEUE_112", Testcase, TEST_POSIX, TEST_QUE, TEST_LEVEL2, TEST_FUNCTION);
}
