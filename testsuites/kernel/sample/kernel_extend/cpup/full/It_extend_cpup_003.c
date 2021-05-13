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
    UINT32 cpupUse, ret;
    g_testTaskID02 = LOS_GetCurrProcessID();

    ret = LOS_SetProcessPriority(g_testTaskID02, TASK_PRIO_TEST - 1);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ICUNIT_ASSERT_EQUAL(g_cpupTestCount, 2, g_cpupTestCount); // 2, Here, assert that g_cpupTestCount is equal to the expected value.
    g_cpupTestCount++;
    cpupUse = LOS_HistoryProcessCpuUsage(g_testTaskID01, CPUP_ALL_TIME);
    if (cpupUse > LOS_CPUP_PRECISION || cpupUse < CPU_USE_MIN) {
        ret = LOS_NOK;
    } else {
        ret = LOS_OK;
    }
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);
    cpupUse = LOS_HistoryProcessCpuUsage(g_testTaskID02, CPUP_ALL_TIME);
    if (cpupUse > LOS_CPUP_PRECISION || cpupUse < CPU_USE_MIN) {
        ret = LOS_NOK;
    } else {
        ret = LOS_OK;
    }
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);
    LOS_TaskDelay(3); // 3, set delay time.

    ICUNIT_ASSERT_EQUAL(g_cpupTestCount, 4, g_cpupTestCount); // 4, Here, assert that g_cpupTestCount is equal to the expected value.
    g_cpupTestCount++;

    return LOS_OK;

EXIT:
    return ret;
}

static UINT32 TaskF01(VOID)
{
    UINT32 cpupUse, ret;
    g_testTaskID01 = LOS_GetCurrProcessID();

    // 2, used to calculate the task priority.
    ret = LOS_SetProcessPriority(g_testTaskID01, TASK_PRIO_TEST - 2);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ICUNIT_ASSERT_EQUAL(g_cpupTestCount, 0, g_cpupTestCount);
    g_cpupTestCount++;

    cpupUse = LOS_HistorySysCpuUsage(CPUP_ALL_TIME);
    if (cpupUse > LOS_CPUP_PRECISION || cpupUse < CPU_USE_MIN) {
        ret = LOS_NOK;
    } else {
        ret = LOS_OK;
    }
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);
    LOS_TaskDelay(4); // 4, set delay time.

    ICUNIT_ASSERT_EQUAL(g_cpupTestCount, 3, g_cpupTestCount); // 3, Here, assert that g_cpupTestCount is equal to the expected value.
    g_cpupTestCount++;

    return LOS_OK;

EXIT:
    return ret;
}

static UINT32 Testcase(VOID)
{
    UINT32 ret, cpupUse;
    INT32 pid1;

    g_cpupTestCount = 0;
    INT32 pid = LOS_Fork(0, "TestCpupTsk1", TaskF01, TASK_STACK_SIZE_TEST);
    if (pid < 0) {
        ICUNIT_ASSERT_EQUAL(1, LOS_OK, 1);
    }

    LOS_TaskDelay(2); // 2, set delay time.

    ICUNIT_ASSERT_EQUAL(g_cpupTestCount, 1, g_cpupTestCount);
    g_cpupTestCount++;

    pid1 = LOS_Fork(0, "TestCpupTsk2", TaskF02, TASK_STACK_SIZE_TEST);
    if (pid1 < 0) {
        ICUNIT_ASSERT_EQUAL(1, LOS_OK, 1);
    }

    LOS_TaskDelay(2); // 2, set delay time.

    cpupUse = LOS_HistorySysCpuUsage(CPUP_ALL_TIME);
    if (cpupUse > LOS_CPUP_PRECISION || cpupUse < CPU_USE_MIN) {
        ret = LOS_NOK;
    } else {
        ret = LOS_OK;
    }
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    cpupUse = LOS_HistorySysCpuUsage(CPU_USE_MODE1);
    if (cpupUse > LOS_CPUP_PRECISION || cpupUse < CPU_USE_MIN) {
        ret = LOS_NOK;
    } else {
        ret = LOS_OK;
    }
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);
    LOS_TaskDelay(3); // 3, set delay time.

    ICUNIT_ASSERT_EQUAL(g_cpupTestCount, 5, g_cpupTestCount); // 5, Here, assert that g_cpupTestCount is equal to the expected value.

    (VOID)LOS_Wait(pid, NULL, 0, NULL);
    (VOID)LOS_Wait(pid1, NULL, 0, NULL);

    return LOS_OK;
EXIT:
    return ret;
}

VOID ItExtendCpup003(VOID) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("ItExtendCpup003", Testcase, TEST_EXTEND, TEST_CPUP, TEST_LEVEL2, TEST_FUNCTION);
}
#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
