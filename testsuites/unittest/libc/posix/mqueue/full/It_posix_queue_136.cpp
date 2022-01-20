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
    INT32 index;

    mqd_t queueID[LOSCFG_BASE_IPC_QUEUE_CONFIG + 1];
    CHAR qName[LOSCFG_BASE_IPC_QUEUE_CONFIG + 1][MQUEUE_STANDARD_NAME_LENGTH];

    const CHAR *msgPtr = MQUEUE_SEND_STRING_TEST;
    CHAR msgRcd[MQUEUE_STANDARD_NAME_LENGTH] = {0};

    struct mq_attr msgAttr = { 0 };
    msgAttr.mq_msgsize = 20; // 20, mqueue message size.
    msgAttr.mq_maxmsg = 20; // 20, mqueue message size.

    for (index = 0; index < (LOSCFG_BASE_IPC_QUEUE_CONFIG - QUEUE_EXISTED_NUM); index++) {
        snprintf(qName[index], MQUEUE_STANDARD_NAME_LENGTH, "/mq136_%d", index);
        queueID[index] = mq_open(qName[index], O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, &msgAttr);
        ICUNIT_GOTO_NOT_EQUAL(queueID[index], (mqd_t)-1, queueID[index], EXIT);
    }

EXIT:
    for (; index >= 1;) {
        ret = mq_close(queueID[--index]);
        ICUNIT_ASSERT_EQUAL(ret, 0, index);

        ret = mq_unlink(qName[index]);
        ICUNIT_ASSERT_EQUAL(ret, MQUEUE_NO_ERROR, ret);
    }

    return MQUEUE_NO_ERROR;
}

VOID ItPosixQueue136(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("IT_POSIX_QUEUE_136", Testcase, TEST_POSIX, TEST_QUE, TEST_LEVEL2, TEST_FUNCTION);
}
