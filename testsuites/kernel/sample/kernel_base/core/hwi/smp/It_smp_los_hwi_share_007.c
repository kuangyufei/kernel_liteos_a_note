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
extern EVENT_CB_S g_event;
#ifndef LOSCFG_NO_SHARED_IRQ

static HwiIrqParam g_dev[LOSCFG_KERNEL_CORE_NUM];

static VOID HwiF01(void)
{
    LOS_AtomicInc(&g_testCount);
}

static VOID TaskF02(VOID)
{
    TestHwiTrigger(HWI_NUM_TEST);
    LOS_EventWrite(&g_event, 0x1 << LOSCFG_KERNEL_CORE_NUM);
    TestDumpCpuid();
    return;
}

static VOID TaskF01(UINTPTR devIndex)
{
    UINT32 ret;
    // 3, Set the interrupt priority
    ret = LOS_HwiCreate(HWI_NUM_TEST, 3, IRQF_SHARED, (HWI_PROC_FUNC)HwiF01, &(g_dev[devIndex]));
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    ret = LOS_EventWrite(&g_event, 0x1 << devIndex);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    TestDumpCpuid();
    return;
}

static UINT32 Testcase(void)
{
    TSK_INIT_PARAM_S testTask;
    UINT32 ret, testid, currCpuid;
    UINT32 i;

    g_testCount = 0;
    LOS_EventInit(&g_event);

    for (i = 0; i < LOSCFG_KERNEL_CORE_NUM; i++) {
        g_dev[i].pDevId = (void *)(i + 1);
        g_dev[i].swIrq = HWI_NUM_TEST;
    }

    for (i = 0; i < LOSCFG_KERNEL_CORE_NUM; i++) {
        TEST_TASK_PARAM_INIT(testTask, "it_hwi_share_task", TaskF01,
            TASK_PRIO_TEST + 1); // not set cpuaffi
        testTask.auwArgs[0] = i;
        ret = LOS_TaskCreate(&testid, &testTask);
        ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);
    }

    ret = LOS_EventRead(&g_event, (0x1 << LOSCFG_KERNEL_CORE_NUM) - 1, LOS_WAITMODE_AND, LOS_WAIT_FOREVER);
    ICUNIT_GOTO_EQUAL(ret, (0x1 << LOSCFG_KERNEL_CORE_NUM) - 1, ret, EXIT);

    TEST_TASK_PARAM_INIT(testTask, "it_hwi_share_task", TaskF02,
        TASK_PRIO_TEST + 1); // not set cpuaffi
    ret = LOS_TaskCreate(&testid, &testTask);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    ret = LOS_EventRead(&g_event, (0x1 << (LOSCFG_KERNEL_CORE_NUM + 1)) - 1, LOS_WAITMODE_AND, LOS_WAIT_FOREVER);
    ICUNIT_GOTO_EQUAL(ret, (0x1 << (LOSCFG_KERNEL_CORE_NUM + 1)) - 1, ret, EXIT);

    LOS_TaskDelay(2); // 2, set delay time

    ICUNIT_GOTO_EQUAL(g_testCount, LOSCFG_KERNEL_CORE_NUM, g_testCount, EXIT);
EXIT:
    for (i = 0; i < LOSCFG_KERNEL_CORE_NUM; i++) {
        LOS_HwiDelete(HWI_NUM_TEST, &(g_dev[i]));
    }
    LOS_EventDestroy(&g_event);
    return LOS_OK;
}

VOID ItSmpLosHwiShare007(VOID)
{
    TEST_ADD_CASE("ItSmpLosHwiShare007", Testcase, TEST_LOS, TEST_HWI, TEST_LEVEL1, TEST_FUNCTION);
}
#endif
#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
