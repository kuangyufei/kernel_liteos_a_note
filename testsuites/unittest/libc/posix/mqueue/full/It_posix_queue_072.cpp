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

    CHAR qName[MQUEUE_STANDARD_NAME_LENGTH] = "";
    const CHAR *msgPtr = MQUEUE_SEND_STRING_TEST;
    struct mq_attr msgAttr = { 0 };
    CHAR msgRcd[MQUEUE_STANDARD_NAME_LENGTH] = {0};
    mqd_t msgQId;

    msgAttr.mq_msgsize = MQUEUE_STANDARD_NAME_LENGTH;
    msgAttr.mq_maxmsg = MQUEUE_STANDARD_NAME_LENGTH;

    snprintf(qName, MQUEUE_STANDARD_NAME_LENGTH, "/mq072_%d", LosCurTaskIDGet());

    msgQId = mq_open(qName, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, &msgAttr);
    ICUNIT_GOTO_NOT_EQUAL(msgQId, (mqd_t)-1, msgQId, EXIT);

    ret = mq_send(msgQId, msgPtr, strlen(msgPtr), 0);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT2);

    ret = mq_receive(msgQId, msgRcd, MQUEUE_STANDARD_NAME_LENGTH, NULL);
    ICUNIT_GOTO_EQUAL(ret, MSGLEN, ret, EXIT2);
    ICUNIT_GOTO_STRING_EQUAL(msgRcd, MQUEUE_SEND_STRING_TEST, msgRcd, EXIT2);

    ret = mq_close(msgQId);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT2);

    ret = mq_unlink(qName);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT1);

    return MQUEUE_NO_ERROR;
EXIT2:
    mq_close(msgQId);
EXIT1:
    mq_unlink(qName);
EXIT:
    return MQUEUE_NO_ERROR;
}

VOID ItPosixQueue072(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("IT_POSIX_QUEUE_072", Testcase, TEST_POSIX, TEST_QUE, TEST_LEVEL2, TEST_FUNCTION);
}
