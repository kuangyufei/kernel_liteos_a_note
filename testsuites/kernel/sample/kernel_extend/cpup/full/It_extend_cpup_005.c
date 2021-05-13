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

#include "It_extend_cpup.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cpluscplus */
#endif /* __cpluscplus */

static UINT32 TaskF01(VOID)
{
    UINT32 cpupUse, ret;
    g_cpupTestCount++;

    cpupUse = LOS_HistoryProcessCpuUsage(LOS_GetCurrProcessID(), CPUP_ALL_TIME);
    if (cpupUse > LOS_CPUP_PRECISION || cpupUse < CPU_USE_MIN) {
        ret = LOS_NOK;
    } else {
        ret = LOS_OK;
    }
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    cpupUse = LOS_HistoryProcessCpuUsage(LOS_GetCurrProcessID(), CPU_USE_MODE1);
    if (cpupUse > LOS_CPUP_PRECISION || cpupUse < CPU_USE_MIN) {
        ret = LOS_NOK;
    } else {
        ret = LOS_OK;
    }
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    ret = LOS_TaskDelete(g_testTaskID01);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);
    return LOS_OK;

EXIT:
    ret = LOS_TaskDelete(g_testTaskID01);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);
    return ret;
}

static UINT32 Testcase(VOID)
{
    UINT32 ret;
    TSK_INIT_PARAM_S task1 = { 0 };
    task1.pfnTaskEntry = (TSK_ENTRY_FUNC)TaskF01;
    task1.uwStackSize = TASK_STACK_SIZE_TEST;
    task1.pcName = "TskTst1";
    task1.usTaskPrio = TASK_PRIO_TEST - 1; // 1, used to calculate the task priority.
    task1.uwResved = TASK_STATUS_UNDETACHED;
#if (LOSCFG_KERNEL_SMP == YES)
    task1.usCpuAffiMask = CPUID_TO_AFFI_MASK(ArchCurrCpuid());
#endif
    g_cpupTestCount = 0;

    LOS_TaskLock();

    ret = LOS_TaskCreate(&g_testTaskID01, &task1);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    ICUNIT_ASSERT_EQUAL(g_cpupTestCount, 0, g_cpupTestCount);

    LOS_TaskUnlock();

    ICUNIT_ASSERT_EQUAL(g_cpupTestCount, 1, g_cpupTestCount); // 1, Here, assert that g_cpupTestCount is equal to the expected value.

    return LOS_OK;
}

VOID ItExtendCpup005(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("ItExtendCpup005", Testcase, TEST_EXTEND, TEST_CPUP, TEST_LEVEL2, TEST_FUNCTION);
}
#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
