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
    CHAR qname[MQUEUE_STANDARD_NAME_LENGTH] = "";
    CHAR msgptr[MQUEUE_STANDARD_NAME_LENGTH];
    struct timespec ts;
    struct mq_attr attr = { 0 };
    mqd_t queue;
    INT32 unresolved = 0, failure = 0, i, maxreached = 0, ret = 0;

    snprintf(qname, MQUEUE_STANDARD_NAME_LENGTH, "/mq083-1_%d", LosCurTaskIDGet());

    attr.mq_msgsize = 100; // 100, queue message size.
    attr.mq_maxmsg = MAXMSG5;
    queue = mq_open(qname, O_CREAT | O_RDWR | O_NONBLOCK, S_IRUSR | S_IWUSR, &attr);
    ICUNIT_GOTO_NOT_EQUAL(queue, (mqd_t)-1, queue, EXIT);

    ts.tv_sec = 1;
    ts.tv_nsec = 0;
    for (i = 0; i < MAXMSG5; i++) {
        snprintf(msgptr, MQUEUE_STANDARD_NAME_LENGTH, "message %d", i);
        ret = mq_timedsend(queue, msgptr, strlen(msgptr), 0, &ts);
        ICUNIT_GOTO_NOT_EQUAL(ret, -1, ret, EXIT);
    }

    snprintf(msgptr, MQUEUE_STANDARD_NAME_LENGTH, "message %d", i);
    ret = mq_timedsend(queue, msgptr, strlen(msgptr), 0, &ts);
    ICUNIT_GOTO_EQUAL(ret, -1, ret, EXIT);
    ICUNIT_GOTO_EQUAL(errno, EAGAIN, errno, EXIT);

    ret = mq_close(queue);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = mq_unlink(qname);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    return MQUEUE_NO_ERROR;
EXIT:
    mq_close(queue);
    mq_unlink(qname);
    return MQUEUE_NO_ERROR;
}

VOID ItPosixQueue083(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("IT_POSIX_QUEUE_083", Testcase, TEST_POSIX, TEST_QUE, TEST_LEVEL2, TEST_FUNCTION);
}
