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

static UINT32 g_testSmpCpupTaskID01, g_testSmpCpupTaskID02;
static volatile UINT32 g_testSmpCpupStop = 0;
static void Task01(void)
{
    while (g_testSmpCpupStop < 1) {
        __asm__ volatile("nop");
    }
    return;
}
static void Task02(void)
{
    UINT32 tempCpup;

    LOS_TaskDelay(LOSCFG_BASE_CORE_TICK_PER_SECOND * CPUP_BACKWARD);
    tempCpup = LOS_HistoryTaskCpuUsage(g_testSmpCpupTaskID01, CPUP_LAST_ONE_SECONDS);
    ICUNIT_GOTO_WITHIN_EQUAL(tempCpup, LOS_CPUP_SINGLE_CORE_PRECISION - CPUP_TEST_TOLERANT,
        LOS_CPUP_SINGLE_CORE_PRECISION, tempCpup, EXIT2);

EXIT2:
    g_testSmpCpupStop = 1;

    LOS_TaskDelay(LOSCFG_BASE_CORE_TICK_PER_SECOND * CPUP_BACKWARD);
    tempCpup = LOS_HistoryTaskCpuUsage(g_testSmpCpupTaskID01, CPUP_LAST_ONE_SECONDS);
    ICUNIT_ASSERT_EQUAL_VOID(tempCpup, CPU_USE_MIN, tempCpup);

    return;
}


static UINT32 Testcase(VOID)
{
    TSK_INIT_PARAM_S taskInitParam;
    UINT32 ret;

    (VOID)memset_s((void *)(&taskInitParam), sizeof(TSK_INIT_PARAM_S), 0, sizeof(TSK_INIT_PARAM_S));
    taskInitParam.pfnTaskEntry = (TSK_ENTRY_FUNC)Task01;
    taskInitParam.uwStackSize = LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE;
    taskInitParam.pcName = "SmpCpup001_task01";
    taskInitParam.usTaskPrio = TASK_PRIO_TEST - 2; // 2, used to calculate the task priority.
#ifdef LOSCFG_KERNEL_SMP
    /* make sure that created test task is definitely on another core */
    UINT32 currCpuid = (ArchCurrCpuid() + 1) % LOSCFG_KERNEL_CORE_NUM;
    taskInitParam.usCpuAffiMask = CPUID_TO_AFFI_MASK(currCpuid);
#endif
    ret = LOS_TaskCreate(&g_testSmpCpupTaskID01, &taskInitParam);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    (VOID)memset_s((void *)(&taskInitParam), sizeof(TSK_INIT_PARAM_S), 0, sizeof(TSK_INIT_PARAM_S));
    taskInitParam.pfnTaskEntry = (TSK_ENTRY_FUNC)Task02;
    taskInitParam.uwStackSize = LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE;
    taskInitParam.pcName = "SmpCpup001_task02";
    taskInitParam.usTaskPrio = TASK_PRIO_TEST - 1; // 1, used to calculate the task priority.
#ifdef LOSCFG_KERNEL_SMP
    taskInitParam.usCpuAffiMask = CPUID_TO_AFFI_MASK(ArchCurrCpuid());
#endif
    ret = LOS_TaskCreate(&g_testSmpCpupTaskID02, &taskInitParam);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EIXT1);

    LOS_TaskDelay(LOSCFG_BASE_CORE_TICK_PER_SECOND * (CPUP_BACKWARD * 2 + 1)); // 2, used to calculate the delay time.

EIXT1:
    LOS_TaskDelete(g_testSmpCpupTaskID02);
EXIT:
    LOS_TaskDelete(g_testSmpCpupTaskID01);

    return LOS_OK;
}

VOID ItSmpExtendCpup001(VOID)
{
    TEST_ADD_CASE("ItSmpExtendCpup001", Testcase, TEST_EXTEND, TEST_CPUP, TEST_LEVEL2, TEST_FUNCTION);
}
#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
