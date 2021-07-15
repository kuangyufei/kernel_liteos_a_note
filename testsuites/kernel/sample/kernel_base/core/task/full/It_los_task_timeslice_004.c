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

#include "It_los_task.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cpluscplus */
#endif /* __cpluscplus */

UINT32 g_cmsisRobinTestCount2 = 0;
static void TaskF02(void const * argument)
{
    UINT32 ret;

    ICUNIT_GOTO_EQUAL(g_timesliceTestCount, 1, ret, EXIT);
    g_timesliceTestCount++;
    return;

EXIT:;
}

static void SwtmrF01(UINT32 *arg)
{
    // 100, set g_cmsisRobinTestCount2 num
    g_cmsisRobinTestCount2 = 100;
    return;
}

static void TaskF01(void const * argument)
{
    UINT32 ret;
    UINT32 arg;
    UINT32 intSave;

    ICUNIT_GOTO_EQUAL(g_timesliceTestCount, 0, ret, EXIT);
    g_timesliceTestCount++;

    ret = LOS_SwtmrCreate(1, LOS_SWTMR_MODE_PERIOD, (SWTMR_PROC_FUNC)SwtmrF01, &g_swTmrID, 0xffff);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    ret = LOS_SwtmrStart(g_swTmrID);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    intSave = LOS_IntLock();
    g_itTimesliceTestCount1 = TestTickCountGet();

    LOS_IntRestore(intSave);

    while (1) {
        dprintf("");
        intSave = LOS_IntLock();

        if ((g_itTimesliceTestCount1 + LOSCFG_BASE_CORE_TIMESLICE_TIMEOUT) < TestTickCountGet()) {
            LOS_IntRestore(intSave);
            break;
        }

        LOS_IntRestore(intSave);
    }

    // 2, assert that result is equal to this.
    ICUNIT_GOTO_EQUAL(g_timesliceTestCount, 2, ret, EXIT);
    g_timesliceTestCount++;

    // 100, assert that result is equal to this.
    if (g_cmsisRobinTestCount2 == 100) {
        // 2, set g_cmsisRobinTestCount2 for future Tests.
        g_cmsisRobinTestCount2 = 2;
    } else {
        dprintf("ERROR It_Timeslice_004_f01 g_cmsis_robinTestCount %d\n", g_cmsisRobinTestCount2);
    }

    ret = LOS_SwtmrDelete(g_swTmrID); // Los_SwTmrDelete
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);
    return;

EXIT:
    ret = LOS_SwtmrDelete(g_swTmrID); // Los_SwTmrDelete
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    // 2, set g_cmsisRobinTestCount2 for future Tests.
    g_cmsisRobinTestCount2 = 2;

    return;
}

static UINT32 Testcase(void)
{
    UINT32 ret;

    g_cmsisRobinTestCount2 = 0;
    g_itTimesliceTestCount1 = 0;
    g_timesliceTestCount = 0;

    LOS_TaskLock();
    TSK_INIT_PARAM_S task;

    task.pfnTaskEntry = (TSK_ENTRY_FUNC)TaskF01;
    task.pcName = "TimesTsk004A";
    // 3, Used to calculate stack space
    task.uwStackSize = TASK_STACK_SIZE_TEST * 3;
    task.usTaskPrio = TASK_PRIO_TEST_TASK - 1;
    task.uwResved = LOS_TASK_STATUS_DETACHED;
#ifdef LOSCFG_KERNEL_SMP
    task.usCpuAffiMask = CPUID_TO_AFFI_MASK(ArchCurrCpuid());
#endif

    ret = LOS_TaskCreate(&g_testTaskID01, &task);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    task.pfnTaskEntry = (TSK_ENTRY_FUNC)TaskF02;
    task.pcName = "TimesTsk004B";
    task.uwStackSize = TASK_STACK_SIZE_TEST;
    task.usTaskPrio = TASK_PRIO_TEST_TASK - 1;
    task.uwResved = LOS_TASK_STATUS_DETACHED;
#ifdef LOSCFG_KERNEL_SMP
    task.usCpuAffiMask = CPUID_TO_AFFI_MASK(ArchCurrCpuid());
#endif

    ret = LOS_TaskCreate(&g_testTaskID02, &task);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);
    LOS_TaskUnlock();

    return LOS_OK;
}

void ItLosTaskTimeslice004(void) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("ItLosTaskTIMESLICE_004", Testcase, TEST_LOS, TEST_TASK, TEST_LEVEL0, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
