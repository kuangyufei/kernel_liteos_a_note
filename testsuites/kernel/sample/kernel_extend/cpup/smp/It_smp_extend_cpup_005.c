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

static UINT32 g_testSmpCpupTaskID01, g_testSmpCpupTaskID02, g_testSmpCpupTaskID03, g_testSmpCpupTaskID04;
static volatile UINT32 g_testSmpCpupStop = 0;
static volatile UINT32 g_testSmpCpupCount = 0;

static void Task03(void)
{
    while (g_testSmpCpupStop < 1) {
        g_testSmpCpupCount++;
        LOS_TaskYield();
    }
    return;
}
static void Task04(void)
{
    while (g_testSmpCpupStop < 1) {
        g_testSmpCpupCount++;
        LOS_TaskYield();
    }
    return;
}

static void Task01(void)
{
    TSK_INIT_PARAM_S taskInitParam;
    UINT32 ret;

    (VOID)memset_s((void *)(&taskInitParam), sizeof(TSK_INIT_PARAM_S), 0, sizeof(TSK_INIT_PARAM_S));
    taskInitParam.pfnTaskEntry = (TSK_ENTRY_FUNC)Task03;
    taskInitParam.uwStackSize = LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE;
    taskInitParam.pcName = "SmpCpup005_task03";
    taskInitParam.usTaskPrio = TASK_PRIO_TEST - 2; // 2, used to calculate the task priority.
#if (LOSCFG_KERNEL_SMP == YES)
    taskInitParam.usCpuAffiMask = CPUID_TO_AFFI_MASK(ArchCurrCpuid());
#endif
    ret = LOS_TaskCreate(&g_testSmpCpupTaskID03, &taskInitParam);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT2);

    (VOID)memset_s((void *)(&taskInitParam), sizeof(TSK_INIT_PARAM_S), 0, sizeof(TSK_INIT_PARAM_S));
    taskInitParam.pfnTaskEntry = (TSK_ENTRY_FUNC)Task04;
    taskInitParam.uwStackSize = LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE;
    taskInitParam.pcName = "SmpCpup005_task04";
    taskInitParam.usTaskPrio = TASK_PRIO_TEST - 2; // 2, used to calculate the task priority.
#if (LOSCFG_KERNEL_SMP == YES)
    taskInitParam.usCpuAffiMask = CPUID_TO_AFFI_MASK(ArchCurrCpuid());
#endif
    ret = LOS_TaskCreate(&g_testSmpCpupTaskID04, &taskInitParam);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT3);

    while (g_testSmpCpupStop < 1) {
        g_testSmpCpupCount++;
        LOS_TaskYield();
    }

    // 2, the g_testSmpCpupStop possible values.
    while (g_testSmpCpupStop < 2) {
        // 4, used to calculate the delay time.
        LOS_TaskDelay(LOSCFG_BASE_CORE_TICK_PER_SECOND / 4);
    }
EXIT3:
    LOS_TaskDelete(g_testSmpCpupTaskID04);
EXIT2:
    LOS_TaskDelete(g_testSmpCpupTaskID03);
    return;
}
static void Task02(void)
{
    UINT32 tempCpup, tempCpup1, tempCpup2, tempCpup3;

    LOS_TaskDelay(LOSCFG_BASE_CORE_TICK_PER_SECOND * CPUP_BACKWARD);
    tempCpup1 = LOS_HistoryTaskCpuUsage(g_testSmpCpupTaskID01, CPUP_LAST_ONE_SECONDS);
    tempCpup2 = LOS_HistoryTaskCpuUsage(g_testSmpCpupTaskID03, CPUP_LAST_ONE_SECONDS);
    tempCpup3 = LOS_HistoryTaskCpuUsage(g_testSmpCpupTaskID04, CPUP_LAST_ONE_SECONDS);
    tempCpup = tempCpup1 + tempCpup2 + tempCpup3;
    ICUNIT_GOTO_WITHIN_EQUAL(tempCpup, LOS_CPUP_SINGLE_CORE_PRECISION - CPUP_TEST_TOLERANT,
        LOS_CPUP_SINGLE_CORE_PRECISION, tempCpup, EXIT3);

EXIT3:
    g_testSmpCpupStop = 1;

    LOS_TaskDelay(LOSCFG_BASE_CORE_TICK_PER_SECOND * CPUP_BACKWARD);
    tempCpup1 = LOS_HistoryTaskCpuUsage(g_testSmpCpupTaskID01, CPUP_LAST_ONE_SECONDS);
    tempCpup2 = LOS_HistoryTaskCpuUsage(g_testSmpCpupTaskID03, CPUP_LAST_ONE_SECONDS);
    tempCpup3 = LOS_HistoryTaskCpuUsage(g_testSmpCpupTaskID04, CPUP_LAST_ONE_SECONDS);
    tempCpup = tempCpup1 + tempCpup2 + tempCpup3;
    ICUNIT_GOTO_WITHIN_EQUAL(tempCpup, CPU_USE_MIN, CPU_USE_MIN + CPUP_TEST_TOLERANT, tempCpup, EXIT2);

EXIT2:
    // 2, set the g_testSmpCpupStop possible values.
    g_testSmpCpupStop = 2;

    LOS_TaskDelay(LOSCFG_BASE_CORE_TICK_PER_SECOND * CPUP_BACKWARD);

    return;
}

static UINT32 Testcase(VOID)
{
    TSK_INIT_PARAM_S taskInitParam;
    UINT32 ret;

    (VOID)memset_s((void *)(&taskInitParam), sizeof(TSK_INIT_PARAM_S), 0, sizeof(TSK_INIT_PARAM_S));
    taskInitParam.pfnTaskEntry = (TSK_ENTRY_FUNC)Task01;
    taskInitParam.uwStackSize = LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE;
    taskInitParam.pcName = "SmpCpup005_task01";
    taskInitParam.usTaskPrio = TASK_PRIO_TEST - 2; // 2, used to calculate the task priority.
#if (LOSCFG_KERNEL_SMP == YES)
    /* make sure that created test task is definitely on another core */
    UINT32 currCpuid = (ArchCurrCpuid() + 1) % LOSCFG_KERNEL_CORE_NUM;
    taskInitParam.usCpuAffiMask = CPUID_TO_AFFI_MASK(currCpuid);
#endif
    ret = LOS_TaskCreate(&g_testSmpCpupTaskID01, &taskInitParam);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    (VOID)memset_s((void *)(&taskInitParam), sizeof(TSK_INIT_PARAM_S), 0, sizeof(TSK_INIT_PARAM_S));
    taskInitParam.pfnTaskEntry = (TSK_ENTRY_FUNC)Task02;
    taskInitParam.uwStackSize = LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE;
    taskInitParam.pcName = "SmpCpup005_task02";
    taskInitParam.usTaskPrio = TASK_PRIO_TEST - 1; // 1, used to calculate the task priority.
#if (LOSCFG_KERNEL_SMP == YES)
    taskInitParam.usCpuAffiMask = CPUID_TO_AFFI_MASK(ArchCurrCpuid());
#endif
    ret = LOS_TaskCreate(&g_testSmpCpupTaskID02, &taskInitParam);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EIXT1);

    LOS_TaskDelay(LOSCFG_BASE_CORE_TICK_PER_SECOND * (CPUP_BACKWARD * 3 + 1)); // 3, used to calculate the delay time.

EIXT1:
    LOS_TaskDelete(g_testSmpCpupTaskID02);
EXIT:
    LOS_TaskDelete(g_testSmpCpupTaskID01);

    return LOS_OK;
}

VOID ItSmpExtendCpup005(VOID)
{
    TEST_ADD_CASE("ItSmpExtendCpup005", Testcase, TEST_EXTEND, TEST_CPUP, TEST_LEVEL2, TEST_FUNCTION);
}
#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
