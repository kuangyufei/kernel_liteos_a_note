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
#ifdef LOSCFG_KERNEL_SMP

static UINT32 g_ret = 0;
static VOID HwiF01(void)
{
    LOS_AtomicInc(&g_testCount);
}

static VOID TaskF01(void)
{
    UINT32 ret;
    ret = LOS_HwiDelete(HWI_NUM_TEST, NULL);
    g_ret |= ret;
    TestDumpCpuid();

    return;
}

static UINT32 Testcase(void)
{
    TSK_INIT_PARAM_S testTask;
    UINT32 ret, i, j;

    for (j = 0; j < LOOP; j++) {
        g_testCount = 0;
        g_ret = 0;
        ret = LOS_HwiCreate(HWI_NUM_TEST, 1, 0, (HWI_PROC_FUNC)HwiF01, 0);
        ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

        for (i = 0; i < LOSCFG_KERNEL_CORE_NUM; i++) {
            TEST_TASK_PARAM_INIT_AFFI(testTask, "it_hwi_007_task01", TaskF01, TASK_PRIO_TEST + 1,
                CPUID_TO_AFFI_MASK(i));
            ret = LOS_TaskCreate(&g_testTaskID01, &testTask);
            ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);
        }

        LOS_TaskDelay(2); // 2, set delay time

#ifdef TEST1980
        HalIrqSetAffinity(HWI_NUM_TEST, CPUID_TO_AFFI_MASK(ArchCurrCpuid()));
#endif
        TestHwiTrigger(HWI_NUM_TEST);

        LOS_TaskDelay(1);
        ICUNIT_GOTO_EQUAL(g_testCount, 0, g_testCount, EXIT);
    }
EXIT:
    LOS_HwiDelete(HWI_NUM_TEST, NULL);
    return LOS_OK;
}

VOID ItSmpLosHwi009(VOID)
{
    TEST_ADD_CASE("ItSmpLosHwi009", Testcase, TEST_LOS, TEST_HWI, TEST_LEVEL1, TEST_FUNCTION);
}
#endif
#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
