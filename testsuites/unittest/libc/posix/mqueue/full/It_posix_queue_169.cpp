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
    INT32 ret;
    UINT32 pri;
    CHAR mqname[MQUEUE_STANDARD_NAME_LENGTH] = {0};
    CHAR msgrcd[MQUEUE_STANDARD_NAME_LENGTH] = {0};
    const CHAR *msgptr = MQUEUE_SEND_STRING_TEST;
    mqd_t mqueue;
    struct mq_attr attr = { 0 };
    struct timespec ts;

    snprintf(mqname, MQUEUE_STANDARD_NAME_LENGTH, "/mq169_%d", LosCurTaskIDGet());

    attr.mq_msgsize = MQUEUE_STANDARD_NAME_LENGTH;
    attr.mq_maxmsg = MQUEUE_SHORT_ARRAY_LENGTH;
    mqueue = mq_open(mqname, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, &attr);
    ICUNIT_ASSERT_NOT_EQUAL(mqueue, (mqd_t)-1, mqueue);

    ret = mq_send(mqueue, msgptr, strlen(msgptr), MQUEUE_PRIORITY_TEST + 1);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_IS_ERROR, ret, EXIT1);
    ICUNIT_GOTO_EQUAL(errno, EINVAL, errno, EXIT1);

    ret = mq_send(mqueue, msgptr, strlen(msgptr), MQUEUE_PRIORITY_TEST + 1);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_IS_ERROR, ret, EXIT1);
    ICUNIT_GOTO_EQUAL(errno, EINVAL, errno, EXIT1);

    ret = mq_send(mqueue, msgptr, strlen(msgptr), MQUEUE_PRIORITY_TEST + 2); // 2, Mqueue priority.
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_IS_ERROR, ret, EXIT1);
    ICUNIT_GOTO_EQUAL(errno, EINVAL, errno, EXIT1);

    ts.tv_sec = 1;
    ts.tv_nsec = 0;
    ret = mq_timedreceive(mqueue, msgrcd, MQUEUE_STANDARD_NAME_LENGTH, NULL, &ts);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_IS_ERROR, ret, EXIT1);
    ICUNIT_GOTO_EQUAL(errno, ETIMEDOUT, errno, EXIT1);

    ret = strncmp(msgptr, msgrcd, strlen(msgptr));
    ICUNIT_GOTO_NOT_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT1);

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

VOID ItPosixQueue169(VOID)
{
    TEST_ADD_CASE("IT_POSIX_QUEUE_169", Testcase, TEST_POSIX, TEST_QUE, TEST_LEVEL2, TEST_FUNCTION);
}
