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

static VOID HwiF01(VOID *arg)
{
    INT32 ret;
    CHAR msgrcd[MQUEUE_STANDARD_NAME_LENGTH] = {0};
    struct timespec ts = { 0 };

    ts.tv_sec = 0;

    TEST_HwiClear(HWI_NUM_TEST);

    LOS_AtomicInc(&g_testCount);

    ret = mq_timedreceive(g_gqueue, msgrcd, MQUEUE_STANDARD_NAME_LENGTH, NULL, &ts);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_SHORT_ARRAY_LENGTH, ret, EXIT);
    ICUNIT_GOTO_STRING_EQUAL(msgrcd, MQUEUE_SEND_STRING_TEST, msgrcd, EXIT);

    LOS_AtomicInc(&g_testCount);

    return;

EXIT:
    g_testCount = 0;
    return;
}

static UINT32 Testcase(VOID)
{
    INT32 ret;
    UINT32 uret;
    CHAR mqname[MQUEUE_STANDARD_NAME_LENGTH] = "";
    const CHAR *msgptr = MQUEUE_SEND_STRING_TEST;
    struct mq_attr attr = { 0 };

    attr.mq_msgsize = MQUEUE_STANDARD_NAME_LENGTH;
    attr.mq_maxmsg = MQUEUE_STANDARD_NAME_LENGTH;

    g_testCount = 0;

    snprintf(mqname, MQUEUE_STANDARD_NAME_LENGTH, "/mq122_%d", LosCurTaskIDGet());

    g_gqueue = mq_open(mqname, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, &attr);
    ICUNIT_GOTO_NOT_EQUAL(g_gqueue, (mqd_t)-1, g_gqueue, EXIT1);

    ret = mq_send(g_gqueue, msgptr, strlen(msgptr), 0);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT1);

    uret = LOS_HwiCreate(HWI_NUM_TEST, 1, 0, (HWI_PROC_FUNC)HwiF01, 0);
    ICUNIT_GOTO_EQUAL(uret, MQUEUE_NO_ERROR, uret, EXIT2);

    TEST_HwiTrigger(HWI_NUM_TEST);

    uret = LosTaskDelay(2); // 2, Set delay time.
    ICUNIT_GOTO_EQUAL(uret, MQUEUE_NO_ERROR, uret, EXIT2);

    ICUNIT_GOTO_EQUAL(g_testCount, 2, g_testCount, EXIT2); // 2, Here, assert the g_testCount.

    TEST_HwiDelete(HWI_NUM_TEST);

    ret = mq_close(g_gqueue);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT1);

    ret = mq_unlink(mqname);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT);

    return MQUEUE_NO_ERROR;
EXIT2:
    TEST_HwiDelete(HWI_NUM_TEST);
EXIT1:
    mq_close(g_gqueue);
EXIT:
    mq_unlink(mqname);
    return MQUEUE_NO_ERROR;
}

VOID ItPosixQueue122(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("IT_POSIX_QUEUE_122", Testcase, TEST_POSIX, TEST_QUE, TEST_LEVEL2, TEST_FUNCTION);
}
