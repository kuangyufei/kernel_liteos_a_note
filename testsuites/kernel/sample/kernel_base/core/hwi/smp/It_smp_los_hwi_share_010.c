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

static HwiIrqParam g_dev[3]; // 3, Three devices

static VOID HwiF01(void)
{
    LOS_AtomicInc(&g_testCount);
}

static VOID TaskF01()
{
    UINT32 ret, index, i;
    index = ArchCurrCpuid();
    switch (index) {
        case 0:
            for (i = 0; i < LOOP; i++) {
                // 3, Set the interrupt priority
                ret = LOS_HwiCreate(HWI_NUM_TEST, 3, IRQF_SHARED, (HWI_PROC_FUNC)HwiF01, &(g_dev[0]));
                ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

                TestHwiTrigger(HWI_NUM_TEST);
                LOS_HwiDelete(HWI_NUM_TEST, &(g_dev[0]));
                TestDumpCpuid();
            }
            break;
        case 1:
            for (i = 0; i < LOOP; i++) {
                // 3, Set the interrupt priority
                ret = LOS_HwiCreate(HWI_NUM_TEST, 3, IRQF_SHARED, (HWI_PROC_FUNC)HwiF01, &(g_dev[1]));
                ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

                TestHwiTrigger(HWI_NUM_TEST);
                LOS_HwiDelete(HWI_NUM_TEST, &(g_dev[1]));
                TestDumpCpuid();
            }
            break;
        case 2: // 2, when CPU id is
            for (i = 0; i < LOOP; i++) {
                // 3, Set the interrupt priority; 2, index
                ret = LOS_HwiCreate(HWI_NUM_TEST, 3, IRQF_SHARED, (HWI_PROC_FUNC)HwiF01, &(g_dev[2]));
                ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

                TestHwiTrigger(HWI_NUM_TEST);
                LOS_HwiDelete(HWI_NUM_TEST, &(g_dev[2])); // 2, index
                TestDumpCpuid();
            }
            break;
        default:
            TestDumpCpuid();
            break;
    }

    return;
EXIT:
    LOS_HwiDelete(HWI_NUM_TEST, &(g_dev[0]));
    LOS_HwiDelete(HWI_NUM_TEST, &(g_dev[1]));
    LOS_HwiDelete(HWI_NUM_TEST, &(g_dev[2])); // 2, index
    return;
}

static UINT32 Testcase(void)
{
    TSK_INIT_PARAM_S testTask;
    UINT32 ret, testid, currCpuid;
    UINT32 i;

    g_testCount = 0;

    for (i = 0; i < 3; i++) { // 3, max index
        g_dev[i].pDevId = (void *)(i + 1);
        g_dev[i].swIrq = HWI_NUM_TEST;
    }

    for (i = 0; i < LOSCFG_KERNEL_CORE_NUM; i++) {
        TEST_TASK_PARAM_INIT_AFFI(testTask, "it_hwi_share_task", TaskF01, TASK_PRIO_TEST + 1, CPUID_TO_AFFI_MASK(i));
        ret = LOS_TaskCreate(&testid, &testTask);
        ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);
    }

    LOS_TaskDelay(100); // 100, set delay time

    for (i = 0; i < 3; i++) { // 3, max index
        ret = LOS_HwiDelete(HWI_NUM_TEST, &(g_dev[i]));
        PRINT_DEBUG("ret = 0x%x\n", ret);
        ICUNIT_ASSERT_NOT_EQUAL(ret, LOS_OK, ret);
    }
    return LOS_OK;
}

VOID ItSmpLosHwiShare010(VOID)
{
    TEST_ADD_CASE("ItSmpLosHwiShare010", Testcase, TEST_LOS, TEST_HWI, TEST_LEVEL1, TEST_FUNCTION);
}
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
