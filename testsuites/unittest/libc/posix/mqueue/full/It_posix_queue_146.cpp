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

typedef struct {
    INT32 pthread_ID;
    mqd_t mqueue_ID;
} mq_info;

static VOID *PthreadF01(VOID *info)
{
    INT32 i, ret;
    const CHAR *msgptr[] = { "msg test 1", "msg test 2", "msg test 3" };
    mq_info sendInfo;

    sendInfo.pthread_ID = ((mq_info *)info)->pthread_ID;
    sendInfo.mqueue_ID = ((mq_info *)info)->mqueue_ID;
    for (i = 0; i < 3; i++) { // 3, the loop frequency.
        ret = mq_send(sendInfo.mqueue_ID, msgptr[i], MQUEUE_STANDARD_NAME_LENGTH, 0);
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

static VOID *PthreadF02(VOID *info)
{
    INT32 i, ret;
    CHAR msgrcd[3][MQUEUE_STANDARD_NAME_LENGTH * 2];
    mq_info recvInfo;

    recvInfo.pthread_ID = ((mq_info *)info)->pthread_ID;
    recvInfo.mqueue_ID = ((mq_info *)info)->mqueue_ID;
    for (i = 0; i < 3; i++) { // 3, the loop frequency.
        ret = mq_receive(recvInfo.mqueue_ID, msgrcd[i], MQUEUE_STANDARD_NAME_LENGTH, NULL);
        ICUNIT_GOTO_EQUAL(ret, MQUEUE_STANDARD_NAME_LENGTH, ret, EXIT);
    }

    LOS_AtomicInc(&g_testCount);

    pthread_exit((void *)0);

    return NULL;
EXIT:
    pthread_exit((void *)0);
    g_testCount = 0;
    return NULL;
}

static UINT32 Testcase(VOID)
{
    INT32 i, ret, oflag;
    UINT32 uret;
    const CHAR *mqname[MQUEUE_PTHREAD_NUM_TEST] = {"/msg1", "/msg2", "/msg3", "/msg4", "/msg5"};
    mqd_t mqueue[MQUEUE_PTHREAD_NUM_TEST];
    struct mq_attr mqstat;
    pthread_t pthreadSend[MQUEUE_PTHREAD_NUM_TEST];
    pthread_t pthreadRecev[MQUEUE_PTHREAD_NUM_TEST];
    mq_info info[MQUEUE_PTHREAD_NUM_TEST];

    g_testCount = 0;

    oflag = O_CREAT | O_NONBLOCK | O_RDWR;

    memset(&mqstat, 0, sizeof(mqstat));
    mqstat.mq_maxmsg = MQUEUE_SHORT_ARRAY_LENGTH;
    mqstat.mq_msgsize = MQUEUE_STANDARD_NAME_LENGTH;
    mqstat.mq_flags = 0;

    for (i = 0; i < MQUEUE_PTHREAD_NUM_TEST; i++) {
        mqueue[i] = mq_open(mqname[i], oflag,  S_IRWXU | S_IRWXG | S_IRWXO, &mqstat);
        ICUNIT_GOTO_NOT_EQUAL(mqueue[i], (mqd_t)-1, mqueue[i], EXIT);
    }
    for (i = 0; i < MQUEUE_PTHREAD_NUM_TEST; i++) {
        info[i].pthread_ID = i;
        info[i].mqueue_ID = mqueue[i];
        ret = pthread_create(&pthreadSend[i], NULL, PthreadF01, (void *)&info[i]);
        ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT1);

        ret = pthread_create(&pthreadRecev[i], NULL, PthreadF02, (void *)&info[i]);
        ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT1);
    }

    uret = sleep(5); // 5, seconds.
    ICUNIT_GOTO_EQUAL(uret, 0, uret, EXIT1);

    ICUNIT_GOTO_EQUAL(g_testCount, MQUEUE_PTHREAD_NUM_TEST * 2, g_testCount, EXIT1); // 2, assert the g_testCount.

    for (i = 0; i < MQUEUE_PTHREAD_NUM_TEST; i++) {
        ret = pthread_join(pthreadSend[i], NULL);
        ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT1);

        ret = pthread_join(pthreadRecev[i], NULL);
        ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT1);
    }

    for (i = 0; i < MQUEUE_PTHREAD_NUM_TEST; i++) {
        ret = mq_close(mqueue[i]);
        ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT);

        ret = mq_unlink(mqname[i]);
        ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT);
    }

    return MQUEUE_NO_ERROR;
EXIT1:
    for (i = 0; i < MQUEUE_PTHREAD_NUM_TEST; i++) {
        pthread_join(pthreadSend[i], NULL);
        pthread_join(pthreadRecev[i], NULL);
    }
EXIT:
    for (i = 0; i < MQUEUE_PTHREAD_NUM_TEST; i++) {
        mq_close(mqueue[i]);
        mq_unlink(mqname[i]);
    }

    return MQUEUE_NO_ERROR;
}

VOID ItPosixQueue146(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("IT_POSIX_QUEUE_146", Testcase, TEST_POSIX, TEST_QUE, TEST_LEVEL2, TEST_FUNCTION);
}
