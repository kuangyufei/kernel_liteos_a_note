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

    ICUNIT_GOTO_EQUAL(g_testCount, 3, g_testCount, EXIT); // 3, Here, assert the g_testCount.

    LOS_AtomicInc(&g_testCount);

    ret = mq_timedreceive(g_gqueue, msgrcd, MQUEUE_STANDARD_NAME_LENGTH, NULL, &ts);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_SHORT_ARRAY_LENGTH, ret, EXIT);
    ICUNIT_GOTO_STRING_EQUAL(msgrcd, MQUEUE_SEND_STRING_TEST, msgrcd, EXIT);

    LOS_AtomicInc(&g_testCount);

    ret = mq_close(g_gqueue);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT);

    ret = mq_unlink(g_gqname);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT);
    return;

EXIT:
    mq_close(g_gqueue);
    mq_unlink(g_gqname);
    return;
}

static VOID HwiF02(VOID *arg)
{
    INT32 ret;
    const CHAR *msgptr = MQUEUE_SEND_STRING_TEST;
    struct timespec ts = { 0 };
    struct mq_attr attr = { 0 };

    attr.mq_msgsize = MQUEUE_STANDARD_NAME_LENGTH;
    attr.mq_maxmsg = MQUEUE_STANDARD_NAME_LENGTH;

    ts.tv_sec = 0;

    TEST_HwiClear(HWI_NUM_TEST1);

    LOS_AtomicInc(&g_testCount);

    g_gqueue = mq_open(g_gqname, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, &attr);
    ICUNIT_GOTO_NOT_EQUAL(g_gqueue, (mqd_t)-1, g_gqueue, EXIT);

    ret = mq_timedsend(g_gqueue, msgptr, strlen(msgptr), 0, &ts);
    ICUNIT_GOTO_EQUAL(ret, MQUEUE_NO_ERROR, ret, EXIT);

    LOS_AtomicInc(&g_testCount);

    return;

EXIT:
    mq_close(g_gqueue);
    mq_unlink(g_gqname);
    return;
}

static UINT32 Testcase(VOID)
{
    UINT32 uret;

    g_testCount = 0;

    snprintf(g_gqname, MQUEUE_STANDARD_NAME_LENGTH, "/mq124_%d", LosCurTaskIDGet());

    uret = LOS_HwiCreate(HWI_NUM_TEST1, 1, 0, (HWI_PROC_FUNC)HwiF02, 0);
    ICUNIT_GOTO_EQUAL(uret, MQUEUE_NO_ERROR, uret, EXIT);

#if (LOSCFG_KERNEL_SMP == YES)
    HalIrqSetAffinity(HWI_NUM_TEST1, CPUID_TO_AFFI_MASK(ArchCurrCpuid()));
#endif

    TEST_HwiTrigger(HWI_NUM_TEST1);

    ICUNIT_GOTO_EQUAL(g_testCount, 2, g_testCount, EXIT); // 2, Here, assert the g_testCount.

    uret = LOS_HwiCreate(HWI_NUM_TEST, 1, 0, (HWI_PROC_FUNC)HwiF01, 0);
    ICUNIT_GOTO_EQUAL(uret, MQUEUE_NO_ERROR, uret, EXIT1);

#if (LOSCFG_KERNEL_SMP == YES)
    HalIrqSetAffinity(HWI_NUM_TEST, CPUID_TO_AFFI_MASK(ArchCurrCpuid()));
#endif

    LOS_AtomicInc(&g_testCount);
    ICUNIT_GOTO_EQUAL(g_testCount, 3, g_testCount, EXIT1); // 3, Here, assert the g_testCount.

    TEST_HwiTrigger(HWI_NUM_TEST);

    ICUNIT_GOTO_EQUAL(g_testCount, 5, g_testCount, EXIT1); // 5, Here, assert the g_testCount.

    TEST_HwiDelete(HWI_NUM_TEST);

    TEST_HwiDelete(HWI_NUM_TEST1);

    return MQUEUE_NO_ERROR;

EXIT1:
    TEST_HwiDelete(HWI_NUM_TEST);
EXIT:
    TEST_HwiDelete(HWI_NUM_TEST1);
    return MQUEUE_NO_ERROR;
}

VOID ItPosixQueue124(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("IT_POSIX_QUEUE_124", Testcase, TEST_POSIX, TEST_QUE, TEST_LEVEL2, TEST_FUNCTION);
}
