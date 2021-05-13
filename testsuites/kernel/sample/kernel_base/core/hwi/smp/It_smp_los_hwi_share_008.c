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

static UINT32 g_ret = 0;
static HwiIrqParam g_dev;

static VOID HwiF01(void)
{
    TEST_HwiClear(HWI_NUM_TEST);
    LOS_AtomicInc(&g_testCount);
}

static VOID TaskF01()
{
    UINT32 ret;
    ret = LOS_HwiCreate(HWI_NUM_TEST, 3, IRQF_SHARED, (HWI_PROC_FUNC)HwiF01, &g_dev); // 3, Set the interrupt priority
    g_ret |= ret;
    TestDumpCpuid();
    return;
}

static UINT32 Testcase(void)
{
    TSK_INIT_PARAM_S testTask;
    UINT32 ret, testid;
    UINT32 i, j;

    g_dev.pDevId = (void *)(1);
    g_dev.swIrq = HWI_NUM_TEST;

    for (j = 0; j < LOOP; j++) {
        g_testCount = 0;
        g_ret = 0;

        for (i = 0; i < LOSCFG_KERNEL_CORE_NUM; i++) {
            TEST_TASK_PARAM_INIT_AFFI(testTask, "it_hwi_share_task", TaskF01, TASK_PRIO_TEST + 1,
                CPUID_TO_AFFI_MASK(i));
            ret = LOS_TaskCreate(&testid, &testTask);
            ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);
        }

        LOS_TaskDelay(1);
        TestHwiTrigger(HWI_NUM_TEST);
        LOS_TaskDelay(1);
        ICUNIT_GOTO_NOT_EQUAL(g_ret, 0, g_ret, EXIT);
        ICUNIT_GOTO_EQUAL(g_testCount, 1, g_testCount, EXIT);
        LOS_HwiDelete(HWI_NUM_TEST, &g_dev);
    }
    LOS_HwiDelete(HWI_NUM_TEST, &g_dev);
    return LOS_OK;
EXIT:
    LOS_HwiDelete(HWI_NUM_TEST, &g_dev);
    return LOS_OK;
}

VOID ItSmpLosHwiShare008(VOID)
{
    TEST_ADD_CASE("ItSmpLosHwiShare008", Testcase, TEST_LOS, TEST_HWI, TEST_LEVEL1, TEST_FUNCTION);
}
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
