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

static void TaskF02(void)
{
    UINT32 ret;

    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 0, g_testCount);

    g_testCount++;
}

static void TaskF01(void)
{
    UINT32 ret;
    TSK_INIT_PARAM_S task1 = { 0 };
    task1.pfnTaskEntry = (TSK_ENTRY_FUNC)TaskF02;
    task1.uwStackSize = TASK_STACK_SIZE_TEST;
    task1.pcName = "Tsk097B";
    // 3, It is used to calculate a priority relative to TASK_PRIO_TEST_TASK.
    task1.usTaskPrio = TASK_PRIO_TEST_TASK - 3;
    task1.uwResved = 0;
    task1.processID = LOS_GetCurrProcessID();
#ifdef LOSCFG_KERNEL_SMP
    task1.usCpuAffiMask = CPUID_TO_AFFI_MASK(ArchCurrCpuid());
#endif
    task1.processID = LOS_GetCurrProcessID();

    ret = LOS_TaskCreateOnly(&g_testTaskID02, &task1);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);
    ICUNIT_GOTO_EQUAL(g_testCount, 0, g_testCount, EXIT);

    ret = LOS_SetTaskScheduler(g_testTaskID02, LOS_SCHED_RR, task1.usTaskPrio);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    ICUNIT_GOTO_EQUAL(g_testCount, 1, g_testCount, EXIT);
    g_testCount++;

EXIT:
    LOS_TaskDelete(g_testTaskID01);
    return;
}

static UINT32 Testcase(void)
{
    UINT32 ret;
    TSK_INIT_PARAM_S task1 = { 0 };
    task1.pfnTaskEntry = (TSK_ENTRY_FUNC)TaskF01;
    task1.uwStackSize = TASK_STACK_SIZE_TEST;
    task1.pcName = "Tsk097A";
    // 2, It is used to calculate a priority relative to TASK_PRIO_TEST_TASK.
    task1.usTaskPrio = TASK_PRIO_TEST_TASK - 2;
    task1.uwResved = LOS_TASK_STATUS_DETACHED;
#ifdef LOSCFG_KERNEL_SMP
    task1.usCpuAffiMask = CPUID_TO_AFFI_MASK(ArchCurrCpuid());
#endif
    task1.processID = -1;
    g_testCount = 0;
    ret = LOS_TaskCreate(&g_testTaskID01, &task1);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    LOS_TaskDelay(10); // 10, set delay time.

    ICUNIT_GOTO_EQUAL(g_testCount, 2, g_testCount, EXIT); // 2, assert that g_testCount is equal to this.

    ret = LOS_TaskDelete(g_testTaskID01);
    ICUNIT_GOTO_EQUAL(ret, LOS_ERRNO_TSK_NOT_CREATED, ret, EXIT);

    return LOS_OK;

EXIT:
    LOS_TaskDelete(g_testTaskID01);

    return LOS_OK;
}

void ItLosTask097(void) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("ItLosTask097", Testcase, TEST_LOS, TEST_TASK, TEST_LEVEL0, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
