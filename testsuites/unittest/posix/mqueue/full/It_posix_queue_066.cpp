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
    CHAR mqname[MQUEUE_STANDARD_NAME_LENGTH] = "";
    mqd_t mqdes;
    struct mq_attr mqstat = { 0 };
    struct mq_attr nmqstat = { 0 };
    INT32 unresolved = 0;
    INT32 failure = 0, ret = 0;

    snprintf(mqname, MQUEUE_STANDARD_NAME_LENGTH, "/mq066-1_%d", LosCurTaskIDGet());

    mqdes = mq_open(mqname, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 0);
    ICUNIT_GOTO_NOT_EQUAL(mqdes, (mqd_t)-1, mqdes, EXIT);

    memset(&mqstat, 0, sizeof(mqstat));
    memset(&nmqstat, 0, sizeof(nmqstat));

    ret = mq_getattr(mqdes, &mqstat);
    ICUNIT_GOTO_NOT_EQUAL(ret, -1, ret, EXIT);

    mqstat.mq_flags |= O_NONBLOCK;

    ret = mq_setattr(mqdes, &mqstat, NULL);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = mq_getattr(mqdes, &nmqstat);
    ICUNIT_GOTO_NOT_EQUAL(ret, -1, ret, EXIT);

    ret = mq_close(mqdes);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = mq_unlink(mqname);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    if (nmqstat.mq_flags != mqstat.mq_flags) {
        failure = 1;
        ICUNIT_GOTO_EQUAL(failure, 0, failure, EXIT);
    }

    return MQUEUE_NO_ERROR;
EXIT:
    mq_close(mqdes);
    mq_unlink(mqname);
    return MQUEUE_NO_ERROR;
}

VOID ItPosixQueue066(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("IT_POSIX_QUEUE_066", Testcase, TEST_POSIX, TEST_QUE, TEST_LEVEL2, TEST_FUNCTION);
}
