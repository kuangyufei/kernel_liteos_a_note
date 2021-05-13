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
    INT32 ret = 0;
    CHAR mqname[MQUEUE_STANDARD_NAME_LENGTH] = "";
    mqd_t mqueue;
    struct mq_attr mqstat1 = { 0 };
    struct mq_attr mqstat2 = { 0 };

    (void)snprintf_s(mqname, MQUEUE_STANDARD_NAME_LENGTH - 1, MQUEUE_STANDARD_NAME_LENGTH, "/mq062_%d",
        LosCurTaskIDGet());

    mqueue = mq_open(mqname, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, NULL);
    ICUNIT_GOTO_NOT_EQUAL(mqueue, (mqd_t)-1, mqueue, EXIT1);

    (void)memset_s(&mqstat1, sizeof(mqstat1), 0, sizeof(mqstat1));
    (void)memset_s(&mqstat2, sizeof(mqstat2), 0, sizeof(mqstat2));

    ret = mq_getattr(mqueue, &mqstat1);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT1);

    mqstat1.mq_flags |= O_NONBLOCK;
    ret = mq_setattr(mqueue, &mqstat1, NULL);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT1);

    ret = mq_getattr(mqueue, &mqstat2);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT1);
    ICUNIT_GOTO_EQUAL(mqstat2.mq_flags, mqstat1.mq_flags, mqstat2.mq_flags, EXIT1);
    ICUNIT_GOTO_EQUAL(mqstat2.mq_curmsgs, 0, mqstat2.mq_curmsgs, EXIT1);
    ICUNIT_GOTO_EQUAL(mqstat2.mq_flags, O_CREAT | O_RDWR | O_NONBLOCK, mqstat2.mq_flags, EXIT1);
    ICUNIT_GOTO_EQUAL(mqstat2.mq_maxmsg, mqstat1.mq_maxmsg, mqstat2.mq_maxmsg, EXIT1);
    ICUNIT_GOTO_EQUAL(mqstat2.mq_msgsize, mqstat1.mq_msgsize, mqstat2.mq_msgsize, EXIT1);

    ret = mq_close(mqueue);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT1);

    ret = mq_unlink(mqname);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT);

    return MQUEUE_NO_ERROR;
EXIT1:
    mq_close(mqueue);
EXIT:
    mq_unlink(mqname);
    return MQUEUE_NO_ERROR;
}

VOID ItPosixQueue062(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("IT_POSIX_QUEUE_062", Testcase, TEST_POSIX, TEST_QUE, TEST_LEVEL0, TEST_FUNCTION);
}
