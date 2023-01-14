/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2023 Huawei Device Co., Ltd. All rights reserved.
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
    INT32 ret;
    CHAR mqname[MQUEUE_STANDARD_NAME_LENGTH] = {0};
    CHAR msgrcd[MQUEUE_STANDARD_NAME_LENGTH] = {0};
    const CHAR *msgptr = MQUEUE_SEND_STRING_TEST;
    mqd_t mqueue1, mqueue2;
    struct mq_attr attr = {0};

    ret = snprintf_s(mqname, MQUEUE_STANDARD_NAME_LENGTH, MQUEUE_STANDARD_NAME_LENGTH - 1,
                     "/mq019_%d", LosCurTaskIDGet());
    ICUNIT_GOTO_NOT_EQUAL(ret, MQUEUE_IS_ERROR, ret, EXIT3);

    attr.mq_msgsize = MQUEUE_STANDARD_NAME_LENGTH;
    attr.mq_maxmsg = MQUEUE_STANDARD_NAME_LENGTH;

    mqueue1 = mq_open(mqname, O_CREAT | O_RDWR | O_NONBLOCK, S_IRUSR | S_IWUSR, &attr);
    ICUNIT_GOTO_NOT_EQUAL(mqueue1, (mqd_t)-1, mqueue1, EXIT3);

    ret = mq_receive(mqueue1, msgrcd, MQUEUE_STANDARD_NAME_LENGTH, NULL);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_IS_ERROR, ret, EXIT1);

    ret = mq_send(mqueue1, msgptr, strlen(msgptr), 0);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT1);

    mqueue2 = mq_open(mqname, O_RDWR | O_NONBLOCK, S_IRUSR | S_IWUSR, NULL);
    ICUNIT_GOTO_NOT_EQUAL(mqueue1, (mqd_t)-1, mqueue1, EXIT1);

    ret = mq_receive(mqueue1, msgrcd, MQUEUE_STANDARD_NAME_LENGTH, NULL);
    ICUNIT_GOTO_EQUAL(ret, strlen(msgptr), ret, EXIT2);

    ret = mq_close(mqueue1);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT2);

    ret = mq_close(mqueue2);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT1);

    ret = mq_unlink(mqname);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT);

    return MQUEUE_NO_ERROR;

EXIT2:
    mq_close(mqueue2);
EXIT1:
    mq_close(mqueue1);
EXIT:
    mq_unlink(mqname);
EXIT3:
    return MQUEUE_IS_ERROR;
}

VOID ItPosixQueue019(VOID)
{
    TEST_ADD_CASE("IT_POSIX_QUEUE_019", Testcase, TEST_POSIX, TEST_QUE, TEST_LEVEL2, TEST_FUNCTION);
}
