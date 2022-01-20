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
    TEST_HwiTrigger(HWI_NUM_TEST);

    LOS_AtomicInc(&g_testCount);

    return NULL;
}

static VOID HwiF01(VOID *arg)
{
    INT32 ret;
    const CHAR *msgptr = MQUEUE_SEND_STRING_TEST;
    struct timespec ts = { 0 };

    ts.tv_sec = 0;

    TEST_HwiClear(HWI_NUM_TEST);

    LOS_AtomicInc(&g_testCount);

    ret = mq_timedsend(g_gqueue, msgptr, strlen(msgptr), 0, &ts);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT);

    return;
EXIT:
    g_testCount = 0;
    return;
}

static VOID *PthreadF02(VOID *arg)
{
    INT32 ret;
    UINT32 uret;
    CHAR mqname[MQUEUE_STANDARD_NAME_LENGTH] = "";
    CHAR msgrcd[MQUEUE_STANDARD_NAME_LENGTH] = {0};
    pthread_t pthread1;
    struct mq_attr attr = { 0 };
    struct timespec ts = { 0 };

    ts.tv_sec = 10; // 10, secend.

    attr.mq_msgsize = MQUEUE_STANDARD_NAME_LENGTH;
    attr.mq_maxmsg = MQUEUE_STANDARD_NAME_LENGTH;

    LOS_AtomicInc(&g_testCount);

    snprintf(mqname, MQUEUE_STANDARD_NAME_LENGTH, "/mq119_%d", LosCurTaskIDGet());

    g_gqueue = mq_open(mqname, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, &attr);
    ICUNIT_GOTO_NOT_EQUAL(g_gqueue, (mqd_t)-1, g_gqueue, EXIT);

    ret = LOS_HwiCreate(HWI_NUM_TEST, 0, 0, (HWI_PROC_FUNC)HwiF01, 0);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT2);

    ret = pthread_create(&pthread1, NULL, PthreadF01, NULL);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT1);

    usleep(10000); // 10000, Microseconds.

    ICUNIT_GOTO_EQUAL(g_testCount, 3, g_testCount, EXIT2); // 3, Here, assert the g_testCount.

    ret = mq_timedreceive(g_gqueue, msgrcd, MQUEUE_STANDARD_NAME_LENGTH, NULL, &ts);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_SHORT_ARRAY_LENGTH, ret, EXIT2);
    ICUNIT_GOTO_STRING_EQUAL(msgrcd, MQUEUE_SEND_STRING_TEST, msgrcd, EXIT2);

    ret = pthread_join(pthread1, NULL);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT2);

    ret = mq_close(g_gqueue);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT);

    ret = mq_unlink(mqname);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT);

    TEST_HwiDelete(HWI_NUM_TEST);

    LOS_AtomicInc(&g_testCount);

    return MQUEUE_NO_ERROR;
EXIT2:
    TEST_HwiDelete(HWI_NUM_TEST);
EXIT1:
    pthread_join(pthread1, NULL);
EXIT:
    mq_close(g_gqueue);
    mq_unlink(mqname);
    return MQUEUE_NO_ERROR;
}

static UINT32 Testcase(VOID)
{
    INT32 ret;
    pthread_t pthread1;
    pthread_attr_t attr1;
    struct sched_param schedparam;

    g_testCount = 0;

    ret = pthread_attr_init(&attr1);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT);

    schedparam.sched_priority = MQUEUE_PTHREAD_PRIORITY_TEST2;
    ret = pthread_attr_setschedparam(&attr1, &schedparam);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT1);

    ret = pthread_create(&pthread1, &attr1, PthreadF02, NULL);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT1);

    ret = pthread_join(pthread1, NULL);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT1);

    ret = pthread_attr_destroy(&attr1);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT);

    ICUNIT_GOTO_EQUAL(g_testCount, 4, g_testCount, EXIT); // 4, Here, assert the g_testCount.

    return MQUEUE_NO_ERROR;

EXIT1:
    pthread_join(pthread1, NULL);
EXIT:
    pthread_attr_destroy(&attr1);
    return MQUEUE_NO_ERROR;
}

VOID ItPosixQueue119(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("IT_POSIX_QUEUE_119", Testcase, TEST_POSIX, TEST_QUE, TEST_LEVEL2, TEST_FUNCTION);
}
