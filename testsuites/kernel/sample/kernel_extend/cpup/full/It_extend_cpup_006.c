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

static UINT32 TaskF02(VOID)
{
    UINT32 ret, cpupUse;

    ICUNIT_ASSERT_EQUAL(g_cpupTestCount, 2, g_cpupTestCount); // 2, Here, assert that g_cpupTestCount is equal to the expected value.
    g_cpupTestCount++;

    cpupUse = LOS_HistoryProcessCpuUsage(g_testTaskID01, CPUP_ALL_TIME);
    if (cpupUse > LOS_CPUP_PRECISION || cpupUse < CPU_USE_MIN) {
        ret = LOS_NOK;
    } else {
        ret = LOS_OK;
    }
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    ret = LOS_TaskDelete(g_testTaskID02);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);
    return LOS_OK;

EXIT:
    ret = LOS_TaskDelete(g_testTaskID02);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);
    return ret;
}

static VOID TaskF01(VOID)
{
    UINT32 ret;
    TSK_INIT_PARAM_S task1 = { 0 };
    task1.pfnTaskEntry = (TSK_ENTRY_FUNC)TaskF02;
    task1.uwStackSize = TASK_STACK_SIZE_TEST;
    task1.pcName = "TskTst2";
    task1.usTaskPrio = TASK_PRIO_TEST - 1; // 1, used to calculate the task priority.
    task1.uwResved = TASK_STATUS_UNDETACHED;
#ifdef LOSCFG_KERNEL_SMP
    task1.usCpuAffiMask = CPUID_TO_AFFI_MASK(ArchCurrCpuid());
#endif
    ICUNIT_ASSERT_EQUAL_VOID(g_cpupTestCount, 0, g_cpupTestCount);
    g_cpupTestCount++;

    g_testTaskID01 = LOS_GetCurrProcessID();

    ret = LOS_GetTaskScheduler(LOS_CurTaskIDGet());
    // 2, used to calculate the task priority.
    ret = LOS_SetTaskScheduler(LOS_CurTaskIDGet(), ret, TASK_PRIO_TEST - 2);
    ICUNIT_ASSERT_EQUAL_VOID(ret, 0, ret);

    ret = LOS_TaskCreate(&g_testTaskID02, &task1);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    ICUNIT_ASSERT_EQUAL_VOID(g_cpupTestCount, 1, g_cpupTestCount);
    g_cpupTestCount++;

    LOS_TaskDelay(30); // 30, set delay time.

    ICUNIT_ASSERT_EQUAL_VOID(g_cpupTestCount, 3, g_cpupTestCount); // 3, Here, assert that g_cpupTestCount is equal to the expected value.
    g_cpupTestCount++;
}

static UINT32 Testcase(VOID)
{
    UINT32 ret, cpupUse;
    TSK_INIT_PARAM_S task1 = { 0 };

    cpupUse = LOS_HistorySysCpuUsage(CPUP_ALL_TIME);
    if (cpupUse > LOS_CPUP_PRECISION || cpupUse < CPU_USE_MIN) {
        ret = LOS_NOK;
    } else {
        ret = LOS_OK;
    }
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    task1.pfnTaskEntry = (TSK_ENTRY_FUNC)TaskF01;
    task1.uwStackSize = TASK_STACK_SIZE_TEST;
    task1.pcName = "TskTst1";
    task1.usTaskPrio = TASK_PRIO_TEST - 2; // 2, used to calculate the task priority.
    task1.uwResved = TASK_STATUS_UNDETACHED;
#ifdef LOSCFG_KERNEL_SMP
    task1.usCpuAffiMask = CPUID_TO_AFFI_MASK(ArchCurrCpuid());
#endif
    g_cpupTestCount = 0;
    INT32 pid = LOS_Fork(0, "TestCpupTsk1", TaskF01, TASK_STACK_SIZE_TEST);
    if (pid < 0) {
        ICUNIT_ASSERT_EQUAL(1, LOS_OK, 1);
    }

    ret = LOS_Wait(pid, NULL, 0, NULL);
    ICUNIT_ASSERT_EQUAL(pid, ret, pid);

    ICUNIT_ASSERT_EQUAL(g_cpupTestCount, 4, g_cpupTestCount); // 4, Here, assert that g_cpupTestCount is equal to the expected value.

    cpupUse = LOS_HistorySysCpuUsage(CPU_USE_MODE1);
    if (cpupUse > LOS_CPUP_PRECISION || cpupUse < CPU_USE_MIN) {
        ret = LOS_NOK;
    } else {
        ret = LOS_OK;
    }
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    return LOS_OK;

EXIT:
    return ret;
}

VOID ItExtendCpup006(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("ItExtendCpup006", Testcase, TEST_EXTEND, TEST_CPUP, TEST_LEVEL2, TEST_FUNCTION);
}
#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
