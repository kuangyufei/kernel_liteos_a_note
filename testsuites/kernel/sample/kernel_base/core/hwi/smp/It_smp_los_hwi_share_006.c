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

#include "It_smp_hwi.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cpluscplus */
#endif /* __cpluscplus */
#ifndef LOSCFG_NO_SHARED_IRQ

static HwiIrqParam g_dev1, g_dev2;
extern EVENT_CB_S g_event;
static VOID HwiF01(void)
{
    LOS_AtomicInc(&g_testCount);
}

static VOID HwiF02(void)
{
    LOS_AtomicAdd(&g_testCount, 0xff);
}

static VOID TaskF01(void)
{
    UINT32 ret;
    ret = LOS_HwiDelete(HWI_NUM_TEST, &g_dev1);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);
    LOS_EventWrite(&g_event, 0x1);
    TestDumpCpuid();
    return;
}

static UINT32 Testcase(void)
{
    TSK_INIT_PARAM_S testTask;
    UINT32 ret, testid, currCpuid;

    g_testCount = 0;
    LOS_EventInit(&g_event);

    g_dev1.pDevId = (void *)1;
    g_dev1.swIrq = HWI_NUM_TEST;
    ret = LOS_HwiCreate(HWI_NUM_TEST, 3, IRQF_SHARED, (HWI_PROC_FUNC)HwiF01, &g_dev1); // 3, Set the interrupt priority
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    g_dev2.pDevId = (void *)2; // 2, Set device ID
    g_dev2.swIrq = HWI_NUM_TEST;
    ret = LOS_HwiCreate(HWI_NUM_TEST, 3, IRQF_SHARED, (HWI_PROC_FUNC)HwiF02, &g_dev2); // 3, Set the interrupt priority
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    currCpuid = (ArchCurrCpuid() + 1) % (LOSCFG_KERNEL_CORE_NUM);

    TestDumpCpuid();

    TEST_TASK_PARAM_INIT_AFFI(testTask, "it_hwi_002_task", TaskF01, TASK_PRIO_TEST - 1,
        CPUID_TO_AFFI_MASK(currCpuid)); // cross task
    ret = LOS_TaskCreate(&g_testTaskID01, &testTask);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    ret = LOS_EventRead(&g_event, 0x1, LOS_WAITMODE_AND, LOS_WAIT_FOREVER);
    ICUNIT_GOTO_EQUAL(ret, 0x1, ret, EXIT);

    TestHwiTrigger(HWI_NUM_TEST);

    LOS_TaskDelay(1);
    ICUNIT_GOTO_EQUAL(g_testCount, 0xff, g_testCount, EXIT);

    ret = LOS_HwiDelete(HWI_NUM_TEST, &g_dev2);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);
    LOS_EventDestroy(&g_event);
    return LOS_OK;
EXIT:
    LOS_EventDestroy(&g_event);
    LOS_HwiDelete(HWI_NUM_TEST, &g_dev1);
    LOS_HwiDelete(HWI_NUM_TEST, &g_dev2);
    return LOS_OK;
}

VOID ItSmpLosHwiShare006(VOID)
{
    TEST_ADD_CASE("ItSmpLosHwiShare006", Testcase, TEST_LOS, TEST_HWI, TEST_LEVEL1, TEST_FUNCTION);
}
#endif
#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
