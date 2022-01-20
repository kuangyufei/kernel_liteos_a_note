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

static UINT32 Testcase(VOID)
{
    UINT32 ret;

    mqd_t msgQIdA; // Define queue ID for queue A
    mqd_t msgQIdB; // Define queue ID for queue B

    CHAR qNameA[MQUEUE_STANDARD_NAME_LENGTH] = "/mq156_A";
    CHAR qNameB[MQUEUE_STANDARD_NAME_LENGTH] = "/mq156_B";

    const CHAR *msgPtr = MQUEUE_SEND_STRING_TEST;
    CHAR msgRcd[MQUEUE_STANDARD_NAME_LENGTH] = {0};

    struct mq_attr msgAttr = { 0 };
    msgAttr.mq_msgsize = MQUEUE_STANDARD_NAME_LENGTH;
    msgAttr.mq_maxmsg = MQUEUE_STANDARD_NAME_LENGTH;

    msgQIdA = mq_open(qNameA, O_CREAT | O_RDWR | O_NONBLOCK, S_IRUSR | S_IWUSR, &msgAttr);
    ICUNIT_ASSERT_NOT_EQUAL(msgQIdA, (mqd_t)-1, msgQIdA);

    ret = mq_receive(msgQIdA, msgRcd, MQUEUE_STANDARD_NAME_LENGTH, NULL);
    ICUNIT_GOTO_EQUAL(ret, (UINT32)-1, ret, EXIT);

    msgQIdB = mq_open(qNameB, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, &msgAttr);
    ICUNIT_ASSERT_NOT_EQUAL(msgQIdB, (mqd_t)-1, msgQIdB);

    ret = mq_send(msgQIdB, msgPtr, strlen(msgPtr), 0);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT1);

    ret = mq_receive(msgQIdB, msgRcd, MQUEUE_STANDARD_NAME_LENGTH, NULL);
    ICUNIT_GOTO_EQUAL(ret, MSGLEN, ret, EXIT1);
    ICUNIT_GOTO_STRING_EQUAL(msgRcd, MQUEUE_SEND_STRING_TEST, msgRcd, EXIT1);

    ret = mq_close(msgQIdB);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT1);

    ret = mq_unlink(qNameB);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT1);

    ret = mq_close(msgQIdA);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT);

    ret = mq_unlink(qNameA);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT);

    return MQUEUE_NO_ERROR;

EXIT1:
    mq_close(msgQIdB);
    mq_unlink(qNameB);
EXIT:
    mq_close(msgQIdA);
    mq_unlink(qNameA);
    return MQUEUE_NO_ERROR;
}

VOID ItPosixQueue156(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("IT_POSIX_QUEUE_156", Testcase, TEST_POSIX, TEST_QUE, TEST_LEVEL2, TEST_FUNCTION);
}
