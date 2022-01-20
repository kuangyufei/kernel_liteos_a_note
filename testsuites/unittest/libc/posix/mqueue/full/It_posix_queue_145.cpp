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

static VOID *PthreadF01(VOID *mq)
{
    INT32 i, ret;
    mqd_t mqueue1 = *(mqd_t *)mq;
    const CHAR *msgptr[] = { "msg test 1", "msg test 2", "msg test 3" };

    for (i = 0; i < 3; i++) { // 3, the loop frequency.
        ret = mq_send(mqueue1, msgptr[i], MQUEUE_STANDARD_NAME_LENGTH, 0);
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

static VOID *PthreadF02(VOID *mq)
{
    INT32 i, ret;
    mqd_t mqueue2 = *(mqd_t *)mq;
    const CHAR *msgptr[] = { "msg test 1", "msg test 2", "msg test 3" };

    for (i = 0; i < 3; i++) { // 3, the loop frequency.
        ret = mq_send(mqueue2, msgptr[i], MQUEUE_STANDARD_NAME_LENGTH, 0);
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

static VOID *PthreadF03(VOID *mq)
{
    INT32 i, ret;
    mqd_t mqueue1 = *(mqd_t *)mq;
    CHAR msgrcd[3][MQUEUE_STANDARD_NAME_LENGTH] = {{0, 0}};

    for (i = 0; i < 3; i++) { // 3, the loop frequency.
        ret = mq_receive(mqueue1, msgrcd[i], MQUEUE_STANDARD_NAME_LENGTH, NULL);
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

static VOID *PthreadF04(VOID *mq)
{
    INT32 i, ret;
    mqd_t mqueue2 = *(mqd_t *)mq;
    CHAR msgrcd[3][MQUEUE_STANDARD_NAME_LENGTH];

    for (i = 0; i < 3; i++) { // 3, the loop frequency.
        ret = mq_receive(mqueue2, msgrcd[i], MQUEUE_STANDARD_NAME_LENGTH, NULL);
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
    INT32 ret, oflag;
    CHAR mqname1[MQUEUE_STANDARD_NAME_LENGTH] = "";
    CHAR mqname2[MQUEUE_STANDARD_NAME_LENGTH] = "";
    mqd_t mqueue1 = 0, mqueue2 = 0;
    struct mq_attr mqstat;
    pthread_t pthreadSend1, pthreadSend2, pthreadRecev1, pthreadRecev2;

    snprintf(mqname1, MQUEUE_STANDARD_NAME_LENGTH, "/mqueue145_1_%d", LosCurTaskIDGet());
    snprintf(mqname2, MQUEUE_STANDARD_NAME_LENGTH, "/mqueue145_2_%d", LosCurTaskIDGet());

    g_testCount = 0;

    memset(&mqstat, 0, sizeof(mqstat));
    mqstat.mq_maxmsg = MQUEUE_SHORT_ARRAY_LENGTH;
    mqstat.mq_msgsize = MQUEUE_STANDARD_NAME_LENGTH;
    mqstat.mq_flags = 0;

    oflag = O_CREAT | O_NONBLOCK | O_RDWR;

    mqueue1 = mq_open(mqname1, oflag,  S_IRWXU | S_IRWXG | S_IRWXO, &mqstat);
    ICUNIT_GOTO_NOT_EQUAL(mqueue1, (mqd_t)-1, mqueue1, EXIT);

    mqueue2 = mq_open(mqname2, oflag,  S_IRWXU | S_IRWXG | S_IRWXO, &mqstat);
    ICUNIT_GOTO_NOT_EQUAL(mqueue2, (mqd_t)-1, mqueue2, EXIT1);

    ret = pthread_create(&pthreadSend1, NULL, PthreadF01, (void *)&mqueue1);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT2);

    ret = pthread_create(&pthreadSend2, NULL, PthreadF02, (void *)&mqueue2);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT3);

    ret = pthread_create(&pthreadRecev1, NULL, PthreadF03, (void *)&mqueue1);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT4);

    ret = pthread_create(&pthreadRecev2, NULL, PthreadF04, (void *)&mqueue2);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT5);

    ret = pthread_join(pthreadSend1, NULL);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT5);

    ret = pthread_join(pthreadSend2, NULL);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT5);

    ret = pthread_join(pthreadRecev1, NULL);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT5);

    ret = pthread_join(pthreadRecev2, NULL);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT5);

    ICUNIT_GOTO_EQUAL(g_testCount, 4, g_testCount, EXIT1); // 4, Here, assert the g_testCount.

    ret = mq_close(mqueue2);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT1);

    ret = mq_unlink(mqname2);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT1);

    ret = mq_close(mqueue1);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT);

    ret = mq_unlink(mqname1);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT);

    return MQUEUE_NO_ERROR;
EXIT5:
    pthread_join(pthreadRecev2, NULL);
EXIT4:
    pthread_join(pthreadRecev1, NULL);
EXIT3:
    pthread_join(pthreadSend2, NULL);
EXIT2:
    pthread_join(pthreadSend1, NULL);
EXIT1:
    mq_close(mqueue2);
    mq_unlink(mqname2);
EXIT:
    mq_close(mqueue1);
    mq_unlink(mqname1);
    return MQUEUE_NO_ERROR;
}

VOID ItPosixQueue145(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("IT_POSIX_QUEUE_145", Testcase, TEST_POSIX, TEST_QUE, TEST_LEVEL2, TEST_FUNCTION);
}
