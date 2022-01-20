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
    mqd_t queue[_POSIX_OPEN_MAX + _POSIX_MQ_OPEN_MAX + 1];
    INT32 i, unresolved = 0, failure = 0, numqueues = 0, ret;

    for (i = 0; (i < _POSIX_OPEN_MAX) && (i < _POSIX_MQ_OPEN_MAX); i++, numqueues++) {
        snprintf(qname, MQUEUE_STANDARD_NAME_LENGTH, "/mq090%d_%d", i, LosCurTaskIDGet());

        queue[i] = mq_open(qname, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, NULL);
        if (queue[i] == (mqd_t)-1) {
            unresolved = 1;
            break;
        }
    }

    queue[numqueues] = mq_open(qname, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, NULL);
    if (queue[numqueues] != (mqd_t)-1) {
    } else {
        if (errno != EMFILE) {
            failure = 1;
        }
    }

    for (i = 0; i <= numqueues; i++) {
        ret = mq_close(queue[i]);
        ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);
    }
    for (i = 0; i < numqueues; i++) {
        snprintf(qname, MQUEUE_STANDARD_NAME_LENGTH, "/mq090%d_%d", i, LosCurTaskIDGet());
        ret = mq_unlink(qname);
        ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);
    }

    snprintf(qname, MQUEUE_STANDARD_NAME_LENGTH, "/mq090%d_%d", numqueues, LosCurTaskIDGet());
    ret = mq_unlink(qname);
    ICUNIT_GOTO_EQUAL(ret, -1, ret, EXIT);

    if (failure == 1) {
        ICUNIT_ASSERT_NOT_EQUAL(failure, 1, failure);
    }

    if (unresolved == 1) {
        ICUNIT_ASSERT_NOT_EQUAL(unresolved, 1, unresolved);
    }
    return MQUEUE_NO_ERROR;
EXIT:
    for (i = 0; i <= numqueues; i++) {
        mq_close(queue[i]);
        mq_unlink(qname);
    }
    return MQUEUE_NO_ERROR;
}

VOID ItPosixQueue090(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("IT_POSIX_QUEUE_090", Testcase, TEST_POSIX, TEST_QUE, TEST_LEVEL2, TEST_FUNCTION);
}
