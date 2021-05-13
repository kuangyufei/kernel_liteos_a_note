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

static VOID *PthreadF01(VOID *arg)
{
    INT32 i, ret;
    const CHAR *msgptr[] = { "send_111 1", "send_111 2", "send_111 3", "send_111 4", "send_111 5" };

    for (i = 0; i < MQUEUE_PTHREAD_NUM_TEST; i++) {
        ret = mq_send(g_gqueue, msgptr[i], strlen(msgptr[i]), 0);
        ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT);
    }
    LOS_AtomicInc(&g_testCount);

    pthread_exit((void *)0);

    return NULL;
EXIT:
    pthread_exit((void *)0);
    g_testCount = 0;
    return NULL;
}

static VOID *PthreadF02(VOID *arg)
{
    INT32 i, ret;
    INT32 pthreadCount = *(INT32 *)arg;
    CHAR msgrcd[MQUEUE_PTHREAD_NUM_TEST][MQUEUE_PTHREAD_NUM_TEST][MQUEUE_STANDARD_NAME_LENGTH];

    for (i = 0; i < MQUEUE_PTHREAD_NUM_TEST; i++) {
        ret = mq_receive(g_gqueue, msgrcd[pthreadCount][i], MQUEUE_STANDARD_NAME_LENGTH, NULL);
        ICUNIT_GOTO_EQUAL(ret, MQUEUE_SHORT_ARRAY_LENGTH, ret, EXIT);
    }

    LOS_AtomicInc(&g_testCount);

    pthread_exit((void *)0);

    return NULL;
EXIT:
    g_testCount = 0;
    pthread_exit((void *)0);
    return NULL;
}

static UINT32 Testcase(VOID)
{
    INT32 oflag, i, ret = 0;
    INT32 pthreadCount[MQUEUE_PTHREAD_NUM_TEST];

    CHAR mqname[MQUEUE_STANDARD_NAME_LENGTH] = "";
    pthread_t pthreadSend[MQUEUE_PTHREAD_NUM_TEST];
    pthread_t pthreadRecev[MQUEUE_PTHREAD_NUM_TEST];
    pthread_attr_t attr1;
    struct mq_attr mqstat;

    ret = pthread_attr_init(&attr1);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT);

    g_testCount = 0;

    snprintf(mqname, MQUEUE_STANDARD_NAME_LENGTH, "/mq162_%d", LosCurTaskIDGet());

    memset(&mqstat, 0, sizeof(mqstat));
    mqstat.mq_maxmsg = MQUEUE_PTHREAD_NUM_TEST;
    mqstat.mq_msgsize = MQUEUE_STANDARD_NAME_LENGTH;
    mqstat.mq_flags = 0;

    oflag = O_CREAT | O_NONBLOCK | O_RDWR;

    g_gqueue = mq_open(mqname, oflag,  S_IRWXU | S_IRWXG | S_IRWXO, &mqstat);
    ICUNIT_GOTO_NOT_EQUAL(g_gqueue, (mqd_t)-1, g_gqueue, EXIT1);

    for (i = 0; i < MQUEUE_PTHREAD_NUM_TEST; i++) {
        pthreadCount[i] = i;
        ret = pthread_create(&pthreadSend[i], &attr1, PthreadF01, NULL);
        ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT2);

        ret = pthread_create(&pthreadRecev[i], &attr1, PthreadF02, (void *)&pthreadCount[i]);
        ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT2);
    }

    for (i = 0; i < MQUEUE_PTHREAD_NUM_TEST; i++) {
        ret = pthread_join(pthreadSend[i], NULL);
        ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT2);

        ret = pthread_join(pthreadRecev[i], NULL);
        ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT2);
    }
    ICUNIT_GOTO_EQUAL(g_testCount, MQUEUE_PTHREAD_NUM_TEST * 2, g_testCount, EXIT1); // 2, assert the g_testCount.

    ret = mq_close(g_gqueue);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT1);

    ret = mq_unlink(mqname);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT1);

    ret = pthread_attr_destroy(&attr1);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT);

    return MQUEUE_NO_ERROR;
EXIT2:
    for (i = 0; i < MQUEUE_PTHREAD_NUM_TEST; i++) {
        pthread_join(pthreadSend[i], NULL);
        pthread_join(pthreadRecev[i], NULL);
    }
EXIT1:
    mq_close(g_gqueue);
    mq_unlink(mqname);
EXIT:
    pthread_attr_destroy(&attr1);
    return MQUEUE_NO_ERROR;
}

VOID ItPosixQueue162(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("IT_POSIX_QUEUE_162", Testcase, TEST_POSIX, TEST_QUE, TEST_LEVEL2, TEST_FUNCTION);
}
