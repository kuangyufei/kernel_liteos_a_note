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

static VOID *PthreadF01(VOID *argument)
{
    INT32 i, ret;
    const CHAR *msgptr[] = { "msg test 1", "msg test 2", "msg test 3" };
    struct mq_attr attr = { 0 };

    g_testCount = 1;

    mq_getattr(g_gqueue, &attr);
    for (i = 0; i < 3; i++) { // 3, the loop frequency.
        LosTaskDelay(2); // 2, delay
        ret = mq_send(g_gqueue, msgptr[i], attr.mq_msgsize, 0);
        ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT);
    }

    g_testCount = 2; // 2, Init test count value.

    return NULL;
EXIT:
    g_testCount = 0;
    return NULL;
}

static VOID *PthreadF02(VOID *argument)
{
    INT32 i, ret;
    UINT32 uret;
    const CHAR *msgptr[] = { "msg test 1", "msg test 2", "msg test 3" };
    CHAR msgrcd[3][MQUEUE_STANDARD_NAME_LENGTH] = {0};
    struct mq_attr attr = { 0 };
    UINT32 msgPrio = 1;

    g_testCount = 3; // 3, Init test count value.

    mq_getattr(g_gqueue, &attr);
    for (i = 0; i < 3; i++) { // 3, the loop frequency.
        uret = LosTaskDelay(2); // 2, delay
        ICUNIT_GOTO_EQUAL(uret, MQUEUE_NO_ERROR, uret, EXIT);

        ret = mq_receive(g_gqueue, msgrcd[i], attr.mq_msgsize, &msgPrio);
        ICUNIT_GOTO_EQUAL(ret, attr.mq_msgsize, ret, EXIT);
        ICUNIT_GOTO_NOT_EQUAL(ret, strlen(msgptr[i]), ret, EXIT);
        ICUNIT_GOTO_STRING_EQUAL(msgrcd[i], msgptr[i], msgrcd[i], EXIT);
    }

    g_testCount = 4; // 4, Init test count value.

    return NULL;
EXIT:
    g_testCount = 0;
    return NULL;
}

static UINT32 Testcase(VOID)
{
    INT32 ret;
    UINT32 uret;
    CHAR mqname[MQUEUE_STANDARD_NAME_LENGTH] = "";
    struct mq_attr mqstat, attr;
    pthread_attr_t attr1;
    pthread_t newTh, newTh2;

    memset(&mqstat, 0, sizeof(mqstat));
    mqstat.mq_maxmsg = MQUEUE_SHORT_ARRAY_LENGTH;
    mqstat.mq_msgsize = MQUEUE_SHORT_ARRAY_LENGTH * 2; // 2, mqueue message size.
    mqstat.mq_flags = 0;

    snprintf(mqname, MQUEUE_STANDARD_NAME_LENGTH, "/mq144_%d", LosCurTaskIDGet());

    g_gqueue = mq_open(mqname, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG | S_IRWXO, &mqstat);
    ICUNIT_GOTO_NOT_EQUAL(g_gqueue, (mqd_t)-1, g_gqueue, EXIT);

    g_testCount = 0;

    ret = PosixPthreadInit(&attr1, MQUEUE_PTHREAD_PRIORITY_TEST1);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT1);

    ret = pthread_create(&newTh, &attr1, PthreadF01, NULL);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT1);

    LosTaskDelay(1);
    ICUNIT_GOTO_EQUAL(g_testCount, 1, g_testCount, EXIT1);

    ret = PosixPthreadInit(&attr1, MQUEUE_PTHREAD_PRIORITY_TEST2);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT2);

    ret = pthread_create(&newTh2, &attr1, PthreadF02, NULL);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT2);

    uret = LosTaskDelay(12); // 12, Set delay time.
    ICUNIT_GOTO_EQUAL(uret, MQUEUE_NO_ERROR, uret, EXIT2);

    ICUNIT_GOTO_EQUAL(g_testCount, 4, g_testCount, EXIT2); // 4, Here, assert the g_testCount.

    ret = PosixPthreadDestroy(&attr1, newTh2);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT2);

    ret = PosixPthreadDestroy(&attr1, newTh);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT1);

    ret = mq_close(g_gqueue);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT);

    ret = mq_unlink(mqname);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT);

    return MQUEUE_NO_ERROR;
EXIT2:
    PosixPthreadDestroy(&attr1, newTh2);
EXIT1:
    PosixPthreadDestroy(&attr1, newTh);
EXIT:
    mq_close(g_gqueue);
    mq_unlink(mqname);
    return MQUEUE_NO_ERROR;
}

VOID ItPosixQueue144(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("ItPosixQueue144", Testcase, TEST_POSIX, TEST_QUE, TEST_LEVEL2, TEST_FUNCTION);
}
